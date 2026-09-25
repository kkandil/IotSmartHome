# SmartHome devices — VS Code and PlatformIO guide

These are separate development copies of the eight Arduino device projects. Everything under the original `Devices` folder remains unchanged. Continue using those original `.ino` files in Arduino IDE whenever you want.

## 1. What was created

| Project folder | Purpose |
|---|---|
| `TestDev_1` | First test device |
| `TestDev_2` | Second test device |
| `PlantsWatering` | Plant watering controller |
| `TempSensor_BedRoom` | Bedroom sensor |
| `TempSensor_Kitchen` | Kitchen sensor |
| `TempSensor_Living` | Living-room sensor |
| `TempSensor_OutSide` | Outside sensor |
| `WaterHeater` | Legacy water-heater controller; see special note below |

Each project contains:

```text
TestDev_1/
  platformio.ini          Board, framework, library and serial settings
  src/
    TestDev_1.ino          Your application code
    SmartSnapHubConfig.h   Hub address (where present in the original)
  .pio/                   Generated automatically when you build
```

PlatformIO supports Arduino `.ino` files, so you can keep the familiar `setup()` and `loop()` structure. There is no need to rewrite the firmware in a different language.

`shared/` contains separate copies of SmartSnap and its dependencies. Changes here affect the new PlatformIO projects, **not** the original Arduino libraries. The projects reference this folder using relative paths, so keep the entire `PlatformIO` folder together when moving it.

The bundled dependency versions are ArduinoJson 7.0.4, SocketIoClient 0.3, WebSockets 2.4.0, DHT sensor library for ESPx 1.19, and Ultrasonic 3.0.0. These match the installed Arduino libraries used for this migration. Existing library compiler warnings may appear; a build ending in `SUCCESS` is successful.

## 2. Open a project in VS Code

1. Install Visual Studio Code if necessary.
2. Open **Extensions** with `Ctrl+Shift+X`.
3. Search for **PlatformIO IDE**, published by PlatformIO, and install it. Let its initial setup finish; restart VS Code if asked. The extension includes the PlatformIO command-line tools.
4. Choose **File → Open Folder** and select, for example:
   `C:\Users\khale\OneDrive\Documents\ChatGPT\IOT dashboard App\PlatformIO\TestDev_1`
5. Trust the folder if VS Code asks. Wait for PlatformIO to initialize it.
6. Open `src/TestDev_1.ino` to edit the device code.

Alternatively, use **File → Open Workspace from File** and open `SmartHomeDevices.code-workspace` in this folder. This shows all eight projects. Use the project-specific tasks in the PlatformIO sidebar to avoid building or uploading the wrong device.

The first setup on a new computer needs internet access to download the pinned ESP8266 platform and toolchain. The application libraries are already included here.

## 3. Build a `.bin` without flashing anything

1. Save your changes with `Ctrl+S`.
2. Open the PlatformIO sidebar (alien-head icon).
3. Under your intended project, expand **Project Tasks → nodemcuv2 → General**.
4. Click **Build**. The check-mark toolbar button also builds the selected project.
5. Wait for `SUCCESS` in the terminal.

The generated file is:

The output subfolder follows the `[env:NAME]` heading, not the `board` value. The bedroom sensor now uses `[env:esp01s]`, so its output is `TempSensor_BedRoom/.pio/build/esp01s/firmware.bin`. The other projects currently use `nodemcuv2`. An old environment's output folder may remain after renaming; use the binary from the environment you just built.

```text
<project>/.pio/build/nodemcuv2/firmware.bin
```

For TestDev_1, the full path is:

```text
C:\Users\khale\OneDrive\Documents\ChatGPT\IOT dashboard App\PlatformIO\TestDev_1\.pio\build\nodemcuv2\firmware.bin
```

This is the application binary for your firmware upload webpage. Do not select `firmware.elf`, a filesystem image, or a full flash backup.

Every project produces a file with the same filename, so check the parent project folder before uploading. You can copy and rename the binary for storage, for example `TestDev_1_1.4.1.bin`. Renaming the file does not change the version reported by the device.

### Build using a terminal

