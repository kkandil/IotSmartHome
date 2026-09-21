# Raspberry Pi migration

## Installed architecture

- Pi: `SmartHubEgypt`, user `khaled`, current LAN address `192.168.178.89`.
- Local Socket.IO server: `http://192.168.178.89:3000`.
- Gateway: `https://smarthomehub-1aec4600a734.herokuapp.com`.
- The Pi initiates an authenticated outbound WebSocket connection. No inbound router forwarding or static public IP is needed.
- The Pi owns the SQLite data. Heroku relays phone requests and responses; it does not keep a second database.
- The original `smarthome` Heroku app and `Server/` source remain unchanged.
- Weekly variable scheduling is now installed; see `HubServer/SCHEDULES.md`.

Current official MongoDB ARM packages do not support the Pi 4 CPU. The separate `HubServer/` implementation uses Node 24's SQLite module and preserves the existing app/device event names, IDs, values, and scheduling fields.

## Data cutover

The initial read-only MongoDB export contains two homes, nine devices, 34 variables, and the global ID counter. It is retained in `private/source-snapshot.json` and on the Pi at `/home/khaled/smarthome/source-snapshot.json`.

**The old cloud database and the Pi are separate after this snapshot.** Devices still using Heroku continue updating only the old app/database. Values on the Pi become live when devices are reflashed to connect locally. `TestDev_1` (Home_Germany, ID 1007) has now been flashed on COM7 and verified against the Pi through the gateway; other devices have not been flashed.

Both homes were copied to preserve existing IDs. A device must be on a network that can reach this Pi; a device at a different physical home cannot use this private LAN address without an additional network arrangement or local hub.

## Android

Install `SmartHome-hub-debug.apk`. It uses the existing app ID, so an update should preserve dashboards when signing certificates match. Do not uninstall the existing app just to bypass a signing mismatch; that could erase its locally saved dashboard layouts.

Open the overflow menu → **Server connection**:

1. Press **Use SmartHome gateway**.
2. Enter the access key from `private/android-connection.txt`.
3. Press **Connect**.

The app keeps the original endpoint until this setting is changed. While at home, enter `http://192.168.178.89:3000` for direct operation, including internet outages. Switching between local and gateway is manual in this migration. The gateway key is sent only for HTTPS addresses.

Dashboard layouts remain on the phone. On gateway/hub reconnection, the app reloads homes/devices and refreshes bound widget values from the Pi. No commands are queued for replay while the hub is unavailable.

## Firmware

`Devices/SmartSnap/src/SmartSnapAPI.cpp` now honors `serverHost` and `serverPort`. Its default endpoint is unchanged for existing sketches that omit those arguments.

The living-room and TestDev_1 examples include `SmartSnapHubConfig.h`, selecting `192.168.178.89:3000`. Existing Wi-Fi settings, home names, and IDs are preserved. Both compile for `esp8266:esp8266:nodemcuv2`. The living-room example has not been flashed.

TestDev_1 was flashed on COM7 (ESP8266EX, detected 4 MB flash); the upload checksum passed. Live gateway tests set its integer control to 37 and float control to 12.50, received the device's state updates, and verified fresh device reads. Both controls were restored to their initial runtime values (0 and 0.00). The reproducible check is `HubServer/scripts/test-physical-testdev.js`.

For another sketch using this library, pass the Pi host and port after the existing Wi-Fi password argument to `Initialize`. Some other device directories contain independent older SmartSnap copies; those require individual review before flashing.

Reserve the Pi's LAN address in the router before device rollout, or update the configuration when the address changes. This is unrelated to static public IPs or port forwarding.

## Pi operation

Files:

- Service: `/etc/systemd/system/smarthome-local.service`
- Code: `/home/khaled/smarthome/current` → `releases/schedules-v1` (initial release remains at `releases/v1`)
- Runtime: `/home/khaled/smarthome/runtime/node`
- Environment: `/home/khaled/smarthome/hub.env` (private)
- Database: `/home/khaled/smarthome/data/smarthome.sqlite`

SSH commands:

```sh
systemctl status smarthome-local
journalctl -u smarthome-local -n 50 --no-pager
sudo systemctl restart smarthome-local
curl http://127.0.0.1:3000/health
```

Backup the live database using SQLite's backup API:

```sh
DATA_FILE=/home/khaled/smarthome/data/smarthome.sqlite \
  /home/khaled/smarthome/runtime/node/bin/node \
  /home/khaled/smarthome/current/scripts/backup.js \
  /home/khaled/smarthome/backups/manual-backup.sqlite
```

The Pi's local service works without internet while the Pi, router/Wi-Fi, and devices have power. Remote gateway access needs internet. No internet connection was physically disconnected during verification; outage/recovery was exercised with the bridge in the integration test.

## Validation

- Android `assembleDebug` succeeded using the existing SDK/JDK and isolated build output.
- ESP8266 living-room sketch compiled successfully; not flashed.
- Integration test uses legacy Socket.IO clients and covers CRUD, command delivery, live updates, on-demand reads, authentication rejection, concurrent phones, gateway outage/recovery, and server restart persistence.
- Live Pi + Heroku smoke test uses a temporary home and simulated device, then removes its test home. It never controls existing physical devices.
- Snapshot verification compares all imported device IDs/names and variable values/types/schedule fields through the deployed gateway.
- The Pi service was restarted and the gateway reconnected automatically; all nine devices and 34 variables still matched the snapshot. A consistent post-migration backup is saved at `/home/khaled/smarthome/backups/after-migration.sqlite`.
- Production npm dependencies have zero reported audit vulnerabilities at deployment. The legacy client used only for compatibility testing has old dependencies and is pruned from production installations.

## Rollback and hosting

Choose `https://smarthome.herokuapp.com/` in the app to return to the original server. Existing unflashed devices still use that server. A flashed device would also need its server address restored to revert it.

Eco hours are shared between the old app and gateway. The new gateway uses the existing Eco plan; no upgrade was purchased. Two continuously active dynos can exceed the monthly shared allowance. Exhausting it affects remote access but not the Pi's local service. The original app has deliberately not been scaled down.

Keep `private/` out of version control; it contains database exports and gateway keys. The local Pi retains the legacy trusted-LAN device interface and must not be exposed directly with router port forwarding.
