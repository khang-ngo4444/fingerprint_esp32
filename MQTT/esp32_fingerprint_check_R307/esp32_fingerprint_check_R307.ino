#include <WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <PubSubClient.h> // Added for MQTT
#include <ArduinoJson.h>  // Added for JSON formatting

// WiFi credentials
const char* ssid = "VNUK4-10";
const char* password = "Z@q12wsx";

// MQTT Configuration
const char* mqtt_server = "172.16.5.124";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt_user";       // Change if your broker requires authentication
const char* mqtt_password = "mqtt_password";   // Change if your broker requires authentication
const char* mqtt_client_id = "ESP32_Fingerprint_System";
const char* mqtt_topic_attendance = "fingerprint/attendance";
const char* mqtt_topic_status = "fingerprint/status";
const char* mqtt_topic_command = "fingerprint/command";
// --- ADDED central-authority topics ---
const char* TOPIC_SCAN   = "fingerprint/scan";
const char* TOPIC_ACCESS = "fingerprint/access";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// I2C LCD Display (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4); // Address 0x27, 20 chars, 4 lines

// Attendance logging in EEPROM
struct AttendanceRecord {
  int empId;
  char empName[10];
  char action[10]; // "IN" or "OUT"
  unsigned long timestamp; // millis() timestamp
};

#define MAX_ATTENDANCE_RECORDS 100
AttendanceRecord attendanceLog[MAX_ATTENDANCE_RECORDS];
int attendanceCount = 0;

// Hardware Serial for fingerprint sensor AS608
HardwareSerial mySerial(2); // Use Serial2 (GPIO16=RX, GPIO17=TX)
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Keypad setup (4x3 matrix)
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'}, 
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Define keypad pins (4x3 matrix)
byte rowPins[ROWS] = {13, 12, 14, 27}; // GPIO pins for rows
byte colPins[COLS] = {26, 25, 32, 33}; // GPIO pins for columns

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// System variables
String inputPassword = "";
String adminPassword = "1234"; // Admin password
bool adminMode = false;
unsigned long lastActivity = 0;
const unsigned long TIMEOUT = 30000; // 30 seconds timeout
unsigned long systemStartTime = 0; // System start time for relative timestamps

// MQTT related variables
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000; // 5 seconds between reconnect attempts
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 60000; // 1 minute between status updates

// Attendance data structure
struct Employee {
  int id;
  char name[20];
  bool isActive;
  unsigned long lastCheckIn;
  unsigned long lastCheckOut;
};

Employee employees[50]; // Support up to 50 employees
int employeeCount = 0;

// Relay pin (Buzzer removed)
#define RELAY 33 // For door lock

// EEPROM addresses
#define EEPROM_SIZE 4096  // Increased size for attendance records
#define EMPLOYEE_DATA_START 0
#define ATTENDANCE_DATA_START 2048  // Start attendance data at offset 2048

// --- ADDED central-authority state ---
bool  waitingForResponse = false;
int   lastFingerprintId  = -1;
int   lastConfidence     = 0;
unsigned long responseTimer = 0;
const unsigned long RESPONSE_TIMEOUT = 5000; // ms