Choose **PlatformIO: New Terminal** from the VS Code command palette (`Ctrl+Shift+P`). From a project directory:

```powershell
pio run
```

From the `PlatformIO` parent folder:

```powershell
pio run --project-dir TestDev_1
```

The included helper builds every project, or just one, without uploading:

```powershell
.\Build.ps1
.\Build.ps1 -Device TestDev_1
```

Run these from this `PlatformIO` folder. Build logs go into `build-logs`. If your organization blocks PowerShell scripts, use the individual `pio run` commands instead.

## 4. Change firmware configuration and version

Edit the project copy under `PlatformIO/<device>/src`, not the original under `Devices`.

- Application logic, pins and variable callbacks remain in the `.ino` file.
- For projects containing `SmartSnapHubConfig.h`, set the Raspberry Pi host and port there.
- Review the `Initialize(...)` call for the intended home, initial device ID and Wi-Fi configuration.
- Set the reported version using `SetFirmwareVersion(...)`, for example:

```cpp
api.SetFirmwareVersion("TempSens_1.0.1");
```

Use the SmartSnap object name already present in that sketch (`api`, `smartSnap`, etc.). Build again after any change.

The projects preserve the existing configuration, including connection credentials. Treat source folders and firmware binaries as private. Do not publish them to a public repository without removing secrets first.

### Device identity and reusable firmware

On current SmartSnap firmware, connection settings saved in LittleFS can override the compiled defaults. A normal firmware update intentionally preserves that identity. Changing an ID in the source alone may therefore not change an already configured device's identity.

For an OTA update, use the webpage's **Device ID after update** field when assigning a different ID. Create the corresponding device in the app first. The same compatible application binary can then be installed on multiple devices, assigning each its intended ID during installation.

A blank device still needs initial working Wi-Fi, hub and identity configuration to connect for its first OTA update. The supplied projects preserve their original bootstrap settings; they do not introduce a new first-boot provisioning screen.

## 5. First installation through USB

1. Connect the intended ESP12E/NodeMCU board to the laptop with a data-capable USB cable.
2. Close Arduino Serial Monitor and any other program using its COM port.
3. Check the port in Windows Device Manager or run `pio device list`.
4. Select the correct project and choose **General → Upload** in PlatformIO.
5. If more than one serial device is connected, specify the port explicitly:

```powershell
pio run --target upload --upload-port COM7
```

`COM7` is only an example; check the current port. The command uploads the project in the terminal's current directory. Upload also builds first if required.

To make a port the default for one project, add this under `[env:nodemcuv2]` in its `platformio.ini`:

```ini
upload_port = COM7
monitor_port = COM7
```

The supplied files leave the port unset so PlatformIO can detect it. No hardware was flashed as part of creating these projects.

## 6. Read serial output

Use **General → Monitor**, or:

```powershell
pio device monitor --port COM7 --baud 115200
```

Close the monitor before another application uses the same port. In the terminal, `Ctrl+C` exits the monitor. ESP8266 startup ROM output can initially look garbled because it uses a different baud rate; the application uses 115200.

## 7. Install updates wirelessly through your existing webpage

PlatformIO **Upload** is configured for USB. Your SmartSnap OTA system uses the existing firmware webpage; it is not PlatformIO's `espota`/ArduinoOTA upload protocol.

