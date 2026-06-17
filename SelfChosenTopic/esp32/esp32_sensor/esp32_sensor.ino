#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Preferences.h>
#include "esp_mac.h"
#include <SimpleKalmanFilter.h>

// Pin Definitions
const int flexPins[5] = {39, 34, 35, 32, 33}; 
const int bootButton = 0; 
const int sdaPin = 17; 
const int sclPin = 16; 
const int ledPin = 2;   

// MPU6050
Adafruit_MPU6050 mpu;
Preferences preferences;

// Kalman Filter settings
SimpleKalmanFilter kf[5] = {
  SimpleKalmanFilter(2, 2, 0.05),
  SimpleKalmanFilter(5, 5, 0.05),
  SimpleKalmanFilter(5, 5, 0.05),
  SimpleKalmanFilter(5, 5, 0.05),
  SimpleKalmanFilter(5, 5, 0.05)
};

float filteredFlex[5] = {2000, 2000, 2000, 2000, 2000};

struct CalibrationData {
  int openVal;
  int halfVal;
  int closedVal;
};
CalibrationData fingerCal[5];

// Data structure to send (PACKED)
const float accelerationThreshold = 15.0f; // 1.5g in m/s^2 (1.5 * 9.81)

typedef struct struct_message {
  uint8_t finger_angles[5];
  uint8_t trigger_punch; // 1 = trigger punch, 0 = idle
} __attribute__((packed)) struct_message;

struct_message myData;
uint8_t broadcastAddress[] = {0x68, 0xFE, 0x71, 0x0C, 0xDF, 0x30}; 

// Manual override settings for Serial control
bool manualMode = false;
uint8_t manual_angles[5] = {90, 90, 90, 90, 90}; 
bool mpuPresent = false;

// Low-pass filter
const float ALPHA = 0.2f;
// float filteredFlex[5]  = {2000, 2000, 2000, 2000, 2000};
float filteredAccX = 0.0f, filteredAccY = 0.0f, filteredAccZ = 0.0f;

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  digitalWrite(ledPin, (status == ESP_NOW_SEND_SUCCESS) ? HIGH : LOW);
}
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  digitalWrite(ledPin, (status == ESP_NOW_SEND_SUCCESS) ? HIGH : LOW);
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

// single finger calibration
void calibrateSingleFinger(int fingerIdx, char state) {
  if (fingerIdx < 0 || fingerIdx >= 5) return;
  
  // average of 10 raw data
  long sum = 0;
  for(int j=0; j<10; j++) {
    sum += analogRead(flexPins[fingerIdx]);
    delay(10);
  }
  int avgVal = sum / 10;

  if (state == 'o' || state == 'O') {
    fingerCal[fingerIdx].openVal = avgVal;
    Serial.print("-> Finger "); Serial.print(fingerIdx); Serial.print(" [OPEN] calibrated to: "); Serial.println(avgVal);
  } else if (state == 'h' || state == 'H') {
    fingerCal[fingerIdx].halfVal = avgVal;
    Serial.print("-> Finger "); Serial.print(fingerIdx); Serial.print(" [HALF] calibrated to: "); Serial.println(avgVal);
  } else if (state == 'c' || state == 'C') {
    fingerCal[fingerIdx].closedVal = avgVal;
    Serial.print("-> Finger "); Serial.print(fingerIdx); Serial.print(" [CLOSED] calibrated to: "); Serial.println(avgVal);
  }
  
  saveCalibration(); 
}


void runCalibration() {
  Serial.println("\n=== STARTING FULL CALIBRATION ===");
  const int targetAngles[3] = {0, 90, 180};
  const char states[3] = {'o', 'h', 'c'};

  for (int i = 0; i < 5; i++) {
    Serial.print("\n--- Calibrating Finger "); Serial.print(i); Serial.println(" ---");
    for (int p = 0; p < 3; p++) {
      int angle = targetAngles[p];
      Serial.print("Position: "); Serial.print(angle); Serial.println(" degrees");
      Serial.println("Hold position and press BOOT button...");
  
      unsigned long lastPrint = 0;
      while (digitalRead(bootButton) == HIGH) {
        if (millis() - lastPrint >= 1000) {
          lastPrint = millis();
          Serial.print("Current Raw for Finger "); Serial.print(i); Serial.print(": ");
          Serial.println(analogRead(flexPins[i]));
        }
        delay(10);
      }
      delay(500); 
      
      calibrateSingleFinger(i, states[p]);
      
      while (digitalRead(bootButton) == LOW) delay(10);
      delay(500); 
    }
  }
  Serial.println("=== FULL CALIBRATION COMPLETE ===\n");
}

uint8_t mapFlex(int val, CalibrationData cal) {
  float angle;
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
  digitalWrite(ledPin, LOW); 

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
  Serial.println("Press BOOT button within 3 seconds to enter FULL CALIBRATION...");
  
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
  Serial.println("Type 'help' or 'h' to view manual/calibration controls.");
}

