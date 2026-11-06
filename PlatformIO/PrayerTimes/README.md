# PrayerTimes — ESP-12E / ESP8266

PlatformIO port of `Devices/PrayerTimes/PrayerTimes.ino`. The original sketch is unchanged. Keeps the Nokia PCD8544 layout, Europe/Berlin timezone/DST, Mawaqit calendar extraction (skipping sunrise), DFPlayer track 1 and existing button behavior.

## Add to SmartSnap

1. Open this folder in PlatformIO. Review `src/DeviceConfig.h` for your Wi-Fi, home and Pi address. The local file has been prepared from your existing MultiSens connection settings and is ignored by Git. `DeviceConfig.example.h` is the shareable template.
2. `DEVICE_ID = 0` enables server-assigned discovery. No manual ID reservation is required.
3. Build/upload the `nodemcuv2` environment. Use your ESP-12E serial programming hardware and correct port. This project uses the same 4 MB / 2 MB filesystem layout as the other ESP8266 projects.
4. In the app: Devices > Add device > SmartSnap. Select PrayerTimes from discovered devices and add it. The eight variables below are declared by `DeviceDefinition.h` and imported for this new device.
5. Long-press the device > Configuration to edit the seven settings, then Save. Current app/server already support these capabilities; no new APK or backend deployment is required.

## Variables

| Name | Type | Default | Configuration | Meaning |
|---|---|---|---|---|
| Volume | int | 15 | Yes | DFPlayer volume, 0–30; 0 is silent |
| DataSourceUrl | string | https://mawaqit.net/de/m/izstuttgart | Yes | HTTPS Mawaqit mosque page with the same calendar format, up to 256 characters |
| Fajr | int | 1 | Yes | 1 enables sound, 0 disables it |
| Dhuhr | int | 1 | Yes | 1 enables sound, 0 disables it |
| Asr | int | 1 | Yes | 1 enables sound, 0 disables it |
| Maghrib | int | 1 | Yes | 1 enables sound, 0 disables it |
| Isha | int | 1 | Yes | 1 enables sound, 0 disables it |
| Stop | int | 0 | No | Send 1 to stop current audio; device resets it to 0 |

Link a momentary button widget to `Stop`, sending 1. Stop only affects current playback; it does not disable future prayers. Disabling the active prayer's sound also stops it. Disabled prayers still appear on the LCD.

## Startup and persistence

- Load checksum-validated EEPROM settings first; otherwise use defaults.
- Request all seven configuration variables once after each hub connection. Valid server values override the local fallback and are saved to EEPROM. No 30-second polling.
- Live app writes are applied and saved through normal SmartSnap variable callbacks. Newer app/button edits supersede pending startup reads for that setting.
- If the server cannot be reached, cached settings remain active. Invalid server values are rejected. Settings are not blindly published at startup, which prevents overwriting pending offline app edits.
- The button saves volume locally and reports it when connected. On a later reconnect, saved server settings win, including over volume changes made locally while offline.
- Writes are committed only when settings change. ESP8266 EEPROM is flash-emulated, so a full flash erase removes this fallback (as well as discovery/provisioning).

## Wiring preserved from the original sketch

| Function | GPIO | NodeMCU label |
|---|---|---|
| LCD CLK | 14 | D5 |
| LCD DIN | 5 | D1 |
| LCD DC | 4 | D2 |
| LCD CE | 16 | D0 |
| LCD RST | 15 | D8 |
| DFPlayer TX -> ESP RX | 12 | D6 |
| DFPlayer RX <- ESP TX | 13 | D7 |
| Button to GND | 0 | D3 |

Common ground is required. Preserve the existing ESP8266 boot strapping; holding the GPIO0 button during reset enters programming mode. No wiring changes are required by this port.

The button stops audio while the prayer screen is active. Otherwise it increases volume by 5, wrapping above 30 to 5, as in the original sketch.

## Timing and network behavior

Time synchronization is nonblocking. Today's timetable is refreshed at startup/day change/source change, with failed fetches retried at most once per minute. Stale dates do not trigger playback. Refreshing the same day's timetable does not reset already-played flags. The prayer screen lasts 203 seconds, matching the original.

The parser services SmartSnap and the button while reading the response. TLS/HTTP connection establishment remains synchronous and can briefly delay controls. The original `setInsecure()` TLS policy is retained: traffic is encrypted, but the website certificate is not verified. The source must provide the same Mawaqit calendar structure; this is not a generic URL/API parser. The timezone remains Europe/Berlin even if another mosque URL is selected.

Without internet, an already downloaded current-day schedule and synchronized clock can keep running, but a cold boot needs NTP and a timetable download. Offline multi-day timetable storage is not included.

## First hardware test

1. Flash and claim through discovery. Confirm Configuration contains all seven settings.
2. Set Volume to a quiet value such as 5, save, and verify the LCD changes. Press the volume button outside the prayer screen and verify the app reads the new value.
3. Power-cycle without the hub available: verify the saved volume is restored. Reconnect and verify server settings take precedence.
4. Power the device off, change Volume in the app, then restart it: verify it retrieves the new server value.
5. Confirm the five displayed times match the source page. Test Stop=1 during playback and ensure it returns to 0. Verify a disabled prayer does not play.

Validation: successful ESP8266 firmware build; compiled C++ tests cover defaults, checksum corruption, volume bounds, prayer flags and URL validation. LCD/audio, actual EEPROM retention and live server restoration require the physical hardware; it has not been flashed by this task.
