#include <Adafruit_Fingerprint.h>
#include <ESP32Servo.h>  // ESP32 compatible servo library

// ==================== PIN CONFIG ====================
#define FINGER_RX 16       // Fingerprint TX -> ESP32 RX
#define FINGER_TX 17       // Fingerprint RX -> ESP32 TX
#define BUZZER_PIN 25
#define MODE_PIN 26
#define VEHICLE_PIN 33
#define SERVO_PIN 13
#define HEARTBEAT_LED 2    // Single heartbeat LED

#define SIM900_TX 27       // SIM900 TX -> ESP32 RX
#define SIM900_RX 14       // SIM900 RX -> ESP32 TX

HardwareSerial fingerSerial(2);   // Fingerprint serial
HardwareSerial SIM900(1);         // SIM900 serial

Adafruit_Fingerprint finger(&fingerSerial); 
Servo vehicleLock;                      

bool vehicleState = false;
bool lastMode = HIGH;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(VEHICLE_PIN, OUTPUT);
  pinMode(HEARTBEAT_LED, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(VEHICLE_PIN, LOW);
  digitalWrite(HEARTBEAT_LED, LOW);

  // Servo lock initial
  vehicleLock.attach(SERVO_PIN);
  vehicleLock.write(90); // locked initially

  // Fingerprint sensor
  fingerSerial.begin(57600, SERIAL_8N1, FINGER_RX, FINGER_TX);
  finger.begin(57600);
  delay(10);

  Serial.println("\n===============================");
  Serial.println("🚗 SMART VEHICLE SAFETY SYSTEM");
  Serial.println("===============================");

  if (finger.verifyPassword()) {
    Serial.println("✅ Fingerprint sensor connected!");
  } else {
    Serial.println("❌ ERROR: Check sensor wiring!");
    while (1) { toneBuzz(1, 500); }
  }

  finger.getTemplateCount();
  Serial.print("📁 Fingerprints stored: ");
  Serial.println(finger.templateCount);

  // SIM900 setup
  SIM900.begin(9600, SERIAL_8N1, SIM900_RX, SIM900_TX);
  Serial.println("📡 SIM900 initializing...");
  delay(1000);
  SIM900.println("AT");       // Test command
  delay(500);
  SIM900.println("AT+CMGF=1"); // SMS text mode
  delay(500);
  Serial.println("✅ SIM900 ready");

  Serial.println("System Ready!");
  Serial.println("===============================\n");
}

// ==================== LOOP ====================
void loop() {
  // -------- Heartbeat LED --------
  digitalWrite(HEARTBEAT_LED, HIGH);
  delay(500);
  digitalWrite(HEARTBEAT_LED, LOW);
  delay(500);

  // -------- Mode Handling ---------
  bool currentMode = digitalRead(MODE_PIN);

  if (currentMode != lastMode) {
    Serial.println();
    Serial.println("===============================");
    if (currentMode == LOW) {
      Serial.println("📥 ENROLL MODE ENABLED (-1 to DELETE ALL fingerprints)");
    } else {
      Serial.println("🚗 ACTIVE MODE ENABLED");
      Serial.println("Place finger to START / STOP vehicle");
    }
    Serial.println("===============================");
    lastMode = currentMode;
    Serial.flush();
    delay(500);
  }

  if (currentMode == LOW)
    enrollMode();
  else
    activeMode();
}

// ==================== ENROLL MODE (Only -1) ====================
void enrollMode() {
  if (!Serial.available()) return;
  int inputValue = Serial.parseInt();    

  // Only delete all fingerprints
  if (inputValue != -1) return;

  Serial.println("🧹 Clearing all fingerprints from sensor...");
  uint8_t p = finger.emptyDatabase();
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ All fingerprints deleted successfully!");
    toneBuzz(2, 100);
  } else {
    Serial.println("❌ Error clearing database!");
    toneBuzz(3, 150);
  }
}

// ==================== ACTIVE MODE ====================
void activeMode() {
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) return;
  if (p != FINGERPRINT_OK) return;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return;

  p = finger.fingerFastSearch();

  if (p == FINGERPRINT_OK) {
    Serial.print("✅ Access Granted! ID: ");
    Serial.println(finger.fingerID);

    vehicleState = !vehicleState; // toggle ON/OFF

    if (vehicleState) {
      vehicleLock.write(0); // UNLOCK servo
      Serial.println("🔓 Vehicle Unlocked");
      delay(1000);          
      digitalWrite(VEHICLE_PIN, HIGH);
      Serial.println("🚘 Vehicle Started!");
    } else {
      digitalWrite(VEHICLE_PIN, LOW);
      Serial.println("🛑 Vehicle Stopped!");
      delay(1000);          
      vehicleLock.write(90); // LOCK servo
      Serial.println("🔒 Vehicle Locked");
    }
  } else {
    Serial.println("❌ ACCESS DENIED! Unknown Finger.");
    toneBuzz(5, 200);        
    vehicleLock.write(90);       
    digitalWrite(VEHICLE_PIN, LOW); 
    emergencyAlert();
  }
}

// ==================== BUZZER HELPER ====================
void toneBuzz(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(delayMs);
    digitalWrite(BUZZER_PIN, LOW);
    delay(delayMs);
  }
}

// ==================== EMERGENCY ALERT ====================
void emergencyAlert() {
  Serial.println("🚨 ACCESS DENIED! Sending emergency alert...");

  // --- Send SMS ---
  SIM900.println("AT+CMGF=1");   // Set text mode
  delay(500);

  SIM900.println("AT+CMGS=\"8549990019\""); // Recipient
  delay(500);

  SIM900.print("ALERT! Unauthorized access detected!"); // Message
  delay(500);

  SIM900.write(26); // Ctrl+Z
  delay(5000);      // Wait for SMS
  Serial.println("📩 SMS sent");

  // --- Make Call ---
  SIM900.println("ATD8549990019;");  
  delay(1000);
  Serial.println("📞 Calling...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(15000); 
  digitalWrite(BUZZER_PIN, LOW);
  SIM900.println("ATH"); 
  Serial.println("📴 Call ended");
}
