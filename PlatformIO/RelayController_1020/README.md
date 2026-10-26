# RelayController_1020

ESP-12E / device ID **1020**, three maintained wall switches and three
HF3F-L-5-1ZL2T dual-coil latching relays driven through a 74HC595.
Pin mapping checked against `IOT.kicad_pcb` and the schematic at
`C:/Users/khale/OneDrive/Documents/KiCad/8.0/projects/IOT/IOT`.

## Wiring

| Signal | ESP GPIO / output |
|---|---|
| Shift DS | 14 |
| Shift clock | 13 |
| Shift latch | 12 |
| Shift OE, active low | 3 / RXD, through JP1 |
| Button1 / J6 pin 1 | 4, internal pull-up |
| Button2 / J6 pin 2 | 5, internal pull-up |
| Button3 / J6 pin 3 | 16, **external 10k pull-up required** |
| J6 pin 4 | GND |
| Relay1 RESET / SET | Q0 / Q1 |
| Relay2 RESET / SET | Q2 / Q3 |
| Relay3 RESET / SET | Q4 / Q5 |

Connect switches between their input and GND. GPIO16 has no internal pull-up;
the supplied design has no external Button3 pull-up. Add 10k from Button3 to
3.3V before using input 3. R19 already pulls OE high when disconnected/reset.

## Behavior

- Each maintained switch toggles its relay on either debounced position change.
  An app change does not require moving the wall switch to a matching position.
- Inputs are debounced for 40ms. A closed switch at boot does not count as a press.
- SET/RESET are 120ms pulses, one coil at a time. OE is disabled between pulses;
  the register is cleared afterwards. No networking or OTA is serviced inside a pulse.
- Default startup policy resets the three relays OFF sequentially before Wi-Fi.
  Firmware 3IOSwitch_1.1.0 also supports Preserve through the app (see below).
  Latching contacts can remain ON while the ESP is unpowered or held in reset;
  OE only disables coils.
- Local switches work without Wi-Fi. Firmware 1.2.0 requests each saved server
  relay value on reconnect before publishing startup state (see below).
- State reports are the commanded relay states, **not measured contact feedback**.
  Open JP1, a missing coil supply, or a hardware fault cannot be detected in firmware.
- The shared library has an opt-in background connection option. Other existing
  projects keep their original startup behavior unless they enable that option.

## App setup

Add/use device **1020** (suggested name `RelayController`) in Home_Germany.
Create three writable `int` variables: `Relay1`, `Relay2`, `Relay3`.
For each switch card select the corresponding variable, ON = `1`, OFF = `0`.
Only exact values 0 and 1 are accepted. The Pi can control the same variables.

### Startup behavior (firmware 3IOSwitch_1.1.0)

After updating the app and firmware, long-press this device in Devices, then
choose **Startup Behavior**. This appears only for devices advertising
`SmartSnap::Capability::StartupBehavior` (`startup_behavior`, with `relay_startup` retained for compatibility), never based on their name
or ID. The server enforces that capability and device-edit permission too.

- **Always start OFF** (default): reset all three contacts on each boot.
- **Preserve the previous state**: keep the physically latched contacts and
  restore their last commanded states in software, without startup pulses.

Saving does not move any relay. It requires an online device and acknowledges
only after the ESP writes and reads back its local settings. No separate app
variable is required. The setting survives power loss and normal firmware updates;
erasing flash removes it. A first boot with missing/invalid settings resets OFF.

Preserve writes a pending journal before each relay pulse and commits it after
the pulse, for both app and wall-switch changes. An interrupted operation, invalid
file, or incomplete commit triggers an OFF recovery on reboot. A storage failure
before a pulse prevents that operation rather than knowingly losing recovery
information. These are commanded states, not physical contact feedback. Flash
writes occur only when saving settings/changing state, not continuously.

## Restore server values (firmware 3IOSwitch_1.2.0)

