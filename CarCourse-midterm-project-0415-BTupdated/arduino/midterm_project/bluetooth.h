/***************************************************************************/
// File			  [bluetooth.h]
// Author		  [Erik Kuo]
// Synopsis		[Code for bluetooth communication]
// Functions  [ask_BT, send_msg, send_byte]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 2*/

/*=====Import variable=====*/
extern bool BTConnected;
extern bool movesStarted;
extern int moveBuffer[3];
extern int bufferCount;
extern String pendingRFID;
/*=====Import variable=====*/

enum BT_CMD {
    NOTHING,
    FORWARD,    // 1
    TURN_LEFT,  // 2
    U_TURN,     // 3
    TURN_RIGHT, // 4
    HALT,       // 5
    // TODO: add your own command type here

};

// NEW: Process Bluetooth commands and update buffer/state
void process_BT() {
    if (Serial3.available()) {
        static String inputBuffer = "";
        while (Serial3.available()) {
            char c = Serial3.read();
            if (c == '\n' || c == '\r') {
                inputBuffer.trim();
                if (inputBuffer.length() > 0) {
                    // Ignore ESP32 internal status messages
                    if (inputBuffer.indexOf("OK+") != -1 || inputBuffer.indexOf("ERROR+") != -1) {
                        inputBuffer = "";
                        continue; 
                    }

                    BTConnected = true;
#ifdef DEBUG
                    Serial.print("Remote Command: ");
                    Serial.println(inputBuffer);
#endif

                    if (inputBuffer.startsWith("nxtMove:")) {
                        String movePart = inputBuffer.substring(8);
                        int count = 0;
                        int start = 0;
                        int commaIndex = movePart.indexOf(',');
                        
                        while (commaIndex != -1 && count < 3) {
                            moveBuffer[count++] = movePart.substring(start, commaIndex).toInt();
                            start = commaIndex + 1;
                            commaIndex = movePart.indexOf(',', start);
                        }
                        if (count < 3) {
                            moveBuffer[count++] = movePart.substring(start).toInt();
                        }
                        bufferCount = count;
                        movesStarted = true;
                        
#ifdef DEBUG
                        Serial.print("Buffer updated: ");
                        for(int i=0; i<bufferCount; i++) {
                            Serial.print(moveBuffer[i]);
                            if(i < bufferCount-1) Serial.print(",");
                        }
                        Serial.println();
#endif

                        // acknowledge with the full current buffer
                        Serial3.print("nxtMoveRecived:");
                        for(int i=0; i<bufferCount; i++) {
                            Serial3.print(moveBuffer[i]);
                            if(i < bufferCount-1) Serial3.print(",");
                        }
                        Serial3.println();
                        movesReceived = true;
                    }

                    if (inputBuffer.startsWith("rfidAck:")) {
                        String ackUID = inputBuffer.substring(8);
                        ackUID.trim();
                        if (ackUID == pendingRFID) {
#ifdef DEBUG
                            Serial.print("RFID confirmed: ");
                            Serial.println(pendingRFID);
#endif
                            pendingRFID = "";
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

// Bluetooth Helper Functions for initialization
bool waitForResponse(const char* expected, unsigned long timeout) {
    unsigned long start = millis();
    Serial3.setTimeout(timeout);
    String response = Serial3.readString();
#ifdef DEBUG
    if (response.length() > 0) {
        Serial.print("HM10 Response: ");
        Serial.println(response);
    }
#endif
    return (response.indexOf(expected) != -1);
}

void sendATCommand(const char* command) {
    Serial3.print(command);
    waitForResponse("", 1000); 
}

BT_CMD ask_BT() {
    BT_CMD message = NOTHING;
    // (Legacy function kept for compatibility if needed elsewhere)
    return message;
}  // ask_BT

// NEW: request next move from Python
void request_next_move() {
    Serial3.println("reqNxtMove");
#ifdef DEBUG
    Serial.println("Sent: reqNxtMove");
#endif
}  // request_next_move

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