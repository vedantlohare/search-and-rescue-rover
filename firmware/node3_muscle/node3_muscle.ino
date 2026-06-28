#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

bool emergencyStop = false;

// ==========================================
// COMMUNICATIONS (Node 2 -> Node 3)
// ==========================================
// RX on Pin 12 (Connect to ESP32 TX2).
// Pin 13 is TX (leave disconnected for safety).
SoftwareSerial espSerial(12, 13);

// ==========================================
// PCA9685 SERVO DRIVER SETUP
// ==========================================
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo Pulse Width Settings (SG90 & MG996R)
#define SERVOMIN  150 // Minimum pulse length count (out of 4096)
#define SERVOMAX  600 // Maximum pulse length count (out of 4096)
#define SERVO_FREQ 50 // Analog servos run at 50 Hz updates

// PCA9685 Pin Mapping
const int SHOULDER_PIN = 0; 
const int ELBOW_PIN = 1;    
const int CLAW_PIN = 2;     
const int FLIP_L_PIN = 3;   // Left Flipper (MG996R)
const int FLIP_R_PIN = 4;   // Right Flipper (MG996R)

// --- Servo Smoothing Variables ---
// Index maps directly to Pin: {Shoulder, Elbow, Claw, Flip_L, Flip_R}
int currentAngles[5] = {90, 90, 90, 90, 90}; 
int targetAngles[5]  = {90, 90, 90, 90, 90};

// NEW: Instead of one global timer, each servo gets its own timer
unsigned long lastServoTimes[5] = {0, 0, 0, 0, 0};

// NEW: Custom speeds (Milliseconds per 1-degree step. Higher = Slower)
// Shoulder (15), Elbow (15), Claw (10 - Fast!), Flipper L (35 - Slow!), Flipper R (35 - Slow!)
int servoSpeeds[5] = {30, 30, 20, 35, 35};

// ==========================================
// DRIVE BASE PINS (L298N)
// ==========================================
const int enA = 5;
const int in1 = 2;
const int in2 = 4;
const int enB = 6;  
const int in3 = 7;
const int in4 = 8;

int motorSpeed = 240; // PWM speed (0-255)

// ==========================================
// INITIALIZATION
// ==========================================
void setup() {
  // Serial for USB debugging to your laptop
  Serial.begin(115200);
  
  // CRITICAL FIX: SoftwareSerial on Arduino Uno cannot handle 115200 stably.
  // 38400 is the highest safe speed for error-free data transfer.
  espSerial.begin(38400); 
  
  // Initialize L298N Motor Pins
  pinMode(enA, OUTPUT); pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(enB, OUTPUT); pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  stopMotors();

  // Initialize PCA9685
  pwm.begin();
  pwm.setOscillatorFrequency(27000000); 
  pwm.setPWMFreq(SERVO_FREQ);  
  
  // Set arm and flippers to default safe starting positions
  moveServo(SHOULDER_PIN, 90);
  moveServo(ELBOW_PIN, 90);
  moveServo(CLAW_PIN, 90);
  moveServo(FLIP_L_PIN, 90);
  moveServo(FLIP_R_PIN, 90);
  
  Serial.println("Vanguard Muscle Node (Node 3) Ready. Awaiting 38400 baud.");
}

