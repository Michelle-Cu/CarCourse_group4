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
  float y_acceleration;
  float z_acceleration;
} __attribute__((packed)) struct_message;

struct_message myData;
volatile bool dataReady = false; // Flag to tell loop we have new data
const int ledPin = 2;            // Built-in LED on Ruilong ESP32-S
volatile unsigned long lastRecvTime = 0;

// Punch settings
const float accelerationThreshold = 15;
bool isPunching = false;

void triggerPunch() {
  isPunching = true;
  Serial.println("PUNCH TRIGGERED!");
  digitalWrite(ledPin, HIGH); // Turn LED on when punch is triggered
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
  digitalWrite(ledPin, LOW); // Turn LED off when punch finishes
  isPunching = false;
}

// VERY FAST CALLBACK
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
  }
}
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
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
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Start with LED off
  
  Serial.println("Ready.");
}

void loop() {
  // Connection status watchdog LED control disabled to allow LED to act as punch indicator
  /*
  if (lastRecvTime > 0 && millis() - lastRecvTime < 1000) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  */

  if (dataReady) {
    dataReady = false; // Reset flag

    // 1. Debug Output (Now safe to print in loop)
    Serial.print("Angles: ");
    for(int i=0; i<5; i++) {
      Serial.print(myData.finger_angles[i]); Serial.print(" ");
    }
    Serial.print("| AccX: "); Serial.print(myData.x_acceleration);
    Serial.print(" AccY: "); Serial.print(myData.y_acceleration);
    Serial.print(" AccZ: "); Serial.println(myData.z_acceleration);

    // 2. Move Servos
    for (int i = 0; i < 5; i++) {
      int mappedAngle = map(myData.finger_angles[i], 0, 180, servoMinAngles[i], servoMaxAngles[i]);
      servos[i].write(mappedAngle);
    }

    // 3. Punch Check
    // You can choose which axis/axes to monitor (myData.x_acceleration, myData.y_acceleration, myData.z_acceleration)
    // Or monitor a combination, e.g., the total magnitude: sqrt(ax^2 + ay^2 + az^2)
    float accX = myData.x_acceleration;
    float accY = myData.y_acceleration;
    float accZ = myData.z_acceleration;
    
    // Choose your punch condition here:
    // e.g. for Y-axis: if (accY > accelerationThreshold && !isPunching)
    // e.g. for Z-axis: if (accZ > accelerationThreshold && !isPunching)
    if (sqrt(accX*accX+accY*accY+accZ*accZ) > accelerationThreshold && !isPunching) {
      triggerPunch();
    }
  }
  
  // Give the system time to breathe
  delay(1); 
}
