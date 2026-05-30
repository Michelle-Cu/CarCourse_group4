#include <AccelStepper.h>

const int stepPin = 8; 
const int dirPin = 9;  
#define motorInterfaceType 1
AccelStepper stepper(motorInterfaceType, stepPin, dirPin);

const int stepsPerRotation = 6400; 

long angleToSteps(float angle) {
  return (long)((angle / 360.0) * stepsPerRotation);
}

void setup() {
  stepper.setMaxSpeed(800);       
  stepper.setAcceleration(400);    
}

void loop() {
  // Move an ADDED 90 degrees forward from wherever it is right now
  stepper.move(angleToSteps(90.0));
  
  // Drive the motor until it finishes traveling those 90 degrees
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  
  delay(500); // Pause for half a second before advancing another 90 degrees
}