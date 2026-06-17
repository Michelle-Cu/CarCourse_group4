#include <esp_now.h>

#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_mac.h"
#include "DFRobotDFPlayerMini.h" 

// Pin Definitions
const int servoPins[5] = {32, 33, 25, 26, 27};
const int stepPin = 14;
const int dirPin = 13; 
const int bootButton = 0; 
const int ledPin = 2;     

// === AUDIO PIN DEFINITIONS ===
#define ESP32_RX_PIN 16  // Connects to DFPlayer TX
#define ESP32_TX_PIN 17  // Connects to DFPlayer RX
HardwareSerial dfPlayerSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

// Hardware Timer for Stepper Polling
hw_timer_t * stepperTimer = NULL;

// Stepper control variables
volatile long stepperRemainingSteps = 0;
const int punchDirection = LOW; 

// Helper to set/change timer alarm interval and start the timer
void setTimerInterval(uint32_t intervalUs) {
  if (stepperTimer == NULL) return;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  timerAlarm(stepperTimer, intervalUs, true, 0);
  timerStart(stepperTimer);
#else
  timerAlarmWrite(stepperTimer, intervalUs, true);
  timerAlarmEnable(stepperTimer);
#endif
}

// Helper to stop the timer
void stopTimer() {
  if (stepperTimer == NULL) return;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  timerStop(stepperTimer);
#else
  timerAlarmDisable(stepperTimer);
#endif
}

// IRAM-safe hardware timer ISR for generating step pulses
void IRAM_ATTR onStepperTimer() {
  if (stepperRemainingSteps > 0) {
    digitalWrite(stepPin, HIGH);
    ets_delay_us(10); 
    digitalWrite(stepPin, LOW);
    stepperRemainingSteps--;
    if (stepperRemainingSteps == 0) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
      timerStop(stepperTimer);
#else
      timerAlarmDisable(stepperTimer);
#endif
    }
  }
}

// Punch Stepper Settings
const int stepsPerRotation = 1600; 
const float punchAngle = 90.0;     
const float rechargeAngle = 270.0; 
const float punchSpeed = 400.0;    
const float rechargeSpeed = 400.0; 
const int punchPauseMs = 1000;     

// Servos
Servo servos[5];

// Data structure to receive
typedef struct struct_message {
  uint8_t finger_angles[5];
  uint8_t trigger_punch; 
} __attribute__((packed)) struct_message;

struct_message myData;
volatile bool dataReady = false; 
volatile unsigned long lastRecvTime = 0;

// Punch settings
bool isPunching = false;
unsigned long lastPunchEndTime = 0;
const unsigned long punchCooldownMs = 1000; 

unsigned long lastSoundTime = 0;  //Sound settings
bool straight[5];

// === 1-BASED ENUM MODIFICATION ===
enum PunchState {
  PUNCH_IDLE = 1,       // 1
  PUNCH_EXTENDING,      // 2
  PUNCH_PAUSED,         // 3
  PUNCH_RECHARGING      // 4
};
PunchState punchState = PUNCH_IDLE; // Starts tracking at 1
unsigned long punchPauseStartTime = 0;

void gestureSound(){
  if( millis() - lastSoundTime < 1000) return; 
  int count = 0;
  for (int i = 0; i < 5; i++) {
      int rawAngle = myData.finger_angles[i];
      if( rawAngle < 90 ) {straight[i] = 1; count++; }
      else{ straight[i] = 0; }
  }
  if( straight[0] == 1 && straight[4] == 1 && count == 2) { myDFPlayer.play(6); lastSoundTime = millis();} 
  else if( straight[0] == 1 && straight[1] == 1 && count == 2) { myDFPlayer.play(7); lastSoundTime = millis();}
  else if( straight[1] == 1 && straight[2] == 1 && straight[3] == 1 && count == 3) { myDFPlayer.play(3); lastSoundTime = millis();}
  else if( count == 5) { myDFPlayer.play(5); lastSoundTime = millis();}
}

