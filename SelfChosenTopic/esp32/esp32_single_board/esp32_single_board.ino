/*
  ESP32 Single Board Integrated Controller (Robotic Arm Glove + Hand)
  
  This sketch integrates the MPU-6050 IMU, 5x Carbon Flex Sensors, 
  5x Servos, and 1x A4988 Stepper Motor onto a single ESP32 board.
  ESP-NOW communication has been removed.
  
  PIN CONFLICT RESOLUTION:
  - Glove originally used:
      Flex pins: 34, 35, 36, 39, 32
  - Hand originally used:
      Servo pins: 12, 13, 14, 25, 27
      Stepper pins: step=32, dir=33
  - Since pin 32 was shared between the 5th flex sensor and the stepper step pin:
      We have moved the 5th flex sensor pin to GPIO 26 (ADC2_CH9).
      Since Wi-Fi/ESP-NOW is disabled, ADC2 pins can be used for analog reads.
*/

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Preferences.h>
#include <ESP32Servo.h>

// Pin Definitions
// Flex Sensors (Analog Inputs)
const int flexPins[5] = {34, 35, 36, 39, 26}; // Pin 26 replaces Pin 32 to avoid conflict with stepper step pin
const int bootButton = 0;                      // Used to trigger calibration

// Actuators
const int servoPins[5] = {12, 13, 14, 25, 27};
const int stepPin = 32;
const int dirPin = 33;

// Servos
Servo servos[5];

// Servo mapping ranges (finger angle 0-180 maps to servoMin-servoMax)
const int servoMinAngles[5] = {0, 0, 30, 10, 30};
const int servoMaxAngles[5] = {180, 180, 180, 180, 180};

// MPU6050
Adafruit_MPU6050 mpu;
Preferences preferences;

// Calibration Data
struct CalibrationData {
  int openVal;
  int halfVal;
  int closedVal;
};
CalibrationData fingerCal[5];

// Current State
uint8_t finger_angles[5] = {0, 0, 0, 0, 0};
float x_acceleration = 0.0;

// Manual override settings for Serial control
bool manualMode = false;
uint8_t manual_angles[5] = {0, 0, 0, 0, 0};

// Punch settings
const float accelerationThreshold = 9.81; // 1.0g in m/s^2 (1.0 * 9.81)
bool isPunching = false;
bool mpuPresent = false;

void saveCalibration() {
  preferences.begin("flex-cal", false);
  for (int i = 0; i < 5; i++) {
    String key = "f" + String(i);
    preferences.putInt((key + "o").c_str(), fingerCal[i].openVal);
    preferences.putInt((key + "h").c_str(), fingerCal[i].halfVal);
    preferences.putInt((key + "c").c_str(), fingerCal[i].closedVal);
  }
  preferences.end();
  Serial.println("Calibration saved to NVS.");
}

void loadCalibration() {
  preferences.begin("flex-cal", true);
  for (int i = 0; i < 5; i++) {
    String key = "f" + String(i);
    fingerCal[i].openVal = preferences.getInt((key + "o").c_str(), 2000);
    fingerCal[i].halfVal = preferences.getInt((key + "h").c_str(), 2500);
    fingerCal[i].closedVal = preferences.getInt((key + "c").c_str(), 3000);
  }
  preferences.end();
  Serial.println("Calibration loaded from NVS.");
}

void runCalibration() {
  Serial.println("\n=== STARTING CALIBRATION ===");
  const char* labels[3] = {"FULLY OPEN", "HALF CLOSED", "FULLY CLOSED"};

  for (int p = 0; p < 3; p++) {
    Serial.print("\nPosition: "); Serial.println(labels[p]);
    Serial.println("Hold position and press BOOT button...");
    
    // Wait for press
    while (digitalRead(bootButton) == HIGH) delay(10);
    delay(500); // Debounce
    
    // Read values
    for (int i = 0; i < 5; i++) {
      int val = analogRead(flexPins[i]);
      if (p == 0) fingerCal[i].openVal = val;
      else if (p == 1) fingerCal[i].halfVal = val;
      else fingerCal[i].closedVal = val;
      Serial.print("F"); Serial.print(i); Serial.print(": "); Serial.println(val);
    }
    
    // Wait for release
    while (digitalRead(bootButton) == LOW) delay(10);
    delay(500);
  }
  saveCalibration();
  Serial.println("=== CALIBRATION COMPLETE ===\n");
}

uint8_t mapFlex(int val, CalibrationData cal) {
  float angle;
  // Segment 1: Open to Half
  if ((cal.openVal < cal.closedVal && val <= cal.halfVal) || (cal.openVal > cal.closedVal && val >= cal.halfVal)) {
    angle = map(val, cal.openVal, cal.halfVal, 0, 90);
  } else {
    angle = map(val, cal.halfVal, cal.closedVal, 90, 180);
  }
  return (uint8_t)constrain(angle, 0, 180);
}

void triggerPunch() {
  isPunching = true;
  Serial.println("PUNCH TRIGGERED!");
  digitalWrite(dirPin, HIGH);
  for (int i = 0; i < 200; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(500);
  }
  delay(500);
  digitalWrite(dirPin, LOW);
  for (int i = 0; i < 200; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(2000);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(2000);
  }
  isPunching = false;
}

