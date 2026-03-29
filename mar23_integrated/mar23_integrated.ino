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

unsigned long lastLoopTime = 0;
unsigned long lastIrTime = 0;
unsigned long irCyclePeriod = 50;
unsigned long irCycleCnt = 0;
unsigned long loopCycleCnt = 0;
unsigned long avgLoopTime = 0;
unsigned long avgIrCycleTime = 0;

int motorSpeed = 150; // Default speed (0-255)
// int thres[6] = { 0, 500, 555, 686, 400, 583 };
// int Cal = -350;
int turning = 0, turnProgress = 0;
int moveSq[8] = {2, 3, 0, 3, 1, 3, 0, 3}; // 0: forward, 1: left, 2: right, 3: uturn, 4: stop
int sqIdx = -1;

void setup() {

  // for(int i=1; i<6; i++) thres[i] += Cal;
  
  Serial.begin(115200); 
  Serial3.begin(115200);  // HM-10 Bluetooth (Higher baud rate)

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);               //Motor
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(analog1, INPUT);pinMode(analog2, INPUT);pinMode(analog3, INPUT);pinMode(analog4, INPUT);pinMode(analog5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  lastLoopTime = millis();
  lastIrTime = millis();
  

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

  // 4. Set Baud Rate to 115200 (AT+BAUD4)
  // Note: Different HM-10 clones use different indices. 4 is usually 115200.
  Serial.println("Setting HM-10 baud rate to 115200...");
  sendATCommand("AT+BAUD4"); 
  Serial3.begin(115200);
  delay(200);

  // 5. Set Custom Name via Macro
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
  unsigned long currentMillis = millis();
  unsigned long thisLoopTime = currentMillis - lastLoopTime;
  lastLoopTime = currentMillis;

  loopCycleCnt++;
  // avgLoopTime = (avgLoopTime * (loopCycleCnt - 1) + thisLoopTime) / loopCycleCnt;

  // 1. Process ESP32 commands (placeholder for future addons - e.g. move, turn)
  if (Serial3.available()) {
    String cmd = Serial3.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      Serial.print("Received from ESP32: ");
      Serial.println(cmd);
      // Future: parse cmd and execute turns/movements here
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

  // 3. IR Reading and Tracking
  if (currentMillis - lastIrTime >= irCyclePeriod) {
    unsigned long thisIrTime = currentMillis - lastIrTime;
    lastIrTime = currentMillis;
    
    irCycleCnt++;
    // avgIrCycleTime = (avgIrCycleTime * (irCycleCnt - 1) + thisIrTime) / irCycleCnt;
    
    // int aval[6]; // index 1 to 5
    // aval[1] = analogRead(analog1);
    // aval[2] = analogRead(analog2);
    // aval[3] = analogRead(analog3);
    // aval[4] = analogRead(analog4);
    // aval[5] = analogRead(analog5);

    int dval[6]; // index 1 to 5
    dval[1] = digitalRead(analog1);
    dval[2] = digitalRead(analog2);
    dval[3] = digitalRead(analog3);
    dval[4] = digitalRead(analog4);
    dval[5] = digitalRead(analog5);

    int cnt = 0;
    for (int i = 1; i <= 5; i++) {
      if (dval[i]) cnt++;
    }

    if (cnt >= 3 && turning == 0) {
      turning = 1;
      sqIdx += 1;
      if (sqIdx >= 8) {
        sqIdx = 0;
        stopMotors();
      }
    }
    if (turning == 1 && cnt <= 2) turning = 2;

    // Uncomment and implement when ready:
    if (turning == 0) track(dval);
    else if (turning == 1) moveForward(motorSpeed, motorSpeed); // on black node
    else if (turning == 2) startTurn(dval, moveSq[sqIdx]);
    else if (turning == 3) endTurn(dval, moveSq[sqIdx]);

    // Send data to ESP32 via Bluetooth (Serial3) every IR cycle
    // Format: LoopCnt,LoopTime,AvgLoopTime,IrCnt,IrTime,AvgIrTime,IR1..5,Thresh1..5
    Serial3.print(loopCycleCnt); Serial3.print(",");
    Serial3.print(thisLoopTime); Serial3.print(",");
    // Serial3.print(avgLoopTime); Serial3.print(",");
    Serial3.print(irCycleCnt); Serial3.print(",");
    Serial3.print(thisIrTime); Serial3.print(",");
    // Serial3.print(avgIrCycleTime); Serial3.print(",");
    
    for (int i = 1; i <= 5; i++) {
      Serial3.print(dval[i]);
      Serial3.print(",");
    }
    for (int i = 1; i <= 5; i++) {
      // Serial3.print(val[i] > thres[i] ? "1" : "0");
      Serial3.print("1");
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
  else if (turnType == 3) leftTurn(90); // U-turn (left)
  turning = 3; turnProgress = 0;
}

void endTurn(int* dval, int turnType) {
  // Check if turn is complete based on IR readings
  if (turnType == 1) { // left turn 
    if (turnProgress == 0 && (dval[3] == 1 || dval[4] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[2] == 1 || dval[3] == 1)) { // Completed left turn
      turning = 0; // Start tracking again
      track(dval);
    }
  }
  else if (turnType == 2) { // right turn
    if (turnProgress == 0 && (dval[2] == 1 || dval[3] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[3] == 1 || dval[4] == 1)) { // Completed right turn  
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
    if (turnProgress == 0 && (dval[3] == 1 || dval[4] == 1)) { // Passed middle sensor
      turnProgress = 1; 
    }
    else if (turnProgress == 1 && (dval[2] == 1 || dval[3] == 1)) { // Completed left turn
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