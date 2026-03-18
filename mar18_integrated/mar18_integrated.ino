#include <SPI.h>
#include <MFRC522.h>
#define CUSTOM_NAME "HM10-10"

#define analog1  A3
#define analog2  A4
#define analog3  A5
#define analog4  A6
#define analog5  A7

const int PWMA = 10;   // Speed Motor A
const int AIN1 = 7;    // Direction A1
const int AIN2 = 6;    // Direction A2
const int BIN1 = 8;    // Direction B1
const int BIN2 = 9;   // Direction B2
const int PWMB = 11;    // Speed Motor B (Check if wired to Pin 5 or 4)

// RFID MFRC522 (Mega Hardware SPI)
const int RST_PIN = 3; 
const int SS_PIN  = 2; 
MFRC522 mfrc522(SS_PIN, RST_PIN);


long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

int time = 0;
int motorSpeed = 150; // Default speed (0-255)

void setup() {
  
  Serial.begin(115200); 
  Serial3.begin(9600);  // HM-10 Bluetooth (Pins 14, 15)

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);               //Motor
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(analog1, INPUT);pinMode(analog2, INPUT);pinMode(analog3, INPUT);pinMode(analog4, INPUT);pinMode(analog5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  while (!Serial);
  Serial.println("Initializing HM-10...");

  // 1. Automatic Baud Rate Detection
  for (int i = 0; i < 9; i++) {
    Serial.print("Testing baud rate: ");
    Serial.println(baudRates[i]);
    Serial3.begin(baudRates[i]);
    Serial3.setTimeout(100);
    delay(100);

    // 2. Force Disconnection, Sending "AT" while connected forces the module to disconnect [2].
    Serial3.print("AT"); 
    
    if (waitForResponse("OK", 800)) {
      Serial.println("HM-10 detected and ready.");
      moduleReady = true;
      break; 
    } else {
      Serial3.end();
      delay(100);
    }
  }

  if (!moduleReady) {
    Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
    return;
  }

  // 3. Restore Factory Defaults
  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW"); // Restores all setup values
  delay(300);

  // 4. Set Custom Name via Macro
  Serial.print("Setting name to: ");
  Serial.println(CUSTOM_NAME);
  String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
  sendATCommand(nameCmd.c_str()); // Max length is 12
 
  sendATCommand("AT+NOTI1"); 
  sendATCommand("AT+ADDR?");
  sendATCommand("AT+RESET");
  Serial.println("Initialization Complete.");
}


void loop() {

  if (Serial3.available()) {
    Serial.println(Serial3.readString());
  }

  // 2. PC to Module: Read user input and truncate line endings
  if (Serial.available()) {
    static String inputBuffer = ""; 
    
    while (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\r' || c == '\n') {    // Check if the character is a line ending
        if (inputBuffer.length() > 0) {
          Serial3.print(inputBuffer);
          // Debug: Show what was actually sent
          Serial.print("\n[Sent to HM-10: ");
          Serial.print(inputBuffer);
          Serial.println("]");
          
          inputBuffer = ""; // Clear buffer for next command
        }
      } else {
        inputBuffer += c; // Add character to buffer
      }
    }
  }

  int val[10];
  val[1] = analogRead( analog1 ), val[2] = analogRead( analog2 ), val[3] = analogRead( analog3 ), val[4] = analogRead( analog4 ), val[5] = analogRead( analog5 );
  for(int i=1; i<=5; i++){
    Serial.print(val[i]); Serial.print("  "); if(i==5) Serial.print("\n");
  }

  // 3. RFID Scanning
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("Card UID:");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    mfrc522.PICC_HaltA(); // Stop reading
  }
  
  if( time % 20000 == 0) moveForward();
  else if( time % 20000 == 5000) moveBackward();
  else if( time % 20000 == 10000) turnLeft();
  else if( time % 20000 == 15000) turnRight();

  delay(200); time += 200;
}

void executeCommand(char cmd) {
  switch (cmd) {
    case 'F': moveForward(); break;
    case 'B': moveBackward(); break;
    case 'L': turnLeft();    break;
    case 'R': turnRight();   break;
    case 'S': stopMotors();  break;
  }
}

void moveForward() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, motorSpeed); analogWrite(PWMB, motorSpeed);
}

void moveBackward() {
  digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, motorSpeed); analogWrite(PWMB, motorSpeed);
}

void turnLeft() {
  digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, motorSpeed); analogWrite(PWMB, motorSpeed);
}

void turnRight() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, motorSpeed); analogWrite(PWMB, motorSpeed);
}

void stopMotors() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 0);    analogWrite(PWMB, 0);
}


/**
 * Helper to send AT commands (Uppercase, no \r or \n) [6]
 */
void sendATCommand(const char* command) {
  Serial3.print(command);
  waitForResponse("", 1000); 
}

/**
 * Helper to check response for specific substrings
 */
bool waitForResponse(const char* expected, unsigned long timeout) {
  unsigned long start = millis();
  Serial3.setTimeout(timeout);
  String response = Serial3.readString();
  if (response.length() > 0) {
    Serial.print("HM10 Response: ");
    Serial.println(response);
  }
  return (response.indexOf(expected) != -1);
}