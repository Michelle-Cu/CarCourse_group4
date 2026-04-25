/***************************************************************************/
// File			  [bluetooth.h]
// Author		  [Erik Kuo]
// Synopsis		[Code for bluetooth communication]
// Functions  [ask_BT, send_msg, send_byte]
// Modify		  [2020/03/27 Erik Kuo]
/***************************************************************************/

/*if you have no idea how to start*/
/*check out what you have learned from week 2*/

// #define DEBUG

/*=====Import variable=====*/
extern bool BTConnected;
extern bool movesStarted;
extern bool movesReceived;
extern bool gameStarted;
extern int moveBuffer[3];
extern int bufferCount;
extern String pendingRFID;
extern int executedMovesCount;
extern int nextAbsIdx;
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

                    if (inputBuffer == "gameStart") {
                        gameStarted = true;
                        Serial.println("Game started from Python!");
                    }
                    else if (inputBuffer == "rfidResend") {
                        if (pendingRFID != "") {
                            Serial3.println("RFID:" + pendingRFID);
#ifdef DEBUG
                            Serial.println("Resending RFID on Python request: " + pendingRFID);
#endif
                        }
                    }
                    else if (inputBuffer.startsWith("nxtMove:")) {
                        String movePart = inputBuffer.substring(8);
                        int firstComma = movePart.indexOf(',');
                        if (firstComma != -1) {
                            int startIdx = movePart.substring(0, firstComma).toInt();
                            String movesOnly = movePart.substring(firstComma + 1);
                            
                            int tempMoves[3];
                            int tempCount = 0;
                            int start = 0;
                            int commaIndex = movesOnly.indexOf(',');
                            
                            while (commaIndex != -1 && tempCount < 3) {
                                tempMoves[tempCount++] = movesOnly.substring(start, commaIndex).toInt();
                                start = commaIndex + 1;
                                commaIndex = movesOnly.indexOf(',', start);
                            }
                            if (tempCount < 3 && start < movesOnly.length()) {
                                tempMoves[tempCount++] = movesOnly.substring(start).toInt();
                            }

                            // Use absolute indexing to avoid duplicates or gaps
                            for (int i = 0; i < tempCount; i++) {
                                int absIdxOfMove = startIdx + i;
                                // Only care about moves we haven't executed yet
                                if (absIdxOfMove > executedMovesCount) {
                                    int relIdx = absIdxOfMove - executedMovesCount - 1;
                                    if (relIdx >= 0 && relIdx < 3) {
                                        // If this is a new move for this slot, or we are filling a gap
                                        if (moveBuffer[relIdx] == 0 || absIdxOfMove >= nextAbsIdx) {
                                            moveBuffer[relIdx] = tempMoves[i];
                                            // Update bufferCount to the highest filled index
                                            if (relIdx + 1 > bufferCount) bufferCount = relIdx + 1;
                                            // Update nextAbsIdx to the next index after the highest move we've received
                                            if (absIdxOfMove + 1 > nextAbsIdx) nextAbsIdx = absIdxOfMove + 1;
                                        }
                                    }
                                }
                            }
                            movesStarted = true;
                            movesReceived = true;
#ifdef DEBUG
                            Serial.print("Buffer synced. nextAbsIdx: "); Serial.print(nextAbsIdx);
                            Serial.print(", bufferCount: "); Serial.println(bufferCount);
#endif
                        }
                    }
                    else if (inputBuffer.startsWith("rfidAck:")) {
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
