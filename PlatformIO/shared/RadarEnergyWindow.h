#pragma once
#include <stdint.h>
#include <string.h>

// Four seconds of UART-frame extrema. Reading does not reset the window,
// so multiple tuning clients cannot consume each other's measurements.
class RadarEnergyWindow {
    struct Bucket {
        uint32_t start = 0, count = 0;
        uint16_t peak[16] = {}, low[16] = {};
    } buckets[8];
public:
    void clear() { for (auto &b : buckets) b = Bucket(); }
    void add(uint32_t now, const uint16_t *energy) {
        const uint32_t start = now - now % 500;
        Bucket &b = buckets[(now / 500) % 8];
        if (!b.count || b.start != start) {
            b = Bucket(); b.start = start;
            memcpy(b.low, energy, sizeof(b.low));
        }
        ++b.count;
        for (int g = 0; g < 16; ++g) {
            if (energy[g] > b.peak[g]) b.peak[g] = energy[g];
            if (energy[g] < b.low[g]) b.low[g] = energy[g];
        }
    }
    uint32_t read(uint32_t now, uint16_t *peak, uint16_t *low, uint32_t &span) const {
        uint32_t count = 0; span = 0;
        for (int g = 0; g < 16; ++g) { peak[g] = 0; low[g] = 65535; }
        for (const auto &b : buckets) {
            if (!b.count || uint32_t(now - b.start) >= 4000) continue;
            count += b.count;
            if (uint32_t(now - b.start) > span) span = now - b.start;
            for (int g = 0; g < 16; ++g) {
                if (b.peak[g] > peak[g]) peak[g] = b.peak[g];
                if (b.low[g] < low[g]) low[g] = b.low[g];
            }
        }
        return count;
    }
};
