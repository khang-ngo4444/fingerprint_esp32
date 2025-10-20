import json
import psycopg2
import paho.mqtt.client as mqtt
from datetime import datetime
import base64

# Hàm giải mã template từ base64 sang bytes và list[int]
def decode_fingerprint_template(template_b64):
    try:
        template_bytes = base64.b64decode(template_b64)
        template_ints = list(template_bytes)
        print(f"[INFO] Độ dài template (bytes): {len(template_bytes)}")
        print(f"[INFO] Mẫu dữ liệu đầu tiên: {template_ints[:16]} ...")
        return template_bytes, template_ints
    except Exception as e:
        print(f"[ERROR] Lỗi giải mã template: {e}")
        return b'', []

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

        # Nếu có template vân tay gửi lên
        if "template" in data and "user_id" in data:
            user_id = data["user_id"]
            template_b64 = data["template"]
            print(f"[MQTT] Nhận template vân tay cho user_id={user_id}")
            template_bytes, template_ints = decode_fingerprint_template(template_b64)

            # Lưu template vào DB
            cursor.execute("""
                UPDATE users SET fingerprint_template = %s WHERE id = %s
            """, (psycopg2.Binary(template_bytes), user_id))
            conn.commit()
            print(f"[DB] Đã lưu template cho user_id={user_id}, bytes={len(template_bytes)}")

            # Phản hồi về ESP32
            response = {
                "result": "template_saved",
                "user_id": user_id,
                "length": len(template_bytes)
            }
            client.publish("fingerprint/access", json.dumps(response))
            print("Template saved to server!")

        # Check if this is a scan event from fingerprint/scan topic
        elif "fingerprint_id" in data:
            fingerprint_id = data["fingerprint_id"]
            confidence = data["confidence"]
            # ...existing code...
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
                # ...existing code...
                cursor.execute("""
                    SELECT check_type FROM checkin_logs
                    WHERE user_id = %s
                    ORDER BY timestamp DESC, id DESC
                    LIMIT 1
                """, (fingerprint_id,))
                last_log = cursor.fetchone()
                if last_log and last_log[0] == "checkin":
                    check_type = "checkout"
                else:
                    check_type = "checkin"
                print("🔍 USER DETECTED")
                print(f"   👤 Name: {name}")
                print(f"   💼 Role: {role}")
                print(f"   🏢 Department: {department}") 
                print(f"   📍 Status: {check_type.upper()}")
                cursor.execute("""
                    INSERT INTO checkin_logs (user_id, check_type)
                    VALUES (%s, %s)
                """, (fingerprint_id, check_type))
                conn.commit()
                response = {
                    "result": "access_granted",
                    "name": name,
                    "role": role if role else "Unknown",
                    "status": check_type
                }
                client.publish("fingerprint/access", json.dumps(response))
                print("✅ Access granted and logged successfully!")
            else:
                print(f"❌ User ID {fingerprint_id} not found in database")
                error_response = {
                    "result": "access_denied",
                    "message": "User not found in database"
                }
                client.publish("fingerprint/access", json.dumps(error_response))

        # Handle original fingerprint/attendance messages for backward compatibility
        elif "user_id" in data:
            user_id = data["user_id"]
            check_type = data["check_type"]
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
                print("🔍 USER DETECTED")
                print(f"   👤 Name: {name}")
                print(f"   💼 Role: {role}")
                print(f"   🏢 Department: {department}") 
                print(f"   📍 Action: {check_type.upper()}")
                cursor.execute("""
                    INSERT INTO checkin_logs (user_id, check_type)
                    VALUES (%s, %s)
                """, (user_id, check_type))
                conn.commit()
                response = {
                    "status": "success",
                    "user_name": name,
                    "role": role if role else "Unknown"
                }
                client.publish("fingerprint/access", json.dumps(response))
                print("Logged successfully!")
            else:
                print(f"User ID {user_id} not found in database")
                error_response = {
                    "status": "error",
                    "message": "User not found"
                }
                client.publish("fingerprint/access", json.dumps(error_response))

    except Exception as e:
        print(f"Error: {e}")
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
# client.connect("172.16.5.124", 1883, 60)
client.connect("192.168.1.14", 1883, 60)
client.loop_forever()