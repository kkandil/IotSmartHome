#pragma once
#include <math.h>

// Track the rounded value actually passed to WriteVariableValue, independently
// of the full-report timer. Invalid samples must not erase the last good report.
struct ReportBaseline {
    float last = NAN;
    bool exceeds(float value, float threshold) const {
        return isfinite(value) && isfinite(last) && fabsf(value - last) > threshold;
    }
    void reported(float value) {
        if (isfinite(value)) last = value;
    }
};

// Below is strictly < threshold; equality belongs to the at/above side.
// Invalid samples leave the previous side intact.
struct LightThresholdState {
    bool initialized = false;
    bool below = false;
    bool update(float value, float threshold) {
        if (!isfinite(value) || !isfinite(threshold)) return false;
        bool nextBelow = value < threshold;
        bool crossed = initialized ? nextBelow != below : nextBelow;
        below = nextBelow;
        initialized = true;
        return crossed;
    }
};
