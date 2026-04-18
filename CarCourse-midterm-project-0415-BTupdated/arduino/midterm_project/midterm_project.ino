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
#define CUSTOM_NAME "HM10_Mega" // Max length is 12 characters [1]

// BT related global variables (updated)
bool BTConnected = false;
bool movesStarted = false;
String pendingRFID = "";
unsigned long lastRfidTime = 0;
unsigned long lastBtTime = 0;
const unsigned long btCyclePeriod = 200;

/*=====Import header files=====*/
#include "RFID.h"
#include "bluetooth.h"
#include "node.h"
#include "track.h"
/*=====Import header files=====*/

/*===========================define pin & create module object================================*/
// BlueTooth
// BT connect to Serial1 (Hardware Serial)
// Mega               HC05
// Pin  (Function)    Pin
// 18    TX       ->  RX
// 19    RX       <-  TX
// TB6612, 請按照自己車上的接線寫入腳位(左右不一定要跟註解寫的一樣)


/*===========================define pin & create module object===========================*/

/*============setup============*/
void setup() {
    // bluetooth initialization
    Serial3.begin(9600);  
    // Serial window
    Serial.begin(9600);
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
#ifdef DEBUG
    Serial.println("Start!");
#endif

    // request_next_move();
    Serial3.println("reqFirstMove");  // first move request for BT
}
/*============setup============*/


/*===========================initialize variables===========================*/
int l2 = 0, l1 = 0, m0 = 0, r1 = 0, r2 = 0;  // 紅外線模組的讀值(0->white,1->black)
int _Tp = 150;                                // set your own value for motor power
bool state = true;     // set state to false to halt the car, set state to true to activate the car
BT_CMD _cmd = NOTHING;  // enum for bluetooth message, reference in bluetooth.h line 2


// cross test
int moveSequence[] = {1, 2, 3, 1, 3, 4, 3, 1, 5};
int moveIndex = 0;
int totalMoves = 9;
int currentMove = moveSequence[0];
//

int Act = 1;          // current action state

// int currentMove = 0;  // current move command (1=forward, 2=left, 3=U-turn, 4=right)
unsigned long step[5][5];  // timestamps for state machine
int val[6];           // IR sensor readings

/*===========================initialize variables===========================*/

/*===========================declare function prototypes===========================*/
void Search();    // search graph
void SetState();  // switch the state
/*===========================declare function prototypes===========================*/

/*===========================define function===========================*/
void loop() {
    unsigned long currentMillis = millis();

    if (!state)
        MotorWriting(0, 0);
    else
        Search();
    SetState();

    // keep-alive: re-request move if idle every 200ms
    if (currentMillis - lastBtTime >= btCyclePeriod) {
        lastBtTime = currentMillis;
        if (!BTConnected || currentMove == 0) {
            if (!movesStarted) Serial3.println("reqFirstMove");
            else Serial3.println("reqNxtMove");
        }
    }
}

void SetState() {
    // TODO:
    // 1. Get command from bluetooth
    // 2. Change state if need

    // Original code, probably won't need them anymore

    // _cmd = ask_BT();
    // if (_cmd == HALT) {
    //     state = !state;
    // }
    // else if (_cmd != NOTHING) {
    //     currentMove = (int)_cmd;  
    // }

    // end of original code

    if (Serial3.available()) {
        static String inputBuffer = "";
        while (Serial3.available()) {
            char c = Serial3.read();
            if (c == '\n' || c == '\r') {
                inputBuffer.trim();
                if (inputBuffer.length() > 0) {
                    BTConnected = true;

                    if (inputBuffer.startsWith("nxtMove:")) {
                        int next_val = inputBuffer.substring(8).toInt();
                        if (currentMove == 0) {
                            currentMove = next_val;
                            movesStarted = true;
                        }
                        // send acknowledgment
                        Serial3.print("nxtMoveRecived: ");
                        Serial3.println(currentMove);
                    }

                    if (inputBuffer.startsWith("rfidAck:")) {
                        String ackUID = inputBuffer.substring(8);
                        ackUID.trim();
                        if (ackUID == pendingRFID) {
                            pendingRFID = "";  // confirmed received
                        }
                    }
                }
                inputBuffer = "";
            } else if (isPrintable(c)) {
                inputBuffer += c;
            }
        }
    }
}

// Search() with hardcoded route, no BT signals required

