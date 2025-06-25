# ESP32 Fingerprint Attendance System

A comprehensive fingerprint-based attendance and access control system using the ESP32 microcontroller and R307 fingerprint sensor, with MQTT integration for real-time data processing and a Next.js web dashboard.

## System Overview

This project implements a complete attendance and access control solution with the following components:

1. **ESP32 Hardware Controller**:
   - R307/AS608 Fingerprint Sensor for biometric identification
   - I2C LCD Display (20x4) for user interface
   - 4x4 Matrix Keypad for PIN code and admin functions
   - Relay output for access control (door lock)

2. **Backend Components**:
   - MQTT broker for real-time messaging
   - Python script for processing fingerprint events and database integration
   - PostgreSQL database for storing user data and attendance records

3. **Web Dashboard**:
   - Next.js admin interface for managing users and viewing attendance data

## Features

- **Biometric Authentication**: Secure fingerprint-based access control
- **MQTT Communication**: Real-time data transmission between components
- **Central Authority Model**: Access decisions made by a central server
- **Admin Functions**: 
  - Enroll/delete fingerprints directly from the device
  - Generate attendance reports
  - Reset and manage the system
- **Database Integration**: Store attendance records in PostgreSQL
- **Web Dashboard**: Modern UI for system administration

## Hardware Requirements

- ESP32 microcontroller board
- R307/AS608 Fingerprint sensor
- 20x4 I2C LCD Display
- 4x4 Matrix Keypad
- Relay module for door lock control
- Power supply (5V)
- Connecting wires

## Wiring Diagram

### ESP32 to R307 Fingerprint Sensor
- ESP32 GPIO16 (RX2) → R307 TX
- ESP32 GPIO17 (TX2) → R307 RX
- ESP32 5V → R307 VCC
- ESP32 GND → R307 GND

### ESP32 to LCD Display
- ESP32 GPIO21 (SDA) → LCD SDA
- ESP32 GPIO22 (SCL) → LCD SCL
- ESP32 5V → LCD VCC
- ESP32 GND → LCD GND

### ESP32 to 4x4 Keypad
- ESP32 GPIO13, 12, 14, 27 → Keypad Row pins (1-4)
- ESP32 GPIO26, 25, 32, 33 → Keypad Column pins (1-4)

## Software Setup

### 1. ESP32 Setup
- Install the Arduino IDE
- Install the ESP32 board package via Board Manager
- Install the following libraries:
  - Adafruit Fingerprint Sensor Library
  - PubSubClient (MQTT)
  - LiquidCrystal_I2C
  - Keypad Library
  - ArduinoJson

### 2. PostgreSQL Database Setup
- Create a new database named `checkin_system`
- Create the following tables:
  - `users`: Store user information
  - `roles`: Define user roles
  - `departments`: Define departments
  - `checkin_logs`: Store attendance records
- Create a user with appropriate permissions (example: `esp_user` with password `esp_password`)

### 3. Python MQTT-PostgreSQL Bridge
- Install required Python packages:
  ```bash
  pip install paho-mqtt psycopg2-binary
  ```
- Configure the MQTT broker address, PostgreSQL connection details
- Run the Python script

### 4. MQTT Broker Setup
- Install an MQTT broker (e.g., Mosquitto)
- Configure the broker to accept connections from ESP32 and the Python script

### 5. Web Dashboard Setup
- Install Node.js and npm
- Navigate to the IOT-Dashboard directory
- Install dependencies:
  ```bash
  npm install
  ```
- Run the development server:
  ```bash
  npm run dev
  ```

## Usage

### Device Operation

1. **Normal Mode**:
   - The LCD displays a welcome message
   - Users can scan their fingerprint for attendance/access
   - System sends the fingerprint ID to central server via MQTT
   - Access is granted or denied based on server response

2. **Admin Mode**:
   - Enter admin password (default: 1234) followed by * on the keypad
   - Access admin functions:
     - 1# = Add new employee fingerprint
     - 2# = Delete fingerprint
     - 3# = Generate attendance report
     - 4# = Reset fingerprint sensor
     - 9# = Exit admin mode

### Web Dashboard

- Access the admin dashboard at `http://localhost:3000`
- Login with default credentials:
  - Username: admin
  - Password: 123qwe!@#
- Manage users, view attendance records, and system status

## MQTT Topics

- `fingerprint/scan`: ESP32 publishes fingerprint scan events
- `fingerprint/access`: Server publishes access decisions to ESP32
- `fingerprint/attendance`: ESP32 publishes attendance records
- `fingerprint/status`: ESP32 publishes system status updates
- `fingerprint/command`: Server sends commands to ESP32

## System Architecture

![System Architecture Diagram](./images/heading.png)

```
┌───────────────┐     MQTT     ┌───────────────┐
│  ESP32 with   │◄───Topics────┤ Python MQTT   │
│  Fingerprint  │              │ Bridge Script │
│  Sensor       │─────────────►│               │
└───────────────┘              └───────┬───────┘
                                       │
                                       │ PostgreSQL
                                       ▼
                              ┌────────────────┐
                              │  Database      │
                              │               │
                              └───────┬────────┘
                                      │
                                      │ API
                                      ▼
                              ┌────────────────┐
                              │  Next.js       │
                              │  Web Dashboard │
                              └────────────────┘
```

## Database Structure

### Users Table
```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    fingerprint_id INTEGER UNIQUE,
    user_role INTEGER REFERENCES roles(id),
    department_id INTEGER REFERENCES departments(id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Roles Table
```sql
CREATE TABLE roles (
    id SERIAL PRIMARY KEY,
    role_name VARCHAR(50) NOT NULL,
    permission_level INTEGER NOT NULL
);
```

### Departments Table
```sql
CREATE TABLE departments (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    location VARCHAR(100)
);
```

### Checkin Logs Table
```sql
CREATE TABLE checkin_logs (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    event_type VARCHAR(20) NOT NULL,
    device_id VARCHAR(50)
);
```

## Division of Labor

![Division of Labor](./images/capture.png)

## Known Issues and Limitations

- The system currently supports up to 50 employees in local memory
- WiFi reconnection may be needed in unstable network conditions
- The fingerprint sensor has limited storage (typically 127 templates)