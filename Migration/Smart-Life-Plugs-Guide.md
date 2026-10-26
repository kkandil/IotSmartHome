# Smart Life smart plugs

## Add a plug
1. Install the updated SmartHome APK.
2. Select the home whose Pi shares the plug's Wi-Fi network.
3. Open Devices, tap +, and choose Smart Life plug.
4. Paste the Tuya project's Access ID and Access Secret. Choose Central Europe for your project. Project Code is not required.
5. Tap Load smart plugs. Only single-outlet Wi-Fi plugs with a recognized power datapoint and local key are listed.
6. Select a plug, choose its SmartHome name, and leave the optional LAN IP blank for automatic discovery from the Pi. Discovery matches the selected device ID and checks its local connection. If discovery fails, enter its LAN IP manually.
7. Test connection reads the current power state without switching the appliance. Tap Add plug after a successful test.
8. Bind a switch/button/checkbox widget to the new device's Power variable. Use ON/checked = 1 and OFF/unchecked = 0.

The Pi polls the plug every 5 seconds and verifies commanded state changes. Routine control uses the LAN; Tuya cloud is used to obtain setup information and keys. The phone still uses the existing SmartHome gateway to reach the Pi. Keep Enable Offline Write disabled for these plugs in this initial version: deferred writes are not replayed by the plug adapter.

## Credentials and maintenance
The app does not save the project credentials. The authenticated gateway relays setup requests to the Pi. The Pi stores project credentials and plug keys in its private tuya.json file (mode 0600); device lists returned to the phone contain no keys. Leave both credential fields blank to reuse the saved project.

Reserve the plug's IP in the router. If its IP changes or it is re-paired in Smart Life, load devices and test/add the same plug again to update its local configuration. Importing the same Tuya device is idempotent and preserves its SmartHome device ID. Deleting the SmartHome device stops local polling once configuration synchronizes.

An authorization error requires checking the exact Client ID/Secret, data center, Smart Life account link, and Tuya API authorization/subscription. The corrected Client ID authenticated successfully. Tuya returned error 28841002: IoT Core service subscription has expired. Request a trial extension under Cloud > My Services > IoT Core, then retry discovery. No real plug control has been verified yet.

## Pi installation
Use Python 3 with a dedicated virtual environment and install HubServer/tuya-requirements.txt. Set TUYA_PYTHON to that environment's Python and TUYA_CONFIG_FILE to a persistent private path outside the release directory. Deploy tuya-local.js, tuya-worker.py, bridge.js and local-server.js, then restart the service. The gateway also needs tuya-cloud.js and the updated config-store.js/multi-gateway.js.