void Search() {
    // 1. read IR sensors
    val[1] = digitalRead(IRpin_LL);
    val[2] = digitalRead(IRpin_L);
    val[3] = digitalRead(IRpin_M);
    val[4] = digitalRead(IRpin_R);
    val[5] = digitalRead(IRpin_RR);

    int count = 0;
    for (int i = 1; i < 6; i++) if (val[i] == HIGH) count++;

    // 2. scan RFID

    // original code for scanning RFID, can delete in the future
//     byte idSize;
//     byte* id = rfid(idSize);
//     if (id != 0) {
//         send_rfid(id, idSize);
// #ifdef DEBUG
//         Serial.println("RFID scanned!");
// #endif
//     }
    // end of original code

    // updated version of RFID scanning
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String rfidStr = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            if (mfrc522.uid.uidByte[i] < 0x10) rfidStr += "0";
            rfidStr += String(mfrc522.uid.uidByte[i], HEX);
        }
        if (rfidStr != pendingRFID || (millis() - lastRfidTime > 2000)) {
            pendingRFID = rfidStr;
            lastRfidTime = millis();
            Serial3.print("RFID:");
            Serial3.println(pendingRFID);
    #ifdef DEBUG
            Serial.print("RFID scanned: ");
            Serial.println(pendingRFID);
    #endif
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

    // finished → load next move from sequence
    else if (Act == 22 && millis() - step[2][2] > 400 && count > 0 && count <= 3) {
        Act = 1; loadNextMove();
    }
    else if (Act == 42 && millis() - step[4][2] > 400 && count > 0 && count <= 3) {
        Act = 1; loadNextMove();
    }
    else if (Act == 32 && count > 0 && count <= 3) {
        Act = 1; loadNextMove();
    }
    else if (Act == 12 && count > 0 && count <= 3) {
        Act = 1; loadNextMove();
    }

    // 4. execute movement
    if      (Act == 22) left_turn(180, 120);
    else if (Act == 42) right_turn(180, 150);
    else if (Act == 31) left_turn(150, 150);
    else if (Act == 32) left_turn(100, 100);
    else tracking(val[1], val[2], val[3], val[4], val[5]);
}

void loadNextMove() {
    moveIndex++;
    if (moveIndex < totalMoves) {
        currentMove = moveSequence[moveIndex];
#ifdef DEBUG
        Serial.print("Next move: ");
        Serial.println(currentMove);
#endif
        if (currentMove == 5) {
            state = false;  // HALT
            MotorWriting(0, 0);
#ifdef DEBUG
            Serial.println("Done! Car halted.");
#endif
        }
    } else {
        state = false;
        MotorWriting(0, 0);
    }
}






// Search() with bluetooth signals
// void Search() {
//     // TODO: let your car search graph(maze) according to bluetooth command from computer(python
//     // code)

//     // whole searching process

//     // 1. read IR sensors
//     val[1] = digitalRead(IRpin_LL);
//     val[2] = digitalRead(IRpin_L);
//     val[3] = digitalRead(IRpin_M);
//     val[4] = digitalRead(IRpin_R);
//     val[5] = digitalRead(IRpin_RR);

//     int count = 0;
//     for (int i = 1; i < 6; i++) if (val[i] == HIGH) count++;

//     // 2. scan RFID
//     byte idSize;
//     byte* id = rfid(idSize);
//     if (id != 0) {
//         send_rfid(id, idSize);
//     }

//     // 3. get next move command from BT
//     // removed because it's already called in SetState()
    
//     // 4. node detection state machine
//     if      (currentMove == 2 && count >= 4 && Act == 1)  { Act = 21; step[2][1] = millis(); }  // left: reach node
//     else if (Act == 21 && count <= 3 && millis() - step[2][1] > 500) { Act = 22; step[2][2] = millis(); }  // left: past node, turn

//     else if (currentMove == 4 && count >= 4 && Act == 1)  { Act = 41; step[4][1] = millis(); }  // right: reach node
//     else if (Act == 41 && count <= 1 && millis() - step[4][1] > 500) { Act = 42; step[4][2] = millis(); }  // right: past node, turn

//     else if (currentMove == 1 && count >= 4 && Act == 1)  { Act = 11; step[1][1] = millis(); }  // forward: reach node
//     else if (Act == 11 && count <= 1 && millis() - step[1][1] > 500) { Act = 12; step[1][2] = millis(); }  // forward: past node

//     else if (currentMove == 3 && count >= 4 && Act == 1)  { Act = 31; step[3][1] = millis(); }  // U-turn: reach node
//     else if (Act == 31 && millis() - step[3][1] > 700)    { Act = 32; step[3][2] = millis(); }  // U-turn: start turning

//     // finished turning → back to tracking, request next move
//     else if (Act == 22 && millis() - step[2][2] > 400 && count > 0 && count <= 3) { Act = 1; currentMove = 0; request_next_move(); }
//     else if (Act == 42 && millis() - step[4][2] > 400 && count > 0 && count <= 3) { Act = 1; currentMove = 0; request_next_move(); }
//     else if (Act == 32 && count > 0 && count <= 3) { Act = 1; currentMove = 0; request_next_move(); }
//     else if (Act == 12 && count > 0 && count <= 3) { Act = 1; currentMove = 0; request_next_move(); }

//     // 5. execute movement
//     if      (Act == 22) left_turn(180, 120);
//     else if (Act == 42) right_turn(180, 150);
//     else if (Act == 31) left_turn(150, 150);   // U-turn first half
//     else if (Act == 32) left_turn(100, 100);   // U-turn second half
//     else tracking(val[1], val[2], val[3], val[4], val[5]);  // default: follow line

// }
/*===========================define function===========================*/
