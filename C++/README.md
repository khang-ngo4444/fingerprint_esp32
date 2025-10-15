# Secure Fingerprint Storage System for Raspberry Pi

This project implements a secure fingerprint storage system that runs on a Raspberry Pi. It receives fingerprint data from an ESP32 via MQTT, encrypts it using homomorphic encryption with the SEAL-Embedded library, and stores the encrypted data in a PostgreSQL database.

## Architecture

The system consists of the following components:

1.  **ESP32 with Fingerprint Scanner**: Captures fingerprint data and publishes it to an MQTT topic. (This component is external to this project).
2.  **MQTT Broker**: A message broker (like Mosquitto) that facilitates communication between the ESP32 and the Raspberry Pi application.
3.  **Raspberry Pi Application (This Project)**:
    *   Subscribes to the `fingerprint/scan` MQTT topic.
    *   Receives fingerprint data.
    *   Encrypts the data using the BFV scheme from the `SEAL-Embedded` library.
    *   Stores the encrypted ciphertext in a PostgreSQL database.
4.  **PostgreSQL Database**: A relational database used to store the encrypted fingerprint data.

## Dependencies

Before you can build and run this application, you need to install the following dependencies on your Raspberry Pi:

### 1. C++ Compiler and Build Tools

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
```

### 2. SEAL-Embedded Library

```bash
git clone https://github.com/microsoft/SEAL-Embedded.git
cd SEAL-Embedded
cmake .
make
sudo make install
```

### 3. Paho C++ MQTT Client

```bash
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
make
sudo make install

cd ..

git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
cmake .
make
sudo make install
```

### 4. PostgreSQL Client Library (`libpqxx`)

```bash
sudo apt-get install -y libpq-dev libpqxx-dev
```

### 5. PostgreSQL Server and MQTT Broker

You'll also need a PostgreSQL server and an MQTT broker running. You can install them on the Raspberry Pi or use a separate server.

```bash
# Install PostgreSQL
sudo apt-get install -y postgresql

# Install Mosquitto MQTT Broker
sudo apt-get install -y mosquitto mosquitto-clients
```

## Database Setup

1.  Switch to the `postgres` user:

    ```bash
    sudo -i -u postgres
    ```

2.  Create a new database user (e.g., `your_user`):

    ```bash
    createuser --interactive
    ```

3.  Create the `checkin_system` database:

    ```bash
    createdb checkin_system
    ```

4.  Connect to the database and set a password for your user:

    ```bash
    psql
    ALTER USER your_user WITH PASSWORD 'your_password';
    \q
    ```

5.  Exit the `postgres` user session:

    ```bash
    exit
    ```

## Building the Application

1.  Clone this repository:

    ```bash
    git clone <repository_url>
    cd <repository_directory>
    ```

2.  Update the database connection string in `main.cpp`:

    Open [`main.cpp`](main.cpp:15) and modify the `DB_CONNECTION_STRING` with your PostgreSQL credentials.

3.  Create a build directory and run CMake:

    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```

## Running the Application

Once the build is complete, you can run the application from the `build` directory:

```bash
./secure_fingerprint_storage
```

The application will connect to the MQTT broker and the PostgreSQL database. It will then wait for messages on the `fingerprint/scan` topic. When a message is received, it will be encrypted and stored in the database.

## Testing

You can test the application by publishing a message to the MQTT topic using `mosquitto_pub`:

```bash
mosquitto_pub -h localhost -t "fingerprint/scan" -m "12345"
```

You should see output in the application console indicating that the message was received, encrypted, and stored. You can also verify the data in the `fingerprints` table in your `checkin_system` database.