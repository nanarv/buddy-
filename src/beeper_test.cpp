/*
  Beeper test: makes a repeating tone for the listener in listener_test.cpp to hear.
  Board: Adafruit ESP32 Feather (HUZZAH32), beeper on GPIO 21
  Framework: Arduino (PlatformIO), environment `beeper_test`

  WIRING (passive piezo disc or passive buzzer):
      GPIO 21 ──[100Ω]── piezo (+ / red)
      GND     ────────── piezo (- / black)
  The 100Ω resistor limits current into the piezo capacitance; it is optional
  for small buzzers.

  For an *active* buzzer (has its own oscillator; beeps on plain DC), set
  kActiveBuzzer = true so the pin is simply driven HIGH/LOW.

  Pattern: kBeepMs on, kSilenceMs off, repeating. The listener should print
  large peak values during beeps and near-baseline values in the gaps. Air
  coupling between two piezo discs is weak; for a clear signal, place the
  beeper against the listening piezo or the same table/surface.
*/

#include <Arduino.h>

#include "BeepTest.h"

namespace {

constexpr int      kBeeperPin    = 21;
constexpr bool     kActiveBuzzer = false;  // true = active buzzer (DC on/off)
constexpr uint32_t kToneHz       = kBeepToneHz;  // shared with the listener
constexpr uint32_t kBeepMs       = 500;
constexpr uint32_t kSilenceMs    = 1000;
constexpr uint8_t  kLedcChannel  = 0;
constexpr uint8_t  kLedcBits     = 8;      // 50% duty = 128 gives a square wave

void beeperOn() {
    if (kActiveBuzzer) {
        digitalWrite(kBeeperPin, HIGH);
    } else {
        ledcWriteTone(kLedcChannel, kToneHz);
    }
}

void beeperOff() {
    if (kActiveBuzzer) {
        digitalWrite(kBeeperPin, LOW);
    } else {
        ledcWrite(kLedcChannel, 0);  // stop the square wave; pin held low
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);

    if (kActiveBuzzer) {
        pinMode(kBeeperPin, OUTPUT);
    } else {
        ledcSetup(kLedcChannel, kToneHz, kLedcBits);
        ledcAttachPin(kBeeperPin, kLedcChannel);
    }
    beeperOff();
}

void loop() {
    // Board uptime lets you line up beeps with the listener's serial output.
    beeperOn();
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.printf("ms:%lu beep:on freq:%lu\n",
                  static_cast<unsigned long>(millis()),
                  static_cast<unsigned long>(kActiveBuzzer ? 0 : kToneHz));
    delay(kBeepMs);

    beeperOff();
    digitalWrite(LED_BUILTIN, LOW);
    Serial.printf("ms:%lu beep:off\n", static_cast<unsigned long>(millis()));
    delay(kSilenceMs);
}
