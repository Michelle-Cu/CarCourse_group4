#include <AccelStepper.h>

const int stepPin = 8; 
const int dirPin = 9;  
#define motorInterfaceType 1
AccelStepper stepper(motorInterfaceType, stepPin, dirPin);

// TB6600 set to 8 microsteps = 1600 steps per full 360° rotation
const int stepsPerRotation = 1600; 

void setup() {
  // CRITICAL FOR TB6600: Ensures the signal pulse is wide enough 
  // for the driver's internal electronics to catch it.
  stepper.setMinPulseWidth(20); 
  
  // Set a robust acceleration so the motor transitions smoothly
  stepper.setAcceleration(2000);    

  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("Arduino Ready! Type 'START' in the Serial Monitor and press Enter.");
}

void loop() {
  // 1. Check the serial monitor for input
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Remove any accidental spaces or hidden characters
    
    Serial.print("You typed: ");
    Serial.println(input);
    
    // 2. If the user types START, run the sequential moves
    if (input == "START") { 
      Serial.println("Starting movement sequence...");
      
      // Move 90 degrees in negative direction at 400 steps/sec
      moveAngleNegative(90.0, 400);
      
      delay(1000); // Pause at destination
      
      // Move an additional 270 degrees in negative direction at 400 steps/sec
      moveAngleNegative(270.0, 400);
      
      Serial.println("Sequence finished! Ready for next command.");
    }
  }
}

/**
 * Moves to a precise angle in the negative direction
 * @param angle - Total degrees to move (positive number, e.g., 90.0)
 * @param speed - Maximum speed during this movement
 */
void moveAngleNegative(float angle, float speed) {
  // Calculate steps required for the requested angle
  long stepsToMove = (long)((angle / 360.0) * stepsPerRotation);
  
  // Subtracting from current position guarantees it rolls backward
  long newTarget = stepper.currentPosition() - stepsToMove;
  
  stepper.setMaxSpeed(speed);
  stepper.moveTo(newTarget);
  
  // Block and drive the motor until it arrives exactly at the angle
  while (stepper.distanceToGo() != 0) {
    stepper.run(); // Must use run() here to utilize acceleration
  }
}

/**
 * Continuous negative rotation (no stopping)
 * @param speed - The speed in steps per second (positive number)
 */
void spinAlwaysNegative(float speed) {
  stepper.setMaxSpeed(speed); 
  stepper.setSpeed(-speed); // Negative speed keeps it spinning backward
}