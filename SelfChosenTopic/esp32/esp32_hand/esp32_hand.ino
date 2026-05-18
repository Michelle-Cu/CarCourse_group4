#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// Pin Definitions
const int servoPins[5] = {12, 13, 14, 25, 27};
const int stepPin = 32;
const int dirPin = 33;

// Servos
Servo servos[5];

// Data structure to receive
typedef struct struct_message {
  uint8_t finger_angles[5];
  float x_acceleration;
} struct_message;

struct_message myData;

// Punch settings
const float accelerationThreshold = 2.0; // G-force threshold
bool isPunching = false;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  
  // Update Servos
  for (int i = 0; i < 5; i++) {
    servos[i].write(myData.finger_angles[i]);
  }

  // Trigger Punch (Stepper)
  if (myData.x_acceleration > accelerationThreshold && !isPunching) {
    triggerPunch();
  }
}

void triggerPunch() {
  isPunching = true;
  Serial.println("PUNCH TRIGGERED!");
  
  // Quick extension
  digitalWrite(dirPin, HIGH);
  for (int i = 0; i < 200; i++) { // Adjust steps for distance
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(500); // Speed
    digitalWrite(stepPin, LOW);
    delayMicroseconds(500);
  }
  
  delay(500); // Hold
  
  // Slow retraction
  digitalWrite(dirPin, LOW);
  for (int i = 0; i < 200; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(2000); // Slower
    digitalWrite(stepPin, LOW);
    delayMicroseconds(2000);
  }
  
  isPunching = false;
}

void setup() {
  Serial.begin(115200);

  // Print MAC Address for sensor pairing
  Serial.print("ESP32 Hand MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Initialize Servos
  for (int i = 0; i < 5; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(0); // Initial position
  }

  // Initialize Stepper
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);

  // WiFi & ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Most logic happens in OnDataRecv
  delay(100); 
}
