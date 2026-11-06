# Device status event conditions

Install SmartHome-device-status-events-debug.apk. No device firmware update is required.

1. Open Events and create or edit an event.
2. Under If, add a condition and select Device status.
3. Choose a device, then Connected or Disconnected.
4. Choose Immediately or After remaining true for and enter hours/minutes/seconds.
5. Add other conditions/groups if needed. Add device actions, notifications or alarms, then save.

Example: Device status = Disconnected, After remaining true for 00:05:00; then Notification: "Bedroom sensor has been offline for five minutes."

The duration begins when the Pi detects disconnection. A reconnect cancels that timer; a later disconnect starts a fresh timer. Unexpected power loss may take a connection timeout to detect. Smart plug status depends on the existing plug connection monitoring; it cannot detect loss before that monitoring reports it.

Status describes the device connection to the home hub, not the home's internet connection. Existing event rules apply: saving/enabling an immediately matching event does not execute it, and repeating the same state does not repeatedly trigger it. Pending duration timers restart after hub restart.

Tests: 154 backend tests passed, including real test-socket disconnect-to-alert routing, cloud save/reopen, variable-free devices, status/variable isolation, duration cancellation and home isolation. Android build and unit tests passed. No physical device was disconnected for these tests.
