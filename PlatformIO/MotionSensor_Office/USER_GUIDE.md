# MotionSensor_Office — LD2420 setup and room tuning

Device ID: **1013**. Router hostname: **ESP-1013**. Firmware: **MotionSensor_Office_1.0.2**.

## Current verification status

Firmware is built for the ESP-12E / NodeMCU 4 MB configuration. The Android editor and server are implemented. No real LD2420 has been connected yet: wiring, acknowledgements, live energy, presence detection and power-cycle retention must be verified on the bench before ceiling installation. UI test screenshots contain example thresholds only; the real app never invents live readings.

This initial driver targets **LD2420 firmware 1.5.4 or newer**, which supports energy reports. It uses 115200 baud and the OT1 serial output. Earlier modules have different baud/pin assignments and may lack energy reports; identify the module revision before wiring. Do not assume LD2410 instructions apply.

## 1. Connect the hardware on the bench

Use a regulated **3.3 V** supply with sufficient headroom for the ESP's Wi-Fi current bursts and the radar. Do not connect 5 V logic to either device. With a bare ESP-12E, provide its normal regulator, decoupling, EN/reset and boot-strapping resistors. A NodeMCU board already provides most of this. Verify your LD2420 module markings and supply specification before powering it.

This project uses the ESP8266's **hardware UART0 with swapped pins**:

| LD2420 (firmware 1.5.4+) | ESP-12E / NodeMCU |
| --- | --- |
| 3V3 | Regulated 3.3 V |
| GND | GND |
| OT1 (radar TX) | GPIO13 / D7 (ESP RX) |
| RX (radar RX) | GPIO15 / D8 (ESP TX) |
| OT2 | Unconnected |

GPIO15 must remain LOW during boot. Preserve its pull-down (normally 10 kΩ); if your sensor/carrier pulls it high, resolve that before using this pin arrangement. Keep leads short. USB flashing remains on GPIO1/GPIO3. During normal operation those pins are no longer the radar UART.

Library logging is redirected to **Serial1 TX on GPIO2 / D4** for this project. An optional 3.3 V USB-UART adapter can read GPIO2 at 115200 baud. Do not let external hardware pull GPIO2 LOW at boot. The normal USB serial monitor will not show application logging after UART swap. Other existing projects keep their logging unchanged.

## 2. Build and flash

1. Open `PlatformIO/MotionSensor_Office` in VS Code with PlatformIO.
2. Review the network arguments in `src/main.cpp` and Pi address in `src/SmartSnapHubConfig.h`; these use your existing local network configuration. Do not share the source file containing Wi-Fi credentials.
3. The selected environment is `nodemcuv2` (ESP-12E with 4 MB flash). Verify actual flash capacity before flashing a bare module with another capacity.
4. Run **Build**, then **Upload** over USB for the initial installation.
5. Firmware output: `.pio/build/nodemcuv2/firmware.bin`.
6. Later builds can use your existing firmware upload page and OTA workflow. Use device ID 1013; no sensor tuning requires rebuilding the firmware.

Existing saved SmartSnap provisioning takes precedence over sketch defaults. If reusing an already provisioned ESP, use the existing reassignment/OTA process to assign **1013**; changing the source ID alone may not replace saved identity.

The radar is read at startup. No default thresholds are automatically written. If it is not ready or connected, the ESP still exposes the configuration endpoint; connect it and use **Read from sensor** to retry.

## 3. Open the app editor

1. Run the updated Android app from Android Studio.
2. Open **Devices → MotionSensor_Office → Presence settings (LD2420)**.
3. The panel appears only after firmware declares the LD2420 capability and connects to the updated server. The device name is irrelevant. Flash version 1.0.2 or later to advertise the capability.
4. Tap **Read from sensor**. The Pi, ESP and radar must be online. Missing wiring, an offline hub or unsupported firmware produces an error, not substitute values.
5. Current range, delay and all 16 pairs of thresholds are loaded from the sensor.

