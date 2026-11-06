# Control history confirmation update

The Pi fix is deployed. Reopen control history or tap Refresh. No new APK or Heroku deployment is required.

## Behavior
- An online command and its matching applied-state report now form one history entry.
- The report timestamp is used for the applied state; the original request timestamp is retained internally.
- A device-originated value change (wall switch, reset, or local logic) still creates a history entry.
- Explicitly queued offline commands remain distinct from applied state: waiting time must not become confirmed ON time.
- Existing adjacent duplicate online-command/report entries within 10 seconds were repaired. Older ambiguous records were not guessed or upgraded.
- The heater's 09:41:37 ON and 09:41:40 OFF remain a real approximately 3.7-second interval. The extra same-second estimated interval is removed.

## Firmware
All 11 production projects that use the shared SmartSnap library have been rebuilt with a reportKind marker: state for ordinary writes and command_result for command-ID-correlated writes. WriteVariableValue remains the API, preserving independent local updates. A report must follow validation/application, not just command receipt. The hub remains compatible with existing firmware; flashing is not required to receive the central duplicate-history fix.

Use only the binary matching your device project and hardware. These retain each project's existing device/network configuration. TestDev_2 is the project used for TestDev_3. RelayController_1020 is the three-relay ESP12E device. MultiSens includes the radar implementation. Upload via the existing OTA process; do not flash unrelated devices with another project's binary.

Voice_Living uses its own audio protocol, not this variable-write acknowledgement library, and needs no change for this issue. Diagnostic-only Health_Compile_Test and Voice_Mic_Test are not deployment firmware. Historical Devices/ sketches may contain old embedded libraries: use the current PlatformIO projects and the binaries here.

## Verification
127 backend tests pass, covering single-entry confirmation, independent wall-switch/reset reports, numeric equivalents, safe repair of old duplicates, and retention of offline unknown intervals. Production firmware build results and SHA256 checksums accompany this folder. No physical heater/pump control was performed during testing.
