# Build verification

Verified on 2026-09-25 with PlatformIO, Espressif8266 4.2.1 and Arduino core 3.1.2.

| Project | Result | Binary size (bytes) |
|---|---|---:|
| PlantsWatering | PASS | 478928 |
| TempSensor_BedRoom | PASS | 477696 |
| TempSensor_Kitchen | PASS | 477696 |
| TempSensor_Living | PASS | 477696 |
| TempSensor_OutSide | PASS | 477696 |
| TestDev_1 | PASS | 476208 |
| TestDev_2 | PASS | 476672 |
| WaterHeater | PASS | 414272 |

Original device-file hashes remain unchanged. The project sketch/header/source copies match their originals.

Validation covers compilation and binary generation. No USB flashing, OTA installation or hardware runtime tests were performed. Existing dependency warnings remain; none prevented the builds. WaterHeater retains its legacy library and server endpoint; see README.md.
