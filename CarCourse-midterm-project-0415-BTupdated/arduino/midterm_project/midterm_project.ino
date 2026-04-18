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
#define MotorR_I1 8     // 定義 A1 接腳（右）
#define MotorR_I2 9     // 定義 A2 接腳（右）
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
String pendingRFID = "";
unsigned long lastRfidTime = 0;
unsigned long lastBtTime = 0;
const unsigned long btCyclePeriod = 500;

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
    // RFID initial
    SPI.begin();
    mfrc522.PCD_Init();
    // TB6612 pin
    pinMode(MotorR_I1, OUTPUT);
    pinMode(MotorR_I2, OUTPUT);
    pinMode(MotorL_I3, OUTPUT);
    pinMode(MotorL_I4, OUTPUT);
    pinMode(MotorL_PWML, OUTPUT);
    pinMode(MotorR_PWMR, OUTPUT);
    // tracking pin
    pinMode(IRpin_LL, INPUT);
    pinMode(IRpin_L, INPUT);
    pinMode(IRpin_M, INPUT);
    pinMode(IRpin_R, INPUT);
    pinMode(IRpin_RR, INPUT);

    // --- Robust HM-10 Initialization (from April4th_remote.ino) ---
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

    Serial.println("Start!");
    Serial.println("Requesting first move...");
    Serial3.println("reqFirstMove");  // first move request for BT
}
/*============setup============*/


/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;  // 紅外線模組的讀值(0->white,1->black)
int _Tp = 150;                                // set your own value for motor power
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

    if (!state || !BTConnected || bufferCount == 0)
        MotorWriting(0, 0);
    else
        Search();
    SetState();

    // keep-alive: re-request moves if buffer is empty or low
    if (currentMillis - lastBtTime >= btCyclePeriod) {
        lastBtTime = currentMillis;
        
        // Only request moves if NOT waiting for an RFID confirmation
        if (!BTConnected || bufferCount < 3 || currentMove == 0) {
            if (!movesStarted) {
                Serial.println("Requesting first moves...");
                Serial3.println("reqFirstMove");
            } else {
                Serial.println("Requesting next moves...");
                Serial3.println("reqNxtMove");
            }
        }
#ifdef DEBUG
        for (int i = 0; i < bufferCount; i++) {
            Serial.print(moveBuffer[i]);
            if (i < bufferCount - 1) Serial.print(",");
        }
        Serial.println();
        Serial.println(Act);
#endif
    }
}

void SetState() {
    process_BT();
}

void shiftBuffer() {
    if (bufferCount > 0) {
        for (int i = 0; i < bufferCount - 1; i++) {
            moveBuffer[i] = moveBuffer[i + 1];
        }
        bufferCount--;
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

    // 2. scan RFID
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String rfidStr = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) rfidStr += "0";
        rfidStr += String(mfrc522.uid.uidByte[i], HEX);
        }
    
        // Only set as pending if it's a DIFFERENT card OR if 2 seconds have passed
        if (rfidStr != pendingRFID) {
        pendingRFID = rfidStr;
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
    if      (currentMove == 2 && count >= 4 && Act == 1)  { Act = 21; step[2][1] = millis(); }
    else if (Act == 21 && count <= 3 && millis() - step[2][1] > 700) { Act = 22; step[2][2] = millis(); }

    else if (currentMove == 4 && count >= 4 && Act == 1)  { Act = 41; step[4][1] = millis(); }
    else if (Act == 41 && count <= 1 && millis() - step[4][1] > 700) { Act = 42; step[4][2] = millis(); }

    else if (currentMove == 1 && count >= 4 && Act == 1)  { Act = 11; step[1][1] = millis(); }
    else if (Act == 11 && count <= 1 && millis() - step[1][1] > 700) { Act = 12; step[1][2] = millis(); }

    else if (currentMove == 3 && count >= 4 && Act == 1)  { Act = 31; step[3][1] = millis(); }
    else if (Act == 31 && count <= 3 && millis() - step[3][1] > 400) { Act = 32; step[3][2] = millis();}

    // finished turning → shift buffer, request next moves
    else if (Act == 22 && millis() - step[2][2] > 400 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer();
    }
    else if (Act == 42 && millis() - step[4][2] > 400 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer();
    }
    else if (Act == 32 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer();
    }
    else if (Act == 12 && count > 0 && count <= 3) {
        Act = 1; shiftBuffer();
    }

    // 4. execute movement
    if      (Act == 22) left_turn(180, 120);
    else if (Act == 42) right_turn(180, 150);
    else if (Act == 31) left_turn(150, 150);
    else if (Act == 32) left_turn(100, 100);
    else if (currentMove == 5) {
        state = false;
        MotorWriting(0, 0);
    }
    else tracking(val[1], val[2], val[3], val[4], val[5]);
// #ifdef DEBUG
//     Serial.println(currentMove);
// #endif
}


/*===========================define function===========================*/
