#pragma once
#include <stdint.h>

// Q0/Q1 = relay 1 RESET/SET, Q2/Q3 = relay 2, Q4/Q5 = relay 3.
constexpr uint8_t coilMask(uint8_t relay, bool on) {
    return relay < 3 ? uint8_t(1U << (2 * relay + (on ? 1 : 0))) : 0;
}

struct SwitchInput {
    bool raw = true;
    bool stable = true;
    uint32_t changedAt = 0;

    void begin(bool level, uint32_t now) {
        raw = stable = level;
        changedAt = now;
    }

    bool update(bool level, uint32_t now, uint32_t debounce, bool momentary) {
        if (level != raw) {
            raw = level;
            changedAt = now;
        }
        if (raw == stable || uint32_t(now - changedAt) < debounce) return false;
        stable = raw;
        return !momentary || !stable;
    }
};
