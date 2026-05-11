#include "BluetoothSerial.h"

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

// LED Configuration
#define STATUS_LED 2  // GPIO 2 is the built-in LED on most ESP32 DevBoards
unsigned long lastBlinkTime = 0;
const long blinkInterval = 500; // 0.5s blink
bool ledState = LOW;

// Sensor variables
float accX, accY, accZ;
float gyroX, gyroY, gyroZ;
int flex1, flex2;

String inputBuffer = "";

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  
  // Initialize Bluetooth
  SerialBT.begin("ESP32_Arm_Controller"); // Bluetooth device name
  Serial.println("Bluetooth started. Ready to pair with name: ESP32_Arm_Controller");
}

void loop() {
  updateStatusLED();
  readBluetoothData();
}

void updateStatusLED() {
  if (SerialBT.hasClient()) {
    // Connected: Solid ON
    digitalWrite(STATUS_LED, HIGH);
  } else {
    // Disconnected: Blink
    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = currentMillis;
      ledState = !ledState;
      digitalWrite(STATUS_LED, ledState);
    }
  }
}

void readBluetoothData() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        parseData(inputBuffer);
      }
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
}

void parseData(String data) {
  // Expected format: S:<acc_x>,<acc_y>,<acc_z>,<gyro_x>,<gyro_y>,<gyro_z>,<flex_1>,<flex_2>
  if (data.startsWith("S:")) {
    String content = data.substring(2);
    
    // Simple parsing using comma as delimiter
    int indices[8];
    int count = 0;
    int lastPos = 0;
    
    for (int i = 0; i < content.length() && count < 7; i++) {
      if (content.charAt(i) == ',') {
        indices[count++] = i;
      }
    }
    
    if (count == 7) {
      accX = content.substring(0, indices[0]).toFloat();
      accY = content.substring(indices[0] + 1, indices[1]).toFloat();
      accZ = content.substring(indices[1] + 1, indices[2]).toFloat();
      gyroX = content.substring(indices[2] + 1, indices[3]).toFloat();
      gyroY = content.substring(indices[3] + 1, indices[4]).toFloat();
      gyroZ = content.substring(indices[4] + 1, indices[5]).toFloat();
      flex1 = content.substring(indices[5] + 1, indices[6]).toInt();
      flex2 = content.substring(indices[6] + 1).toInt();
      
      // Debug output
      Serial.print("Received - Acc: ["); Serial.print(accX); Serial.print(", "); Serial.print(accY); Serial.print(", "); Serial.print(accZ); Serial.print("] ");
      Serial.print("Gyro: ["); Serial.print(gyroX); Serial.print(", "); Serial.print(gyroY); Serial.print(", "); Serial.print(gyroZ); Serial.print("] ");
      Serial.print("Flex: ["); Serial.print(flex1); Serial.print(", "); Serial.print(flex2); Serial.println("]");
    } else {
      Serial.print("Malformed data (found "); Serial.print(count); Serial.print(" commas): "); Serial.println(data);
    }
  }
}