// --- ADD THIS FORWARD DECLARATION ---
void setupMqtt();

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize pins
  // Buzzer pin initialization removed (sound disabled)
  pinMode(RELAY, OUTPUT);
  
  // Initial state
  digitalWrite(RELAY, LOW);
  
  // Connect to WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to WiFi");
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP()[0]); lcd.print(".");
    lcd.print(WiFi.localIP()[1]); lcd.print(".");
    lcd.print(WiFi.localIP()[2]); lcd.print(".");
    lcd.print(WiFi.localIP()[3]);

    // Setup MQTT connection
    setupMqtt();
    // Subscribe to central authority access topic
    mqtt.subscribe(TOPIC_ACCESS);

    delay(4000);
  } else {
    Serial.println("Failed to connect to WiFi.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connect Failed");
    delay(3000);
  }
  
  // Store system start time
  systemStartTime = millis();
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize fingerprint sensor AS608
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  
  if (finger.verifyPassword()) {
    Serial.println("AS608 Fingerprint sensor detected!");
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint: OK");
    
    // Publish sensor status to MQTT
    if (mqtt.connected()) {
      mqtt.publish(mqtt_topic_status, "Fingerprint sensor online", true);
    }
  } else {
    Serial.println("AS608 sensor not found!");
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint: ERROR");
    
    // Publish error status to MQTT
    if (mqtt.connected()) {
      mqtt.publish(mqtt_topic_status, "Fingerprint sensor ERROR", true);
    }
  }
  
  finger.getTemplateCount();
  Serial.print("Sensor contains "); 
  Serial.print(finger.templateCount); 
  Serial.println(" templates");
  
  // Load employee data from EEPROM
  loadEmployeeData();
  loadAttendanceData();
  
  // Display welcome message
  displayWelcomeScreen();
  
  Serial.println("=== ESP32 AttendanceSystem ===");
  Serial.println("Commands:");
  Serial.println("• Scan fingerprint for attendance");
  Serial.println("• Enter admin password + # for admin mode");
  Serial.println("• Admin commands: 1#=Add Employee, 2#=Delete, 3#=Report, 9#=Exit");
  Serial.println("===============================");
  
  lastActivity = millis();
  
  // Send initial status to Raspberry Pi
  sendSystemStatus();
}

void loop() {
  // Handle MQTT connection and messages
  if (!mqtt.connected()) {
    reconnectMqtt();
  }
  mqtt.loop();
  
  // Periodic status update to MQTT server
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
    sendSystemStatus();
    lastStatusUpdate = currentMillis;
  }
  
  // Check for timeout
  if (millis() - lastActivity > TIMEOUT && !adminMode) {
    resetToMainScreen();
  }
  
  // Check keypad input
  char customKey = customKeypad.getKey();
  if (customKey) {
    lastActivity = millis();
    handleKeypadInput(customKey);
  }
  
  // --- central-authority flow ---
  if (waitingForResponse) {
    if (millis() - responseTimer > RESPONSE_TIMEOUT) {
      waitingForResponse = false;
      denyAccess("Timeout");
    }
    return;
  }
  if (!adminMode) {
    int fingerprintResult = getFingerprintID();
    if (fingerprintResult >= 0) {
      lastFingerprintId = fingerprintResult;
      lastConfidence    = finger.confidence;
      publishScanEvent(fingerprintResult, lastConfidence);
      waitingForResponse = true;
      responseTimer      = millis();
      lcd.clear(); lcd.setCursor(0,0);
      lcd.print("Checking server...");
    } else if (fingerprintResult == -3) {
      // Fingerprint detected but not in database
      lastActivity = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("FINGERPRINT");
      lcd.setCursor(0, 1);
      lcd.print("NOT REGISTERED");
      lcd.setCursor(0, 2);
      lcd.print("Contact Admin to");
      lcd.setCursor(0, 3);
      lcd.print("register first!");
      
      Serial.println("Unregistered fingerprint detected");
      
      // Send unauthorized attempt to MQTT server
      if (mqtt.connected()) {
        DynamicJsonDocument doc(256);
        doc["event"] = "unauthorized";
        doc["timestamp"] = millis() - systemStartTime;
        doc["message"] = "Unregistered fingerprint detected";
        
        char buffer[256];
        size_t n = serializeJson(doc, buffer);
        mqtt.publish(mqtt_topic_status, buffer, n);
      }
      
      delay(4000);
      displayWelcomeScreen();
    }
    // fingerprintResult == -2 errors are already handled in getFingerprintID()
    // fingerprintResult == -1 means no finger detected (normal state)
  }
  
  delay(200);
}

// --- ADDED --- Publish fingerprint scan to Pi
void publishScanEvent(int fid, int conf) {
  DynamicJsonDocument doc(128);
  doc["fingerprint_id"] = fid;
  doc["confidence"]     = conf;
  char buf[128];
  size_t n = serializeJson(doc, buf);
  mqtt.publish(TOPIC_SCAN, buf, n);
}

