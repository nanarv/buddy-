#pragma once

#include <stdint.h>

// Shared by src/beeper_test.cpp (makes the tone) and src/listener_test.cpp
// (listens for it) so both sides always use the same frequency. Passive
// buzzers are loudest at their resonance (often 2-4 kHz, see the datasheet);
// change it here and re-upload both boards.
constexpr uint32_t kBeepToneHz = 2700;
