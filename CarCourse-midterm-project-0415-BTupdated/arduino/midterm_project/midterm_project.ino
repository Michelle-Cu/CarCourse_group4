/***************************************************************************/
// File       [final_project.ino]
// Author     [Erik Kuo]
// Synopsis   [Code for managing main process]
// Functions  [setup, loop, Search_Mode, Hault_Mode, SetState]
// Modify     [2020/03/27 Erik Kuo]
/***************************************************************************/

#define DEBUG  // debug flag

// for RFID
#include <MFRC522.h>
#include <SPI.h>

// TODO: 請將腳位寫入下方
#define MotorR_I1 9     // 定義 A1 接腳（右）
#define MotorR_I2 8     // 定義 A2 接腳（右）
#define MotorR_PWMR 11  // 定義 ENA (PWM調速) 接腳
#define MotorL_I3 7     // 定義 B1 接腳（左）
#define MotorL_I4 6     // 定義 B2 接腳（左）
#define MotorL_PWML 10  // 定義 ENB (PWM調速) 接腳
// 循線模組, 請按照自己車上的接線寫入腳位
#define IRpin_LL A3
#define IRpin_L A4
#define IRpin_M A5
#define IRpin_R A6
#define IRpin_RR A7
// RFID, 請按照自己車上的接線寫入腳位
#define RST_PIN 3                 // 讀卡機的重置腳位
#define SS_PIN 2                  // 晶片選擇腳位
MFRC522 mfrc522(SS_PIN, RST_PIN);  // 建立MFRC522物件
// BT
#define CUSTOM_NAME "BT4" // Matches April4th_remote.ino

// BT related global variables (updated)
bool BTConnected = false;
bool movesStarted = false;
bool movesReceived = false;
bool gameStarted = false;
String pendingRFID = "";
unsigned long long lastRfidTime = 0;
unsigned long long lastBtTime = 0;
const unsigned long btCyclePeriod = 500;

// New global variables for indexing
int executedMovesCount = 0;
int nextAbsIdx = 1;

// HM-10 Robust Initialization variables
long baudRates[] = {9600, 19200, 38400, 57600, 115200, 4800, 2400, 1200, 230400};
bool moduleReady = false;

/*=====Import header files=====*/
#include "RFID.h"
#include "bluetooth.h"
#include "node.h"
#include "track.h"
/*=====Import header files=====*/

/*===========================define pin & create module object================================*/
// BlueTooth
// BT connect to Serial3 (Hardware Serial)
// Mega               HC05/HM10
// Pin  (Function)    Pin
// 14    TX       ->  RX
// 15    RX       <-  TX
// Note: Serial3 on Mega uses pins 14 (TX) and 15 (RX)

/*===========================define pin & create module object===========================*/

/*============setup============*/
void setup() {
    // Serial window
    Serial.begin(115200);
    Serial3.begin(9600);
    
    // RFID initial
    SPI.begin();
    mfrc522.PCD_Init();
    
    // Motor & Sensor Pins
    pinMode(MotorL_I3, OUTPUT); pinMode(MotorL_I4, OUTPUT); pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_I1, OUTPUT); pinMode(MotorR_I2, OUTPUT); pinMode(MotorR_PWMR, OUTPUT);
    pinMode(IRpin_LL, INPUT); pinMode(IRpin_L, INPUT); pinMode(IRpin_M, INPUT); pinMode(IRpin_R, INPUT); pinMode(IRpin_RR, INPUT);


    Serial.println("Start!");
    // Serial.println("Requesting first move...");
    // Serial3.println("reqFirstMove");  // first move request for BT
}
/*============setup============*/


/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;  // 紅外線模組的讀值(0->white,1->black)
int _Tp = 240;                                // set your own value for motor power
bool state = true;     // set state to false to halt the car, set state to true to activate the car
BT_CMD _cmd = NOTHING;  // enum for bluetooth message, reference in bluetooth.h line 2

int Act = 1;          // current action state
int moveBuffer[3] = {0, 0, 0}; 
int bufferCount = 0;
#define currentMove (bufferCount > 0 ? moveBuffer[0] : 0)

unsigned long step[5][5];  // timestamps for state machine
int val[6];           // IR sensor readings

/*===========================initialize variables===========================*/

/*===========================declare function prototypes===========================*/
void Search();    // search graph
void SetState();  // switch the state
void shiftBuffer();
/*===========================declare function prototypes===========================*/