// --- MODIFIED --- MQTT Callback Function for receiving commands and access decisions
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  if (t == TOPIC_ACCESS) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, payload, length);
    String result = doc["result"].as<String>();
    waitingForResponse = false;
    if (result == "access_granted") {
      grantAccess(doc["name"].as<String>(), doc["role"].as<String>());
    } else {
      denyAccess(doc["reason"].as<String>());
    }
    return;
  }
  // ...existing code for command topic...
  if (strcmp(topic, mqtt_topic_command) == 0) {
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload, length);
    if (!error) {
      String command = doc["command"].as<String>();
      if (command == "status") {
        sendSystemStatus();
      }
      else if (command == "sync") {
        syncAttendanceData();
      }
      else if (command == "reboot") {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Remote Reboot");
        lcd.setCursor(0, 1);
        lcd.print("Command Received");
        mqtt.publish(mqtt_topic_status, "{\"status\":\"rebooting\"}", true);
        delay(3000);
        ESP.restart();
      }
    }
  }
}

// --- ADDED --- Grant access (unlock, display)
void grantAccess(const String& name, const String& role) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("GRANTED: " + name);
  lcd.setCursor(0,1); lcd.print("Role: " + role);
  // activate relay
  digitalWrite(RELAY, HIGH);
  delay(3000);
  digitalWrite(RELAY, LOW);
  delay(500);
  displayWelcomeScreen();
}

// --- ADDED --- Deny access (sound, display)
void denyAccess(const String& reason) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("ACCESS DENIED");
  lcd.setCursor(0,1); lcd.print(reason);
  delay(2000);
  displayWelcomeScreen();
}

// --- ADDED --- MQTT reconnect helper
void reconnectMqtt() {
  while (!mqtt.connected()) {
    if (mqtt.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      mqtt.subscribe(mqtt_topic_command);
      mqtt.subscribe(TOPIC_ACCESS);
    } else {
      delay(5000);
    }
  }
}

void sendSystemStatus() {
  if (!mqtt.connected()) return;
  
  DynamicJsonDocument doc(512);
  
  doc["device"] = mqtt_client_id;
  doc["status"] = "online";
  doc["uptime"] = (millis() - systemStartTime) / 1000;
  doc["employees"] = employeeCount;
  doc["attendance_records"] = attendanceCount;
  doc["fingerprint_templates"] = finger.templateCount;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  
  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  mqtt.publish(mqtt_topic_status, buffer, n);
}

// Function to sync all attendance data with server
void syncAttendanceData() {
  if (!mqtt.connected()) return;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Syncing Data...");
  
  // Send the last 20 attendance records or all if less than 20
  int startIdx = (attendanceCount > 20) ? attendanceCount - 20 : 0;
  for (int i = startIdx; i < attendanceCount; i++) {
    DynamicJsonDocument doc(256);
    
    doc["id"] = attendanceLog[i].empId;
    doc["name"] = attendanceLog[i].empName;
    doc["action"] = attendanceLog[i].action;
    doc["timestamp"] = attendanceLog[i].timestamp;
    
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt.publish(mqtt_topic_attendance, buffer, n);
    
    // Brief delay to avoid flooding the network
    delay(100);
  }
  
  lcd.setCursor(0, 1);
  lcd.print("Sync Complete");
  delay(2000);
  displayWelcomeScreen();
}

void displayWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ATTENDANCE SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("Scan Fingerprint");
  lcd.setCursor(0, 2);
  lcd.print("or Enter Admin Code");
  
  // Display system uptime instead of RTC time
  unsigned long uptime = (millis() - systemStartTime) / 1000;
  int hours = uptime / 3600;
  int minutes = (uptime % 3600) / 60;
  int seconds = uptime % 60;
  
  lcd.setCursor(0, 3);
  lcd.printf("Uptime: %02d:%02d:%02d", hours, minutes, seconds);
}

void handleKeypadInput(char key) {
  Serial.print("Key pressed: ");
  Serial.println(key);
  
  if (key == '*') {
    if (adminMode) {
      handleAdminCommand();
    } else {
      checkAdminPassword();
    }
  }
  else if (key >= '0' && key <= '9') {
    inputPassword += key;
    lcd.setCursor(0, 1);
    lcd.print("Password: ");
    for (int i = 0; i < inputPassword.length(); i++) {
      lcd.print("*");
    }
  }
}