`REQUEST_SERVER_STATE_ON_CONNECT = true` in BoardConfig.h enables calls to
`RequestVariableValueFromServer` for Relay1/Relay2/Relay3 whenever the device
connects. `onServerValue` applies each 0/1 response. Set this flag false to keep
local startup behavior without pulling the server values. Other device firmware
can opt in by making these calls and registering its own `onServerValue` handler.
No shared-library auto-restore is enabled for other devices.

The OFF/Preserve policy controls the initial state before networking. When server
restore is enabled, the saved server value takes precedence after connection,
even if it differs from the initial state or a change made while Wi-Fi was down.
Startup reports are withheld until the corresponding read completes. Unanswered
reads retry every three seconds; missing/invalid variables are not applied.
A new wall-switch action during synchronization or a live app command takes
precedence over a delayed read. Reads stop after synchronization; there is no
continuous polling.

Widget writes with Enable offline write already save on the Pi. ESP event and
schedule actions now also save values while their target is disconnected and
record `saved_offline`. The latest value replaces the earlier value; this is
state restoration, not replay of a sequence of commands. Without an explicit
firmware request, merely reconnecting does not replay these stored Pi values.
Cloud writes queued while the Pi is offline retain the existing gateway delivery
path when the Pi returns. Events/schedules still execute on the Pi and cannot
trigger while the Pi itself is powered off. Smart Life plug behavior is unchanged.

Bench test: disconnect the device, set Relay1=1 with offline writes enabled,
reconnect, and verify it applies 1 instead of overwriting it with boot-time 0.
Repeat for an event and a schedule; then save 1 followed by 0 while offline and
verify only the final state is restored. Disabling server restore should retain
the selected local startup behavior. Do not erase flash for this update if you
want to retain the saved startup policy.

## Build and flash

Open this folder in PlatformIO or the updated SmartHomeDevices.code-workspace.
The local ignored `src/ConnectionConfig.h` is already populated from the existing
MultiSens connection settings. Check it if the Wi-Fi/hub has changed. For a fresh
checkout copy ConnectionConfig.example.h and fill it in. Saved provisioning on a
previously used ESP can override these defaults; use a fresh/erased device when
reassigning its identity, with JP1 disconnected.

1. Disconnect loads for the initial bench test. Open JP1 before flashing.
2. Use a 3.3V-logic UART programmer, common GND, TX to ESP RX and RX to ESP TX.
   Power the PCB through its intended supply. GPIO0 low during reset enters the
   bootloader; release GPIO0 afterwards. Do not connect 5V UART logic to the ESP.
3. Run `pio run -e esp12e -t upload --upload-port COMx` with the actual port.
4. Power down; disconnect programmer TX from ESP RX/GPIO3; close JP1; power up.
   Leaving programmer TX connected can contend with GPIO3 while OE is driven low.
   TX-only monitoring is possible using ESP TX -> programmer RX and common GND.
5. The three relays reset OFF. Check each switch in both directions and each app
   command. Verify the intended NO/NC terminal with continuity before attaching loads.
6. Disconnect Wi-Fi, toggle each switch, reconnect, and check app state catches up.
   Reboot and confirm OFF startup. Relay operation was confirmed after repairing a PCB connection.

Firmware: `.pio/build/esp12e/firmware.bin`. No board is automatically flashed.

## Logic test

`g++ -std=c++11 -Wall -Wextra -pedantic test/logic_test.cpp -o logic-test`
then run `logic-test`. Tests cover coil masks, invalid indexes, debounce,
startup-held inputs, both switch edges, no repeat while held, timer rollover,
and optional momentary mode. Hardware pulse timing and contact operation require
the bench checks above.

Relay reference: https://en.hongfa.com/product/power-relay/HF3F-L

### Faster startup (1.2.1)
This project enables SMARTSNAP_FAST_WIFI. After its first successful connection it remembers the access point and channel in LittleFS, avoiding a full channel search on later boots. If the hint fails for four seconds, normal Wi-Fi discovery resumes. DHCP remains enabled; no fixed IP is required. Saved relay commands still win after connection. Other device projects do not enable this option. Serial logs report Wi-Fi readiness, hub readiness and each restored relay in milliseconds since boot. Real startup improvement must be measured on the board; the first boot learns the hint.