// ==========================================
// MAIN LOOP: MEMORY-SAFE COMMAND PARSING
// ==========================================
void loop() {
  // Fixed 32-byte memory block. Prevents Arduino RAM crashes.
  char buffer[32]; 

  if (espSerial.available() > 0) {
    // Read the incoming text until the newline, without using String objects
    int len = espSerial.readBytesUntil('\n', buffer, sizeof(buffer) - 1);
    
    // Ignore empty lines
    if (len == 0 || buffer[0] == '\r') return; 
    
    buffer[len] = '\0'; // Add a null terminator to make it a valid C-string
    
    // strtok splits the text at the colon ':'
    char* commandPart = strtok(buffer, ":\r");
    char* valuePart = strtok(NULL, ":\r");
    
    if (commandPart == NULL) return; // Corrupted data catch
    
    // --- 1. ACTUATOR COMMANDS (If a value was sent) ---
    if (valuePart != NULL) {
      int angle = atoi(valuePart); 
      angle = constrain(angle, 0, 180); 
      
      // Simply update the target; do NOT move the servo here!
      if (strcmp(commandPart, "SHOULDER") == 0) { targetAngles[SHOULDER_PIN] = angle; }
      else if (strcmp(commandPart, "ELBOW") == 0) { targetAngles[ELBOW_PIN] = angle; }
      else if (strcmp(commandPart, "CLAW") == 0) { targetAngles[CLAW_PIN] = angle; }
      
      // Flipper Sync Target
      else if (strcmp(commandPart, "FLIPPER") == 0) { 
        targetAngles[FLIP_L_PIN] = angle;
        targetAngles[FLIP_R_PIN] = 180 - angle; 
      }
    }
    // --- 2. DRIVE COMMANDS (No value sent) ---
    else {
      if (strcmp(commandPart, "DRIVE_FWD") == 0) { driveForward(); } 
      else if (strcmp(commandPart, "DRIVE_BACK") == 0) { driveBackward(); } 
      else if (strcmp(commandPart, "DRIVE_LEFT") == 0) { turnLeft(); } 
      else if (strcmp(commandPart, "DRIVE_RIGHT") == 0) { turnRight(); } 
      else if (strcmp(commandPart, "DRIVE_STOP") == 0) { stopMotors(); }
    }
  }

  // ==========================================
  // THE ADVANCED SMOOTHING ENGINE (Runs constantly)
  // ==========================================
  unsigned long currentMillis = millis();
  
  // Check all 5 servos independently
  for (int i = 0; i < 5; i++) {
    if (currentAngles[i] != targetAngles[i]) {
      
      // Check if it is time for THIS specific servo to move one step
      if (currentMillis - lastServoTimes[i] > servoSpeeds[i]) {
        lastServoTimes[i] = currentMillis; // Reset this servo's specific timer
        
        // Step 1 degree closer to the target
        if (currentAngles[i] < targetAngles[i]) currentAngles[i]++;
        else currentAngles[i]--;
        
        // Command the PCA9685 to move to the new micro-step
        int pulse = map(currentAngles[i], 0, 180, SERVOMIN, SERVOMAX);
        pwm.setPWM(i, 0, pulse); 
      }
    }
  }
}

// ==========================================
// HARDWARE HELPER FUNCTIONS (MIRRORED GEOMETRY)
// ==========================================
void moveServo(int pin, int angle) {
  int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  pwm.setPWM(pin, 0, pulse);
}

void driveForward() {
  if(emergencyStop)
  {
      stopMotors();
      return;
  }
  // Left side spins Forward
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
  // Right side spins Forward (Electronically Inverted)
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
  analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
}

void driveBackward() {
  if(emergencyStop)
  {
      stopMotors();
      return;
  }
  // Left side spins Backward
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
  // Right side spins Backward (Electronically Inverted)
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
  analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
}

void turnLeft() {
  if(emergencyStop)
  {
      stopMotors();
      return;
  }
  // Tank Steer: Left track pulls Backward, Right track pushes Forward
  digitalWrite(in1, LOW); digitalWrite(in2, HIGH); 
  digitalWrite(in3, LOW); digitalWrite(in4, HIGH); 
  analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
}

void turnRight() {
  if(emergencyStop)
  {
      stopMotors();
      return;
  }
  // Tank Steer: Left track pushes Forward, Right track pulls Backward
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW); 
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW); 
  analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
}

void stopMotors() {
  digitalWrite(in1, LOW); digitalWrite(in2, LOW);
  digitalWrite(in3, LOW); digitalWrite(in4, LOW);
  analogWrite(enA, 0); analogWrite(enB, 0);
}



// #include <SoftwareSerial.h>
// #include <Wire.h>
// #include <Adafruit_PWMServoDriver.h>

// // --- Communications Setup (Node 2 to Node 3) ---
// // RX on Pin 12 (Connect to ESP32 TX2). Pin 13 is TX (leave disconnected for safety).
// SoftwareSerial espSerial(12, 13); 

// // --- PCA9685 Servo Driver Setup ---
// Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// // Servo Pulse Width Settings
// #define SERVOMIN  150 // Minimum pulse length count (out of 4096)
// #define SERVOMAX  600 // Maximum pulse length count (out of 4096)
// #define SERVO_FREQ 50 // Analog servos run at 50 Hz updates

// // PCA9685 Pin Mapping
// const int SHOULDER_PIN = 0; // SG90
// const int ELBOW_PIN = 1;    // SG90
// const int CLAW_PIN = 2;     // SG90
// const int FLIP_L_PIN = 3;   // MG995 (Left Flipper)
// const int FLIP_R_PIN = 4;   // MG995 (Right Flipper)

