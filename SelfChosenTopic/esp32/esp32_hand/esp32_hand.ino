#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_mac.h"

// Pin Definitions
const int servoPins[5] = {32, 33, 25, 26, 27};
const int stepPin = 14;
const int dirPin = 12;

// Servos
Servo servos[5];

// Servo mapping ranges (finger angle 0-180 maps to servoMin-servoMax)
const int servoMinAngles[5] = {0, 0, 30, 10, 30};
const int servoMaxAngles[5] = {180, 180, 180, 180, 180};

// Data structure to receive (PACKED to prevent alignment issues)
typedef struct struct_message {
  uint8_t finger_angles[5];
  float x_acceleration;
} __attribute__((packed)) struct_message;

struct_message myData;
volatile bool dataReady = false; // Flag to tell loop we have new data

// Manual override settings for Serial control
bool manualMode = false;
uint8_t manual_angles[5] = {0, 0, 0, 0, 0};

// Punch settings
const float accelerationThreshold = 19.62; // 2.0g in m/s^2 (2.0 * 9.81)
bool isPunching = false;

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
    Serial.println("Switched to AUTO (ESP-NOW) mode.");
  } else if (input.equalsIgnoreCase("help") || input.equalsIgnoreCase("h")) {
    Serial.println("\n--- Serial Control Help ---");
    Serial.println("  F <finger_index> <angle_0_to_180> - Set individual finger angle (e.g., 'F 0 90')");
    Serial.println("  M <angle>                         - Set all fingers to angle (e.g., 'M 90')");
    Serial.println("  A                                 - Switch back to AUTO (ESP-NOW) mode");
    Serial.println("  STATUS                            - Show current mode and angles");
    Serial.println("---------------------------");
  } else if (input.equalsIgnoreCase("status")) {
    Serial.print("Mode: "); Serial.println(manualMode ? "MANUAL" : "AUTO");
    Serial.print("Angles: ");
    for (int i = 0; i < 5; i++) {
      Serial.print("F"); Serial.print(i); Serial.print(":"); 
      Serial.print(manualMode ? manual_angles[i] : myData.finger_angles[i]); Serial.print("  ");
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

// VERY FAST CALLBACK
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
  }
}
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 HAND START ---");

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < 5; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], 500, 2500); // Widest standard pulse limits for maximum rotation (0 to 180 degrees)
    servos[i].write(servoMinAngles[i]);
  }
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  
  Serial.println("Ready.");
  Serial.println("Type 'help' or 'h' to view manual controls.");
}

void loop() {
  handleSerialCommands();

  if (manualMode) {
    // 1. Move Servos in manual mode
    for (int i = 0; i < 5; i++) {
      int mappedAngle = map(manual_angles[i], 0, 180, servoMinAngles[i], servoMaxAngles[i]);
      servos[i].write(mappedAngle);
    }
  } else if (dataReady) {
    dataReady = false; // Reset flag

    // 1. Debug Output (Now safe to print in loop)
    Serial.print("Angles: ");
    for(int i=0; i<5; i++) {
      Serial.print(myData.finger_angles[i]); Serial.print(" ");
    }
    Serial.print("| AccX: "); Serial.println(myData.x_acceleration);

    // 2. Move Servos
    for (int i = 0; i < 5; i++) {
      int mappedAngle = map(myData.finger_angles[i], 0, 180, servoMinAngles[i], servoMaxAngles[i]);
      servos[i].write(mappedAngle);
    }

    // 3. Punch Check
    if (myData.x_acceleration > accelerationThreshold && !isPunching) {
      triggerPunch();
    }
  }
  
  // Give the system time to breathe
  delay(1); 
}
