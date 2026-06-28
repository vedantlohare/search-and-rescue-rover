#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <driver/i2s.h>

// ==========================================
// NETWORK CONFIGURATION
// ==========================================
const char* ssid = "Vedant";           // Your WiFi Hotspot Name
const char* password = "123454321";    // Your WiFi Password
const char* targetIP = "xx.xxx.xxx.xx"; // <--- CHANGE THIS TO YOUR LAPTOP'S IP!
const int telemetryPort = 5005;        // Port sending data TO laptop
const int cmdPort = 4210;              // Port receiving commands FROM laptop

WiFiUDP udpTelemetry;
WiFiUDP udpCmd;
char incomingPacket[255];

// ==========================================
// PIN DEFINITIONS
// ==========================================
// Serial2 to Arduino Uno
#define RXD2 16
#define TXD2 17 // Connects to Arduino Uno Pin 12

// Mapping Sensors
#define TRIG_PIN 18 
#define ECHO_PIN 19 // From Voltage Divider
#define GAS_PIN 34
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// I2S Mics (Stereo 2-Mic Setup)
#define I2S_WS 15
#define I2S_SCK 14
#define I2S_SD 32
#define SAMPLE_RATE 16000
#define BUFFER_LEN 1024

// ==========================================
// FreeRTOS TASK HANDLE
// ==========================================
TaskHandle_t MappingTask;

// ==========================================
// HARDWARE INITIALIZATION
// ==========================================
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, 
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setup() {
  Serial.begin(115200); 
  
  // 1. Initialize Comms to Arduino Muscle Node
  Serial2.begin(38400, SERIAL_8N1, RXD2, TXD2);

  // 2. Wi-Fi Bootup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. Node 2 IP:");
  Serial.println(WiFi.localIP());

  // 3. Start UDP Listener
  udpCmd.begin(cmdPort);

  // 4. Sensor Pin Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);

  Wire.begin();
  if (!lox.begin()) {
    Serial.println(F("WARNING: Failed to boot VL53L0X ToF Sensor"));
  }

  setupI2S();

  // 5. BOOT CORE 0 FOR MAPPING (The FreeRTOS Upgrade)
  xTaskCreatePinnedToCore(
    mappingLoop,    // Function to run
    "Mapping",      // Name of task
    10000,          // Stack size
    NULL,           // Parameter
    1,              // Priority
    &MappingTask,   // Task handle
    0               // Pin to Core 0
  );

  Serial.println("Node 2 Brain (Dual-Core) Ready.");
}

// ==========================================
// TELEMETRY HELPER
// ==========================================
void sendTelemetry(String message) {
  udpTelemetry.beginPacket(targetIP, telemetryPort);
  udpTelemetry.print(message);
  udpTelemetry.endPacket();
}