void triggerPunch() {
  if (punchState == PUNCH_IDLE) {
    punchState = PUNCH_EXTENDING; // Changes state to 2
    isPunching = true;
    Serial.println("PUNCH TRIGGERED!");
    digitalWrite(ledPin, HIGH); 
    
    // 1. Punch Stroke (90 degrees)
    long stepsPunch = (long)((punchAngle / 360.0) * stepsPerRotation);
    digitalWrite(dirPin, punchDirection);
    
    uint32_t stepIntervalUs = (uint32_t)(1000000.0 / punchSpeed);
    
    stepperRemainingSteps = stepsPunch;
    setTimerInterval(stepIntervalUs);
  }
}

void updatePunch() {
  switch (punchState) {
    case PUNCH_IDLE: // Case 1
      break;
      
    case PUNCH_EXTENDING: // Case 2
      if (stepperRemainingSteps == 0) {
        punchState = PUNCH_PAUSED; // Changes state to 3
        punchPauseStartTime = millis();
      }
      break;
      
    case PUNCH_PAUSED: // Case 3
      if (millis() - punchPauseStartTime >= punchPauseMs) {
        punchState = PUNCH_RECHARGING; // Changes state to 4
        long stepsRecharge = (long)((rechargeAngle / 360.0) * stepsPerRotation);
        
        digitalWrite(dirPin, punchDirection); 
        
        uint32_t stepIntervalUs = (uint32_t)(1000000.0 / rechargeSpeed);
        
        stepperRemainingSteps = stepsRecharge;
        setTimerInterval(stepIntervalUs);
      }
      break;
      
    case PUNCH_RECHARGING: // Case 4
      if (stepperRemainingSteps == 0) {
        digitalWrite(ledPin, LOW); 
        punchState = PUNCH_IDLE; // Returns back to 1
        isPunching = false;
        lastPunchEndTime = millis(); 
      }
      break;
  }
}

// ESP-NOW CALLBACK
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
  }
}
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 HAND START WITH 1-BASED ENUM ---");

  // Initialize Audio Serial
  dfPlayerSerial.begin(9600, SERIAL_8N1, ESP32_RX_PIN, ESP32_TX_PIN);
  if (!myDFPlayer.begin(dfPlayerSerial)) {
    Serial.println("Warning: DFPlayer Mini not detected! Check wiring/SD card.");
  } else {
    Serial.println("DFPlayer Mini online and ready.");
    myDFPlayer.volume(25); 
  }

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < 5; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(servoPins[i], 500, 2500); 
    servos[i].write(0);
  }
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); 
  pinMode(bootButton, INPUT_PULLUP); 

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  stepperTimer = timerBegin(1000000); 
  timerAttachInterrupt(stepperTimer, &onStepperTimer);
#else
  stepperTimer = timerBegin(0, 80, true); 
  timerAttachInterrupt(stepperTimer, &onStepperTimer, true);
#endif 

  Serial.println("Ready.");
}

void loop() {
  updatePunch();

  if (digitalRead(bootButton) == LOW && !isPunching && (millis() - lastPunchEndTime >= punchCooldownMs)) {
    triggerPunch();
  }

  if (dataReady) {
    dataReady = false; 

    // Move Servos 
    static int lastServoAngles[5] = {-1, -1, -1, -1, -1};
    for (int i = 0; i < 5; i++) {
      int rawAngle = myData.finger_angles[i];

      if (rawAngle != lastServoAngles[i]) {
        servos[i].write(rawAngle);
        lastServoAngles[i] = rawAngle;
      }
    }

    

    // Punch Check from Glove Transmissions
    if (myData.trigger_punch == 1 && !isPunching && (millis() - lastPunchEndTime >= punchCooldownMs)) {
      triggerPunch();
      myDFPlayer.play( PUNCH_IDLE ); // Triggers "0001.mp3" file
      lastSoundTime = millis();
    }
    else if(punchState == PUNCH_RECHARGING && millis() - lastSoundTime > 1000){
        myDFPlayer.play( PUNCH_RECHARGING );  //0004.mp3 file
        lastSoundTime = millis();
    }
    else { gestureSound(); }
  }
  yield(); 
}