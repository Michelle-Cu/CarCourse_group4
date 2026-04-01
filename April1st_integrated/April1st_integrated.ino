#include <SPI.h>
#include <MFRC522.h>
#define CUSTOM_NAME "HM10-10"

#define digital1  A3
#define digital2  A4
#define digital3  A5
#define digital4  A6
#define digital5  A7
int val[10];

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

// PID Constants - You will tune these!
double Kp = 50.0; 
double Kd = 0.0;
double Ki = 0.0;
int Tp = 100; // Base speed

// Memory for PID
double lastError = 0;
double sumError = 0;

// Weights for the sensors (w2 and w3 from your slide)
double w2 = 1.0; 
double w3 = 3.0;

int motorSpeed = 150; // Default speed (0-255)

int Act = 1, d[2000], dp=0;
unsigned long step[5][5];
int tp = 1, todo[1000];         //1 keep going,  2 turn left,  3 U-turn,  4 turn right
unsigned long pMillis = 0, t1Millis = 0, t2Millis = 0;


void setup() {
  
  for(int i=1; i<1000; ){
    todo[i++] = 1; todo[i++] = 3; todo[i++] = 2; todo[i++] = 3; todo[i++] = 4; todo[i++] = 3;
  }

  Serial.begin(115200); 
  Serial3.begin(9600);  // HM-10 Bluetooth (Pins 14, 15)

  pinMode(PWMA, OUTPUT); pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);               //Motor
  pinMode(PWMB, OUTPUT); pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode( digital1 , INPUT);pinMode( digital2, INPUT);pinMode( digital3, INPUT);pinMode( digital4, INPUT);pinMode( digital5, INPUT);

  SPI.begin();
  mfrc522.PCD_Init();
/*
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
    }
  }

  if (!moduleReady) {
    Serial.println("Failed to detect HM-10. Check 3.3V VCC and wiring.");
    return;
  }

  // 3. Restore Factory Defaults
  Serial.println("Restoring factory defaults...");
  sendATCommand("AT+RENEW"); // Restores all setup values

  // 4. Set Custom Name via Macro
  Serial.print("Setting name to: ");
  Serial.println(CUSTOM_NAME);
  String nameCmd = "AT+NAME" + String(CUSTOM_NAME);
  sendATCommand(nameCmd.c_str()); // Max length is 12
 
  sendATCommand("AT+NOTI1"); 
  sendATCommand("AT+ADDR?");
  sendATCommand("AT+RESET");
  Serial.println("Initialization Complete.");
  */
  delay(3000);
}


void loop() {
/*
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
*/

  val[1] = digitalRead( digital1 ), val[2] = digitalRead( digital2 ), val[3] = digitalRead( digital3 ), val[4] = digitalRead( digital4 ), val[5] = digitalRead( digital5 );
  
  if(  millis() - pMillis > 500 ){
    for(int i=1; i<=5; i++){
      //Serial.print(val[i]); Serial.print("  "); if(i==5) Serial.print("\n");
    }
    pMillis = millis();
  }
/*
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
*/
  int count = 0;
  for(int i=1; i<6; i++) {
    if( val[i] == HIGH  ) {  count++; }
  }
  
  int l = 150+ 40*( 1.5*val[1] + val[2]  - val[5] - 1.5*val[4] );
  int r = 150+ 40*( 1.5*val[4] + val[5]  - val[1] - 1.5*val[2] );

  //Serial.print("count = ");Serial.println(count);

  if( todo[tp] == 2 && count >=4 && Act == 1 ) { Act = 21; step[2][1] = millis(); }                        //reach node
  else if ( Act == 21 && count <= 3 && millis() - step[2][1] > 500 ) { Act = 22; step[2][2] = millis();          }//past node, turn

  else if( todo[tp] == 4 && count >=4 && Act == 1 ) { Act = 41; step[4][1] = millis(); }                        //reach node
  else if ( Act == 41 && count <= 1 && millis() - step[4][1] > 500  ) { Act = 42; step[4][2] = millis();          }//past node, turn

  else if( todo[tp] == 1 && count >=4 && Act == 1 ) { Act = 11; step[1][1] = millis(); }                        //reach node
  else if ( Act == 11 && count <= 1 && millis() - step[1][1] > 1200 ) { Act = 12; step[1][2] = millis(); }//past node, turn

  else if( todo[tp] == 3 && count >=4 && Act == 1 ) { Act = 32; step[3][2] = millis(); }                        //32start turning
  

  else if( millis() - step[2][2] > 400 && Act == 22 && count <= 2 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) ) { 
      Act = 1; tp++;  //found line
  }
  else if( millis() - step[4][2] > 400 && Act == 42 && count <= 3 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) ) { 
      Act = 1; tp++;  //found line
  }
  else if( millis() - step[3][2] > 400 && Act == 32 && count <= 3 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) ) { //millis() - step[3][2] > 500 &&
      Act = 1; tp++;  //found line, finished step
  }
  else if( Act == 12 && count <= 3 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) ) { 
      Act = 1; tp++;  //found line, finished step
  }
  if(tp>500) tp = 1;

  if( Act == 32 || Act == 31 ) turnLeft(120, 120);      //U turn does not require going past node
  else if( Act == 22 ) turnLeft( 100, 100 );
  else if( Act == 42 ) turnRight( 100, 100 );
  else if(Act == 12) stopMotors();
  //else moveForward( r , l );       //Change to Tracking later
  else Tracking();
  //if(Act != dp) Serial.println(Act) ;dp = Act;
  
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

void MotorWriting(double vL, double vR) {
  if (vR >= 0) {
    digitalWrite(BIN1, HIGH); // Standard Forward direction
    digitalWrite(BIN2, LOW);
  } else {
    digitalWrite(BIN1, LOW);  // Reverse direction
    digitalWrite(BIN2, HIGH);
    vR = -vR;                 // Convert negative to positive for analogWrite
  }

  if (vL >= 0) {
    digitalWrite(AIN1, HIGH); // Standard Forward direction
    digitalWrite(AIN2, LOW);
  } else {
    digitalWrite(AIN1, LOW);  // Reverse direction
    digitalWrite(AIN2, HIGH);
    vL = -vL;                 // Convert negative to positive for analogWrite
  }

  analogWrite(PWMB, constrain(vR, 0, 255)); 
  analogWrite(PWMA, constrain(vL, 0, 255));
}

void Tracking() {
  int l3 = val[1];
  int l2 = val[2];
  int m  = val[3];
  int r2 = val[4];
  int r3 = val[5];

  double numerator = (l3 * -w3) + (l2 * -w2) + (m * 0) + (r2 * w2) + (r3 * w3);
  double denominator = l3 + l2 + m + r2 + r3;
  // Avoid division by zero if robot is lifted or off-track
  if (denominator == 0) denominator = 0.0001; 
  
  double error = numerator / denominator;

  double dError = error - lastError; // Derivative: Change in error
  sumError += error;                 // Integral: Accumulated error
  
  // Constrain sumError to prevent "Integral Windup"
  sumError = constrain(sumError, -1000, 1000);

  double powerCorrection = (Kp * error) + (Kd * dError) + (Ki * sumError);

  int vR = Tp - powerCorrection;
  int vL = Tp + powerCorrection;

  vR = constrain(vR, -255, 255);
  vL = constrain(vL, -255, 255);

  MotorWriting(vL, vR);

  lastError = error;
}