// ==========================================
// CORE 0: THE MAPPER (Runs constantly in background)
// ==========================================
void mappingLoop(void * pvParameters) {
  for(;;) {
    // 1. Read ToF Sensor (Center)
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) { 
      int distCM = measure.RangeMilliMeter / 10;
      if (distCM < 150) { 
        sendTelemetry("MAP:" + String(distCM) + ",CENTER");
      }
    }

    // 2. Read Ultrasonic (Peripheral)
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    // Max wait time is 15000us (15ms). Because this is on Core 0, 
    // it won't freeze the robot's driving reflexes!
    long duration = pulseIn(ECHO_PIN, HIGH, 15000);
    if (duration > 0) {
      int distCM = duration * 0.034 / 2;
      if (distCM < 150) {
        sendTelemetry("MAP:" + String(distCM) + ",PERIPHERAL");
      }
    }
    // GAS SENSOR
    int gasValue = analogRead(GAS_PIN);

    if(gasValue > 1500)
    {
        sendTelemetry(
            "GAS:" +
            String(gasValue)
        );
    }
    
    // Pause this core for 150ms before scanning again
    vTaskDelay(150 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
// CORE 1: THE REFLEXES (Instant Comm & Audio)
// ==========================================
void loop() {
  // --- A. COMMAND RELAY (LAPTOP -> UNO) ---
  int packetSize = udpCmd.parsePacket();
  if (packetSize) {
    int len = udpCmd.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0; // Null-terminate
    }
    String command = String(incomingPacket);
    
    // Instantly blast it down the serial wire to the Arduino Uno
    Serial2.println(command);
  }

  // --- B. AUDIO LOCALIZATION ---
  int32_t samples[BUFFER_LEN];
  size_t bytesIn = 0;
  
  // 0 timeout means non-blocking. It grabs whatever audio is ready instantly.
  esp_err_t result = i2s_read(I2S_NUM_0, &samples, sizeof(samples), &bytesIn, 0);
  
  if (result == ESP_OK && bytesIn > 0) {
    int samplesRead = bytesIn / 4;
    int indexLeft = -1;
    int indexRight = -1;
    int threshold = 1500; // Trigger volume

    // Scan the audio chunk for the first loud noise
    for (int i = 0; i < samplesRead; i++) {
      int32_t currentSample = abs(samples[i] >> 14);
      if (i % 2 == 0) { 
        if (indexLeft == -1 && currentSample > threshold) indexLeft = i / 2;
      } else { 
        if (indexRight == -1 && currentSample > threshold) indexRight = i / 2;
      }
      if (indexLeft != -1 && indexRight != -1) break; 
    }

    // If both mics heard a loud noise, calculate the Time Difference
    if (indexLeft != -1 && indexRight != -1){
      long energy = 0;

      for(int k=0;k<samplesRead;k++)
      {
          energy += abs(samples[k] >> 14);
      }

      long avgEnergy = energy / samplesRead;

      if(avgEnergy > 2500)
      {
          sendTelemetry("VOICE");
      }

      // int delaySamples = indexRight - indexLeft;
      // int maxExpectedDelay = 7; 
      
      // if (delaySamples > maxExpectedDelay) delaySamples = maxExpectedDelay;
      // if (delaySamples < -maxExpectedDelay) delaySamples = -maxExpectedDelay;
      
      // int d12 = delaySamples;
      // int delaySq = d12 * d12;
      // int maxSq = maxExpectedDelay * maxExpectedDelay;
      // int d13 = (maxSq > delaySq) ? sqrt(maxSq - delaySq) : 0;
      
      // sendTelemetry("TDOA:" + String(d12) + "," + String(d13));
    }
  }
}




// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <Wire.h>
// #include "Adafruit_VL53L0X.h"
// #include <driver/i2s.h>

// // --- Network Setup ---
// const char* ssid = "Vedant";           // Your WiFi Hotspot Name
// const char* password = "123454321";    // Your WiFi Password
// const char* targetIP = "10.239.13.69";  // MUST REPLACE: Your laptop's IP address
// const int telemetryPort = 5005;        // Port sending data TO laptop
// const int cmdPort = 4210;              // Port receiving commands FROM laptop

// WiFiUDP udpTelemetry;
// WiFiUDP udpCmd;
// char incomingPacket[255];

// // --- Serial2 to Arduino Uno ---
// #define RXD2 16
// #define TXD2 17 // This connects to Arduino Uno Pin 12

// // --- Sensor Pins ---
// #define TRIG_PIN 18 // Matching our master blueprint
// #define ECHO_PIN 19 // Goes to Voltage Divider, then to HC-SR04
// Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// // --- I2S Mic Setup ---
// #define I2S_WS 15
// #define I2S_SCK 14
// #define I2S_SD 32
// #define SAMPLE_RATE 16000
// #define BUFFER_LEN 1024

// // --- Timing ---
// unsigned long lastMapTime = 0;
// const int MAP_INTERVAL = 150; // Scan mapping sensors every 150ms

// void setupI2S() {
//   i2s_config_t i2s_config = {
//     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
//     .sample_rate = SAMPLE_RATE,
//     .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
//     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo for 2 Mics
//     .communication_format = I2S_COMM_FORMAT_I2S,
//     .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
//     .dma_buf_count = 4,
//     .dma_buf_len = BUFFER_LEN,
//     .use_apll = false,
//     .tx_desc_auto_clear = false,
//     .fixed_mclk = 0
//   };
//   i2s_pin_config_t pin_config = {
//     .bck_io_num = I2S_SCK,
//     .ws_io_num = I2S_WS,
//     .data_out_num = I2S_PIN_NO_CHANGE,
//     .data_in_num = I2S_SD
//   };
//   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
//   i2s_set_pin(I2S_NUM_0, &pin_config);
// }

// void setup() {
//   Serial.begin(115200); // For Laptop USB Debugging
  
//   // Initialize Serial2 to talk to the Arduino Uno
//   Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

//   // WiFi Setup
//   WiFi.begin(ssid, password);
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nWiFi Connected. IP:");
//   Serial.println(WiFi.localIP());

//   // Start UDP listener for Drive/Arm commands
//   udpCmd.begin(cmdPort);

//   // Sensor Pin Setup
//   pinMode(TRIG_PIN, OUTPUT);
//   pinMode(ECHO_PIN, INPUT);

//   // ToF Sensor Setup
//   Wire.begin();
//   if (!lox.begin()) {
//     Serial.println(F("Failed to boot VL53L0X"));
//   }

//   setupI2S();
//   Serial.println("Node 2 Brain Ready.");
// }

// void sendTelemetry(String message) {
//   udpTelemetry.beginPacket(targetIP, telemetryPort);
//   udpTelemetry.print(message);
//   udpTelemetry.endPacket();
// }

// void loop() {
//   // ==========================================
//   // SUBSYSTEM 1: COMMAND RELAY (LAPTOP -> UNO)
//   // ==========================================
//   int packetSize = udpCmd.parsePacket();
//   if (packetSize) {
//     int len = udpCmd.read(incomingPacket, 255);
//     if (len > 0) {
//       incomingPacket[len] = 0; // Null-terminate
//     }
//     String command = String(incomingPacket);
    
//     // Immediately forward the command to the Arduino Uno via the TX2 wire
//     Serial2.println(command); 
//     Serial.println("Relayed to Uno: " + command);
//   }

//   // ==========================================
//   // SUBSYSTEM 2: AUDIO LOCALIZATION
//   // ==========================================
//   int32_t samples[BUFFER_LEN];
//   size_t bytesIn = 0;
  
//   esp_err_t result = i2s_read(I2S_NUM_0, &samples, sizeof(samples), &bytesIn, 0); // Non-blocking read
  
//   if (result == ESP_OK && bytesIn > 0) {
//     int samplesRead = bytesIn / 4; 
//     int indexLeft = -1;
//     int indexRight = -1;
//     int threshold = 1500; // Trigger volume

//     // First Arrival Detection
//     for (int i = 0; i < samplesRead; i++) {
//       int32_t currentSample = abs(samples[i] >> 14);
//       if (i % 2 == 0) { // Left channel (Mic 1)
//         if (indexLeft == -1 && currentSample > threshold) indexLeft = i / 2;
//       } else { // Right channel (Mic 2)
//         if (indexRight == -1 && currentSample > threshold) indexRight = i / 2;
//       }
//       if (indexLeft != -1 && indexRight != -1) break; // Found both
//     }

//     if (indexLeft != -1 && indexRight != -1) { 
//       int delaySamples = indexRight - indexLeft;
//       int maxExpectedDelay = 20; 
      
//       if (delaySamples > maxExpectedDelay) delaySamples = maxExpectedDelay;
//       if (delaySamples < -maxExpectedDelay) delaySamples = -maxExpectedDelay;

//       int d12 = delaySamples;
//       int delaySq = d12 * d12;
//       int maxSq = maxExpectedDelay * maxExpectedDelay;
//       int d13 = (maxSq > delaySq) ? sqrt(maxSq - delaySq) : 0;
      
//       sendTelemetry("TDOA:" + String(d12) + "," + String(d13));
//     }
//   }

//   // ==========================================
//   // SUBSYSTEM 3: SENSOR FUSION MAPPING
//   // ==========================================
//   // Uses millis() instead of delay() so it never freezes the audio or command streams
//   if (millis() - lastMapTime > MAP_INTERVAL) {
//     lastMapTime = millis();

//     // 1. Read ToF Sensor (Center)
//     VL53L0X_RangingMeasurementData_t measure;
//     lox.rangingTest(&measure, false);
//     if (measure.RangeStatus != 4) { // 4 means out of range
//       int distCM = measure.RangeMilliMeter / 10;
//       if (distCM < 150) { // Only transmit if an obstacle is within 1.5 meters
//         sendTelemetry("MAP:" + String(distCM) + ",CENTER");
//       }
//     }

//     // 2. Read Ultrasonic (Peripheral)
//     digitalWrite(TRIG_PIN, LOW);
//     delayMicroseconds(2);
//     digitalWrite(TRIG_PIN, HIGH);
//     delayMicroseconds(10);
//     digitalWrite(TRIG_PIN, LOW);
    
//     // Timeout set to 15000us (~2.5 meters) to prevent logic freezing
//     long duration = pulseIn(ECHO_PIN, HIGH, 15000); 
//     if (duration > 0) {
//       int distCM = duration * 0.034 / 2;
//       if (distCM < 150) {
//         sendTelemetry("MAP:" + String(distCM) + ",PERIPHERAL");
//       }
//     }
//   }
// }