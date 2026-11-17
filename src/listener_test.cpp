/*
  Listener: piezo as a contact microphone / vibration sensor
  Board: Adafruit ESP32 Feather (HUZZAH32), signal on GPIO 27
  Framework: Arduino (PlatformIO), environment `listener_test`

  WIRING (biased to mid-rail so we capture both + and - swings):
      3V ──[100kΩ]──┬──[100kΩ]── GND
                    │
                    ├──[1kΩ]── GPIO 27     (1k = optional spike protection)
                    │
               piezo (+ / red)
               piezo (- / black) ── GND

  ⚠ GPIO 27 is ADC2 — it does not work while WiFi is on.
    For WiFi projects use an ADC1 pin: 32, 33, 34, 36, or 39.

  TONE DETECTION: besides broadband rms/peak (knocks), each window runs a
  Goertzel filter at kBeepToneHz, the frequency beeper_test.cpp plays. Looking
  at just that one frequency rejects most ADC/broadband noise, so a faint
  over-the-air beep can be detected even when it barely moves `peak`.
  Samples are paced at kSampleHz so the frequency estimate is valid; `late`
  counts samples that could not keep that pace (the window's tone value is then
  less trustworthy).

  Output per window:
    rms:<counts> peak:<counts> tone:<counts> floor:<counts> late:<n> beep:<0|1>
  tone  = amplitude of the kBeepToneHz component (12-bit ADC counts)
  floor = slowly tracked tone level while no beep is detected
  beep  = 1 when tone > kToneRatio * floor (LED on)
*/

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "BeepTest.h"

namespace {

constexpr int      kPiezoPin       = 27;
constexpr uint32_t kSampleHz       = 10000;  // > 2x tone; analogRead keeps up at this rate
constexpr int      kWindow         = 256;    // 25.6 ms per measurement at 10 kHz
constexpr int      kKnockThreshold = 150;    // peak deviation (12-bit ADC counts)
constexpr float    kBaselineAlpha  = 0.001f; // how fast the DC baseline tracks
constexpr bool     kPlotRaw        = false;  // true = raw waveform (slows sampling; see late)

constexpr float    kToneRatio      = 4.0f;   // detect when tone is 4x (~12 dB) above floor
constexpr float    kMinFloor       = 0.2f;   // counts; stops a near-zero floor causing false hits
constexpr float    kFloorAlpha     = 0.05f;  // floor tracking speed (quiet windows only)
constexpr int      kFloorWarmup    = 20;     // quiet windows used to learn the initial floor

class PiezoMic {
public:
    explicit PiezoMic(int pin) : pin_(pin) {}

    void begin() {
        analogReadResolution(12);                   // 0–4095
        analogSetPinAttenuation(pin_, ADC_11db);    // ~0–3.3 V range

        long sum = 0;
        for (int i = 0; i < 512; ++i) sum += analogRead(pin_);
        baseline_ = static_cast<float>(sum) / 512.0f;
    }

    // Reads one sample with the DC bias removed
    int readAC() {
        const int raw = analogRead(pin_);
        baseline_ += (raw - baseline_) * kBaselineAlpha;
        return raw - static_cast<int>(baseline_);
    }

    struct Level {
        float rms;
        int   peak;
        float tone;   // amplitude at toneHz, ADC counts
        int   late;   // samples that missed their scheduled time
    };

    // Measures loudness and the tone component over a window of paced samples
    Level measure(int samples, float toneHz) {
        // Goertzel: single-frequency DFT bin, cheap enough to run per sample.
        const float coeff = 2.0f * std::cos(2.0f * PI * toneHz / kSampleHz);
        float s1 = 0.0f, s2 = 0.0f;

        double sumSq = 0.0;
        int    peak  = 0;
        int    late  = 0;

        const uint32_t periodUs = 1000000UL / kSampleHz;
        uint32_t next = micros();

        for (int i = 0; i < samples; ++i) {
            // Pace samples; if we're already past the slot, count it and resync.
            const uint32_t now = micros();
            if (static_cast<int32_t>(now - next) > static_cast<int32_t>(periodUs)) {
                ++late;
                next = now;
            }
            while (static_cast<int32_t>(micros() - next) < 0) {}
            next += periodUs;

            const int ac = readAC();
            if (kPlotRaw) Serial.println(ac);

            sumSq += static_cast<double>(ac) * ac;
            peak = std::max(peak, std::abs(ac));

            const float s0 = ac + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        const float tone  = 2.0f * std::sqrt(std::max(power, 0.0f)) / samples;
        return { static_cast<float>(std::sqrt(sumSq / samples)), peak, tone, late };
    }

private:
    int   pin_;
    float baseline_ = 2048.0f;
};

PiezoMic mic(kPiezoPin);
float    toneFloor   = 0.0f;
int      quietSeen   = 0;

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    mic.begin();
}

void loop() {
    const auto level = mic.measure(kWindow, static_cast<float>(kBeepToneHz));

    // Learn the floor from the first windows (keep the beeper quiet or start
    // it after the listener), then only update it while no beep is detected.
    bool beep = false;
    if (quietSeen < kFloorWarmup) {
        toneFloor += (level.tone - toneFloor) / static_cast<float>(++quietSeen);
    } else {
        const float floor = std::max(toneFloor, kMinFloor);
        beep = level.tone > kToneRatio * floor;
        if (!beep) toneFloor += (level.tone - toneFloor) * kFloorAlpha;
    }

    if (!kPlotRaw) {
        Serial.printf("rms:%.1f peak:%d tone:%.2f floor:%.2f late:%d beep:%d%s\n",
                      level.rms, level.peak, level.tone, toneFloor, level.late,
                      beep ? 1 : 0, quietSeen < kFloorWarmup ? " warmup" : "");
    }

    const bool knock = level.peak > kKnockThreshold;
    digitalWrite(LED_BUILTIN, (beep || knock) ? HIGH : LOW);
}
