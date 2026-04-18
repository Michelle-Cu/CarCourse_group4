/***************************************************************************/
// File			  [bluetooth.h]
// Author		  [Erik Kuo]
// Synopsis		[Code for bluetooth communication]
// Functions  [ask_BT, send_msg, send_byte]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 2*/

enum BT_CMD {
    NOTHING,
    FORWARD,    // 1
    TURN_LEFT,  // 2
    U_TURN,     // 3
    TURN_RIGHT, // 4
    HALT,       // 5
    // TODO: add your own command type here

};

BT_CMD ask_BT() {
    BT_CMD message = NOTHING;
    char cmd;
    if (Serial3.available()) {
// TODO:
// 1. get cmd from Serial3(bluetooth serial)
// 2. link bluetooth message to your own command type
        // read full "nxtMove:N" string
        static String inputBuffer = "";
        while (Serial3.available()) {
            char c = Serial3.read();
            if (c == '\n' || c == '\r') {
                inputBuffer.trim();
                if (inputBuffer.startsWith("nxtMove:")) {
                    int move = inputBuffer.substring(8).toInt();
                    if      (move == 1) message = FORWARD;
                    else if (move == 2) message = TURN_LEFT;
                    else if (move == 3) message = U_TURN;
                    else if (move == 4) message = TURN_RIGHT;
                    else if (move == 5) message = HALT;
                    cmd = '0' + move; // for debug print below
                }
                inputBuffer = "";
            } else if (isPrintable(c)) {
                inputBuffer += c;
            }
        }
#ifdef DEBUG
        Serial.print("cmd : ");
        Serial.println(cmd);
#endif
    }
    return message;
}  // ask_BT

// send msg back through Serial1(bluetooth serial)
// can use send_byte alternatively to send msg back
// (but need to convert to byte type)
void send_msg(const char& msg) {
    // TODO:
    Serial3.println(msg);
}  // send_msg

// send UID back through Serial3(bluetooth serial)
void send_byte(byte* id, byte& idSize) {
    for (byte i = 0; i < idSize; i++) {  // Send UID consequently.
        Serial3.print(id[i]);
    }
#ifdef DEBUG
    Serial.print("Sent id: ");
    for (byte i = 0; i < idSize; i++) {  // Show UID consequently.
        Serial.print(id[i], HEX);
    }
    Serial.println();
#endif
}  // send_byte

// NEW: send RFID as "RFID:XXXXXXXX" string (matches what Python/ESP32 bridge expects)
void send_rfid(byte* id, byte& idSize) {
    String rfidStr = "RFID:";
    for (byte i = 0; i < idSize; i++) {
        if (id[i] < 0x10) rfidStr += "0";
        rfidStr += String(id[i], HEX);
    }
    Serial3.println(rfidStr);
#ifdef DEBUG
    Serial.println(rfidStr);
#endif
}  // send_rfid

// NEW: request next move from Python
void request_next_move() {
    Serial3.println("reqNxtMove");
#ifdef DEBUG
    Serial.println("Sent: reqNxtMove");
#endif
}  // request_next_move