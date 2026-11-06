# SmartSnap project guide

## Architecture
- `SmartHome/`: Android Java app (Views/XML, Material Components). Main code: `app/src/main/java/com/example/smarthome/`; resources: `app/src/main/res/`. Preserve existing themes and widget layouts.
- `HubServer/`: Node 24 backend. `gateway.js` is the cloud gateway; `local-server.js` runs on the Pi. Cloud configuration uses MongoDB; local persistence uses SQLite. Automations run on the Pi, including without internet.
- Backend feature entry points: `events.js` / `event-clock.js`, `history.js` / `control-history.js` / `activity-history.js`, `config-store.js`, `device-discovery.js`. Read the relevant feature documentation and tests only.
- `PlatformIO/<device>/`: ESP8266/ESP32 firmware projects. Shared libraries live in `PlatformIO/shared/`, especially `SmartSnap/`. Check each project's `platformio.ini` before changing shared code.
- `Devices/`: original Arduino projects and library sources; `Server/`: older server implementation. Do not update these automatically alongside current code; check which copies the task actually uses.
- `Migration/`: guides, deployment helpers, diagnostic scripts, logs and packaged artifacts. Open specific files rather than scanning this directory broadly.
- `HubServer` and `SmartHome` are separate Git submodules. Inspect status/diffs in the repository being edited; preserve existing unrelated changes.

## Working rules
- Start with the requested feature's entry points. Use scoped `rg` searches; exclude generated outputs, dependencies and logs. Avoid repeated repository inventories.
- Prefer small patches. Preserve encoding (UTF-8), formatting, architecture, functionality and design unless explicitly asked to change them.
- Reuse installed dependencies. Do not upgrade libraries/build tools, install packages, rewrite files, build every firmware, or run full suites without a concrete need.
- Match validation to impact: focused tests for logic; affected firmware build for C++; targeted Android tests/build and visual checks for UI. Documentation-only changes need no application build.
- Do not omit security, correctness or regression checks to save time. Shared protocol changes require checking relevant app, gateway, Pi and firmware consumers, including older firmware and local/offline operation.
- Preserve command acknowledgements versus independent device reports, saved configuration, home/device isolation and automation trigger semantics. Do not silently change these to address a display issue.
- Never print or commit credentials from device configuration, environment files or diagnostics. Avoid live actuation, flashing or deployment unless within the user's authorized scope; distinguish built artifacts from deployed changes.
- Report briefly: changed files, verification, and material limitations. Use absolute links for deliverables.

## Focused commands (PowerShell, workspace root)
- Backend: `node --check HubServer/<file>.js`; `node --test HubServer/test/<feature>.test.js` (Node 24 required for `node:sqlite`).
- Android unit tests: `./SmartHome/gradlew.bat -p SmartHome --offline testDebugUnitTest --tests 'com.example.smarthome.<TestClass>'`.
- APK when needed: `./SmartHome/gradlew.bat -p SmartHome --offline assembleDebug`. Use Android Studio's configured JDK; do not change Gradle/JDK versions casually. `SmartHome/build.gradle` redirects Windows build outputs outside OneDrive.
- Firmware: `& "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe" run -d PlatformIO/<device>`.
- Run additional integration/device tests only for affected paths. Inspect failures before broadening validation; report checks that could not run.
