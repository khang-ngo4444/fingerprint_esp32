import json
import psycopg2
import paho.mqtt.client as mqtt

# Kết nối DB
conn = psycopg2.connect(
    dbname="checkin_system",
    user="esp_user",
    password="esp_password",
    host="localhost"
)
cursor = conn.cursor()

# Khi kết nối MQTT thành công
def on_connect(client, userdata, flags, reason_code, properties):
    print("MQTT connected with result code", reason_code)
    client.subscribe("fingerprint/data")

# Khi có message
def on_message(client, userdata, msg):
    print("Message received:", msg.payload)
    try:
        data = json.loads(msg.payload)
        user_id = data["user_id"]
        check_type = data["check_type"]

        cursor.execute("""
            INSERT INTO checkin_logs (user_id, check_type)
            VALUES (%s, %s)
        """, (user_id, check_type))
        conn.commit()
        print("Logged checkin for user:", user_id)
    except Exception as e:
        print("Error:", e)
        # Rollback transaction khi có lỗi
        conn.rollback()

# Tạo client với callback API version 2
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

# Kết nối tới đúng địa chỉ IP của MQTT broker
# client.connect("172.16.5.124", 1883, 60)
client.connect("192.168.1.14", 1883, 60)
client.loop_forever()