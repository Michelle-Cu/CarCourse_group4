#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial1.begin(9600);   // Talk to DFPlayer Mini (Pins 18/19)
  Serial.begin(115200);  // Talk to Serial Monitor on your computer

  Serial.println(F("Initializing DFPlayer..."));

  if (!myDFPlayer.begin(Serial1)) {  
    Serial.println(F("Unable to begin. Please check wiring/SD card!"));
    while(true);
  }
  
  Serial.println(F("DFPlayer Mini online."));
  myDFPlayer.volume(25);  // Set volume (0 to 30)
  
  Serial.println(F("Type '1' in the Serial Monitor and press Enter to play!"));
}

void loop() {
  // Check if data has been typed into the Serial Monitor
  if (Serial.available() > 0) {
    char incomingByte = Serial.read(); // Read the character

    if (incomingByte -'0' > 0 ) {
      //Serial.println(F("Command received: Playing track 0001.mp3"));
      myDFPlayer.play(incomingByte - '0'); 
    }
    
  }
}