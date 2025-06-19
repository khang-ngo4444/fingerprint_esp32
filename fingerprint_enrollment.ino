#include <WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <PubSubClient.h> // Added for MQTT
#include <ArduinoJson.h>  // Added for JSON formatting

// Definir biến và thiết lập ban đầu
// WiFi credentials
extern const char* ssid;
extern const char* password;

// MQTT Configuration
extern const char* mqtt_server;
extern const char* mqtt_port;
extern const char* mqtt_user;
extern const char* mqtt_password;
extern const char* mqtt_client_id;
extern const char* mqtt_topic_status;

// References to external objects
extern WiFiClient espClient;
extern PubSubClient mqtt;
extern LiquidCrystal_I2C lcd;
extern HardwareSerial mySerial;
extern Adafruit_Fingerprint finger;
extern Keypad customKeypad;
extern unsigned long lastActivity;
extern unsigned long systemStartTime;

// Employee data structure reference
extern struct Employee {
  int id;
  char name[20];
  bool isActive;
  unsigned long lastCheckIn;
  unsigned long lastCheckOut;
};
extern Employee employees[50];
extern int employeeCount;

// Hardware pins
extern const int BUZZER;
extern const int RELAY;

// EEPROM addresses
extern const int EEPROM_SIZE;
extern const int EMPLOYEE_DATA_START;
extern const int ATTENDANCE_DATA_START;

// Function declarations from main file (to be used here)
extern void displayWelcomeScreen();
extern void displayAdminMenu();
extern void playSuccessSound();
extern void playErrorSound();
extern String getNumericInput();
extern void saveEmployeeData();

// Functions related to fingerprint enrollment

/**
 * Enroll a new fingerprint with the provided ID
 */
bool enrollFingerprint(int id) {
  Serial.print("Enrolling fingerprint ID #");
  Serial.println(id);
  
  int p = -1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger");
  lcd.setCursor(0, 1);
  lcd.print("on sensor...");
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcd.setCursor(0, 2);
      lcd.print("Image captured");
      break;
    case FINGERPRINT_NOFINGER:
      break;
    default:
      lcd.setCursor(0, 2);
      lcd.print("Error occurred");
      break;
    }
    delay(50);
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 3);
    lcd.print("Conversion error");
    return false;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove finger");
  delay(2000);
  
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(50);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place same finger");
  lcd.setCursor(0, 1);
  lcd.print("again...");
  
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      lcd.setCursor(0, 2);
      lcd.print("Image captured");
      break;
    case FINGERPRINT_NOFINGER:
      break;
    default:
      lcd.setCursor(0, 2);
      lcd.print("Error occurred");
      break;
    }
    delay(50);
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 3);
    lcd.print("Conversion error");
    return false;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Creating model...");
  
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Model creation failed");
    return false;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Stored successfully!");
    
    // Notify MQTT that a new fingerprint was enrolled
    if (mqtt.connected()) {
      mqtt.publish(mqtt_topic_status, String("New fingerprint enrolled, ID: " + String(id)).c_str());
    }
    
    return true;
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Storage failed");
    return false;
  }
}

/**
 * Function for adding a new employee with fingerprint
 */
void addEmployee() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADD EMPLOYEE");
  
  // Get employee ID
  lcd.setCursor(0, 1);
  lcd.print("Enter ID (1-127):");
  
  String idInput = getNumericInput();
  int empId = idInput.toInt();
  
  if (empId < 1 || empId > 127) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalid ID!");
    delay(2000);
    displayAdminMenu();
    return;
  }
  
  // Skip name input - names will be set from dashboard/server
  // Just store placeholder - actual name will come from server
  String empName = "";
  
  // Enroll fingerprint
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger on");
  lcd.setCursor(0, 1);
  lcd.print("sensor...");
  
  if (enrollFingerprint(empId)) {
    // Save employee data with minimal info - just ID
    employees[employeeCount].id = empId;
    strncpy(employees[employeeCount].name, empName.c_str(), 19);
    employees[employeeCount].name[19] = '\0';
    employees[employeeCount].isActive = true;
    employees[employeeCount].lastCheckIn = 0;
    employees[employeeCount].lastCheckOut = 0;
    employeeCount++;
    
    saveEmployeeData();
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint Added!");
    lcd.setCursor(0, 1);
    lcd.print("ID: " + String(empId));
    playSuccessSound();
    
    // Notify MQTT server about new fingerprint
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "fingerprint_enrolled";
      doc["id"] = empId;
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
    
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed to enroll!");
    playErrorSound();
    delay(2000);
  }
  
  displayAdminMenu();
}

/**
 * Function to delete an employee with their fingerprint
 */
void deleteEmployee() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DELETE EMPLOYEE");
  lcd.setCursor(0, 1);
  lcd.print("Enter ID#:");
  
  String idInput = getNumericInput();
  int empId = idInput.toInt();
  
  // Find and remove employee
  bool found = false;
  String deletedName = "";
  
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == empId) {
      deletedName = String(employees[i].name);
      // Delete fingerprint from sensor
      finger.deleteModel(empId);
      
      // Shift array to remove employee
      for (int j = i; j < employeeCount - 1; j++) {
        employees[j] = employees[j + 1];
      }
      employeeCount--;
      found = true;
      break;
    }
  }
  
  if (found) {
    saveEmployeeData();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Employee Deleted!");
    playSuccessSound();
    
    // Notify MQTT server about employee deletion
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "employee_deleted";
      doc["id"] = empId;
      doc["name"] = deletedName;
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Employee Not Found!");
    playErrorSound();
  }
  
  delay(2000);
  displayAdminMenu();
}
