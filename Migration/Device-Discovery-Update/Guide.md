# SmartSnap device discovery — 29 October 2026

## Existing devices: no need to add them again
Flash the matching updated firmware onto each existing board. Keep the existing app device entries. The migration preserves the current device ID and saved connection configuration; the Pi associates the board's hardware identity with that entry. Your variables, values, widgets, schedules, and events are not recreated or overwritten.

Use normal sketch upload or OTA, not an erase-filesystem operation. If you erase all flash, the current project ID/network defaults must still match that board, or the hub must already know its hardware identity. Never delete the app device as part of updating firmware.

## Installation
The Pi and Heroku updates have been deployed. Install SmartHome-device-discovery-debug.apk, then upload the binary matching each project in Firmware/. TestDev_2 is the project used for TestDev_3. RelayController_1020 is the three-relay controller. MultiSens includes radar support. Existing binaries retain their project's migration ID and connection settings. Do not use another project's binary on unrelated hardware.

Voice_Living now registers with SmartSnap alongside its separate audio connection. Its audio satellite name and token stay unchanged. Discovery of a new voice satellite does not automatically provision the voice recognition service's audio identity/token.

## Add a new board
1. Configure its Wi-Fi, home name, and Pi address in the firmware as before. This feature does not provision Wi-Fi over Bluetooth.
2. Power it on. The Pi requests its variable definition when it is not already registered.
3. In Devices, tap +, then SmartSnap device.
4. The screen says **Searching for new devices on your home hub…** and animates while checking the Pi. The hub must be online.
5. Tap the discovered device. The hardware suffix helps distinguish identical boards.
6. Review the suggested name and supplied variables. You can optionally select a template; conflicting definitions must be resolved before saving.
7. Tap **Add device**. The gateway allocates the ID, imports variables together, syncs the Pi, and delivers the ID to the board. The board stores it for subsequent starts.

No variables is a valid definition. The app will say that variables can be added later. Add manually remains available for older firmware.

## Definitions in firmware
Each production PlatformIO project now has src/DeviceDefinition.h. For example:

```cpp
static const SmartSnap::VariableDefinition DEVICE_VARIABLES[] = {
    {"Relay1", "int", "0", false},
    {"Relay2", "int", "0", false},
    {"ReportInterval", "int", "15", true},
};
```

The last field marks a configuration variable. Supported types are `int`, `float`, and `string`; default values are strings. Up to 32 variables can be declared by the firmware. Existing app templates retain their own limits. Keep the array alive throughout the program, as in the static header above.

Before Initialize:

```cpp
smartSnap.EnableDiscovery("Relay_Controller", "Three relay controller", "triple_switch");
smartSnap.SetVariableDefinitions(DEVICE_VARIABLES,
    sizeof(DEVICE_VARIABLES) / sizeof(DEVICE_VARIABLES[0]));
// For a completely new generic firmware, use 0 instead of a fixed device ID.
smartSnap.Initialize("Home_Germany", 0, WIFI_SSID, WIFI_PASSWORD, PI_HOST, 3000);
```

To define no variables, call `SetVariableDefinitions(nullptr, 0)` instead. Capabilities remain declared with SetCapability before Initialize; enable CustomConfiguration if the app should show its Configuration section.

## Lifecycle rules
- A numeric ID is allocated centrally; hardware identity is the stable board MAC, not a user-entered ID.
- Definitions import only when a device is first added, never on ordinary reconnects.
- App edits and variable deletions remain authoritative. Reconnecting cannot recreate deleted variables.
- Repeating an Add request for the same hardware does not create a second device.
- If connection fails after Add, the device remains added; reconnect/retry resolves the assignment.
- Deleting a device removes registration. It can be discovered again after reconnecting; it is not silently recreated.
- New factory firmware should use ID 0. The supplied existing-device projects keep their prior IDs specifically for migration.
- Discovery is scoped to the selected home and requires device-edit permission in the app.
- Diagnostic-only microphone and compile-test projects are not production firmware and are not part of the update bundle.

## Verification
Backend regression suite, Android build/JVM tests, and all 12 production firmware builds were run. A transient unassigned connection against the deployed Pi confirmed its definition request and accepted an empty manifest, without creating a real app device or operating hardware. Real-board flashing, pairing UI validation, and voice audio regression testing remain to be checked after flashing.
