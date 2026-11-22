// Peripheral check for the ESP32 Lite (Wemos/LOLIN32 Lite) glove board.
// Tests the MAX30102 heart-rate sensor (SDA16, SCL17) and the buzzer/vibration
// output on GPIO32. Runs on its own after reset; no typing needed.
// Type z + Enter to repeat the buzzer test. The radio is left idle.
#include <Arduino.h>
#include <Wire.h>
#include <MAX30105.h>
#include <heartRate.h>

constexpr int sdaPin = 16;
constexpr int sclPin = 17;
constexpr int buzzerPin = 32;
constexpr int radioCs = 33;    // Hold the LoRa radio deselected.
constexpr int radioReset = 27; // Keep the radio out of reset but idle.
constexpr uint8_t maxAddress = 0x57;

MAX30105 sensor; // Also supports MAX30102.
bool sensorOk = false;
uint32_t lastBeat = 0, lastPrint = 0;
float bpm = 0;

void scanI2c() {
  Serial.println("I2C scan on SDA16/SCL17:");
  int found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X%s\n", address,
                    address == maxAddress ? " (MAX30102)" : "");
      ++found;
    }
  }
  if (found == 0) Serial.println("  no devices. Check sensor VIN (3V), GND, SDA16, SCL17.");
}

void buzzerTest() {
  Serial.println("Buzzer test 1/2: GPIO32 HIGH for 1 s (active buzzer or vibration module).");
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  digitalWrite(buzzerPin, LOW);
  delay(500);
  Serial.println("Buzzer test 2/2: 2 kHz tone for 1 s (passive buzzer).");
  ledcSetup(0, 2000, 8);
  ledcAttachPin(buzzerPin, 0);
  ledcWrite(0, 128); // 50% duty square wave at 2 kHz.
  delay(1000);
  ledcWrite(0, 0);
  ledcDetachPin(buzzerPin);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  Serial.println("Buzzer test done. Did you hear/feel step 1, step 2, both or neither?");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(radioCs, OUTPUT);
  digitalWrite(radioCs, HIGH);
  pinMode(radioReset, OUTPUT);
  digitalWrite(radioReset, HIGH);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  Serial.println();
  Serial.println("ESP32 Lite peripheral test: MAX30102 SDA16 SCL17, buzzer GPIO32");
  Wire.begin(sdaPin, sclPin);
  scanI2c();

  sensorOk = sensor.begin(Wire, I2C_SPEED_STANDARD);
  if (sensorOk) {
    Serial.printf("PASS: MAX3010x found, part ID 0x%02X (0x15 expected), revision %u\n",
                  sensor.readPartID(), sensor.getRevisionID());
    sensor.setup(0x1F, 4, 2, 400, 411, 4096);
  } else {
    Serial.println("FAIL: MAX30102 not detected. Check VIN (3V), GND, SDA16, SCL17.");
  }

  buzzerTest();
  if (sensorOk) Serial.println("Place a fingertip gently on the sensor and keep still.");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'z' || c == 'Z') buzzerTest();
  }
  if (!sensorOk) {
    delay(10);
    return;
  }

  long ir = sensor.getIR();
  bool beat = checkForBeat(ir);
  uint32_t now = millis();
  if (ir < 50000) {
    lastBeat = 0;
    bpm = 0;
  } else if (beat) {
    if (lastBeat != 0) {
      float reading = 60000.0f / (now - lastBeat);
      if (reading >= 30 && reading <= 220) bpm = reading;
    }
    lastBeat = now;
  }
  if (lastBeat != 0 && now - lastBeat > 3000) {
    lastBeat = 0;
    bpm = 0;
  }

  if (now - lastPrint >= 500) {
    lastPrint = now;
    Serial.printf("IR=%ld RED=%lu", ir, (unsigned long)sensor.getRed());
    if (ir < 50000) Serial.println(" | no finger");
    else if (bpm == 0) Serial.println(" | finger on, finding pulse... keep still");
    else Serial.printf(" | BPM~%.0f\n", bpm);
  }
}
