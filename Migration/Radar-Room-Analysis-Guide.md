# LD2420 room analysis

## Install

1. Install `SmartHome-radar-room-analysis-debug.apk` on the phone.
2. Flash `MultiSens_1.4.4-room-analysis.bin` to MultiSens using the normal firmware update procedure.
3. For the standalone MotionSensor_Office, use `MotionSensor_Office_1.0.6-room-analysis.bin` instead. Never swap these firmware files.
4. No Pi or Heroku deployment is required. Existing device IDs and sensor thresholds are unchanged by this update.

## Use

Open Devices, long-press the device, open Presence settings, then scroll to **Room analysis**.

- **Recent peak:** selected gate's maximum energy from the device's rolling four-second UART-frame window. This captures spikes between phone updates.
- **Possible maintaining gates:** active gates whose recent peak reaches their saved still threshold. These are diagnostic candidates, not a trigger gate reported by the radar.
- **Below-threshold time:** time observed by the firmware with all active gates below their saved still thresholds. Compare it with the saved disappearance delay. It is an estimate, not the sensor's internal countdown.
- Analysis always uses settings read from the sensor. Unsaved chart thresholds are not used for this comparison. Analysis remains live when the chart shows Last detection.

### Compare the room

1. Leave the room empty and tap **Empty room**, then Start. Keep it empty for the test. There are five seconds of settling and sixty seconds of measurement.
2. Stand at a location where you need reliable detection. Tap **Standing still**, then Start, and remain still for the test.
3. Compare every active gate's empty-room peak with its occupied minimum/maximum.
4. **Overlap** means the empty peak reaches the observed standing-still minimum. Raising the threshold enough to reject noise may lose stationary detection. This is a conservative diagnostic; normal energy variations can cause overlap even in a workable setup.
5. **Separated in this test** is useful evidence, not a guarantee. Repeat at other positions and over longer periods before settling on thresholds.

No thresholds are suggested or applied automatically. Use the existing gate editor and Apply & Save only after deciding what to change.

If a higher still threshold misses you, do not keep raising it. Compare other active gates and test sensor position/orientation and room noise sources. Excluding a noisy gate can also remove useful coverage, so verify the whole intended area.

## Measurement limits

Measurements use valid radar frames received by the ESP, not an authoritative trace of the radar's internal decision. A missing UART frame cannot be recovered. The rolling window is approximately 3.5-4 seconds because it uses half-second buckets. Readout requests do not reset it, allowing multiple viewers. Measurements stop on stale data, interrupted phone updates, leaving the screen, or reading/saving settings. Completed results are session-only; reopening the screen starts fresh. A saved-settings change clears comparisons.

There are no additional unsolicited network messages, periodic flash writes, or automatic threshold changes. Statistics occupy bounded RAM and travel in existing tuning responses only. Older firmware retains the normal chart but needs updating for Room analysis.

## Validation

- MultiSens and MotionSensor_Office PlatformIO builds pass.
- Java tests cover warmup exclusion, peak accumulation, overlapping/separated data, update gaps, cancellation and repeating a measurement.
- Native C++ tests cover short spikes, minima, multiple readers, expiration, reset and clock wrap.
- Android emulator tests cover both themes, saved-versus-draft analysis, overlap display, frozen snapshots and existing editor interactions.

The new firmware has been built, not flashed to the physical devices during this change. Validate the two room measurements after installing it.