Device-edit permission is required. Configuration is relayed through authenticated Heroku/Pi connections; there is no separate public ESP web server or router port-forwarding requirement. The sensor continues operating locally when the app closes. Live tuning polling stops when the settings screen is in the background.

## 4. Controls

- **Minimum / maximum gate:** active detection window, numbered **0–15** (16 gates). These are gate indices, not metres. Distance is measured from the radar, which matters on a ceiling.
- **Disappearance delay (s):** how long absence must persist before reporting no presence.
- **Selected gate:** choose with −/+ or tap a bar on the chart.
- **Move / trigger threshold:** energy needed to initially trigger presence for that gate. Higher is less sensitive.
- **Still / maintain threshold:** energy used to maintain presence from smaller movements. Higher is less sensitive.
- Threshold −/+ changes by 10. Tap the numeric value for exact entry. Gate and delay controls change by 1.
- **View all 16 gates:** shows each gate's two values; select a row to edit it.
- **Copy to all active gates:** copies the selected gate's pair into the current min/max range after confirmation. This changes only the draft until you save. Avoid this when gate-specific tuning is already useful.
- **Apply & Save:** confirms the operation, writes all settings, reads them back and compares them, then exits configuration mode. The LD2420 saves configuration when leaving configuration mode. Confirm power-cycle retention on your module.
- **Read from sensor:** discards drafts after confirmation and reloads actual settings.
- **Cancel / Back:** asks before discarding unsaved edits. Settings already applied to the radar are not undone by Cancel.

The chart uses purple signal bars, red move thresholds and blue still thresholds. Threshold lines show **your draft**, including unsaved edits, and can differ at each gate. Gate 0/1's very high thresholds can dominate the vertical scale. No live data means no fresh energy report within three seconds; an old reading is not labelled live. Network polling is approximately once per second, so very short peaks can be missed.

A timeout is not proof that nothing changed: writes may have partially applied. **Read from sensor before retrying**. Do not repeatedly tap Save. Configuration commands are not queued for offline delivery.

## 5. Add dashboard values

For device 1013, create these variables in the app's existing variable/datastream setup if they do not exist yet:

| Variable | Type | Meaning |
| --- | --- | --- |
| `Presence` | `int` | 1 detected, 0 no presence |
| `Distance` | `int` | Radar-reported target distance in centimetres |

Bind display widgets to them. The firmware publishes both values once after connection and whenever presence changes. Unchanged values are not sent periodically. Distance therefore represents the last reported snapshot, not continuous tracking. The device also answers explicit app reads with a fresh value. Optional distance reporting can be enabled with DISTANCE_REPORT_INTERVAL_MS in main.cpp (default 0/off); it then requires a change of at least DISTANCE_CHANGE_CM. If the radar stops reporting, the last stored values may remain visible; check the tuning panel for freshness rather than treating a retained Presence value as a new reading. All gate energies are fetched on demand for tuning and are not continually stored in the database.

## 6. Tune a new room

### Establish a baseline

1. Verify operation on the bench, then mount in the intended position and orientation. Do not finish concealing it until tuning is complete.
2. Read and record the original settings (screenshots of all gates and range/delay are sufficient).
3. Leave the room empty for several minutes while watching the energy chart from outside. Test normal sources of interference: fan, HVAC, moving curtains and activity beyond the room boundary.
4. Note which gates have stable background energy and which show spikes. Avoid copying an arbitrary universal threshold across all gates.

### Choose the detection window

1. Walk through the areas that should be detected and watch which gates respond.
2. Set the minimum and maximum to cover those locations while excluding unwanted near/far areas where practical.
3. With a ceiling sensor, include the distance down to seated occupants as well as people standing or walking. Gate range is not a wall-to-wall room measurement, and it does not select an angular zone.
4. Apply, then walk just outside the intended room to test unwanted detection.

### Tune movement detection

1. Start from the module's existing move thresholds.
2. If entry/walking is missed, lower the move threshold gradually only for the relevant gates.
3. If the empty room triggers, raise thresholds for the gates producing false triggers or narrow the active range.
4. Apply one small group of changes, then repeat the same walk test. Change one type of parameter at a time so you can identify the effect.

