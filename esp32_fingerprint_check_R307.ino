#include <WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// WiFi credentials
const char* ssid = "VNUK4-10";
const char* password = "Z@q12wsx";
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
const byte COLS = 3;
char hexaKeys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'}, 
  {'7','8','9'},
  {'*','0','#'}
};

// Define keypad pins (4x3 matrix)
byte rowPins[ROWS] = {13, 12, 14, 19}; // GPIO pins for rows
byte colPins[COLS] = {18, 4, 2}; // GPIO pins for columns

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// System variables
String inputPassword = "";
String adminPassword = "1234"; // Admin password
bool adminMode = false;
unsigned long lastActivity = 0;
const unsigned long TIMEOUT = 30000; // 30 seconds timeout
unsigned long systemStartTime = 0; // System start time for relative timestamps

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

// Buzzer and Relay pins (LEDs removed)
#define BUZZER 27
#define RELAY 33 // For door lock

// EEPROM addresses
#define EEPROM_SIZE 4096  // Increased size for attendance records
#define EMPLOYEE_DATA_START 0
#define ATTENDANCE_DATA_START 2048  // Start attendance data at offset 2048

void setup() {
  Serial.begin(115200);
  
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

    playSuccessSound();
    delay(4000);
  } else {
    Serial.println("Failed to connect to WiFi.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connect Failed");
    playErrorSound();
    delay(3000);
  }

  // Store system start time
  systemStartTime = millis();
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Initialize pins (LEDs removed)
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY, OUTPUT);
  
  // Initial state
  digitalWrite(RELAY, LOW);
  
  // Initialize fingerprint sensor AS608
  mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  
  if (finger.verifyPassword()) {
    Serial.println("AS608 Fingerprint sensor detected!");
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint: OK");
  } else {
    Serial.println("AS608 sensor not found!");
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint: ERROR");
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
}

void loop() {
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
  
  // Check fingerprint for attendance
  if (!adminMode) {
    int fingerprintResult = getFingerprintID();
    
    if (fingerprintResult >= 0) {
      // Valid fingerprint ID found
      lastActivity = millis();
      processAttendance(fingerprintResult);
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
      playUnauthorizedSound();
      delay(4000);
      displayWelcomeScreen();
    }
    // fingerprintResult == -2 errors are already handled in getFingerprintID()
    // fingerprintResult == -1 means no finger detected (normal state)
  }
  
  delay(200);
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
    // Clear input
    inputPassword = "";
    lcd.setCursor(0, 1);
    lcd.print("Input cleared      ");
    Serial.println("Input cleared");
  }
  else if (key == '#') {
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
    playSuccessSound();
  } else {
    Serial.println("Wrong admin password!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ACCESS DENIED");
    playErrorSound();
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
  lcd.print("1#=Add Employee");
  lcd.setCursor(0, 2);
  lcd.print("2#=Delete 3#=Report");
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
  
  // Get employee name
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Name:");
  lcd.setCursor(0, 1);
  lcd.print("Use keypad...");
  
  String empName = getTextInput();
  
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
    playSuccessSound();
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed to add!");
    playErrorSound();
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
  for (int i = 0; i < employeeCount; i++) {
    if (employees[i].id == empId) {
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
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Employee Not Found!");
    playErrorSound();
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
  
  lcd.setCursor(0, 3);
  lcd.print("Check Serial Monitor");
  playSuccessSound();
  
  delay(5000);
  displayAdminMenu();
}

// Enhanced processAttendance function with better error handling
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
    
    // Play extended error sound for security alert
    playUnauthorizedSound();
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
  
  if (isCheckIn) {
    emp->lastCheckIn = currentTime;
    lcd.setCursor(0, 2);
    lcd.print("CHECK IN");
    logAttendance(emp->id, emp->name, "CHECK_IN", currentTime);
  } else {
    emp->lastCheckOut = currentTime;
    lcd.setCursor(0, 2);
    lcd.print("CHECK OUT");
    logAttendance(emp->id, emp->name, "CHECK_OUT", currentTime);
  }
  
  // Display relative time since system start
  unsigned long relativeTime = (currentTime - systemStartTime) / 1000;
  int hours = relativeTime / 3600;
  int minutes = (relativeTime % 3600) / 60;
  int seconds = relativeTime % 60;
  
  lcd.setCursor(0, 3);
  lcd.printf("Time: %02d:%02d:%02d", hours, minutes, seconds);
  
  saveEmployeeData();
  playAccessGrantedSound();
  
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
  String input = "";
  while (true) {
    char key = customKeypad.getKey();
    if (key) {
      if (key == '#') {
        break;
      } else if (key >= '0' && key <= '9') {
        input += key;
        lcd.print(key);
      } else if (key == '*') {
        input = "";
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Cleared, re-enter:");
      }
    }
    delay(50);
  }
  return input;
}

String getTextInput() {
  // Simplified text input using T9-like method
  // 2=ABC, 3=DEF, 4=GHI, 5=JKL, 6=MNO, 7=PQRS, 8=TUV, 9=WXYZ
  String input = "";
  String keyMap[10] = {"", "", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"};
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T9 Input (2-9)");
  lcd.setCursor(0, 1);
  lcd.print("0=Space *=Clear #=OK");
  lcd.setCursor(0, 2);
  
  while (input.length() < 19) {
    char key = customKeypad.getKey();
    if (key) {
      if (key == '#') {
        break;
      } else if (key == '*') {
        input = "";
        lcd.setCursor(0, 2);
        lcd.print("                    ");
        lcd.setCursor(0, 2);
      } else if (key == '0') {
        input += " ";
        lcd.print(" ");
      } else if (key >= '2' && key <= '9') {
        int keyNum = key - '0';
        if (keyMap[keyNum].length() > 0) {
          input += keyMap[keyNum][0]; // Use first letter for simplicity
          lcd.print(keyMap[keyNum][0]);
        }
      }
    }
    delay(100);
  }
  
  return input;
}

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
    return true;
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Storage failed");
    return false;
  }
}

// Enhanced getFingerprintID function with better error handling
int getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      showFingerprintError("Sensor Error");
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      showFingerprintError("Image Error");
      return -2;
    default:
      Serial.println("Unknown error");
      showFingerprintError("Unknown Error");
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
      showFingerprintError("Poor Quality");
      return -2;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      showFingerprintError("Comm Error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      showFingerprintError("No Features");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      showFingerprintError("Invalid Image");
      return -2;
    default:
      Serial.println("Unknown error");
      showFingerprintError("Process Error");
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
      showFingerprintError("Low Confidence");
      return -2;
    }
    
    return finger.fingerID;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    showFingerprintError("Comm Error");
    return -2;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Fingerprint not found in database");
    // Don't show error here, let processAttendance handle it
    return -3; // Special code for "not found"
  } else {
    Serial.println("Unknown error");
    showFingerprintError("Search Error");
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
  
  playErrorSound();
  delay(2000);
  displayWelcomeScreen();
}

// New function for unauthorized access sound
void playUnauthorizedSound() {
  // Play a distinct pattern for security alerts
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
  }
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
  delay(2000);
  displayWelcomeScreen();
}

void resetToMainScreen() {
  inputPassword = "";
  adminMode = false;
  lastActivity = millis();
  displayWelcomeScreen();
}

void playSuccessSound() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}

void playErrorSound() {
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
}

void playAccessGrantedSound() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
}