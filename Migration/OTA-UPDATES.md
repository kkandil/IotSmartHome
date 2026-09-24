# SmartSnap firmware updates

## Open the upload page

- Remote: https://smarthomehub-1aec4600a734.herokuapp.com/firmware
- At home: http://SmartHubEgypt:3000/firmware (or the Pi LAN IP).
- Android: Devices → Device Details → Firmware update opens the remote page with the home/device preselected. Install the latest Android build using Android Studio Run.

The remote page uses your existing SmartHome email/password and requires device-edit permission for the selected home. The local page uses the separate administrator key in `Migration/private/ota-local-key.txt`. This permits local updates without internet; keep the key private. The local page is HTTP and intended for your trusted home network only. No router port forwarding is required.

## First setup of each ESP8266

Flash a sketch using the updated SmartSnap library over USB. Use the correct Wi-Fi, home, device ID and Pi address in Initialize. Call `api.SetFirmwareVersion("1.1.0")` before Initialize, and call `api.Run()` frequently in loop. The device will report OTA readiness to the Pi.

SmartSnap stores connection provisioning in LittleFS (`/smartsnap-connection.json`) on first initialization. Later sketch OTA preserves this, so changing Initialize defaults does not change an already provisioned device. To deliberately reprovision, call `api.ResetConnectionConfiguration()` and reboot using the desired defaults. Keep the same flash/filesystem layout across builds. This implementation targets ESP8266, including ESP12E; ESP32 OTA is not implemented.

## Upload and install

1. Compile the sketch using the correct ESP8266 board and flash layout. In Arduino IDE use Sketch → Export Compiled Binary. Upload the sketch `.bin`, not a filesystem image or full flash dump.
2. Open either page, authenticate, and select home and target device by name/ID.
3. Enter a version label and choose the `.bin`, then Upload firmware. Upload only stores the image.
4. Choose a stored image and Install on selected device, then confirm the target.
5. Watch Update activity. Completed means that device reconnected reporting the expected sketch MD5; timeout is Unconfirmed, never success. A device running firmware without OTA support requires USB setup first.

SetFirmwareVersion controls the installed version displayed by the device; the upload version is a human label. Use matching values. No device-type compatibility matching is performed, as requested. You are responsible for selecting the appropriate sketch.

Remote files persist in MongoDB's firmware collection, scoped to the home (maximum 30 images per home, 1 MB per unsigned sketch). Heroku does not store firmware on its ephemeral disk. When installing, the gateway sends the image through the Pi's existing outbound connection. The Pi stores images/jobs in SQLite tables `ota_files` and `ota_jobs`. Once copied, the update download is entirely local. Local uploads are stored only on that Pi. They are not automatically copied back into the cloud catalog.

The uploader signs images using RSA/SHA-256. Devices verify them against the public key in SmartSnapOTAKey.h before installing. Keep the private signing key backed up securely (`Migration/private/ota-signing-private.pem`). The Pi reads its copy from `/home/khaled/smarthome/ota-signing-private.pem`; Heroku uses OTA_SIGNING_PRIVATE_KEY_BASE64. Do not regenerate the signing key for existing devices without planning a key transition.

Updates interrupt device operation and reboot it. Standard ESP8266 OTA does not provide automatic application rollback. Retain USB access for recovery and keep OTA support in every new sketch.

## Verified on the physical COM7 device

On 24 September 2026, TestDev_1 (Home_Germany, ID 1007) was flashed by USB with 1.0.0-ota, then updated from the local Pi upload page/API to 1.1.0-ota (22 seconds), and through the remote Heroku/MongoDB upload and relay to 1.2.0-ota (24 seconds). Both reboots were verified using the expected sketch hash. The device is left on 1.2.0-ota.

The original 1 MB sketch region was backed up to `Migration/private/com7-original-sketch-region.bin`; attempts to read all 4 MB encountered serial transfer errors, so this is not a complete flash/filesystem backup. USB is now only supplying power; subsequent successful updates used Wi-Fi.
