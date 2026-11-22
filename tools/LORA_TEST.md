# Standalone RFM96W test

`src/lora_test.cpp` is isolated in the `lora_test` PlatformIO environment.
It targets the Adafruit HUZZAH32 ESP32 Feather shown in the wiring photo,
using its labeled SPI pins (`featheresp32` board configuration).
The existing default environment and sensor/receiver firmwares are unchanged.

> **⚠️ Board 1 has a dead SCK pad.** On the first test Feather, the pad labelled
> SCK (GPIO5) no longer connects to the ESP32. Its radio SCK wire goes to
> **A0 / GPIO26**. Flash board 1 with `make upload-lora-test-board1`, which builds
> this sketch with SCK on GPIO26 and G0 on GPIO25. Don't move SCK back to the
> SCK pad. See `OPERATIONS.md`.

Power off before wiring. Use these GPIO numbers, not physical pin positions:

| RFM96W | ESP32 |
| --- | --- |
| VIN | 3V3 |
| GND | GND |
| RST | GPIO27 |
| CS | GPIO33 |
| MOSI | MOSI / GPIO18 |
| MISO | MISO / GPIO19 |
| SCK | SCK / GPIO5 (board 1: A0 / GPIO26, see below) |
| G0 / DIO0 | GPIO15 (board 1: A1 / GPIO25) |
| SOS button (other leg to GND) | GPIO14 |
| SOS LED (+ via 220–330 Ω, − to GND) | GPIO21 |

## ESP32 Lite version

`src/lora_test_lite.cpp` (environment `lora_test_lite`, board `lolin32_lite`) is
the same test for the ESP32 Lite (Wemos/LOLIN32 Lite). Radio settings match,
so a Feather and a Lite running these tests can talk to each other.

| RFM96W | ESP32 Lite |
| --- | --- |
| VIN | 3V3 |
| GND | GND |
| RST | GPIO27 |
| CS | GPIO33 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| SCK | GPIO18 |
| G0 / DIO0 | GPIO25 |
| SOS button (other leg to GND) | GPIO14 |
| SOS LED | onboard LED on GPIO22 (active LOW, no wiring) |

Upload with `make upload-lora-test-lite PORT=<lite-port>`.

Leave EN and G1–G5 disconnected. Attach a 433 MHz antenna before transmitting.
G0/DIO0 uses GPIO15 to match the CAD reference. This test polls the radio
over SPI, so successful initialization does not verify the G0 connection.
If external wiring prevents boot, disconnect power before isolating connections.

**Board 1 build.** Board 1 is wired differently: SCK on A0 / GPIO26 (its SCK
pad is dead) and G0 on A1 / GPIO25 (moved off strapping pin GPIO12; its G0 was
verified on 2026-11-21). Flash it with `make upload-lora-test-board1 PORT=...`
(environment `lora_test_board1`, same source with `-DLORA_SCK=26 -DLORA_DIO0=25`).
Do not flash plain `lora_test` to board 1.
Disconnect any vibration module from GPIO27 before using that pin for radio reset.

Build without uploading:

```sh
pio run -e lora_test
```

When ready to replace the firmware on the physically identified test board,
close other serial monitors/recorders and run these commands using its actual port:

```sh
make upload-lora-test PORT=<test-board-port>
make monitor PORT=<test-board-port>
```

These wrap `pio run -e lora_test -t upload --upload-port <port>` and
`pio device monitor -p <port> -b 115200`. Use `make monitor`, not `make view`:
`view` only reads, so it cannot send the `t` key. Press Ctrl+C to exit the monitor.

Use your installed PlatformIO executable if `pio` is not on PATH. Reset the
board after opening the monitor to see startup output. `PASS` means the radio
initialized over SPI; failure suggests checking wiring and power.

The test listens by default. Type a word or sentence and press Enter to send it at 5 dBm.
For example, `hello` + Enter appears as `RX 5 bytes: hello` on the other board.
Both CR and LF line endings work. Blank lines are ignored. Messages over 255
bytes are rejected in full; nothing is sent. Backspace edits the current line
until overflow; after overflow, press Enter to discard and start again.
A line that is exactly `t`, `l` or `b` (any case) is a command, not text:
`t` + Enter sends a numbered test packet; there is no automatic repeat.

Local button/LED check (no radio needed): at startup the GPIO21 LED blinks
3 times and the serial output reports whether the button reads released (OK)
or pressed (likely miswired). In `make monitor`, type `l` + Enter to light the LED
for 1 second, or `b` + Enter to toggle button test mode, where each press/release is
printed and the LED follows the button without sending anything. These checks
still work if radio initialization fails.

SOS button: wire a push button between GPIO14 and GND (internal pull-up, no
resistor needed). Each press sends one `SOS #<n>` packet. A receiving board
prints any packet starting with `SOS` (including typed text such as `SOS help`)
as `!!! SOS RECEIVED !!! (<text>)` with RSSI/SNR and lights the LED on GPIO21
for 5 seconds (another SOS restarts the 5 seconds); other packets still print as
`RX ...`. There is no acknowledgement or retry, so the sender cannot tell whether
the SOS arrived. TX completion alone does not prove reception.
For an over-the-air test, use a second ESP32/RFM96W running this sketch with
its own matching wiring. After updating this firmware, re-upload to each board that will send typed text.
Send from one board and look for `RX` plus the matching
packet number on the other, then test the reverse direction. Both use 433 MHz,
SF7, 125 kHz bandwidth, coding rate 4/5, sync word 0x12 and CRC enabled.

Library API: <https://github.com/sandeepmistry/arduino-LoRa/blob/master/API.md>
Radio guide: <https://learn.adafruit.com/adafruit-rfm69hcw-and-rfm96-rfm95-rfm98-lora-packet-padio-breakouts>