void checkAdminPassword() {
  if (inputPassword == adminPassword) {
    adminMode = true;
    Serial.println("Admin mode activated!");
    displayAdminMenu();
    
    // Notify MQTT server about admin mode
    if (mqtt.connected()) {
      mqtt.publish(mqtt_topic_status, "{\"status\":\"admin_mode_active\"}", false);
    }
  } else {
    Serial.println("Wrong admin password!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACCESS DENIED");
    
    // Notify MQTT server about failed admin access
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "admin_access_denied";
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
    
    delay(2000);
    displayWelcomeScreen();
  }
  inputPassword = "";
}

void displayAdminMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADMIN MODE");
  lcd.setCursor(0, 1);
  lcd.print("1#=Add 2#=Delete");
  lcd.setCursor(0, 2);
  lcd.print("3#=Report 4#=Reset");
  lcd.setCursor(0, 3);
  lcd.print("9#=Exit Admin");
}

void handleAdminCommand() {
  if (inputPassword == "1") {
    addEmployee();
  }
  else if (inputPassword == "2") {
    deleteEmployee();
  }
  else if (inputPassword == "3") {
    generateReport();
  }
  else if (inputPassword == "4") {
    // Added new option for resetting the fingerprint sensor
    resetFingerprintSensor();
  }
  else if (inputPassword == "9") {
    exitAdminMode();
  }
  else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalid Command");
    delay(2000);
    displayAdminMenu();
  }
  inputPassword = "";
}

void addEmployee() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADD EMPLOYEE");
  
  // Get employee ID
  lcd.setCursor(0, 1);
  lcd.print("Enter ID (1-127)#:");
  
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
  
  // Use a default name based on ID
  String empName = "Emp" + String(empId);
  
  // Enroll fingerprint
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger on");
  lcd.setCursor(0, 1);
  lcd.print("sensor...");
  
  if (enrollFingerprint(empId)) {
    // Save employee data
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
    lcd.print("Employee Added!");
    lcd.setCursor(0, 1);
    lcd.print("ID: " + String(empId));
    lcd.setCursor(0, 2);
    lcd.print("Name: " + empName);
    
    // Notify MQTT server about new employee
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "employee_added";
      doc["id"] = empId;
      doc["name"] = empName;
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
    
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed to add!");
    delay(2000);
  }
  
  displayAdminMenu();
}

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
  }
  
  delay(2000);
  displayAdminMenu();
}

void generateReport() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ATTENDANCE REPORT");
  lcd.setCursor(0, 1);
  lcd.print("Employees: " + String(employeeCount));
  lcd.setCursor(0, 2);
  lcd.print("Records: " + String(attendanceCount));
  
  // Display report on Serial Monitor
  Serial.println("=== ATTENDANCE REPORT ===");
  unsigned long uptime = (millis() - systemStartTime) / 1000;
  Serial.println("System Uptime: " + String(uptime) + " seconds");
  Serial.println("Total Employees: " + String(employeeCount));
  Serial.println("Total Records: " + String(attendanceCount));
  Serial.println("");
  
  // Show employee list
  Serial.println("EMPLOYEES:");
  for (int i = 0; i < employeeCount; i++) {
    Serial.println("ID: " + String(employees[i].id) + " - " + String(employees[i].name));
  }
  
  Serial.println("\nRECENT ATTENDANCE (Last 20 records):");
  int startIdx = (attendanceCount > 20) ? attendanceCount - 20 : 0;
  for (int i = startIdx; i < attendanceCount; i++) {
    unsigned long relativeTime = attendanceLog[i].timestamp / 1000; // Convert to seconds
    int hours = relativeTime / 3600;
    int minutes = (relativeTime % 3600) / 60;
    int seconds = relativeTime % 60;
    
    Serial.printf("ID:%d %s %s [%02d:%02d:%02d]\n",
                  attendanceLog[i].empId,
                  attendanceLog[i].empName,
                  attendanceLog[i].action,
                  hours, minutes, seconds);
  }
  
  // Send report data to MQTT
  if (mqtt.connected()) {
    DynamicJsonDocument doc(512);
    doc["event"] = "report_generated";
    doc["timestamp"] = millis() - systemStartTime;
    doc["employee_count"] = employeeCount;
    doc["record_count"] = attendanceCount;
    
    char buffer[512];
    size_t n = serializeJson(doc, buffer);
    mqtt.publish(mqtt_topic_status, buffer, n);
    
    // Ask if user wants to sync all data
    lcd.setCursor(0, 3);
    lcd.print("1=Sync All 9=Return");
    
    while (true) {
      char key = customKeypad.getKey();
      if (key == '1') {
        syncAttendanceData();
        break;
      } else if (key == '9') {
        break;
      }
      delay(50);
    }
  } else {
    lcd.setCursor(0, 3);
    lcd.print("Check Serial Monitor");
    delay(5000);
  }
  
  displayAdminMenu();
}

