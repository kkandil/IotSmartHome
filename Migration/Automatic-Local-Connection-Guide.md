# Automatic local home connection

## Install and prepare
1. Install `SmartHome-local-connection-debug.apk` over the current app.
2. While internet is available and the Pi is online, sign in and open each home you want to access locally. This securely prepares this phone for that home.
3. Open **Me → App settings → Home connection**. **Automatic local connection** is enabled by default. Save after changing it.
4. On the home's Wi-Fi, the header shows **Connected locally** when the secure Pi connection is active. Otherwise it uses Heroku and shows **Connected remotely**.

When enabled, the app prefers the reachable Pi on Wi-Fi and falls back to Heroku. When disabled, the app uses Heroku only. The setting applies to this phone; it does not disable automations on the Pi. Route checks run every 15 seconds, so switching is not instantaneous. Pending saves are allowed to finish before switching.

## Internet outages
The Pi can evaluate existing events and schedules locally and control devices that remain connected to its LAN. The router/Wi-Fi, Pi and target devices must remain powered and connected. Internet-only actions, push notifications and external services still require internet.

The phone can show current readings, control variables, and read available local history over the secure local connection. Account and configuration edits are cloud-managed; temporarily disable automatic local connection to make those edits while internet is available. Camera access is not changed by this update.

No firmware update is required for this phone connection feature. Devices must already be configured to connect to the Pi, not an internet-only server.

## Security and prerequisites
- The Pi listens for local HTTPS on port 3443. Do not forward this port on the router.
- The phone verifies the Pi certificate using a fingerprint delivered through the authenticated Heroku connection.
- Local credentials are encrypted on the phone, scoped to the account/home/phone, and expire after 30 days. They refresh while online. Reopen the home online if local access expires.
- Signing out clears this phone's cached local credentials. Membership revocations reach the Pi when its cloud connection is available.
- Keep a DHCP reservation for the Pi so its saved local IP remains stable. Guest Wi-Fi/client isolation can prevent access.
- First-time pairing/setup still requires internet. A phone that has never prepared access cannot discover an untrusted Pi during an outage.

## Verification performed
- 145 backend tests passed, including secure local phone authentication, cross-home denial, denial of firmware impersonation/configuration commands, credential expiry/revocation, and a simulated sensor-triggered automation after removing the gateway link.
- Android debug build and JVM unit tests passed.
- These are automated tests, not a physical phone/router-outage test. The exact cause of the previously reported overnight automation failure was not established.
