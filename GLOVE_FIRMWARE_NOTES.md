# Glove firmware notes

Requirements and lessons from LoRa glove hardware bring-up (Feather board 1 and
the ESP32 Lite, November 2026). Read this before writing the combined glove
firmware. Per `AGENTS.md`, discuss firmware and packet-format choices with the
user before implementing them.

## Radio and packets

- **Use the G0 (DIO0) interrupt, not polling.** Polling `parsePacket()` keeps the
  CPU busy and prevents sleep. G0 goes HIGH on RxDone/TxDone; attach an interrupt
  to it, set a flag in the ISR, and do the SPI reads in the main loop (not in the
  ISR). G0 was verified on both boards on 2026-11-21: HIGH on every packet once
  read properly, and 0 false HIGHs in about 170,000 idle reads.
- **Validate every packet** before acting on it: magic byte or protocol ID,
  sender ID, sequence number, message type and a checksum. A garbage packet
  (RSSI -95, SNR -12) was received during testing. Sync word `0x12` is the
  library default and is shared by other LoRa devices.
- **SOS needs an acknowledgement and retries.** "TX complete" only means the
  packet left the radio. Resend SOS until the buddy acknowledges it.
- **Sequence numbers** to detect lost packets, plus a **link-lost timeout** so
  silence becomes a warning, not "all fine".
- **Avoid collisions.** The radio is half-duplex. Two boards on fixed 2.0 s and
  2.3 s timers collided and lost a packet (Lite #35). Add random jitter to send
  timing; acknowledgements cover the rest.
- Keep settings matched on both boards: 433 MHz, SF7, 125 kHz, CR 4/5, CRC on.
  5 dBm is a desk setting; raise it for range within local legal limits.
- **433 MHz barely propagates in water** (centimetres in saltwater). LoRa works
  at the surface, not between submerged divers. Underwater links likely need
  acoustics (see the hydrophone idea).

## Pins

| Function | Feather (board 1) | ESP32 Lite |
| --- | --- | --- |
| Radio SCK | **GPIO26 / A0** (SCK pad is dead) | GPIO18 |
| Radio MOSI | GPIO18 | GPIO23 |
| Radio MISO | GPIO19 | GPIO19 |
| Radio CS | GPIO33 | GPIO33 |
| Radio RST | GPIO27 | GPIO27 |
| Radio G0 / DIO0 | **GPIO25 / A1** (moved off GPIO12) | GPIO25 |
| MAX30102 SDA / SCL | GPIO23 / GPIO22 | GPIO16 / GPIO17 |
| Buzzer (active) | GPIO32 | GPIO32 |
| SOS input (pull-up, to GND) | GPIO14 | GPIO14 |
| LED | GPIO21 (external) | GPIO22 onboard, **active LOW** |

Board 1 differs from the team CAD wiring for the Feather (SCK GPIO5, G0 GPIO15);
the shared `lora_test` uses the CAD pins. GPIO15 is also a strapping pin (LOW at
reset silences boot messages), which is milder than GPIO12 but worth checking.

- Avoid boot-strapping pins 0, 2, 12 and 15 for anything with a pull-up or a
  signal that can be HIGH at reset. GPIO2 as SDA blocked flashing; G0 on GPIO12
  could stop the Feather from booting.
- The old `sender.cpp` uses `Wire.begin(23, 22)` and holds GPIO27 LOW, which
  would hold the radio in reset. Do not carry that over; the motor is on GPIO32.

## Heart-rate sensor (MAX30102)

- Use the sender's SpO2 + heart-rate algorithm (25 samples/s, 100-sample window),
  not the simple beat counter from the tests (it jumps between ~60 and ~160).
- **Handle I2C errors.** A loose wire produced `Error 263` timeouts and IR=0.
  Detect sensor dropout, re-initialise, and report "no reading" rather than stale
  values.
- Contact detection: IR above about 50,000 with a finger. "No contact" must never
  look like a normal reading.
- Not validated against a medical reference: experimental, not a safety device.

## Alarm and buzzer

- Active buzzer: GPIO32 HIGH = beep. No tone generation needed.
- Reuse `SpO2Alert.h` behaviour: OFF for invalid data or packets older than 2 s.
  The 98% threshold is a testing value.
- Distinct beep patterns for: SOS sent, SOS acknowledged, buddy SOS received,
  low SpO2, link lost.

## SOS input

- **Tap detection on IR spikes**: 3 sharp spikes within about 1 s; ignore slow
  pressure changes. Desk test on 2026-11-21 detected 3/3 triple-taps (spikes of
  1,000-2,000 IR units vs noise of 10-50) and ignored one slow press. **Not yet
  tested against normal hand movement**, so the false-alarm rate is unknown.
- Alternative or fallback: button or magnetic reed switch / Hall sensor on
  GPIO14 with debounce and a long-press or pattern. Capacitive touch and exposed
  contacts do not work in seawater.
- Decide whether SOS latches until acknowledged and how a diver cancels a false
  alarm.