// Modified processAttendance function with MQTT data sending
void processAttendance(int fingerprintId) {
  Serial.print("Fingerprint detected with ID: ");
  Serial.println(fingerprintId);
  
  // Find employee by fingerprint ID
  int empIndex = -1;
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == fingerprintId && employees[i].isActive) {
      empIndex = i;
      break;
    }
  }
  
  if (empIndex == -1) {
    // Enhanced error display for unrecognized fingerprint
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACCESS DENIED");
    lcd.setCursor(0, 1);
    lcd.print("Fingerprint ID: " + String(fingerprintId));
    lcd.setCursor(0, 2);
    lcd.print("Not Registered!");
    lcd.setCursor(0, 3);
    lcd.print("Contact Admin");
    
    // Log the unauthorized attempt
    Serial.println("UNAUTHORIZED ACCESS ATTEMPT - Fingerprint ID: " + String(fingerprintId));
    
    // Send unauthorized access attempt to MQTT
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "unauthorized_access";
      doc["fingerprint_id"] = fingerprintId;
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
    
    // Sound alert removed
    delay(4000); // Show error message longer
    displayWelcomeScreen();
    return;
  }
  
  Employee* emp = &employees[empIndex];
  unsigned long currentTime = millis();
  
  // Determine if this is check-in or check-out
  bool isCheckIn = (emp->lastCheckIn == 0) || 
                   (emp->lastCheckOut > emp->lastCheckIn) ||
                   (currentTime - emp->lastCheckIn > 8 * 3600 * 1000); // 8 hours gap in milliseconds
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ACCESS GRANTED");
  lcd.setCursor(0, 1);
  lcd.print("Welcome " + String(emp->name));
  
  const char* actionType;
  
  if (isCheckIn) {
    emp->lastCheckIn = currentTime;
    lcd.setCursor(0, 2);
    lcd.print("CHECK IN");
    actionType = "CHECK_IN";
    logAttendance(emp->id, emp->name, actionType, currentTime);
  } else {
    emp->lastCheckOut = currentTime;
    lcd.setCursor(0, 2);
    lcd.print("CHECK OUT");
    actionType = "CHECK_OUT";
    logAttendance(emp->id, emp->name, actionType, currentTime);
  }
  
  // Display relative time since system start
  unsigned long relativeTime = (currentTime - systemStartTime) / 1000;
  int hours = relativeTime / 3600;
  int minutes = (relativeTime % 3600) / 60;
  int seconds = relativeTime % 60;
  
  lcd.setCursor(0, 3);
  lcd.printf("Time: %02d:%02d:%02d", hours, minutes, seconds);
  
  // Send attendance data to MQTT
  if (mqtt.connected()) {
    DynamicJsonDocument doc(384);
    doc["event"] = "attendance";
    doc["id"] = emp->id;
    doc["name"] = emp->name;
    doc["action"] = actionType;
    doc["timestamp"] = currentTime - systemStartTime;
    doc["device_time"] = String(hours) + ":" + String(minutes) + ":" + String(seconds);
    
    char buffer[384];
    size_t n = serializeJson(doc, buffer);
    mqtt.publish(mqtt_topic_attendance, buffer, n);
    
    // Also notify with brief status message
    String statusMsg = String(emp->name) + " " + String(actionType);
    mqtt.publish(mqtt_topic_status, statusMsg.c_str());
  }
  
  saveEmployeeData();
  
  // Activate relay for door unlock
  digitalWrite(RELAY, HIGH);
  delay(3000);
  digitalWrite(RELAY, LOW);
  
  displayWelcomeScreen();
}

