# Control confirmations and heater history — 29 October 2026

## Install
1. Install `../SmartHome-heater-history-debug.apk` on the phone.
2. Upload `WaterHeater.bin` to the WaterHeater using its existing OTA firmware upload. This is WaterHeater_1.1.1. Do not use a different device's binary.
3. Optional updates for affected devices: `TestDev_1.bin` (TestDev_1.4.1), `TestDev_2.bin` (TestDev2_1.0.1; deployed device named TestDev_3), and `PlantsWatering.bin`. These are built with each project's existing configuration; verify the target before uploading.
4. The Pi history update is already installed. No additional Heroku deployment is needed for these fixes.

## What changed
WaterHeater previously moved its servo but did not report V1 from the app/event write handler. The hub therefore correctly had no correlated confirmation. It now reports V1 after setting the servo output. Repeated commands also report the applied state.

The TestDev output handlers now report their state after setting GPIO. PlantsWatering reports its actual accepted pump state, so a blocked pump activation cannot be confirmed as successful. The three-relay project's existing post-application reporting already supports this flow.

The shared PlatformIO SmartSnap library attaches the incoming command ID to a matching WriteVariableValue report. Numeric equivalents such as 1 and 1.00 now match. Receipt of a command alone does not acknowledge successful application. New device firmware should validate and apply a command, then call WriteVariableValue for its resulting state.

Confirmation means the device firmware reported the requested state. It does not prove mechanical movement, electrical load, or physical heater operation. Missing confirmations remain unconfirmed rather than falsely successful. Sensor measurement requests and rejected commands are not blindly acknowledged.

The PlatformIO projects and packaged binaries use the shared current library. The historical Arduino WaterHeater sketch retains its older embedded API; use the PlatformIO binary for command-ID confirmations.

## History charts
Older recorded ON/OFF entries now contribute clearly labelled estimated ON time. Explicitly queued offline writes remain unknown until reported by a device. Old records cannot be retroactively confirmed. The logged heater interval 07:51:58–08:39:22 contributes 47 minutes 24 seconds in both daily and weekly views. Weekly bars distinguish confirmed and estimated time, and short-duration axis labels no longer repeat rounded hour labels.

## Validation
- 125 backend regression tests passed, including correlated numeric confirmations and history source transitions.
- Android debug build and JVM chart tests passed, including the exact heater interval, multiple cycles and range boundaries.
- All four PlatformIO firmware builds passed.
- No heater or pump was actuated for testing; hardware/OTA and phone visual verification remain to be done after installation.

After installation, check a new automation run when the appliance is safely usable. Its matching action should show Device confirmed. Older runs may continue to say Sent — unconfirmed.
