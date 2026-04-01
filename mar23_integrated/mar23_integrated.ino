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


int motorSpeed = 150; // Default speed (0-255)
int thres[6] = { 0, 400, 555, 600, 400, 583 };
int Cal = -300;
int isTurning = 0;
int t[1000], tp = 0;         //1 keep going,  2 turn left,  3 U-turn,  4 turn right
unsigned long pMillis = 0, t1Millis = 0, t2Millis = 0;
int l=100, r=100;

void setup() {

  for(int i=1; i<6; i++) thres[i] += Cal;
  
  for(int i=0; i<1000; i++){
    if( i%2 == 1 ) t[i] = 2;
    else t[i] = 1;
  }

  Serial.begin(115200); 
  
  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(analog1, INPUT);pinMode(analog2, INPUT);pinMode(analog3, INPUT);pinMode(analog4, INPUT);pinMode(analog5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();

  lastLoopTime = millis();
  lastIrTime = millis();

  while (!Serial);
  Serial.println("\n--- HM-10 Initialization (9600 Baud) ---");

  // 1. Find Current Baud Rate
  int currentBaudIdx = -1;
  for (int i = 0; i < 9; i++) {
    Serial.print("Probing "); Serial.print(baudRates[i]); Serial.print("... ");
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
    }
    Serial3.end();
  }

  if (currentBaudIdx == -1) {
    Serial.println("CRITICAL ERROR: HM-10 not responding.");
    return;
  }

  // 3. Restore Factory Defaults
  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW"); // Restores all setup values

  // 4. Set Custom Name via Macro
  Serial.print("Setting name to: ");
  Serial.println(CUSTOM_NAME);
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
  unsigned long thisLoopTime = currentMillis - lastLoopTime;
  lastLoopTime = currentMillis;

  loopCycleCnt++;
  // avgLoopTime = (avgLoopTime * (loopCycleCnt - 1) + thisLoopTime) / loopCycleCnt;

  // 1. Process ESP32 commands
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
  val[1] = digitalRead( analog1 ), val[2] = digitalRead( analog2 ), val[3] = digitalRead( analog3 ), val[4] = digitalRead( analog4 ), val[5] = digitalRead( analog5 );
  
  if(  millis() - pMillis > 500 ){
    for(int i=1; i<=5; i++){
      Serial.print(val[i]); Serial.print("  "); if(i==5) Serial.print("\n");
    }
    pMillis = millis();
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

  int count = 0;
  for(int i=1; i<6; i++) {
    if( val[i] == HIGH  ) {
      val[i] = 30;    //accelerate at black
      count++;
    }
    else val[i] = 0;
  }
  Serial.print("count = ");Serial.println(count);

  if( count == 5 && isTurning == 0 ) { isTurning = 1; t1Millis = millis(); }                        //reach node
  if (isTurning == 1 && count <= 1 && millis() - t1Millis > 1200  ) { isTurning = 2; t2Millis = millis();          }//past node, turn

  int l = val[1] + val[2] + 100 - val[5] - 1.5*val[4] ;
  int r = val[5] + val[4] + 100 - val[1] - 1.5*val[2];
  while( l > 200 || r > 200){ l *= 0.5; r *= 0.5; }
  
  if( isTurning == 2 && count <= 3 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) && millis() - t2Millis > 500) { 
      isTurning = 0;  //found line, finished step
      tp++;
      moveForward( r , l );
  }
  else if( isTurning == 2 ) {
    if( t[tp] == 1) moveForward( r, l );
    else if( t[tp] == 2) turnLeft( 60, 60 );
    else if( t[tp] == 3) ;
    else if( t[tp] == 4) turnRight( 60, 60 );
  }
  else moveForward( r , l );
    
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