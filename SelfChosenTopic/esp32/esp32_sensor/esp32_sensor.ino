#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Preferences.h>

// Pin Definitions
const int flexPins[5] = {34, 35, 36, 39, 32};
const int bootButton = 0; // BOOT button for calibration trigger

// MPU6050
Adafruit_MPU6050 mpu;

// Preferences for NVS
Preferences preferences;

// Calibration Data (3 points per finger: Open, Half, Closed)
struct CalibrationData {
  int openVal;
  int halfVal;
  int closedVal;
};
CalibrationData fingerCal[5];

// Data structure to send
typedef struct struct_message {
  uint8_t finger_angles[5];
  float x_acceleration;
} struct_message;

struct_message myData;

// REPLACE WITH YOUR RECEIVER MAC ADDRESS
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
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

void runCalibration() {
  Serial.println("=== STARTING CALIBRATION ===");
  
  const char* labels[3] = {"FULLY OPEN", "HALF CLOSED", "FULLY CLOSED"};
  int* targets[3];

  for (int p = 0; p < 3; p++) {
    Serial.print("Position: ");
    Serial.println(labels[p]);
    Serial.println("Hold position and press BOOT button...");
    
    while (digitalRead(bootButton) == HIGH) delay(10);
    delay(500); // Debounce
    
    for (int i = 0; i < 5; i++) {
      int val = analogRead(flexPins[i]);
      if (p == 0) fingerCal[i].openVal = val;
      else if (p == 1) fingerCal[i].halfVal = val;
      else fingerCal[i].closedVal = val;
      
      Serial.print("Finger "); Serial.print(i); Serial.print(": "); Serial.println(val);
    }
    
    while (digitalRead(bootButton) == LOW) delay(10);
    delay(500);
  }
  
  saveCalibration();
  Serial.println("=== CALIBRATION COMPLETE ===");
}

// Piecewise Linear Interpolation
uint8_t mapFlex(int val, CalibrationData cal) {
  float angle;
  // Segment 1: Open to Half (0-90 degrees)
  if ((cal.openVal < cal.closedVal && val <= cal.halfVal) || (cal.openVal > cal.closedVal && val >= cal.halfVal)) {
    angle = map(val, cal.openVal, cal.halfVal, 0, 90);
  } 
  // Segment 2: Half to Closed (90-180 degrees)
  else {
    angle = map(val, cal.halfVal, cal.closedVal, 90, 180);
  }
  return (uint8_t)constrain(angle, 0, 180);
}

void setup() {
  Serial.begin(115200);
  pinMode(bootButton, INPUT_PULLUP);

  // MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
  }

  // WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  loadCalibration();
  
  // Trigger calibration if button is held at boot
  if (digitalRead(bootButton) == LOW) {
    runCalibration();
  }
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  myData.x_acceleration = a.acceleration.x;

  for (int i = 0; i < 5; i++) {
    int raw = analogRead(flexPins[i]);
    myData.finger_angles[i] = mapFlex(raw, fingerCal[i]);
  }

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  // Debug output
  Serial.print("AccX: "); Serial.print(myData.x_acceleration);
  Serial.print(" | Angles: ");
  for(int i=0; i<5; i++) {
    Serial.print(myData.finger_angles[i]); Serial.print(" ");
  }
  Serial.println();

  delay(20); // ~50Hz refresh
}