void logAttendance(int empId, const char* empName, const char* action, unsigned long timestamp) {
  // Store attendance in EEPROM (circular buffer)
  if (attendanceCount >= MAX_ATTENDANCE_RECORDS) {
    // Shift array to make room (remove oldest record)
    for (int i = 0; i < MAX_ATTENDANCE_RECORDS - 1; i++) {
      attendanceLog[i] = attendanceLog[i + 1];
    }
    attendanceCount = MAX_ATTENDANCE_RECORDS - 1;
  }
  
  // Add new record
  attendanceLog[attendanceCount].empId = empId;
  strncpy(attendanceLog[attendanceCount].empName, empName, 9);
  attendanceLog[attendanceCount].empName[9] = '\0';
  strncpy(attendanceLog[attendanceCount].action, action, 9);
  attendanceLog[attendanceCount].action[9] = '\0';
  attendanceLog[attendanceCount].timestamp = timestamp - systemStartTime; // Store relative time
  
  attendanceCount++;
  saveAttendanceData();
  
  // Also log to Serial Monitor for real-time monitoring
  unsigned long relativeTime = (timestamp - systemStartTime) / 1000;
  int hours = relativeTime / 3600;
  int minutes = (relativeTime % 3600) / 60;
  int seconds = relativeTime % 60;
  
  Serial.printf("ATTENDANCE: ID:%d %s %s [%02d:%02d:%02d]\n",
                empId, empName, action, hours, minutes, seconds);
}

String getNumericInput() {
  // Simplified input for ID numbers only
  String input = "";
  while (true) {
    char key = customKeypad.getKey();
    if (key) {
      if (key == '#') {
        // Confirm input
        break;
      } else if (key >= '0' && key <= '9') {
        // Add digit to input
        input += key;
        lcd.print(key);
      } else if (key == '*') {
        // Clear input
        input = "";
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Cleared, re-enter ID:");
      }
    }
    delay(50);
  }
  return input;
}

// This function has been removed as we're using ID-based naming
// The simplified system no longer needs text input

bool enrollFingerprint(int id) {
  Serial.print("Enrolling fingerprint ID #");
  Serial.println(id);
  
  int p = -1;
  int timeoutCounter = 0;
  const int MAX_TIMEOUT = 200; // 10 seconds timeout (50ms * 200)
  
  // Check if ID already exists and delete it if necessary
  if (finger.loadModel(id) == FINGERPRINT_OK) {
    Serial.println("Existing model found, deleting...");
    finger.deleteModel(id);
    delay(500);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger");
  lcd.setCursor(0, 1);
  lcd.print("on sensor...");
  
  // First image capture
  timeoutCounter = 0;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        lcd.setCursor(0, 2);
        lcd.print("Image captured");
        Serial.println("Image captured");
        break;
      case FINGERPRINT_NOFINGER:
        if (timeoutCounter % 30 == 0) { // Update display every ~1.5 seconds
          lcd.setCursor(0, 2);
          lcd.print("Waiting...       ");
        }
        break;
      default:
        lcd.setCursor(0, 2);
        lcd.print("Error: " + String(p));
        delay(1000);
        break;
    }
    
    delay(50);
    timeoutCounter++;
    if (timeoutCounter > MAX_TIMEOUT) {
      lcd.setCursor(0, 3);
      lcd.print("Timeout! Try again");
      Serial.println("Enrollment timeout");
      delay(2000);
      return false;
    }
  }

  // First conversion
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 3);
    lcd.print("Conv err: " + String(p));
    Serial.print("Conversion error: ");
    Serial.println(p);
    delay(2000);
    return false;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove finger");
  lcd.setCursor(0, 1);
  lcd.print("Wait...");
  Serial.println("Remove finger");
  
  delay(1000); // Wait a bit before checking
  
  // Wait for finger removal
  timeoutCounter = 0;
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(50);
    timeoutCounter++;
    if (timeoutCounter > 100) { // 5 second timeout
      lcd.setCursor(0, 2);
      lcd.print("Please remove finger");
      timeoutCounter = 0;
    }
  }
  
  Serial.println("Finger removed");
  
  // Wait a bit before asking for second placement
  delay(1000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place same finger");
  lcd.setCursor(0, 1);
  lcd.print("again...");
  
  // Second capture
  p = -1;
  timeoutCounter = 0;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        lcd.setCursor(0, 2);
        lcd.print("Image captured");
        Serial.println("Second image captured");
        break;
      case FINGERPRINT_NOFINGER:
        if (timeoutCounter % 30 == 0) { // Update display every ~1.5 seconds
          lcd.setCursor(0, 2);
          lcd.print("Waiting...       ");
        }
        break;
      default:
        lcd.setCursor(0, 2);
        lcd.print("Error: " + String(p));
        delay(1000);
        break;
    }
    
    delay(50);
    timeoutCounter++;
    if (timeoutCounter > MAX_TIMEOUT) {
      lcd.setCursor(0, 3);
      lcd.print("Timeout! Try again");
      Serial.println("Enrollment timeout");
      delay(2000);
      return false;
    }
  }

  // Second conversion
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 3);
    lcd.print("Conv err: " + String(p));
    Serial.print("Conversion error 2: ");
    Serial.println(p);
    delay(2000);
    return false;
  }

  // Create model from the two images
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Creating model...");
  Serial.println("Creating fingerprint model...");
  
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    if (p == FINGERPRINT_ENROLLMISMATCH) {
      lcd.print("Finger didn't match");
      Serial.println("Fingerprints did not match");
    } else {
      lcd.print("Model err: " + String(p));
      Serial.print("Model error: ");
      Serial.println(p);
    }
    delay(2000);
    return false;
  }

  // Store the model
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Stored successfully!");
    lcd.setCursor(0, 2);
    lcd.print("ID #" + String(id));
    Serial.println("Fingerprint stored successfully");
    
    // Notify MQTT that a new fingerprint was enrolled
    if (mqtt.connected()) {
      mqtt.publish(mqtt_topic_status, String("New fingerprint enrolled, ID: " + String(id)).c_str());
    }
    
    delay(2000);
    return true;
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Storage failed: " + String(p));
    Serial.print("Storage error: ");
    Serial.println(p);
    delay(2000);
    return false;
  }
}

