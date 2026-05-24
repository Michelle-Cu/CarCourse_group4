#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_mac.h"

// Pin Definitions
const int servoPins[5] = {12, 13, 14, 25, 27};
const int stepPin = 32;
const int dirPin = 33;

// Servos
Servo servos[5];

// Data structure to receive (PACKED to prevent alignment issues)
typedef struct struct_message {
  uint8_t finger_angles[5];
  float x_acceleration;
} __attribute__((packed)) struct_message;

struct_message myData;
volatile bool dataReady = false; // Flag to tell loop we have new data

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
    servos[i].write(0);
  }
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  
  Serial.println("Ready.");
}

void loop() {
  if (dataReady) {
    dataReady = false; // Reset flag

    // 1. Debug Output (Now safe to print in loop)
    Serial.print("Angles: ");
    for(int i=0; i<5; i++) {
      Serial.print(myData.finger_angles[i]); Serial.print(" ");
    }
    Serial.print("| AccX: "); Serial.println(myData.x_acceleration);

    // 2. Move Servos
    for (int i = 0; i < 5; i++) {
      servos[i].write(myData.finger_angles[i]);
    }

    // 3. Punch Check
    if (myData.x_acceleration > accelerationThreshold && !isPunching) {
      triggerPunch();
    }
  }
  
  // Give the system time to breathe
  delay(1); 
}
