#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Preferences.h>
#include "esp_mac.h"

// Pin Definitions
const int flexPins[5] = {39, 34, 35, 32, 33}; // SVN, 34, 35, 32, 33 (all ADC1 pins, safe for ESP-NOW)
const int bootButton = 0; 
const int sdaPin = 17; // Custom I2C SDA
const int sclPin = 16; // Custom I2C SCL 
const int ledPin = 2;   // Built-in LED on NodeMCU-32S

// MPU6050
Adafruit_MPU6050 mpu;
Preferences preferences;

struct CalibrationData {
  int openVal;
  int halfVal;
  int closedVal;
};
CalibrationData fingerCal[5];

// Data structure to send (PACKED)
typedef struct struct_message {
  uint8_t finger_angles[5];
  float x_acceleration;
} __attribute__((packed)) struct_message;

struct_message myData;
uint8_t broadcastAddress[] = {0x68, 0xFE, 0x71, 0x0C, 0xDF, 0x30}; //68:FE:71:0C:DF:30

// Manual override settings for Serial control
bool manualMode = false;
uint8_t manual_angles[5] = {90, 90, 90, 90, 90};
bool mpuPresent = false;

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
}
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
}
#endif

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
  Serial.println("Calibration loaded.");
}

void runCalibration() {
  Serial.println("\n=== STARTING CALIBRATION ===");
  const char* labels[3] = {"FULLY OPEN", "HALF CLOSED", "FULLY CLOSED"};

  for (int p = 0; p < 3; p++) {
    Serial.print("\nPosition: "); Serial.println(labels[p]);
    Serial.println("Hold position and press BOOT button...");
    
    // Wait for press while continuously printing current values
    unsigned long lastPrint = 0;
    while (digitalRead(bootButton) == HIGH) {
      if (millis() - lastPrint >= 200) {
        lastPrint = millis();
        Serial.print("Current: ");
        for (int i = 0; i < 5; i++) {
          Serial.print("F"); Serial.print(i); Serial.print(":"); Serial.print(analogRead(flexPins[i]));
          if (i < 4) Serial.print(" | ");
        }
        Serial.println();
      }
      delay(10);
    }
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

void setup() {
  Serial.begin(115200);
  pinMode(bootButton, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Start with LED off

  // Start I2C with custom pins (SDA = 17, SCL = 16)
  Wire.begin(sdaPin, sclPin);

  if (!mpu.begin()) {
    Serial.println("MPU6050 missing.");
    mpuPresent = false;
  } else {
    mpuPresent = true;
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

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
      Serial.print(myData.finger_angles[i]); Serial.print("  ");
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

void loop() {
  handleSerialCommands();

  sensors_event_t a, g, temp;
  if (mpuPresent && mpu.getEvent(&a, &g, &temp)) {
    myData.x_acceleration = a.acceleration.x;
  } else {
    myData.x_acceleration = 0.0;
  }

  for (int i = 0; i < 5; i++) {
    if (manualMode) {
      myData.finger_angles[i] = manual_angles[i];
    } else {
      int raw = analogRead(flexPins[i]);
      myData.finger_angles[i] = mapFlex(raw, fingerCal[i]);
      Serial.print(myData.finger_angles[i]);
      Serial.print(" ");
    }
  }
  if (!manualMode) Serial.println();

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  delay(20); 
}