void processCommand(String input) {
  input.trim();
  
  if (input.equalsIgnoreCase("auto") || input.equalsIgnoreCase("a")) {
    manualMode = false;
    Serial.println("Switched to AUTO (sensor) mode.");
  } 
  else if (input.equalsIgnoreCase("help") || input.equalsIgnoreCase("h")) {
    Serial.println("\n--- Serial Control Help ---");
    Serial.println("  F <index> <angle>   - Set individual finger angle (e.g., 'F 0 90')");
    Serial.println("  M <angle>           - Set all fingers to angle (e.g., 'M 90')");
    Serial.println("  A                   - Switch back to AUTO (sensor) mode");
    Serial.println("  STATUS              - Show current mode, values, and calibration maps");
    Serial.println("\n--- Live Calibration Commands (Anytime) ---");
    Serial.println("  CAL <index> <state> - Calibrate single finger index (0-4)");
    Serial.println("                        states: O = Open (0°), H = Half (90°), C = Closed (180°)");
    Serial.println("                        Example: 'CAL 0 O'");
    Serial.println("---------------------------");
  } 
  else if (input.equalsIgnoreCase("status")) {
    Serial.print("Mode: "); Serial.println(manualMode ? "MANUAL" : "AUTO");
    Serial.println("[Current Angles & Calibration Limits]");
    for (int i = 0; i < 5; i++) {
      Serial.print("Finger "); Serial.print(i); Serial.print(" -> Ang: ");
      Serial.print(myData.finger_angles[i]);
      Serial.print("° | Limits: [O:"); Serial.print(fingerCal[i].openVal);
      Serial.print(" H:"); Serial.print(fingerCal[i].halfVal);
      Serial.print(" C:"); Serial.print(fingerCal[i].closedVal); Serial.println("]");
    }
  } 
  else if (input.startsWith("CAL") || input.startsWith("cal")) {
    String cmd = input.substring(3);
    cmd.trim(); 
    int spaceIdx = cmd.indexOf(' ');
    if (spaceIdx != -1) {
      int fingerIdx = cmd.substring(0, spaceIdx).toInt();
      String stateStr = cmd.substring(spaceIdx + 1);
      stateStr.trim();
      char state = stateStr.charAt(0);
      
      if (fingerIdx >= 0 && fingerIdx < 5 && (state == 'O' || state == 'o' || state == 'H' || state == 'h' || state == 'C' || state == 'c')) {
        calibrateSingleFinger(fingerIdx, state);
      } else {
        Serial.println("Error: Invalid CAL parameters. Example: 'CAL 0 O'");
      }
    } else {
      Serial.println("Error: Use format 'CAL <index> <O/H/C>'");
    }
  }
  else if (input.startsWith("F") || input.startsWith("f")) {
    String cmd = input.substring(1);
    cmd.trim();
    int delimIdx = cmd.indexOf(' ');
    if (delimIdx == -1) delimIdx = cmd.indexOf('=');
    if (delimIdx != -1) {
      int fingerIdx = cmd.substring(0, delimIdx).toInt();
      int angle = cmd.substring(delimIdx + 1).toInt();
      if (fingerIdx >= 0 && fingerIdx < 5 && angle >= 0 && angle <= 180) {
        manual_angles[fingerIdx] = angle;
        manualMode = true;
        Serial.print("Manual Mode. Finger "); Serial.print(fingerIdx); Serial.print(" -> "); Serial.println(angle);
      }
    }
  } 
  else if (input.startsWith("M") || input.startsWith("m")) {
    int angle = input.substring(1).toInt();
    if (angle >= 0 && angle <= 180) {
      manualMode = true;
      for (int i = 0; i < 5; i++) manual_angles[i] = angle;
      Serial.print("Manual Mode. All fingers -> "); Serial.println(angle);
    }
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
      if (inputBuffer.length() > 64) inputBuffer = "";
    }
  }
}

void loop() {
  handleSerialCommands();

  sensors_event_t a, g, temp;
  if (mpuPresent && mpu.getEvent(&a, &g, &temp)) {
    float accX = a.acceleration.x;
    float accY = a.acceleration.y;
    float accZ = a.acceleration.z;
    float totalAcc = sqrt(accX * accX + accY * accY + accZ * accZ);
    if (totalAcc > accelerationThreshold) {
      myData.trigger_punch = 1;
    } else {
      myData.trigger_punch = 0;
    }
  } else {
    myData.trigger_punch = 0;
  }

  // Serial.print((uint16_t)filteredFlex[0]);
  // Serial.print(" ");

  for (int i = 0; i < 5; i++) {
    if (manualMode) {
      myData.finger_angles[i] = manual_angles[i];
    } else {
      int raw = analogRead(flexPins[i]);
      // filteredFlex[i] = ALPHA * raw + (1.0f - ALPHA) * filteredFlex[i];
      filteredFlex[i] = kf[i].updateEstimate(raw);
      
      // 輸出對應的 0 ~ 180 度穩定角度
      myData.finger_angles[i] = mapFlex((int)filteredFlex[i], fingerCal[i]);
      
      Serial.print(myData.finger_angles[i]);
      Serial.print(" ");
    }
  }
  if (!manualMode) Serial.println();

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  delay(20); 
}