#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <AccelStepper.h>
#include "esp_mac.h"

// Pin Definitions
const int servoPins[5] = {32, 33, 25, 26, 27};
const int stepPin = 14;
const int dirPin = 13; // Changed from 12 to 13 to avoid MTDI boot strapping conflicts
const int bootButton = 0; // BOOT button pin on standard ESP32 boards

#define motorInterfaceType 1
AccelStepper stepper(motorInterfaceType, stepPin, dirPin);

// Task handle for stepper task
TaskHandle_t stepperTaskHandle = NULL;

// Hardware Timer for Stepper Polling
hw_timer_t * stepperTimer = NULL;

void IRAM_ATTR onStepperTimer() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(stepperTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

void stepperTask(void *pvParameters) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    stepper.run();
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

// Servo mapping ranges (finger angle 0-180 maps to servoMin-servoMax)
const int servoMinAngles[5] = {0, 0, 30, 10, 30};
const int servoMaxAngles[5] = {180, 180, 180, 180, 180};

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
    
    // 1. Punch Stroke (90 degrees positive - reversed direction for ESP32 wiring)
    long stepsPunch = (long)((punchAngle / 360.0) * stepsPerRotation);
    long targetPunch = stepper.currentPosition() + stepsPunch;
    stepper.setMaxSpeed(punchSpeed);
    stepper.moveTo(targetPunch);
  }
}

void updatePunch() {
  switch (punchState) {
    case PUNCH_IDLE:
      break;
      
    case PUNCH_EXTENDING:
      if (stepper.distanceToGo() == 0) {
        punchState = PUNCH_PAUSED;
        punchPauseStartTime = millis();
      }
      break;
      
    case PUNCH_PAUSED:
      if (millis() - punchPauseStartTime >= punchPauseMs) {
        punchState = PUNCH_RECHARGING;
        long stepsRecharge = (long)((rechargeAngle / 360.0) * stepsPerRotation);
        long targetRecharge = stepper.currentPosition() + stepsRecharge;
        stepper.setMaxSpeed(rechargeSpeed);
        stepper.moveTo(targetRecharge);
      }
      break;
      
    case PUNCH_RECHARGING:
      if (stepper.distanceToGo() == 0) {
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
    servos[i].write(servoMinAngles[i]);
  }
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // Start with LED off
  pinMode(bootButton, INPUT_PULLUP); // Boot button input

  // Initialize stepper parameters
  stepper.setMinPulseWidth(20); 
  stepper.setAcceleration(2000); 

  // Create high-priority stepper task pinned to Core 1
  xTaskCreatePinnedToCore(
    stepperTask,
    "StepperTask",
    2048,             // Stack size
    NULL,             // Parameter
    24,               // Very high priority
    &stepperTaskHandle,
    1                 // Core 1
  );

  // Initialize hardware timer to notify the stepper task every 100 microseconds (10 kHz)
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  stepperTimer = timerBegin(1000000); // 1 MHz frequency (1 tick = 1 us)
  timerAttachInterrupt(stepperTimer, &onStepperTimer);
  timerAlarm(stepperTimer, 100, true, 0); // alarm value 100, autoreload, no reload limit
  timerStart(stepperTimer);
#else
  stepperTimer = timerBegin(0, 80, true); // Timer 0, prescaler 80 (1 tick = 1 us)
  timerAttachInterrupt(stepperTimer, &onStepperTimer, true);
  timerAlarmWrite(stepperTimer, 100, true); // alarm every 100 ticks (100 us), autoreload
  timerAlarmEnable(stepperTimer);
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

    // 2. Move Servos (only if the mapped angle changes to save CPU time)
    static int lastServoAngles[5] = {-1, -1, -1, -1, -1};
    for (int i = 0; i < 5; i++) {
      int mappedAngle = map(myData.finger_angles[i], 0, 180, servoMinAngles[i], servoMaxAngles[i]);
      if (mappedAngle != lastServoAngles[i]) {
        Serial.print("Writing Servo "); Serial.print(i);
        Serial.print(" from "); Serial.print(lastServoAngles[i]);
        Serial.print(" to "); Serial.println(mappedAngle);
        servos[i].write(mappedAngle);
        lastServoAngles[i] = mappedAngle;
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
