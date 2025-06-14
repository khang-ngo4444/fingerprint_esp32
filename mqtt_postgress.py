import json
import psycopg2
import paho.mqtt.client as mqtt
from datetime import datetime

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
    client.subscribe("fingerprint/attendance")
    print("Subscribed to fingerprint/attendance")
    print("="*50)

# Khi có message
def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] Message received: {msg.payload.decode()}")
    
    try:
        data = json.loads(msg.payload)
        user_id = data["user_id"]
        check_type = data["check_type"]

        # Query thông tin user
        cursor.execute("""
            SELECT name, role, department, room 
            FROM users 
            WHERE id = %s
        """, (user_id,))
        
        user_info = cursor.fetchone()
        
        if user_info:
            name, role, department, room = user_info
            
            # Hiển thị thông tin đẹp
            print("🔍 USER DETECTED")
            print(f"   👤 Name: {name}")
            print(f"   💼 Role: {role}")
            print(f"   🏢 Department: {department}") 
            print(f"   🚪 Room: {room}")
            print(f"   📍 Action: {check_type.upper()}")
            
            # Insert checkin log
            cursor.execute("""
                INSERT INTO checkin_logs (user_id, check_type)
                VALUES (%s, %s)
            """, (user_id, check_type))
            conn.commit()
            
            # Gửi notification trở lại ESP32 (tùy chọn)
            response = {
                "status": "success",
                "user_name": name,
                "role": role,
                "room": room
            }
            client.publish("fingerprint/response", json.dumps(response))
            
            print("✅ Logged successfully!")
            
        else:
            print(f"❌ User ID {user_id} not found in database")
            
            # Gửi lỗi trở lại ESP32
            error_response = {
                "status": "error",
                "message": "User not found"
            }
            client.publish("fingerprint/response", json.dumps(error_response))
            
    except Exception as e:
        print(f"❌ Error: {e}")
        conn.rollback()
        
        error_response = {
            "status": "error", 
            "message": "System error"
        }
        client.publish("fingerprint/response", json.dumps(error_response))
    
    print("-" * 50)

# Tạo client với callback API version 2
client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

print(" Starting MQTT to PostgreSQL Bridge...")
print(" Connecting to MQTT Broker at 172.16.5.124:1883")

# Kết nối tới đúng địa chỉ IP của MQTT broker
client.connect("172.16.5.124", 1883, 60)
client.loop_forever()