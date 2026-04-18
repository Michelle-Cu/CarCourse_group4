#include <SPI.h>
#include <MFRC522.h>

// --- Configuration & Constants ---
#define CUSTOM_NAME "BT4"

// Pin Definitions (Common to both versions)
#define digital1  A3
#define digital2  A4
#define digital3  A5
#define digital4  A6
#define digital5  A7

const int PWMA = 10;   // Speed Motor A
const int AIN1 = 7;    // Direction A1
const int AIN2 = 6;    // Direction A2
const int BIN1 = 8;    // Direction B1
const int BIN2 = 9;    // Direction B2
const int PWMB = 11;   // Speed Motor B

// RFID MFRC522 (Mega Hardware SPI)
const int RST_PIN = 3; 
const int SS_PIN  = 2; 
MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- Bluetooth & Time Recording Global Variables ---
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;
bool BTConnected = false;
bool movesStarted = false;

unsigned long lastLoopTime = 0;
unsigned long lastIrTime = 0;
unsigned long lastBtTime = 0;

const unsigned long irCyclePeriod = 0; // Fast tracking (20ms)
const unsigned long btCyclePeriod = 200; // Stable BT reporting (200ms)

unsigned long irCycleCnt = 0;
unsigned long loopCycleCnt = 0;
unsigned long btCycleCnt = 0;

unsigned long lastThisLoopTime = 0;
unsigned long lastThisIrTime = 0;

// --- PID & Movement Global Variables ---
int val[10];
double Kp = 50.0; 
double Kd = 0.0;
double Ki = 0.0;
int Tp = 150; // Base speed

double lastError = 0;
double sumError = 0;
double w2 = 1.0; 
double w3 = 3.0;

int Act = 1, currentMove = -1, pt;     // 1: keep going, 2: turn left, 3: U-turn, 4: turn right
String pendingRFID = "";
unsigned long lastRfidTime = 0;
unsigned long step[5][5];
unsigned long pMillis = 0;
int count;