void processCommand(String input) {
  if (input.equalsIgnoreCase("auto") || input.equalsIgnoreCase("a")) {
    manualMode = false;
    Serial.println("Switched to AUTO (sensor) mode.");
  } else if (input.equalsIgnoreCase("help") || input.equalsIgnoreCase("h")) {
    Serial.println("\n--- Serial Control Help ---");
    Serial.println("  F <finger_index> <angle_0_to_180> - Set individual finger angle (e.g., 'F 0 90')");
    Serial.println("  M <angle>                         - Set all fingers to angle (e.g., 'M 90')");
    Serial.println("  A                                 - Switch back to AUTO (sensor) mode");
    Serial.println("  STATUS                            - Show current mode and angles");
    Serial.println("---------------------------");
  } else if (input.equalsIgnoreCase("status")) {
    Serial.print("Mode: "); Serial.println(manualMode ? "MANUAL" : "AUTO");
    Serial.print("Angles: ");
    for (int i = 0; i < 5; i++) {
      Serial.print("F"); Serial.print(i); Serial.print(":"); 
      Serial.print(finger_angles[i]); Serial.print("  ");
    }
    Serial.println();
  } else if (input.startsWith("F") || input.startsWith("f")) {
    String cmd = input.substring(1);
    cmd.trim();
    int delimIdx = cmd.indexOf(' ');
    if (delimIdx == -1) delimIdx = cmd.indexOf('=');
    if (delimIdx != -1) {
      String indexStr = cmd.substring(0, delimIdx);
      String valStr = cmd.substring(delimIdx + 1);
      indexStr.trim();
      valStr.trim();
      int fingerIdx = indexStr.toInt();
      int angle = valStr.toInt();
      if (fingerIdx >= 0 && fingerIdx < 5 && angle >= 0 && angle <= 180) {
        manual_angles[fingerIdx] = angle;
        manualMode = true;
        Serial.print("Manual Mode Active. Set finger "); Serial.print(fingerIdx);
        Serial.print(" to "); Serial.print(angle); Serial.println(" degrees.");
      } else {
        Serial.println("Error: Invalid finger index (0-4) or angle (0-180).");
      }
    } else {
      Serial.println("Error: Invalid command format. Use 'F <index> <angle>' (e.g., 'F 0 90')");
    }
  } else if (input.startsWith("M") || input.startsWith("m")) {
    String valStr = input.substring(1);
    valStr.trim();
    int angle = valStr.toInt();
    if (angle >= 0 && angle <= 180) {
      manualMode = true;
      for (int i = 0; i < 5; i++) {
        manual_angles[i] = angle;
      }
      Serial.print("Manual Mode Active. Set ALL fingers to "); Serial.print(angle); Serial.println(" degrees.");
    } else {
      Serial.println("Error: Invalid angle (0-180).");
    }
  } else {
    Serial.print("Unknown command: '"); Serial.print(input); Serial.println("'. Type 'h' or 'help' for help.");
  }
}

void handleSerialCommands() {
  static String inputBuffer = "";
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
      if (inputBuffer.length() > 64) {
        inputBuffer = "";
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 SINGLE BOARD CONTROLLER START ---");

  // Input Pin Setup
  pinMode(bootButton, INPUT_PULLUP);

  // Stepper Pin Setup
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);

  // Servo Setup
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < 5; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], 500, 2500); // Widest standard pulse limits for maximum rotation (0 to 180 degrees)
    servos[i].write(servoMinAngles[i]);
  }

  // MPU6050 Setup
  if (!mpu.begin()) {
    Serial.println("MPU6050 missing or connection failed.");
    mpuPresent = false;
  } else {
    Serial.println("MPU6050 initialized successfully.");
    mpuPresent = true;
  }

  loadCalibration();

  Serial.println("\n--- SETUP COMPLETE ---");
  Serial.println("Press BOOT button within 3 seconds to enter CALIBRATION...");
  
  unsigned long startWait = millis();
  while (millis() - startWait < 3000) {
    if (digitalRead(bootButton) == LOW) {
      delay(50);
      runCalibration();
      break;
    }
    delay(10);
  }
  Serial.println("Starting normal operation...");
  Serial.println("Type 'help' or 'h' to view manual controls.");
}

void loop() {
  handleSerialCommands();

  // 1. Read MPU6050
  sensors_event_t a, g, temp;
  if (mpuPresent && mpu.getEvent(&a, &g, &temp)) {
    x_acceleration = a.acceleration.x;
  } else {
    x_acceleration = 0.0;
  }

  // 2. Read and Map Flex Sensors
  for (int i = 0; i < 5; i++) {
    if (manualMode) {
      finger_angles[i] = manual_angles[i];
    } else {
      int raw = analogRead(flexPins[i]);
      finger_angles[i] = mapFlex(raw, fingerCal[i]);
    }
  }

  // 3. Update Servos
  for (int i = 0; i < 5; i++) {
    int mappedAngle = map(finger_angles[i], 0, 180, servoMinAngles[i], servoMaxAngles[i]);
    servos[i].write(mappedAngle);
  }

  // 4. Print Status at a reasonable interval to prevent serial flooding
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime >= 500) {
    lastPrintTime = millis();
    // Serial.print("Mode: "); Serial.print(manualMode ? "MANUAL" : "AUTO");
    // Serial.print(" | Angles: ");
    // for (int i = 0; i < 5; i++) {
    //   Serial.print(finger_angles[i]); Serial.print(" ");
    // }
    Serial.print("| AccelX: "); Serial.println(x_acceleration);
  }

  // 5. Punch Check
  if (x_acceleration > accelerationThreshold && !isPunching) {
    triggerPunch();
  }

  delay(20); // Maintain ~50Hz loop rate
}
