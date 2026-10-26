# MultiSens — ESP8266

VEML7700 ambient light + AHT20 temperature/humidity + BMP280 pressure/temperature.
Original Arduino projects are unchanged. This is a separate PlatformIO project.

## Wiring

Use 3.3 V power and I2C pull-ups to 3.3 V. Do not connect 5 V logic to the ESP8266.
Both sensor boards share the same four wires:

| ESP8266 NodeMCU / ESP-12E | Both sensor boards |
| --- | --- |
| 3V3 | VCC/VIN (check your breakout supports 3.3 V) |
| GND | GND |
| D2 / GPIO4 | SDA |
| D1 / GPIO5 | SCL |

Addresses: VEML7700 0x10; AHT20 0x38; BMP280 0x76 or 0x77 (auto-detected).
For a bare BMP280, CSB must select I2C mode and SDO sets the address; combined
breakout boards normally handle these connections. Keep wires short. Place the
AHT20 away from ESP/regulator heat and expose the light sensor to the area measured.

## Set up and build

1. The project is configured for MultiSens, device ID 1014.
2. Edit `src/DeviceConfig.h`: fill in the Wi-Fi SSID/password placeholders.
   Verify Home_Germany and hub 192.168.178.89:3000 match your installation.
3. Open this folder in VS Code with PlatformIO, or open the parent
   `SmartHomeDevices.code-workspace` and select MultiSens.
4. Build the `nodemcuv2` environment. Libraries download automatically.
5. For the first USB upload choose the correct port, then PlatformIO Upload.
   Do not select another existing device's port accidentally.
6. For subsequent OTA updates use the firmware upload page and this project's
   `.pio/build/nodemcuv2/firmware.bin`. Version: `MultiSens_1.1.0`.

The default board/flash layout is NodeMCU/ESP-12E with 4 MB flash. If using another
ESP8266 module, adjust the board and flash layout in platformio.ini first.
The SmartSnap library retains OTA support and honors a previously provisioned
identity; changing DEVICE_ID alone does not override saved provisioning.

Commands from this project folder:

```text
pio run -e nodemcuv2
pio run -e nodemcuv2 -t upload --upload-port COMx
pio device monitor -b 115200
```

## Variables

Create/select these exact names and types for this device in the app:

| Name | Type | Meaning/unit |
| --- | --- | --- |
| Light | float | VEML7700 illuminance, lux (automatic range) |
| Temp | float | AHT20 air temperature, degrees C |
| Humd | float | AHT20 relative humidity, % |
| Pressure | float | BMP280 local pressure, hPa; not sea-level corrected |
| BMPTemp | float | BMP280 temperature, degrees C |
| Update | int | Write 1 to request a sample; device writes 0 after the attempt |
| status | string | OK or the names of sensors that failed |

Use Temp for room temperature; BMPTemp is a secondary reading from the pressure
sensor. Bind sensor cards to these float variables and select the corresponding
units. Configure a button with ON value 1 and OFF value 0 for Update.

## Timing and update behavior

- A fresh report is sent after connection, then every 15 minutes.
- Update=1 queues a fresh measurement of all sensors outside the socket callback.
- An on-demand report restarts the 15-minute timer.
- The loop services SmartSnap/OTA continuously between reports; there is no
  15-minute blocking sleep or deep sleep.
- Reconnection publishes fresh readings and checks the server's saved Update
  value, supporting offline writes when enabled on the app widget.
- Normal variable reads return the last sampled value. Use Update for a fresh one.
- Invalid readings are not published as zero or NaN. Other working sensors still
  publish, and status identifies failures. The server may retain the last valid
  value for a failed sensor; check status and timestamps.
- Update returns to 0 after the attempt, including partial failures. This is an
  acknowledgment of the attempt, not a guarantee all sensors succeeded.
- Missing sensors are retried on the next scheduled or requested sample.

## References

- https://github.com/adafruit/Adafruit_VEML7700/tree/master/examples/veml7700_autolux
- https://github.com/adafruit/Adafruit_AHTX0
- https://github.com/adafruit/Adafruit_BMP280_Library

Hardware readings and wiring still require testing on your physical sensor boards.


## Configurable reporting (1.1.0)

Add these writable variables to MultiSens (1014) in the app:

| Variable | Type | Default | Unit |
| --- | --- | --- | --- |
| ReportInterval | int | 15 | seconds (1–86400) |
| LightThreshold | float | 13 | lux |
| LightUpdateThr | float | 50 | lux |
| TempUpdateThreshold | float | 0.5 | degrees C |

The device samples once per second while connected. All readings are published at
ReportInterval, or on Update=1 (which still resets to 0 afterward). Extra Light
updates are sent when lux crosses below LightThreshold or returns to at/above it,
or when its absolute change
from the last published light reading exceeds LightUpdateThr. Extra Temp updates use the AHT20
temperature and an absolute change exceeding TempUpdateThreshold. Threshold updates
do not reset the full-report timer. Each published Light or Temp reading updates
its own comparison baseline, including threshold-triggered and on-demand reports. Identical two-decimal readings
are not repeatedly published between full reports; remaining on the same side of the lux threshold does not itself trigger updates.
Delta updates still work on either side, and full interval reports remain unchanged.

Settings are requested once after every connection, with no periodic polling or
retry timer. Later changes arrive through PhoneWriteVariable, like normal variables.
If a startup response is missing or invalid, the device uses its last saved settings,
or the defaults above. Settings are validated and saved atomically to LittleFS
before becoming active; unchanged settings do not cause flash writes. Normal OTA
preserves this file; erase-flash removes it. Sensor measurements are not written to
flash. A live settings write supersedes a pending startup response for that setting.

## Configuration screen (1.2.0)

Install the updated app and flash MultiSens 1.2.0. The device advertises
SmartSnap::Capability::CustomConfiguration. In Add Variable, choose Int, float,
or String, enter the intended initial value (the generic form defaults to 0),
and enable Configuration variable for ReportInterval, LightThreshold,
LightUpdateThr and TempUpdateThreshold. Use their defaults 15, 13, 50 and 0.5.
Long-press MultiSens in Devices and select Configuration to edit these values.
The app uses the flag, not variable names or device IDs. New devices can declare
the same capability and expose their own flagged variables, with firmware handling
writes, validation, persistence and server-value retrieval. Saving confirms server
acceptance, not device-specific validation or flash persistence.
