#include "../src/ReportBaseline.h"
#include <assert.h>
#include <stdio.h>
int main() {
    LightThresholdState threshold;
    assert(!threshold.update(20, 13));
    assert(threshold.update(12, 13)); // Enter below: report once.
    assert(!threshold.update(11, 13));
    assert(!threshold.update(2, 13)); // No repeated low-light reports.
    assert(!threshold.update(NAN, 13));
    assert(threshold.update(13, 13)); // Return to at/above: report once.
    assert(!threshold.update(14, 13));
    assert(!threshold.update(20, 13));
    assert(threshold.update(12, 13)); // A second cycle still works.
    assert(threshold.update(12, 10)); // Changed config reclassifies next sample.
    LightThresholdState initiallyDark;
    assert(initiallyDark.update(2, 13));
    assert(!initiallyDark.update(3, 13));
    ReportBaseline independentDelta;
    independentDelta.reported(100);
    assert(!threshold.update(140, 10)); // Same side, but delta can still trigger.
    assert(independentDelta.exceeds(140, 30));
    ReportBaseline light;
    assert(!light.exceeds(49.15f, 30));
    light.reported(49.15f);
    assert(light.exceeds(2.72f, 30));
    light.reported(2.72f);
    assert(light.exceeds(49.15f, 30)); // Uncovering must report before the timer.
    light.reported(49.15f);
    assert(!light.exceeds(49.15f, 30));
    light.reported(10); // A low-light report also advances the baseline.
    assert(!light.exceeds(40, 30)); // Strictly greater, not equal.
    assert(light.exceeds(41, 30));
    light.reported(NAN);
    assert(light.last == 10);
    assert(!light.exceeds(NAN, 30));
    ReportBaseline temperature;
    temperature.reported(20);
    assert(temperature.exceeds(21, 0.5f));
    temperature.reported(21);
    assert(temperature.exceeds(20, 0.5f));
    assert(!temperature.exceeds(21.5f, 0.5f));
    puts("PASS: threshold crossings, repeated low/high readings, config changes, independent delta, light cover/uncover, low-light baseline, temperature reversal, strict boundary, invalid samples");
}