void setup() {
  Serial.begin(115200); 
  
  // Motor & Sensor Pins
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(digital1, INPUT); pinMode(digital2, INPUT); pinMode(digital3, INPUT); pinMode(digital4, INPUT); pinMode(digital5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  // --- Robust HM-10 Initialization (from mar30) ---
  while (!Serial);
  Serial.println("\n--- HM-10 Robust Initialization ---");

  for (int i = 0; i < 9; i++) {
    Serial.print("Testing baud rate: ");
    Serial.println(baudRates[i]);
    
    Serial3.begin(baudRates[i]);
    Serial3.setTimeout(100);
    delay(100);

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

  if (moduleReady) {
    Serial.println("Restoring factory defaults...");
    sendATCommand("AT+RENEW"); 
    delay(500);

    Serial.print("Setting name to: ");
    Serial.println(CUSTOM_NAME);
    String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
    sendATCommand(nameCmd.c_str()); 
    
    Serial.println("Enabling notifications...");
    sendATCommand("AT+NOTI1"); 

    Serial.println("Restarting module...");
    sendATCommand("AT+RESET"); 
    delay(1000);
    Serial3.begin(9600); 
    Serial.println("--- HM-10 Ready at 9600 Baud ---\n");
  } else {
    Serial.println("CRITICAL ERROR: HM-10 not responding.");
    Serial3.begin(9600); // Fallback
  }

  // Request initial move from Python
  Serial.println("Requesting first move...");
  Serial3.println("reqFirstMove");

  lastLoopTime = millis();
  lastIrTime = millis();
  lastBtTime = millis();
  
  delay(2000); // Give some time before starting
}

void loop() {
  unsigned long currentMillis = millis();
  lastThisLoopTime = currentMillis - lastLoopTime;
  lastLoopTime = currentMillis;
  loopCycleCnt++;

  // 1. Process ESP32 / Bluetooth Commands (Messaging)
  if (Serial3.available()) {
    static String inputBuffer = "";
    while (Serial3.available()) {
      char c = Serial3.read();
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0) {
          inputBuffer.trim();
          if (inputBuffer.length() > 0) {
            // Ignore ESP32 internal status messages that might be concatenated
            if (inputBuffer.indexOf("OK+") != -1 || inputBuffer.indexOf("ERROR+") != -1) {
              inputBuffer = "";
              continue; 
            }

            BTConnected = true;
            Serial.print("Remote Command: ");
            Serial.println(inputBuffer);
            
            if (inputBuffer.startsWith("nxtMove:")) {
              int next_val = inputBuffer.substring(8).toInt();
              if (currentMove == 0) {
                currentMove = next_val;
                movesStarted = true; // Mark that sequence has started
                Serial.print("Next move set to: ");
                Serial.println(currentMove);
              }
              // Always acknowledge with the actual move value we have
              Serial3.print("nxtMoveRecived: ");
              Serial3.println(currentMove);
            }

            if (inputBuffer.startsWith("rfidAck:")) {
              String ackUID = inputBuffer.substring(8);
              ackUID.trim();
              if (ackUID == pendingRFID) {
                Serial.print("RFID confirmed: ");
                Serial.println(pendingRFID);
                pendingRFID = "";
              }
            }
          }
          inputBuffer = "";
        }
      } else if (isPrintable(c)) {
        inputBuffer += c;
      }
    }
  }

  // 2. RFID Scanning
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String rfidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) rfidStr += "0";
      rfidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    
    // Only set as pending if it's a DIFFERENT card OR if 2 seconds have passed
    if (rfidStr != pendingRFID || (currentMillis - lastRfidTime > 2000)) {
      pendingRFID = rfidStr;
      lastRfidTime = currentMillis;
      Serial.print("New RFID scanned: ");
      Serial.println(pendingRFID);
      
      // Initial send
      Serial3.print("RFID:");
      Serial3.println(pendingRFID);
    }

    mfrc522.PICC_HaltA(); 
    mfrc522.PCD_StopCrypto1();
  }

  // 3. Sensor Reading & Movement Logic (Timed Cycle)
  if (currentMillis - lastIrTime >= irCyclePeriod) {
    lastThisIrTime = currentMillis - lastIrTime;
    lastIrTime = currentMillis;
    irCycleCnt++;

    // Read Sensors
    val[1] = digitalRead(digital1);
    val[2] = digitalRead(digital2);
    val[3] = digitalRead(digital3);
    val[4] = digitalRead(digital4);
    val[5] = digitalRead(digital5);

    count = 0;
    for(int i=1; i<6; i++) if(val[i] == HIGH) count++;

    // --- Manual Override via Serial Monitor (New Section) ---
    if (Serial.available()) {
      int cnt = Serial.parseInt();
      // Flush the remaining buffer (like \n or \r) so it doesn't trigger a 0 on the next loop
      while(Serial.available() > 0) Serial.read(); 
      
      if (cnt >= 0 && cnt <= 5) {
        count = cnt;
        Serial.print(">>> Manual Override! count set to: ");
        Serial.println(count);
      }
    }
    // --------------------------------------------------------

    // if (count >= 4) Tp = 100;
    // else Tp = 150;

    // Node Logic (from April1st)
    if( currentMove == 2 && count >=4 && Act == 1 ) { Act = 21; step[2][1] = millis(); }                        //reach node
    else if ( Act == 21 && count <= 3 && millis() - step[2][1] > 700 ) { Act = 22; step[2][2] = millis();          }//past node, turn

    else if( currentMove == 4 && count >=4 && Act == 1 ) { Act = 41; step[4][1] = millis(); }                        //reach node
    else if ( Act == 41 && count <= 1 && millis() - step[4][1] > 700  ) { Act = 42; step[4][2] = millis();          }//past node, turn

    else if( currentMove == 1 && count >=4 && Act == 1 ) { Act = 11; step[1][1] = millis(); }                        //reach node
    else if ( Act == 11 && count <= 1 && millis() - step[1][1] > 700 ) { Act = 12; step[1][2] = millis(); }//past node, turn

    else if( currentMove == 3 && count >=4 && Act == 1 ) { Act = 31; step[3][1] = millis(); }   //U turn at different speed
    else if( Act == 31 && count <= 3 && millis() - step[3][1] > 400) { Act = 32; step[3][2] = millis(); }     
    
    else if( millis() - step[2][2] > 400 && Act == 22 && count > 0 && count <= 3) { 
        Act = 1; currentMove = 0; 
    }
    else if( millis() - step[4][2] > 400 && Act == 42 && count > 0 && count <= 3 ) { 
        Act = 1; currentMove = 0; 
    }
    else if( millis() - step[3][2] > 400 && Act == 32 && count > 0 && count <= 3  ) { 
        Act = 1; currentMove = 0; 
    }
    else if( Act == 12 && count > 0 && count <= 3 ) { 
        Act = 1; currentMove = 0; 
    }

    // Movement Execution
    if( Act == 22 ) turnLeft( 180, 120 );
    else if( Act == 42 ) turnRight( 180, 150 ); 
    else if( Act == 31 ) turnLeft(150, 150);      //U turn 
    else if( Act == 32) turnLeft(100 , 100);
    else Tracking();
  }

  // 4. Connection Keep-Alive (Request move if idle)
  if (currentMillis - lastBtTime >= btCyclePeriod) {
    lastBtTime = currentMillis;
    if (!BTConnected || currentMove == 0) {
      if (!movesStarted) {
        Serial.println("Requesting first move...");
        Serial3.println("reqFirstMove");
      } else {
        Serial.println("Requesting next move...");
        Serial3.println("reqNxtMove");
      }
    }
    // Only send count to keep bandwidth free for moves and RFID scans
    Serial3.println(count);
  }
}

// --- Motor & PID Functions ---

void Tracking() {
  int l3 = val[1];
  int l2 = val[2];
  int m  = val[3];
  int r2 = val[4];
  int r3 = val[5];

  double numerator = (l3 * -w3) + (l2 * -w2) + (m * 0) + (r2 * w2) + (r3 * w3);
  double denominator = l3 + l2 + m + r2 + r3;
  if (denominator == 0) denominator = 0.0001; 
  
  double error = numerator / denominator;
  double dError = error - lastError; 
  sumError += error;                 
  sumError = constrain(sumError, -1000, 1000);

  double powerCorrection = (Kp * error) + (Kd * dError) + (Ki * sumError);

  int vR = Tp - powerCorrection;
  int vL = Tp + powerCorrection;

  vR = constrain(vR, -255, 255);
  vL = constrain(vL, -255, 255);

  MotorWriting(vL, vR);
  lastError = error;
}

void MotorWriting(double vL, double vR) {
  if (vR >= 0) {
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  } else {
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    vR = -vR;
  }
  if (vL >= 0) {
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  } else {
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    vL = -vL;
  }
  analogWrite(PWMB, constrain(vR, 0, 255)); 
  analogWrite(PWMA, constrain(vL, 0, 255));
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

// --- Bluetooth Helper Functions ---

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
