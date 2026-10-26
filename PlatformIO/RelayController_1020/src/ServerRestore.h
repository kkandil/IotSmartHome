#pragma once
#include <stdint.h>

struct ServerRestore {
    bool waiting[3] = {false, false, false};
    uint32_t lastRequest = 0;
    void connect(bool enabled, uint32_t now) {
        for (bool& value : waiting) value = enabled;
        lastRequest = now;
    }
    void accept(uint8_t relay) { if (relay < 3) waiting[relay] = false; }
    bool retryDue(uint32_t now) {
        if (uint32_t(now - lastRequest) < 3000) return false;
        lastRequest = now;
        return waiting[0] || waiting[1] || waiting[2];
    }
};
