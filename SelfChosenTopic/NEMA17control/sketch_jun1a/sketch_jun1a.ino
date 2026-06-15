#include <Servo.h>

// Create a servo object to control your SG90
Servo myservo;  

// Define the PWM pin connected to the servo signal wire
const int servoPin = 9; 

void setup() {
  // Attach the servo object to pin 9
  myservo.attach(servoPin);  
}

void loop() {
  // Sweep from 0 degrees to 180 degrees
  for (int pos = 0; pos <= 180; pos += 1) { 
    myservo.write(pos);              // Tell servo to go to position in variable 'pos'
    delay(15);                       // Waits 15ms for the servo to reach the position
  }
  
  delay(1000);                       // Wait 1 second at 180 degrees

  // Sweep back from 180 degrees to 0 degrees
  for (int pos = 180; pos >= 0; pos -= 1) { 
    myservo.write(pos);              // Tell servo to go to position in variable 'pos'
    delay(15);                       // Waits 15ms for the servo to reach the position
  }
  
  delay(2000);                       // Wait 1 second at 0 degrees
}