// Enhanced getFingerprintID function with better error handling
int getFingerprintID() {
  static unsigned long lastErrorTime = 0;
  static int consecutiveErrors = 0;
  
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      consecutiveErrors = 0; // Reset error counter on success
      break;
    case FINGERPRINT_NOFINGER:
      return -1; // Normal state, no finger detected
    case FINGERPRINT_PACKETRECIEVEERR:
      // Only show error if it persists
      consecutiveErrors++;
      if (consecutiveErrors > 3 && (millis() - lastErrorTime > 5000)) {
        Serial.println("Communication error");
        showFingerprintError("Sensor Comm Error");
        lastErrorTime = millis();
        consecutiveErrors = 0;
      }
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      showFingerprintError("Image Error");
      return -2;
    default:
      Serial.println("Unknown error code: " + String(p));
      // Check if sensor is connected or responsive
      if (!finger.verifyPassword()) {
        showFingerprintError("Sensor Disconnected");
        // Try to reconnect to the sensor
        mySerial.begin(57600, SERIAL_8N1, 16, 17);
        delay(1000);
      } else {
        showFingerprintError("Unknown Error: " + String(p));
      }
      return -2;
  }

  // Convert image to template
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      showFingerprintError("Poor Quality - Try Again");
      return -2;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error during conversion");
      showFingerprintError("Comm Error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      showFingerprintError("No Features Found");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      showFingerprintError("Invalid Image");
      return -2;
    default:
      Serial.println("Unknown conversion error: " + String(p));
      showFingerprintError("Process Error: " + String(p));
      return -2;
  }

  // Search for fingerprint match
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Found ID #"); 
    Serial.print(finger.fingerID); 
    Serial.print(" with confidence of "); 
    Serial.println(finger.confidence);
    
    // Check confidence level (optional security feature)
    if (finger.confidence < 50) {
      Serial.println("Low confidence match - access denied");
      showFingerprintError("Low Confidence: " + String(finger.confidence));
      
      // Notify MQTT server about low confidence match
      if (mqtt.connected()) {
        DynamicJsonDocument doc(256);
        doc["event"] = "low_confidence_match";
        doc["fingerprint_id"] = finger.fingerID;
        doc["confidence"] = finger.confidence;
        doc["timestamp"] = millis() - systemStartTime;
        
        char buffer[256];
        size_t n = serializeJson(doc, buffer);
        mqtt.publish(mqtt_topic_status, buffer, n);
      }
      
      return -2;
    }
    
    return finger.fingerID;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error during search");
    showFingerprintError("Comm Error in Search");
    return -2;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Fingerprint not found in database");
    // Don't show error here, let processAttendance handle it
    return -3; // Special code for "not found"
  } else {
    Serial.println("Unknown search error: " + String(p));
    showFingerprintError("Search Error: " + String(p));
    return -2;
  }
}