/*===========================define function===========================*/
void loop() {
    unsigned long currentMillis = millis();

    if (!state || !BTConnected || !gameStarted)
        MotorWriting(0, 0);
    else
        Search();
    SetState();

    // Periodic status report for robust synchronization
    if (currentMillis - lastBtTime >= btCyclePeriod) {
        lastBtTime = currentMillis;
        
        if (BTConnected) {
            Serial3.print("S:");
            Serial3.print(executedMovesCount);
            Serial3.print(",");
            Serial3.print(moveBuffer[0]);
            Serial3.print(",");
            Serial3.print(moveBuffer[1]);
            Serial3.print(",");
            Serial3.println(moveBuffer[2]);
        }

        // Resend RFID if it hasn't been acknowledged
        if (pendingRFID != "" && currentMillis - lastRfidTime >= 1000) {
            lastRfidTime = currentMillis;
            Serial3.print("RFID:");
            Serial3.println(pendingRFID);
#ifdef DEBUG
            Serial.print("Resending RFID: ");
            Serial.println(pendingRFID);
#endif
        }

#ifdef DEBUG
        Serial.print("S:"); Serial.print(executedMovesCount);
        Serial.print(" | Moves: ");
        Serial.print(moveBuffer[0]); Serial.print(",");
        Serial.print(moveBuffer[1]); Serial.print(",");
        Serial.println(moveBuffer[2]);
#endif
    }
}

void SetState() {
    process_BT();
}

void shiftBuffer() {
    if (bufferCount > 0) {
        int finished = moveBuffer[0];
        // Shift moves left
        moveBuffer[0] = moveBuffer[1];
        moveBuffer[1] = moveBuffer[2];
        moveBuffer[2] = 0; // Clear the last slot
        
        bufferCount--;
        executedMovesCount++;
        
        // Notify Python that the move is finished (fast path)
        Serial3.print("moveDone:");
        Serial3.println(finished);
        movesReceived = false;
    }
}

void Search() {
    // 1. read IR sensors
    val[1] = digitalRead(IRpin_LL);
    val[2] = digitalRead(IRpin_L);
    val[3] = digitalRead(IRpin_M);
    val[4] = digitalRead(IRpin_R);
    val[5] = digitalRead(IRpin_RR);

    int count = 0;
    for (int i = 1; i < 6; i++) if (val[i] == HIGH) count++;

#ifdef DEBUG
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
#endif

    // 2. scan RFID
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String rfidStr = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) rfidStr += "0";
        rfidStr += String(mfrc522.uid.uidByte[i], HEX);
        }
    
        // Only set as pending if it's a DIFFERENT card OR if the previous one was acknowledged
        if (rfidStr != pendingRFID) {
            pendingRFID = rfidStr;
            lastRfidTime = millis();
#ifdef DEBUG
            Serial.print("New RFID scanned: ");
            Serial.println(pendingRFID);
#endif
            // Initial send
            Serial3.print("RFID:");
            Serial3.println(pendingRFID);
        }

        mfrc522.PICC_HaltA(); 
        mfrc522.PCD_StopCrypto1();
    }

    // 3. node detection state machine
    if (currentMove == 5 && count >= 4 && Act == 1) state = false;
    else if (currentMove == 2 && count >= 3 && Act == 1)  { Act = 21; step[2][1] = millis(); }
    else if (Act == 21 && count <= 3 && millis() - step[2][1] > 400) { Act = 22; step[2][2] = millis(); }

    else if (currentMove == 4 && count >= 3 && Act == 1)  { Act = 41; step[4][1] = millis(); }
    else if (Act == 41 && count <= 1 && millis() - step[4][1] > 300) { Act = 42; step[4][2] = millis(); }

    else if (currentMove == 1 && count >= 3 && Act == 1)  { Act = 11; step[1][1] = millis(); }
    else if (Act == 11 && count <= 1 && millis() - step[1][1] > 350) { Act = 12; step[1][2] = millis(); }

    else if (currentMove == 3 && count >= 3 && Act == 1)  { Act = 31; step[3][1] = millis(); }
    else if (Act == 31 && count <= 3 && millis() - step[3][1] > 500) { Act = 32; step[3][2] = millis();}

    else if( millis() - step[2][2] > 200 && Act == 22) { //count <= 2 && (val[2] > 0 || val[3] > 0 || val[4] > 0 ) ) { 
      Act = 23; step[2][3] = millis();
    }
    else if( millis() - step[4][2] > 200 && Act == 42) { 
      Act = 43; step[4][3] = millis();
    }
    // finished turning → shift buffer, request next moves
    else if (Act == 23 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer(); 
    }
    else if (Act == 43 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer(); 
    }
    else if (Act == 32 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer(); 
    }
    else if (Act == 12 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer(); 
    }

    if( Act == 22 ) MotorWriting( -230, 150 );
    else if( Act == 42 ) MotorWriting( 230, -150 );

    else if( Act == 23 ) MotorWriting( -150, 100 );
    else if( Act == 43 ) MotorWriting( 150, -100 );
   
    else if( Act == 31 ) MotorWriting(-200, 200);      //U turn 
    else if( Act == 32) MotorWriting(-100 , 100);
    else tracking(val[1], val[2], val[3], val[4], val[5]);
// #ifdef DEBUG
//     Serial.println(currentMove);
// #endif
}


/*===========================define function===========================*/
