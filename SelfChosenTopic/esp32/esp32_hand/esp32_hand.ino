#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_mac.h"

// Pin Definitions
const int servoPins[5] = {32, 33, 25, 26, 27};
const int stepPin = 14;
const int dirPin = 13; // Changed from 12 to 13 to avoid MTDI boot strapping conflicts
const int bootButton = 0; // BOOT button pin on standard ESP32 boards

// Hardware Timer for Stepper Polling
hw_timer_t * stepperTimer = NULL;

// Stepper control variables (volatile as they are modified in ISR)
volatile long stepperRemainingSteps = 0;
const int punchDirection = LOW; // Set to HIGH or LOW to reverse physical rotation direction

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
    ets_delay_us(10); // Standard TB6600 driver needs at least 2.2us high pulse width
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
const int stepsPerRotation = 1600; // TB6600 set to 8 microsteps
const float punchAngle = 90.0;     // Angle to move for the punch stroke
const float rechargeAngle = 270.0; // Angle to move to recharge/retract
const float punchSpeed = 400.0;    // Speed in steps/sec for the punch stroke
const float rechargeSpeed = 400.0; // Speed in steps/sec for the recharge stroke
const int punchPauseMs = 1000;     // Pause time in milliseconds at punch extension

// Servos
Servo servos[5];



// Data structure to receive (PACKED to prevent alignment issues)
typedef struct struct_message {
  uint8_t finger_angles[5];
  uint8_t trigger_punch; // 1 = trigger punch, 0 = idle
} __attribute__((packed)) struct_message;

struct_message myData;
volatile bool dataReady = false; // Flag to tell loop we have new data
const int ledPin = 2;            // Built-in LED on Ruilong ESP32-S
volatile unsigned long lastRecvTime = 0;

// Punch settings
bool isPunching = false;
unsigned long lastPunchEndTime = 0;
const unsigned long punchCooldownMs = 1000; // 1 second cooldown after punch completes

enum PunchState {
  PUNCH_IDLE,
  PUNCH_EXTENDING,
  PUNCH_PAUSED,
  PUNCH_RECHARGING
};
PunchState punchState = PUNCH_IDLE;
unsigned long punchPauseStartTime = 0;

void triggerPunch() {
  if (punchState == PUNCH_IDLE) {
    punchState = PUNCH_EXTENDING;
    isPunching = true;
    Serial.println("PUNCH TRIGGERED!");
    digitalWrite(ledPin, HIGH); // Turn LED on when punch is triggered
    
    // 1. Punch Stroke (90 degrees)
    long stepsPunch = (long)((punchAngle / 360.0) * stepsPerRotation);
    digitalWrite(dirPin, punchDirection);
    
    // Calculate step interval: 1,000,000 us / steps_per_sec
    uint32_t stepIntervalUs = (uint32_t)(1000000.0 / punchSpeed);
    
    stepperRemainingSteps = stepsPunch;
    setTimerInterval(stepIntervalUs);
  }
}

void updatePunch() {
  switch (punchState) {
    case PUNCH_IDLE:
      break;
      
    case PUNCH_EXTENDING:
      if (stepperRemainingSteps == 0) {
        punchState = PUNCH_PAUSED;
        punchPauseStartTime = millis();
      }
      break;
      
    case PUNCH_PAUSED:
      if (millis() - punchPauseStartTime >= punchPauseMs) {
        punchState = PUNCH_RECHARGING;
        long stepsRecharge = (long)((rechargeAngle / 360.0) * stepsPerRotation);
        
        digitalWrite(dirPin, punchDirection); // Same direction for 360 degree total rotation
        
        uint32_t stepIntervalUs = (uint32_t)(1000000.0 / rechargeSpeed);
        
        stepperRemainingSteps = stepsRecharge;
        setTimerInterval(stepIntervalUs);
      }
      break;
      
    case PUNCH_RECHARGING:
      if (stepperRemainingSteps == 0) {
        digitalWrite(ledPin, LOW); // Turn LED off when punch finishes
        punchState = PUNCH_IDLE;
        isPunching = false;
        lastPunchEndTime = millis(); // Record end time for cooldown
      }
      break;
  }
}

// VERY FAST CALLBACK
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
  } else {
    Serial.print("ESP-NOW Size Mismatch! Recv: "); Serial.print(len);
    Serial.print(" | Expected: "); Serial.println(sizeof(struct_message));
  }
}
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&myData, incomingData, sizeof(myData));
    dataReady = true;
    lastRecvTime = millis();
  } else {
    Serial.print("ESP-NOW Size Mismatch! Recv: "); Serial.print(len);
    Serial.print(" | Expected: "); Serial.println(sizeof(struct_message));
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP32 HAND START ---");

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
    servos[i].attach(servoPins[i], 500, 2500); // Widest standard pulse limits for maximum rotation (0 to 180 degrees)
    servos[i].write(0);
  }
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Start with LED off
  pinMode(bootButton, INPUT_PULLUP); // Boot button input

  // Initialize hardware timer (1 tick = 1 us) but do not configure/start the alarm yet
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  stepperTimer = timerBegin(1000000); // 1 MHz frequency
  timerAttachInterrupt(stepperTimer, &onStepperTimer);
#else
  stepperTimer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 tick = 1 us)
  timerAttachInterrupt(stepperTimer, &onStepperTimer, true);
#endif 

  Serial.println("Ready.");
}

void loop() {
  // Always update punch state machine
  updatePunch();

  // Check physical BOOT button to trigger punch locally for testing
  if (digitalRead(bootButton) == LOW && !isPunching && (millis() - lastPunchEndTime >= punchCooldownMs)) {
    triggerPunch();
  }

  // Connection status watchdog LED control disabled to allow LED to act as punch indicator
  /*
  if (lastRecvTime > 0 && millis() - lastRecvTime < 1000) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  */

  if (dataReady) {
    dataReady = false; // Reset flag

    // 1. Debug Output (Now safe to print in loop)
    Serial.print("Angles: ");
    for(int i=0; i<5; i++) {
      Serial.print(myData.finger_angles[i]); Serial.print(" ");
    }
    Serial.print("| Punch Trigger: "); Serial.println(myData.trigger_punch);

    // 2. Move Servos (only if the raw angle changes to save CPU time)
    static int lastServoAngles[5] = {-1, -1, -1, -1, -1};
    for (int i = 0; i < 5; i++) {
      int rawAngle = myData.finger_angles[i];
      if (rawAngle != lastServoAngles[i]) {
        Serial.print("Writing Servo "); Serial.print(i);
        Serial.print(" from "); Serial.print(lastServoAngles[i]);
        Serial.print(" to "); Serial.println(rawAngle);
        servos[i].write(rawAngle);
        lastServoAngles[i] = rawAngle;
      }
    }

    // 3. Punch Check
    // Evaluates the punch trigger flag sent by the glove (which includes cooldown and status protection)
    if (myData.trigger_punch == 1 && !isPunching && (millis() - lastPunchEndTime >= punchCooldownMs)) {
      triggerPunch();
    }
  }
  
  // Give the system time to breathe without blocking the stepper timing
  yield(); 
}
