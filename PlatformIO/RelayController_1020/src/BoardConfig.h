#pragma once
#include <stdint.h>

// Verified against IOT.kicad_pcb and the ESP-12E schematic symbol.
constexpr uint8_t DATA_PIN = 14;
constexpr uint8_t CLOCK_PIN = 13;
constexpr uint8_t LATCH_PIN = 12;
constexpr uint8_t OE_PIN = 3; // Active LOW; R19 pulls HIGH when the ESP resets.
constexpr uint8_t SWITCH_PINS[] = {4, 5, 16};
constexpr uint32_t DEBOUNCE_MS = 40;
constexpr uint32_t COIL_PULSE_MS = 120; // HF3F-L minimum specified pulse: 100 ms.
constexpr bool MOMENTARY_SWITCHES = false; // Maintained: toggle on both edges.
// Opt in to server values after EVERY reconnect, not only a power cycle.
// Set false to keep the selected local startup policy without requesting values.
constexpr bool REQUEST_SERVER_STATE_ON_CONNECT = true;
