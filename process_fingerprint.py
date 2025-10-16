# ==========================================
# process_fingerprint.py
# Xử lý dữ liệu vân tay từ ESP32
# Giải mã Base64 -> bytes -> list[int]
# ==========================================

import base64
import json
import sys

def process_template(template_b64: str):
    """
    Giải mã template từ Base64 sang danh sách số nguyên.
    :param template_b64: chuỗi Base64 nhận từ ESP32
    :return: list các giá trị int (0–255)
    """
    try:
        # Giải mã base64 -> bytes
        template_bytes = base64.b64decode(template_b64)
        print(f"[INFO] Độ dài template (bytes): {len(template_bytes)}")

        # Chuyển bytes -> list[int]
        template_ints = list(template_bytes)
        print(f"[INFO] Mẫu dữ liệu đầu tiên: {template_ints[:16]} ...")

        return template_ints
    except Exception as e:
        print(f"[ERROR] Lỗi giải mã template: {e}")
        return []

# Chạy độc lập (CLI)
if __name__ == "__main__":
    # Nếu chạy file trực tiếp bằng terminal, ví dụ:
    # python3 process_fingerprint.py '{"template": "QkM0U0JDQT...=="}'
    if len(sys.argv) < 2:
        print("Vui lòng truyền JSON có khóa 'template'")
        sys.exit(1)

    # Đọc JSON từ command-line
    try:
        data = json.loads(sys.argv[1])
        if "template" not in data:
            raise KeyError("Thiếu khóa 'template'")
        result = process_template(data["template"])
        print("\nKết quả (list int):")
        print(result)
    except Exception as e:
        print(f"[ERROR] Không thể đọc dữ liệu JSON: {e}")
