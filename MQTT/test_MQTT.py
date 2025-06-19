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
    client.subscribe("fingerprint/scan")
    print("Subscribed to fingerprint/scan")
    print("="*50)

# Khi có message
def on_message(client, userdata, msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] Message received: {msg.payload.decode()}")
    
    try:
        data = json.loads(msg.payload)
        
        # Check if this is a scan event from fingerprint/scan topic
        if "fingerprint_id" in data:
            fingerprint_id = data["fingerprint_id"]
            confidence = data["confidence"]
            
            # Query thông tin user với join tables
            cursor.execute("""
                SELECT u.name, r.name as role, d.name as department 
                FROM users u
                LEFT JOIN roles r ON u.role_id = r.id
                LEFT JOIN departments d ON u.department_id = d.id
                WHERE u.id = %s
            """, (fingerprint_id,))
            
            user_info = cursor.fetchone()
            
            if user_info:
                name, role, department = user_info
                
                # Hiển thị thông tin đẹp
                print("🔍 USER DETECTED")
                print(f"   👤 Name: {name}")
                print(f"   💼 Role: {role}")
                print(f"   🏢 Department: {department}") 
                
                # Insert checkin log
                check_type = "checkin"  # Default to checkin
                cursor.execute("""
                    INSERT INTO checkin_logs (user_id, check_type)
                    VALUES (%s, %s)
                """, (fingerprint_id, check_type))
                conn.commit()
                
                # Gửi response trở lại ESP32 với topic fingerprint/access
                response = {
                    "result": "access_granted",
                    "name": name,
                    "role": role if role else "Unknown"
                }
                client.publish("fingerprint/access", json.dumps(response))
                
                print("✅ Access granted and logged successfully!")
                
            else:
                print(f"❌ User ID {fingerprint_id} not found in database")
                
                # Gửi lỗi trở lại ESP32 với topic fingerprint/access
                error_response = {
                    "result": "access_denied",
                    "message": "User not found in database"
                }
                client.publish("fingerprint/access", json.dumps(error_response))
                
        # Handle original fingerprint/attendance messages for backward compatibility
        elif "user_id" in data:
            user_id = data["user_id"]
            check_type = data["check_type"]

            # Query thông tin user với join tables
            cursor.execute("""
                SELECT u.name, r.name as role, d.name as department 
                FROM users u
                LEFT JOIN roles r ON u.role_id = r.id
                LEFT JOIN departments d ON u.department_id = d.id
                WHERE u.id = %s
            """, (user_id,))
            
            user_info = cursor.fetchone()
            
            if user_info:
                name, role, department = user_info
                
                # Hiển thị thông tin đẹp
                print("🔍 USER DETECTED")
                print(f"   👤 Name: {name}")
                print(f"   💼 Role: {role}")
                print(f"   🏢 Department: {department}") 
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
                    "role": role if role else "Unknown"
                }
                client.publish("fingerprint/access", json.dumps(response))
                
                print("✅ Logged successfully!")
                
            else:
                print(f"❌ User ID {user_id} not found in database")
                
                # Gửi lỗi trở lại ESP32
                error_response = {
                    "status": "error",
                    "message": "User not found"
                }
                client.publish("fingerprint/access", json.dumps(error_response))
                
    except Exception as e:
        print(f"❌ Error: {e}")
        conn.rollback()
        
        error_response = {
            "result": "access_denied", 
            "message": f"System error: {str(e)}"
        }
        client.publish("fingerprint/access", json.dumps(error_response))
    
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