### Tune seated/still presence

1. Enter the room to trigger detection, then sit at your desk with minimal movement for several minutes.
2. If presence drops out while you remain seated, gradually lower still thresholds in the relevant gates.
3. If presence remains falsely active after leaving, compare empty-room energy against the still thresholds and raise the affected gates as needed.
4. Still thresholds will generally be lower than move thresholds, but base the choice on measured noise and actual presence tests. Tiny motions and reflections vary with mounting and room layout.

### Choose the delay last

After threshold tuning is reasonably stable, select a delay suitable for your use. A longer delay can bridge brief quiet periods but delays switching off after departure. It should not hide persistent false detections. A trial such as 30 seconds is a tuning choice, not a sensor default or a universal recommendation.

### Validate and keep a record

Test walking in/out, sitting still, different seats, room corners, door open/closed, fan/HVAC states and adjacent-room motion. Repeat under normal daytime/night-time conditions. Power-cycle the ESP and radar, read the settings again and compare them with what you saved. Keep a record of the final 16 threshold pairs, range and delay so they can be restored later.

## References and compatibility

Protocol/register layout and version/pin distinctions were checked against the primary ESPHome LD2420 implementation and documentation:

- https://esphome.io/components/sensor/ld2420/
- https://github.com/esphome/esphome/tree/dev/esphome/components/ld2420

This is a standalone SmartSnap driver, not an ESPHome firmware replacement. Live-energy compatibility with the actual module, tuning quality and persistent storage remain hardware acceptance tests.

## Traffic in version 1.0.1

Routine reports are event-driven. The tuning panel still polls approximately once per second while open, and that stops when it is left/backgrounded. UART traffic between radar and ESP continues locally; it is not all sent over Wi-Fi. Socket connection keepalives and requested snapshots still use some traffic. Flash the new firmware for the reporting change to take effect.

## Declaring device capabilities

The available typed constants are declared in `PlatformIO/shared/SmartSnap/src/SmartSnapAPI.h` (the SmartSnap library header used by these projects). Declare supported features before `Initialize()`:

```cpp
smartSnap.SetCapability(SmartSnap::Capability::LD2420);
```

Use `SetCapability(SmartSnap::Capability::LD2420, false)` before initialization to remove it. Calls are idempotent. Declarations are advertised on every connection, not immediately on a call made after connection. They are stored by the Pi and mirrored to Heroku, then cached by the app. A reconnect without declarations clears previously advertised capabilities. To add another supported capability later, extend the enum and serialization mapping in the library and implement its app UI. The server accepts bounded future capability keys without requiring a new schema. Capabilities control UI availability; they do not grant account permissions. Existing device-edit permission is still required.


## Last detection snapshots (firmware 1.0.3)

Install the updated Android app and flash MotionSensor_Office_1.0.3.
No server change is required: the existing authenticated presence relay passes
snapshots through with tuning responses.

1. Read the sensor settings, then leave the detection area until presence clears
   (including the configured disappearance delay).
2. Enter from the boundary you want to test.
3. In Presence Settings, tap **Last detection**. The chart freezes on the first
   reported absent-to-present frame. Select a gate to see its energy at that time.
4. Candidate gates list their nearby peak energy and the saved trigger threshold.
   Peaks can differ from the chart: they include up to 1.5 seconds before the
   transition, limited to the last 16 UART frames. Multiple or no candidates are
   possible; the sensor does not identify its actual triggering gate.
5. Tap **Live** to resume the current chart. Adjust maximum gate or thresholds,
   Apply & Save, and repeat the leave/re-enter test.

The frozen chart uses thresholds saved at capture time, not your current draft.
Snapshots are captured locally even with the phone closed. Only the most recent
snapshot is retained in RAM and it disappears when the ESP restarts. No extra
continuous network messages or flash writes are added. Startup, stale-stream
recovery and configuration changes must observe a new absence before recording
another transition; existing presence is not treated as a new detection.
