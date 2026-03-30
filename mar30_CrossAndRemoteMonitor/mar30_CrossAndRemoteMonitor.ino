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

// Timers
unsigned long lastLoopTime = 0;
unsigned long lastIrTime = 0;
unsigned long lastBtTime = 0;

const unsigned long irCyclePeriod = 20; // Fast tracking (20ms)
const unsigned long btCyclePeriod = 100; // Stable BT reporting (100ms)

unsigned long irCycleCnt = 0;
unsigned long loopCycleCnt = 0;
unsigned long btCycleCnt = 0;

int motorSpeed = 150; 
int turning = 0, turnProgress = 0;
int moveSq[8] = {2, 3, 0, 3, 1, 3, 0, 3}; 
int sqIdx = -1;

// Global state for BT reporting
int global_dval[6] = {0,0,0,0,0,0};
unsigned long lastThisLoopTime = 0;
unsigned long lastThisIrTime = 0;

void setup() {
  Serial.begin(115200); 
  
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(analog1, INPUT);pinMode(analog2, INPUT);pinMode(analog3, INPUT);pinMode(analog4, INPUT);pinMode(analog5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  lastLoopTime = millis();
  lastIrTime = millis();
  lastBtTime = millis();

  while (!Serial);
  Serial.println("\n--- HM-10 Robust Initialization ---");

  // 1. Find Current Baud Rate
  int currentBaudIdx = -1;
  for (int i = 0; i < 9; i++) {
    Serial.print("Probing "); Serial.print(baudRates[i]); Serial.print("... ");
    Serial3.begin(baudRates[i]);
    delay(200);
    Serial3.print("AT");
    if (waitForResponse("OK", 500)) {
      Serial.println("DETECTED!");
      currentBaudIdx = i;
      break;
    }
    Serial3.end();
  }

  if (currentBaudIdx == -1) {
    Serial.println("CRITICAL ERROR: HM-10 not responding.");
    return;
  }

  // 2. Factory Reset to known state
  Serial.print("Restoring Defaults (AT+RENEW)... ");
  Serial3.print("AT+RENEW");
  delay(600);
  
  // 3. Re-Sync at 9600
  Serial3.begin(9600);
  delay(200);
  Serial.print("Verifying 9600 sync... ");
  Serial3.print("AT");
  if (waitForResponse("OK", 500)) Serial.println("OK.");
  else Serial.println("Proceeding...");

  // 4. Configure Pairing Parameters AT 9600
  Serial.print("Setting Name to "); Serial.print(CUSTOM_NAME); Serial.print("... ");
  String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
  Serial3.print(nameCmd);
  waitForResponse("OK", 1000);

  Serial.print("Setting Mode to Peripheral... ");
  Serial3.print("AT+ROLE0"); 
  waitForResponse("OK", 500);

  Serial.print("Enabling Immediate Advertising... ");
  Serial3.print("AT+IMME0"); 
  waitForResponse("OK", 500);

  Serial.print("Enabling Connect Notification... ");
  Serial3.print("AT+NOTI1"); 
  waitForResponse("OK", 500);

  Serial.print("Finalizing (AT+RESET)... ");
  Serial3.print("AT+RESET");
  waitForResponse("OK", 1000);
  
  delay(1000); 
  Serial.println("--- HM-10 Ready at 9600 Baud ---\n");
}


void loop() {
  unsigned long currentMillis = millis();
  lastThisLoopTime = currentMillis - lastLoopTime;
  lastLoopTime = currentMillis;

  loopCycleCnt++;

  // 1. Process ESP32 commands
  if (Serial3.available()) {
    String cmd = Serial3.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("Remote Command: ");
      Serial.println(cmd);
    }
  }

  // 2. RFID Scanning
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("Card UID:");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    mfrc522.PICC_HaltA(); // Stop reading
  }

  // 3. High-Speed IR Reading and Tracking (20ms)
  if (currentMillis - lastIrTime >= irCyclePeriod) {
    lastThisIrTime = currentMillis - lastIrTime;
    lastIrTime = currentMillis;
    irCycleCnt++;
    
    global_dval[1] = digitalRead(analog1);
    global_dval[2] = digitalRead(analog2);
    global_dval[3] = digitalRead(analog3);
    global_dval[4] = digitalRead(analog4);
    global_dval[5] = digitalRead(analog5);

    int cnt = 0;
    for (int i = 1; i <= 5; i++) {
      if (global_dval[i]) cnt++;
    }

    if (cnt >= 3 && turning == 0) {
      turning = 1;
      sqIdx = (sqIdx + 1) % 8;
    }
    if (turning == 1 && cnt <= 2) turning = 2;

    if (turning == 0) track(global_dval);
    else if (turning == 1) moveForward(motorSpeed, motorSpeed);
    else if (turning == 2) startTurn(global_dval, moveSq[sqIdx]);
    else if (turning == 3) endTurn(global_dval, moveSq[sqIdx]);
  }

  // 4. Decoupled Bluetooth Reporting (100ms)
  if (currentMillis - lastBtTime >= btCyclePeriod) {
    lastBtTime = currentMillis;
    btCycleCnt++;

    // Format: loopCycleCnt, thisLoopTime, irCycleCnt, thisIrTime, IR1..5, T1..5 (Total 14 values)
    Serial3.print(loopCycleCnt); Serial3.print(",");
    Serial3.print(lastThisLoopTime); Serial3.print(",");
    Serial3.print(irCycleCnt); Serial3.print(",");
    Serial3.print(lastThisIrTime); Serial3.print(",");
    
    for (int i = 1; i <= 5; i++) {
      Serial3.print(global_dval[i]);
      Serial3.print(",");
    }
    for (int i = 1; i <= 5; i++) {
      Serial3.print("1"); // Placeholder for thresholds
      if (i < 5) Serial3.print(",");
    }
    Serial3.println();
  }
}

