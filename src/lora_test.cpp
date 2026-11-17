#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <string.h>

// Standalone RFM96W test, using the HUZZAH32 Feather's labeled SPI pins.
constexpr int radioReset = 27;
constexpr int radioCs = 33;
constexpr int radioMosi = 18;
constexpr int radioMiso = 19;
constexpr int radioSck = 5;
constexpr int radioDio0 = 12;

// SOS button: wired between GPIO14 and GND, using the internal pull-up, so the
// pin reads LOW while pressed. One press sends one SOS packet (no auto-repeat).
constexpr int sosButton = 14;
constexpr uint32_t debounceMs = 30;
constexpr char sosPrefix[] = "SOS";

// SOS indicator LED: GPIO21 -> ~220-330 ohm resistor -> LED -> GND (HIGH = on).
// Lit for sosLedMs after each received SOS; a new SOS restarts the timer.
constexpr int sosLed = 21;
constexpr uint32_t sosLedMs = 5000;
bool sosLedOn = false;
uint32_t sosLedSince = 0;
uint32_t sosLedDuration = sosLedMs;

bool radioReady = false;
// Local hardware check (serial b): button presses only print and drive the LED;
// nothing is transmitted.
bool buttonTestMode = false;

uint32_t packetNumber = 0;
int buttonReading = HIGH;
int buttonStable = HIGH;
uint32_t buttonChangedAt = 0;

// Sends "<label> #<n>". Shared by the t command and the SOS button so both use
// the same packet counter and report failures the same way.
// Turns the LED on for durationMs without blocking; the loop turns it off.
void lightLed(uint32_t durationMs) {
  digitalWrite(sosLed, HIGH);
  sosLedOn = true;
  sosLedSince = millis();
  sosLedDuration = durationMs;
}

void sendPacket(const char *label) {
  if (!radioReady) {
    Serial.printf("FAIL: radio not initialized; %s not sent.\n", label);
    return;
  }
  const uint32_t number = ++packetNumber;
  if (!LoRa.beginPacket()) {
    Serial.println("FAIL: could not start TX.");
    return;
  }
  LoRa.print(label);
  LoRa.print(" #");
  LoRa.print(number);
  if (LoRa.endPacket()) {
    Serial.printf("TX complete: %s #%lu (delivery unconfirmed)\n", label,
                  static_cast<unsigned long>(number));
  } else {
    Serial.println("FAIL: TX did not complete.");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(sosButton, INPUT_PULLUP);
  pinMode(sosLed, OUTPUT);
  digitalWrite(sosLed, LOW);
  delay(1000);
  Serial.println("RFM96W standalone test: 433 MHz");
  Serial.println("RST=27 CS=33 MOSI=18 MISO=19 SCK=5 G0=12 SOS button=14 (to GND) SOS LED=21");

  // Local check before the radio: blink the LED so its wiring can be seen.
  Serial.println("LED check: GPIO21 LED should blink 3 times now.");
  for (int i = 0; i < 3; ++i) {
    digitalWrite(sosLed, HIGH);
    delay(200);
    digitalWrite(sosLed, LOW);
    delay(200);
  }
  // Start from the real button level so a stuck/miswired button cannot send
  // an SOS at boot; it must be released and pressed again.
  buttonReading = buttonStable = digitalRead(sosButton);
  if (buttonStable == LOW) {
    Serial.println("WARN: button reads PRESSED at startup. If untouched, check it goes GPIO14 -> button -> GND.");
  } else {
    Serial.println("Button check: reads released (OK). Type b to test presses without transmitting.");
  }
  Serial.println("Commands: l = LED on 1 s, b = button test mode on/off, t = send test packet.");

  SPI.begin(radioSck, radioMiso, radioMosi, radioCs);
  LoRa.setPins(radioCs, radioReset, radioDio0);
  if (!LoRa.begin(433E6)) {
    Serial.println("FAIL: radio initialization. Check power, ground, SPI and reset wiring.");
    Serial.println("Radio disabled; l and b button/LED checks still work.");
    return;
  }
  radioReady = true;
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.setTxPower(5);
  Serial.println("PASS: radio initialized over SPI. This does not verify the RF link or G0 wire.");
  Serial.println("Listening. Attach a 433 MHz antenna before typing t or pressing the SOS button.");
  Serial.println("Use a second matching radio to verify reception; TX completion is not an acknowledgement.");
}

void loop() {
  int packetSize = radioReady ? LoRa.parsePacket() : 0;
  if (packetSize > 0) {
    // LoRa payloads are at most 255 bytes; keep the text for SOS matching.
    char text[256];
    int length = 0;
    while (LoRa.available()) {
      const int byte = LoRa.read();
      if (length < static_cast<int>(sizeof(text)) - 1) text[length++] = static_cast<char>(byte);
    }
    text[length] = '\0';

    if (strncmp(text, sosPrefix, strlen(sosPrefix)) == 0) {
      Serial.printf("!!! SOS RECEIVED !!! (%s) | RSSI=%d dBm SNR=%.1f dB\n", text,
                    LoRa.packetRssi(), LoRa.packetSnr());
      lightLed(sosLedMs);
    } else {
      Serial.printf("RX %d bytes: %s | RSSI=%d dBm SNR=%.1f dB\n", packetSize, text,
                    LoRa.packetRssi(), LoRa.packetSnr());
    }
  }

  // Non-blocking so the radio keeps listening while the LED is lit.
  if (sosLedOn && millis() - sosLedSince >= sosLedDuration) {
    digitalWrite(sosLed, LOW);
    sosLedOn = false;
  }

  // Debounce: act only after the reading has held steady for debounceMs, and
  // send on the press edge so holding the button sends just once.
  const int reading = digitalRead(sosButton);
  if (reading != buttonReading) {
    buttonReading = reading;
    buttonChangedAt = millis();
  }
  if (millis() - buttonChangedAt >= debounceMs && reading != buttonStable) {
    buttonStable = reading;
    if (buttonTestMode) {
      // LED follows the button; no radio activity.
      Serial.println(buttonStable == LOW ? "Button test: pressed (LED on)" : "Button test: released (LED off)");
      digitalWrite(sosLed, buttonStable == LOW ? HIGH : LOW);
    } else if (buttonStable == LOW) {
      Serial.println("SOS button pressed.");
      sendPacket(sosPrefix);
    }
  }

  if (Serial.available()) {
    char command = Serial.read();
    if (command == 't' || command == 'T') {
      sendPacket("RFM96W test");
    } else if (command == 'l' || command == 'L') {
      Serial.println("LED check: GPIO21 LED on for 1 second.");
      lightLed(1000);
    } else if (command == 'b' || command == 'B') {
      buttonTestMode = !buttonTestMode;
      sosLedOn = false;
      digitalWrite(sosLed, LOW);
      Serial.println(buttonTestMode
                         ? "Button test mode ON: press the button; LED follows it. No SOS is sent. Type b to exit."
                         : "Button test mode OFF: button sends SOS again.");
    }
  }
  delay(1);
}