1. Build the intended project and confirm `SUCCESS`.
2. Open [the firmware page](https://smarthomehub-1aec4600a734.herokuapp.com/firmware) and sign in with an account that can edit the target home.
3. Choose the correct **Home** and **Device**.
4. Enter a version label matching the version compiled into the sketch.
5. Select that project's `.pio/build/nodemcuv2/firmware.bin` and upload it. Uploading the file alone does not start installation.
6. Select it under **Available firmware**.
7. Leave **Device ID after update** blank to retain the identity, or enter the intended new device ID.
8. Normally leave **Replace device configuration** off. Enable it only when intentionally replacing the saved connection configuration with the firmware's compiled defaults. Check those defaults first so the device can reconnect.
9. Start installation and watch **Update activity**. Allow the device to finish and reboot.
10. Confirm it reconnects and check its reported firmware version in the app's Device Details.

For local access, the Pi's page is normally `http://SmartHubEgypt:3000/firmware`; use the existing local administrator access configuration. See `Migration/OTA-UPDATES.md` in the parent repository for the complete OTA workflow.

Keep `shared/SmartSnap/src/SmartSnapOTAKey.h`: this is the public verification key required by the existing signed OTA system. Do not replace it casually or copy the server's private signing key into firmware. Upload the normal PlatformIO application binary; the server handles signing.

## 8. Flash layout and hardware assumptions

All projects target **NodeMCU 1.0 / ESP-12E**, ESP8266 Arduino core 3.1.2 through Espressif8266 platform 4.2.1, 4 MB flash and the `eagle.flash.4m2m.ld` layout. This matches the original Arduino NodeMCU default: 2 MB filesystem and approximately 1019 KB application capacity for OTA.

The explicit layout matters: PlatformIO's usual NodeMCU filesystem default differs from the Arduino default used here. Changing layouts can make saved configuration inaccessible. Avoid **Erase Flash**, filesystem upload tasks, or changing the flash layout when you want to preserve device identity. A normal application upload does not intentionally replace the filesystem.

These settings assume the same boards used by the Arduino projects. Check flash capacity and pin mapping before using a different ESP8266 board.

## 9. WaterHeater is a preserved legacy project

`WaterHeater/src` contains its own older `SmartSnapAPI.h` and `.cpp`, just like the original sketch. It still targets the older `smarthome.herokuapp.com` endpoint and does not include the current shared SmartSnap OTA implementation.

It can be built with PlatformIO, but this migration does **not** upgrade its server protocol or add OTA support. Do not install its binary through the modern OTA workflow expecting subsequent OTA updates to remain available. Use USB for the legacy project; migrate and test its control logic and communication separately before adopting the current SmartSnap library.

## 10. Keeping Arduino and PlatformIO versions separate

There is no automatic synchronization between the two trees:

- Arduino IDE projects: `Devices/<device>/...`
- VS Code / PlatformIO projects: `PlatformIO/<device>/src/...`
- New shared library: `PlatformIO/shared/SmartSnap/...`

This protects your Arduino fallback. If you later want a fix in both versions, copy or merge that specific change deliberately. `arduino-source-hashes.json` records the original device-file hashes taken at project creation for verification.

For another device project, copy one appropriate project folder, omit its generated `.pio` directory, update the source configuration, and open the new folder in VS Code. Keep it directly under `PlatformIO` so the relative shared-library paths still work. Do not keep two sketches with separate `setup()`/`loop()` functions in one project's `src` folder.

## 11. Troubleshooting

| Symptom | What to check |
|---|---|
| `pio` is not recognized | Use **PlatformIO: New Terminal** rather than a plain system terminal. |
| Library headers cannot be found | Keep `shared` next to the project folders; open the folder containing `platformio.ini`. |
| Red editor underlines but Build succeeds | Run **PlatformIO: Rebuild C/C++ Project Index** from the command palette. |
| COM port access denied | Close serial monitors and other applications using that port. |
| USB upload does not connect | Check cable, port, board power and USB driver; confirm this is the intended board. |
| Connection still uses old ID/server | Saved LittleFS configuration may override compiled defaults; use the OTA identity/configuration controls described above. |
| Old binary accidentally uploaded | Check the project's path and build timestamp; all output files are named `firmware.bin`. |
| Build files locked under OneDrive | Keep the whole new `PlatformIO` folder together in a non-synced working directory if locks recur; preserve the original Arduino folder. |
| Need a clean rebuild | Run `pio run --target clean`, then `pio run`. This removes generated build output, not device flash. |

## Official references

- [PlatformIO in VS Code](https://docs.platformio.org/en/stable/integration/ide/vscode.html)
- [NodeMCU 1.0 board configuration](https://docs.platformio.org/en/latest/boards/espressif8266/nodemcuv2.html)
- [ESP8266 platform](https://docs.platformio.org/en/latest/platforms/espressif8266.html)
- [Library dependency configuration](https://docs.platformio.org/en/latest/projectconf/sections/env/options/library/lib_deps.html)
