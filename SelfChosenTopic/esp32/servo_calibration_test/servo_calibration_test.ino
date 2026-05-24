/*
  Servo Calibration Test Sketch
  
  Used to determine the physical limit of the servo connected to Pin 12.
  Press and hold the BOOT button (GPIO 0) to slowly turn the servo.
  The current angle will print to the Serial Monitor.
  
  When it reaches 180, it will wrap back to 0.
  0: 30 180
  1: 10 180
*/

#include <ESP32Servo.h>

const int servoPin = 12;
const int bootButton = 0;

Servo testServo;
int currentAngle = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- SERVO CALIBRATION TEST ---");
  Serial.println("Hold the BOOT button to slowly rotate the servo on Pin 12.");

  pinMode(bootButton, INPUT_PULLUP);

  // Allocate a PWM timer and set period hertz
  ESP32PWM::allocateTimer(0);
  testServo.setPeriodHertz(50);
  testServo.attach(servoPin, 500, 2500); // Widest standard pulse limits

  // Start at 0 degrees
  testServo.write(currentAngle);
  Serial.print("Initial Servo Angle: ");
  Serial.println(currentAngle);
}

void loop() {
  // Check if BOOT button is pressed (active LOW)
  if (digitalRead(bootButton) == LOW) {
    currentAngle++;
    if (currentAngle > 180) {
      currentAngle = 0; // Wrap around to 0
      Serial.println("\n--- Wrapped back to 0 ---");
    }
    
    testServo.write(currentAngle);
    
    Serial.print("Current Servo Angle: ");
    Serial.println(currentAngle);
    
    delay(50); // Controls rotation speed (50ms per degree)
  }
  delay(10); // Maintain stability
}