void track(int* dval) {
  int l = 60*(dval[1] + dval[2]) + 100;
  int r = 60*(dval[5] + dval[4]) + 100;
  // l = max(l, 200); r = max(r, 200);
  moveForward(r, l);
}

void startTurn(int* dval, int turnType) {
  if (turnType == 0) {track(dval); turning = 0; return;}
  else if (turnType == 1) leftTurn(90);
  else if (turnType == 2) rightTurn(90);
  else if (turnType == 3) {
    moveForward(150, 150); delay(100); // Move forward a bit before U-turn
    leftTurn(90); // U-turn (left)
  }
  turning = 3; turnProgress = 0;
}

void endTurn(int* dval, int turnType) {
  // Check if turn is complete based on IR readings
  if (turnType == 1) { // left turn 
    if (turnProgress == 0 && (dval[3] == 1 || dval[4] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[1] == 1 || dval[2] == 1)) { // Completed left turn
      turning = 0; // Start tracking again
      track(dval);
    }
  }
  else if (turnType == 2) { // right turn
    if (turnProgress == 0 && (dval[2] == 1 || dval[3] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[4] == 1 || dval[5] == 1)) { // Completed right turn  
      turning = 0; // Start tracking again
      track(dval);
    }
  }
  else if (turnType == 3) { // U-turn
    // if (turnProgress == 0 && (dval[3] == 1 || dval[4] == 1 || dval[5] == 1)) { // Passed middle sensor
    //   turnProgress = 1; 
    // }
    // else if (turnProgress == 1 && (dval[1] == 1 || dval[2] == 1 || dval[3] == 1)) { // Passed middle sensor (if starting on right side)
    //   turnProgress = 2; 
    // }
    // else if (turnProgress == 2 && (dval[3] == 1 || dval[4] == 1 || dval[5] == 1)) { // Completed U-turn
    //   turnProgress = 3;
    // }
    // else if (turnProgress == 3 && (dval[1] == 1 || dval[2] == 1 || dval[3] == 1)) { // Completed U-turn (if starting on right side)
    //   turning = 0; // Start tracking again
    //   track(dval);
    // }
    if (turnProgress == 0 && (dval[2] == 1 || dval[3] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[4] == 1 || dval[5] == 1)) { // Completed left turn
      turning = 0; // Start tracking again
      track(dval);
    }
  }
}


void Motor(int lSpeed, int rSpeed) {
  if (lSpeed > 0) {
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  }else{
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  }
  if (rSpeed > 0) {
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);  
  }else{
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  }
  analogWrite(PWMA, abs(lSpeed));
  analogWrite(PWMB, abs(rSpeed));
}

void moveForward(int lSpeed, int rSpeed) {
  Motor(lSpeed, rSpeed);
}

void leftTurn(int speed) {
  Motor(-speed, speed);
}

void rightTurn(int speed) {
  Motor(speed, -speed);
}

void stopMotors() {
  Motor(0, 0);
}

void sendATCommand(const char* command) {
  Serial3.print(command);
  waitForResponse("", 1000); 
}


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