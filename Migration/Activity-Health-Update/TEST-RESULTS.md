# Verification — 28 October 2026

- Backend: 110 tests passed. Includes history persistence after test-hub restart, access scoping, pagination, command correlation, multi-action failures, superseded commands, and rejection of forged health reports.
- Android: debug APK, instrumentation APK and JVM unit tests built/passed.
- Emulator: three focused UI tests passed individually: history/status rendering, dark-mode health rendering, and run details with condition snapshot and individual action results. Screenshots in this folder use illustrative test data.
- Firmware: ESP8266 relay and MultiSens builds passed; ESP32 shared-library compile check passed.
- Distribution: all four entries in SHA256SUMS.txt verified against the packaged files.

No physical phone was targeted by these tests. No firmware was flashed and the Pi update has not been installed. These checks do not substitute for confirming the features on the actual hub after installation. The cloud deployment status is documented in README.md.
