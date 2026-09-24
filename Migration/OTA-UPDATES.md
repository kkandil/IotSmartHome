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

On 24 September 2026, TestDev_1 (Home_Germany, ID 1007) was flashed by USB with 1.0.0-ota, then updated from the local Pi upload page/API to 1.1.0-ota (22 seconds), and through the remote Heroku/MongoDB upload and relay to 1.2.0-ota (24 seconds). Both reboots were verified using the expected sketch hash. The device was subsequently upgraded normally to 1.3.0-ota, then the configuration replacement path was physically verified through the Pi in 25 seconds, keeping ID 1007 and its connection settings. The device is left on 1.3.0-ota. Changed-ID completion is covered by the automated server test. The remote replacement physical test was blocked by a laptop-to-MongoDB connection timeout; the updated gateway is deployed but that replacement path was not physically retested.

The original 1 MB sketch region was backed up to `Migration/private/com7-original-sketch-region.bin`; attempts to read all 4 MB encountered serial transfer errors, so this is not a complete flash/filesystem backup. USB is now only supplying power; subsequent successful updates used Wi-Fi.

## Replace a device's configuration

In step 3 of either upload page, enable **Replace device configuration** only when deliberately repurposing a device. The default is off. Select the current device (for example 1007), then install a sketch whose Initialize arguments specify the desired new ID/home/Wi-Fi/server. Create the new device record in the destination home first. The original device record is not deleted automatically.

The current firmware must advertise support for configuration replacement, and the uploaded sketch must also contain this updated SmartSnap library. Older OTA devices need one normal update with this option OFF before using it. No USB flash is needed for that upgrade.

A replacement marker is written only after the signed image is accepted. On the next boot, SmartSnap checks the actual sketch hash before erasing the saved connection settings and saving the new sketch defaults. Failed downloads/signature checks keep the old settings. The hub verifies a changed ID using the physical hardware address, job receipt and firmware hash. If the device moves to another hub/server, the original hub cannot verify that new connection and shows Unconfirmed; inspect the destination home. Incorrect Wi-Fi/server settings can require USB recovery.


## Reuse one binary with different device IDs

The upload page now has **Device ID after update** in the Install section. Select the currently connected physical device, choose a firmware file, and enter the destination device ID (already created in the selected home). Leave the field blank for a normal update. An explicit ID overrides both saved and compiled IDs; Wi-Fi/home/server remain unchanged unless Replace device configuration is enabled. This field is per installation, so a single stored binary can be installed for many IDs. An ID already connected on another device is rejected.

Devices need the new `otaDeviceId` capability. Update older devices once with this field blank before assigning an ID. The new ID is stored in LittleFS after the signed firmware is accepted and its sketch hash is verified on boot. Completion requires the assigned ID, hardware address, job receipt and expected image hash.

Application sketches can omit the ID:

```cpp
api.SetFirmwareVersion("TempSens_1.0.1");
api.Initialize("Home_Germany", WIFI_SSID, WIFI_PASSWORD, HUB_HOST, 3000);
```

This form uses previously provisioned identity or the ID supplied by OTA. Keep the existing ID-taking Initialize form for initial USB provisioning of a blank device; a blank device still needs an initial identity and network settings to connect for its first OTA. Existing sketches continue to compile. Do not erase the filesystem during ordinary OTA.

Firmware versions already accept text, including underscores and dots. Set the actual firmware version with SetFirmwareVersion in the sketch; the web upload version is a catalog label and should match it. Changing that label alone does not change the version compiled into a binary.

Validation for ID assignment: server tests cover invalid/missing IDs, unsupported firmware, occupied IDs, and reboot verification against the explicitly assigned ID using the same firmware bytes. TestDev_1 compiled with the upgraded library. Physical OTA assignment was not completed because the currently connected ID 1007 reported no OTA support; it was left unchanged.
