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
unsigned long irCyclePeriod = 10;
unsigned long irCycleCnt = 0;
unsigned long loopCycleCnt = 0;
unsigned long avgLoopTime = 0;
unsigned long avgIrCycleTime = 0;

int motorSpeed = 150; // Default speed (0-255)
int thres[6] = { 0, 500, 555, 686, 400, 583 };
int Cal = -350;
int isTurning = 0;

void setup() {

  for(int i=1; i<6; i++) thres[i] += Cal;
  
  Serial.begin(115200); 
  Serial3.begin(9600);  // HM-10 Bluetooth (Pins 14, 15)

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
    
    int val[6]; // index 1 to 5
    val[1] = analogRead(analog1);
    val[2] = analogRead(analog2);
    val[3] = analogRead(analog3);
    val[4] = analogRead(analog4);
    val[5] = analogRead(analog5);

    // Uncomment and implement when ready:
    // track(val); 

    // Send data to ESP32 via Bluetooth (Serial3) every IR cycle
    // Format: LoopCnt,LoopTime,AvgLoopTime,IrCnt,IrTime,AvgIrTime,IR1..5,Thresh1..5
    Serial3.print(loopCycleCnt); Serial3.print(",");
    Serial3.print(thisLoopTime); Serial3.print(",");
    // Serial3.print(avgLoopTime); Serial3.print(",");
    Serial3.print(irCycleCnt); Serial3.print(",");
    Serial3.print(thisIrTime); Serial3.print(",");
    // Serial3.print(avgIrCycleTime); Serial3.print(",");
    
    for (int i = 1; i <= 5; i++) {
      Serial3.print(val[i]);
      Serial3.print(",");
    }
    for (int i = 1; i <= 5; i++) {
      Serial3.print(val[i] > thres[i] ? "1" : "0");
      if (i < 5) Serial3.print(",");
    }
    Serial3.println();
  }
}



void moveForward( int lSpeed, int rSpeed ) {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, lSpeed ); analogWrite(PWMB, rSpeed );
}

void moveBackward( int lSpeed , int rSpeed ) {
  digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, lSpeed ); analogWrite(PWMB, rSpeed );
}

void turnLeft( int lSpeed , int rSpeed ) {
  digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, lSpeed ); analogWrite(PWMB, rSpeed );
}

void turnRight( int lSpeed , int rSpeed ) {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, lSpeed ); analogWrite(PWMB, rSpeed );
}

void stopMotors() {
  digitalWrite(AIN1, LOW); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 0);    analogWrite(PWMB, 0);
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