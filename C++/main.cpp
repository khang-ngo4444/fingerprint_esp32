#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <mqtt/async_client.h>
#include <pqxx/pqxx>
#include <seal/seal.h>

using namespace std;

// MQTT Configuration
const string MQTT_SERVER_ADDRESS = "tcp://localhost:1883";
const string MQTT_CLIENT_ID = "fingerprint_storage_client";
const string MQTT_TOPIC = "fingerprint/scan";

// PostgreSQL Configuration
const string DB_CONNECTION_STRING = "dbname=checkin_system user=your_user password=your_password hostaddr=127.0.0.1 port=5432";

// SEAL Configuration
const size_t POLY_MODULUS_DEGREE = 4096;
const int PLAIN_MODULUS = 1024;

class MqttCallback : public virtual mqtt::callback {
    seal::SEALContext& context_;
    seal::Encryptor& encryptor_;
    pqxx::connection& db_connection_;

public:
    MqttCallback(seal::SEALContext& context, seal::Encryptor& encryptor, pqxx::connection& db_conn)
        : context_(context), encryptor_(encryptor), db_connection_(db_conn) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        cout << "Message arrived on topic: " << msg->get_topic() << endl;
        string payload = msg->to_string();
        cout << "Payload: " << payload << endl;

        try {
            // Parse the string payload to an integer
            int fingerprint_id = stoi(payload);

            // Convert the integer to a hex string for SEAL
            stringstream hex_ss;
            hex_ss << hex << fingerprint_id;
            string hex_payload = hex_ss.str();

            // Encrypt the fingerprint data
            seal::Plaintext plaintext(hex_payload);
            seal::Ciphertext ciphertext;
            encryptor_.encrypt(plaintext, ciphertext);

            // Serialize the ciphertext to a string for storage
            stringstream ss;
            ciphertext.save(ss);
            string ciphertext_str = ss.str();

            // Store the encrypted data in the database
            pqxx::work txn(db_connection_);
            txn.exec("INSERT INTO fingerprints (encrypted_data) VALUES (" + txn.quotea(vector<unsigned char>(ciphertext_str.begin(), ciphertext_str.end())) + ")");
            txn.commit();
            cout << "Encrypted fingerprint data for ID " << fingerprint_id << " stored in the database." << endl;

        } catch (const invalid_argument& ia) {
            cerr << "Invalid argument: Could not convert payload to integer. " << ia.what() << endl;
        } catch (const out_of_range& oor) {
            cerr << "Out of Range error: Payload is out of range for an integer. " << oor.what() << endl;
        } catch (const exception& e) {
            cerr << "Error processing message: " << e.what() << endl;
        }
    }
};

int main() {
    // Initialize SEAL
    seal::EncryptionParameters parms(seal::scheme_type::bfv);
    parms.set_poly_modulus_degree(POLY_MODULUS_DEGREE);
    parms.set_coeff_modulus(seal::CoeffModulus::BFVDefault(POLY_MODULUS_DEGREE));
    parms.set_plain_modulus(PLAIN_MODULUS);
    seal::SEALContext context(parms);

    seal::KeyGenerator keygen(context);
    seal::SecretKey secret_key = keygen.secret_key();
    seal::PublicKey public_key;
    keygen.create_public_key(public_key);

    seal::Encryptor encryptor(context, public_key);

    // Initialize PostgreSQL connection
    pqxx::connection db_connection(DB_CONNECTION_STRING);
    if (!db_connection.is_open()) {
        cerr << "Can't open database" << endl;
        return 1;
    }
    cout << "Connected to database: " << db_connection.dbname() << endl;

    // Create table if it doesn't exist
    try {
        pqxx::work txn(db_connection);
        txn.exec("CREATE TABLE IF NOT EXISTS fingerprints (id SERIAL PRIMARY KEY, encrypted_data BYTEA NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)");
        txn.commit();
    } catch (const exception& e) {
        cerr << "Error creating table: " << e.what() << endl;
    }

    // Initialize MQTT client
    mqtt::async_client client(MQTT_SERVER_ADDRESS, MQTT_CLIENT_ID);
    MqttCallback cb(context, encryptor, db_connection);
    client.set_callback(cb);

    auto connOpts = mqtt::connect_options_builder()
        .keep_alive_interval(chrono::seconds(20))
        .clean_session(true)
        .finalize();

    try {
        cout << "Connecting to MQTT broker..." << endl;
        client.connect(connOpts)->wait();
        cout << "Connected." << endl;
        client.subscribe(MQTT_TOPIC, 1)->wait();
        cout << "Subscribed to topic: " << MQTT_TOPIC << endl;

        while (true) {
            this_thread::sleep_for(chrono::seconds(1));
        }

    } catch (const mqtt::exception& exc) {
        cerr << "MQTT Error: " << exc.what() << endl;
        return 1;
    }

    return 0;
}