// Function to show fingerprint sensor errors
void showFingerprintError(String errorMsg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FINGERPRINT ERROR");
  lcd.setCursor(0, 1);
  lcd.print(errorMsg);
  lcd.setCursor(0, 2);
  lcd.print("Try Again");
  
  // Notify MQTT server about fingerprint errors
  if (mqtt.connected()) {
    DynamicJsonDocument doc(256);
    doc["event"] = "fingerprint_error";
    doc["error"] = errorMsg;
    doc["timestamp"] = millis() - systemStartTime;
    
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt.publish(mqtt_topic_status, buffer, n);
  }
  
  delay(2000);
  displayWelcomeScreen();
}

void saveEmployeeData() {
  EEPROM.put(EMPLOYEE_DATA_START, employeeCount);
  EEPROM.put(EMPLOYEE_DATA_START + sizeof(int), employees);
  EEPROM.commit();
}

void loadEmployeeData() {
  EEPROM.get(EMPLOYEE_DATA_START, employeeCount);
  if (employeeCount > 50 || employeeCount < 0) {
    employeeCount = 0; // Reset if invalid data
  } else {
    EEPROM.get(EMPLOYEE_DATA_START + sizeof(int), employees);
  }
}

void saveAttendanceData() {
  EEPROM.put(ATTENDANCE_DATA_START, attendanceCount);
  EEPROM.put(ATTENDANCE_DATA_START + sizeof(int), attendanceLog);
  EEPROM.commit();
}

void loadAttendanceData() {
  EEPROM.get(ATTENDANCE_DATA_START, attendanceCount);
  if (attendanceCount > MAX_ATTENDANCE_RECORDS || attendanceCount < 0) {
    attendanceCount = 0; // Reset if invalid data
  } else {
    EEPROM.get(ATTENDANCE_DATA_START + sizeof(int), attendanceLog);
  }
}

void exitAdminMode() {
  adminMode = false;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Exiting Admin Mode");
  
  // Notify MQTT server about exiting admin mode
  if (mqtt.connected()) {
    mqtt.publish(mqtt_topic_status, "{\"status\":\"admin_mode_exited\"}", false);
  }
  
  delay(2000);
  displayWelcomeScreen();
}

void resetToMainScreen() {
  inputPassword = "";
  adminMode = false;
  lastActivity = millis();
  displayWelcomeScreen();
}

// MQTT Setup Function
void setupMqtt() {
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  // Try to connect initially
  reconnectMqtt();
}

// Add a new function to reset the fingerprint sensor
void resetFingerprintSensor() {
  Serial.println("Resetting fingerprint sensor...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resetting Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  
  // Close and reopen the serial connection
  mySerial.end();
  delay(500);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  delay(1000);
  
  // Check if sensor is responsive
  if (finger.verifyPassword()) {
    Serial.println("Sensor reset successful");
    lcd.setCursor(0, 2);
    lcd.print("Reset successful");
    
    // Get template count
    finger.getTemplateCount();
    lcd.setCursor(0, 3);
    lcd.print("Templates: " + String(finger.templateCount));
    
    // Notify MQTT server about successful reset
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "sensor_reset";
      doc["status"] = "success";
      doc["templates"] = finger.templateCount;
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
  } else {
    Serial.println("Sensor reset failed");
    lcd.setCursor(0, 2);
    lcd.print("Reset failed");
    
    // Notify MQTT server about failed reset
    if (mqtt.connected()) {
      DynamicJsonDocument doc(256);
      doc["event"] = "sensor_reset";
      doc["status"] = "failed";
      doc["timestamp"] = millis() - systemStartTime;
      
      char buffer[256];
      size_t n = serializeJson(doc, buffer);
      mqtt.publish(mqtt_topic_status, buffer, n);
    }
  }
  
  delay(3000);
  displayWelcomeScreen();
}