// // --- Drive Base Pins (L298N) ---
// const int enA = 5;  
// const int in1 = 2;
// const int in2 = 4;
// const int enB = 6;  
// const int in3 = 7;
// const int in4 = 8;

// int motorSpeed = 150; // PWM speed (0-255)

// void setup() {
//   // Serial for USB debugging to your laptop (Optional)
//   Serial.begin(115200); 
  
//   // SoftwareSerial to receive commands from the ESP32
//   espSerial.begin(115200); 
  
//   // Initialize L298N Motor Pins
//   pinMode(enA, OUTPUT); pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
//   pinMode(enB, OUTPUT); pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
//   stopMotors();

//   // Initialize PCA9685
//   pwm.begin();
//   pwm.setOscillatorFrequency(27000000); // Standard internal clock
//   pwm.setPWMFreq(SERVO_FREQ);  
  
//   // Set arm and flippers to default safe starting positions (90 degrees)
//   moveServo(SHOULDER_PIN, 90);
//   moveServo(ELBOW_PIN, 90);
//   moveServo(CLAW_PIN, 90);
//   moveServo(FLIP_L_PIN, 90);
//   moveServo(FLIP_R_PIN, 90);
  
//   Serial.println("Vanguard Muscle Node (Node 3) Ready.");
// }

// void loop() {
//   // Listen for commands coming down the wire from Node 2 (ESP32)
//   if (espSerial.available() > 0) {
//     String command = espSerial.readStringUntil('\n');
//     command.trim(); 
    
//     Serial.println("Executing: " + command); // Echo to USB Serial for debugging
    
//     // 1. Check if it's an Actuator command (contains a colon ':')
//     int colonIndex = command.indexOf(':');
    
//     if (colonIndex > 0) {
//       String joint = command.substring(0, colonIndex);
//       int angle = command.substring(colonIndex + 1).toInt();
      
//       // Safety constraint to protect the physical gears
//       angle = constrain(angle, 0, 180);
      
//       if (joint == "SHOULDER") { moveServo(SHOULDER_PIN, angle); }
//       else if (joint == "ELBOW") { moveServo(ELBOW_PIN, angle); }
//       else if (joint == "CLAW") { moveServo(CLAW_PIN, angle); }
      
//       // Flipper Sync Command
//       else if (joint == "FLIPPER") { 
//         moveServo(FLIP_L_PIN, angle);
//         // Mathematically invert the right servo so it mirrors the left side
//         moveServo(FLIP_R_PIN, 180 - angle); 
//       }
//     }
    
//     // 2. Check if it's a DRIVE command
//     else {
//       if (command == "DRIVE_FWD") { driveForward(); } 
//       else if (command == "DRIVE_BACK") { driveBackward(); } 
//       else if (command == "DRIVE_LEFT") { turnLeft(); } 
//       else if (command == "DRIVE_RIGHT") { turnRight(); } 
//       else if (command == "DRIVE_STOP") { stopMotors(); }
//     }
//   }
// }

// // --- Servo Helper Function ---
// void moveServo(int pin, int angle) {
//   // Map the 0-180 degree angle to the 150-600 pulse length expected by the PCA9685
//   int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX);
//   pwm.setPWM(pin, 0, pulse);
// }

// // --- Motor Functions ---
// void driveForward() {
//   digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
//   digitalWrite(in3, HIGH); digitalWrite(in4, LOW);
//   analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
// }
// void driveBackward() {
//   digitalWrite(in1, LOW); digitalWrite(in2, HIGH);
//   digitalWrite(in3, LOW); digitalWrite(in4, HIGH);
//   analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
// }
// void turnLeft() {
//   digitalWrite(in1, LOW); digitalWrite(in2, HIGH); 
//   digitalWrite(in3, HIGH); digitalWrite(in4, LOW); 
//   analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
// }
// void turnRight() {
//   digitalWrite(in1, HIGH); digitalWrite(in2, LOW); 
//   digitalWrite(in3, LOW); digitalWrite(in4, HIGH); 
//   analogWrite(enA, motorSpeed); analogWrite(enB, motorSpeed);
// }
// void stopMotors() {
//   digitalWrite(in1, LOW); digitalWrite(in2, LOW);
//   digitalWrite(in3, LOW); digitalWrite(in4, LOW);
//   analogWrite(enA, 0); analogWrite(enB, 0);
// }