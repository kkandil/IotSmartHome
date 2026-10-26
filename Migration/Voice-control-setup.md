# Voice_Living: your first voice-control test

## What is ready

- Voice device identity: **Voice_Living**, ID **1019**, living room.
- Wake phrase: **Hey Jarvis**. English recognition, no push-to-talk.
- The Pi voice service is installed and running on **192.168.178.89:10400**.
- Firmware project: `PlatformIO/Voice_Living` (environment **esp32dev**).
- The project's private `include/Secrets.h` already contains its satellite token.
- Existing Arduino projects are unchanged. No new Android APK or Heroku deployment
  is needed for this local prototype.

The Pi performs wake detection and speech recognition. Microphone audio streams
over local Wi-Fi while idle/listening, about 32 kB/s before overhead. It is not
sent to Heroku or stored as recordings. The local connection is authenticated,
but not encrypted; keep it on your trusted home LAN without port forwarding.

This first version has a Pi configuration file for voice commands, not an Android
voice-settings screen. The voice satellite uses a dedicated audio connection;
ID 1019 is its identity/router hostname and it does not yet register as an online
device in the app's Devices list. Existing target devices and widgets remain the
normal way to view/control the lamps.

## 1. Connect the hardware

Follow the pin table and power instructions in
`PlatformIO/Voice_Living/README.md`. Use the ESP32-WROOM-32E, GY-SPH0645,
MAX98357A and speaker. A bare ESP32 module needs a 3.3 V regulator; do not connect
5 V directly to it. The amplifier and ESP32 must share ground.

## 2. Enter Wi-Fi details and upload

Open `PlatformIO/Voice_Living` in VS Code/PlatformIO. Edit the **existing**
`include/Secrets.h`, entering `VOICE_WIFI_SSID` and `VOICE_WIFI_PASSWORD`.
Keep its provisioned token and ID. Do not overwrite it with the example file.

Build, then Upload to the ESP32's USB serial port. Use PlatformIO Upload rather
than writing only the application `.bin` to an erased chip. Open Serial Monitor
at **115200 baud** and wait for `Voice: idle`.

The project has compiled successfully; the real microphone and speaker have not
yet been tested. The current build still contains placeholder Wi-Fi credentials
until you edit that file and rebuild.

## 3. Test without controlling anything

Say **Hey Jarvis**, pause briefly for the LED/listening state, then say:

> Check voice connection.

Expected reply: **Voice control is ready.**

The normal serial sequence is idle → listening → processing → speaking → finished
→ idle. Listening pauses during speaker playback. There is an 8-second limit for
the spoken command, so use one short command at a time.

## 4. Try the lamps

| Command | Target |
|---|---|
| Turn on the right lamp | Living_Lamp_Right, ID 1015, Power=1 |
| Turn off the right lamp | Living_Lamp_Right, ID 1015, Power=0 |
| Turn on the left lamp | Living_Lamp_Left, ID 1016, Power=1 |
| Turn off the left lamp | Living_Lamp_Left, ID 1016, Power=0 |

You can also say “right living room lamp” or “left living room lamp.”
The response **Command sent** means the command was dispatched; it does not claim
independent physical confirmation. Offline devices produce an error response.
Repeated identical commands within 10 seconds are suppressed.

## 5. Add commands later

On the Pi, edit `/home/khaled/smarthome/data/voice.json`. Each command has an ID,
explicit English phrases and a target deviceID/varName/varValue. Add its ID to the
living-room satellite's `commands` list too. Preserve the private tokens.

Then run:

```sh
sudo systemctl restart smarthome-voice
journalctl -u smarthome-voice -f
```

The grammar retrains from the allowed phrases. Names/variables are not discovered
automatically from widgets in this prototype. Read-only sensor variables cannot
be written. No free-form AI-generated action is executed.

## Troubleshooting

- No Wi-Fi: check the two credential fields and use a 2.4 GHz network.
- No voice-server connection: check the Pi IP and token, and run
  `systemctl status smarthome-voice` on the Pi.
- No wake: start within 0.5–1 m, check SPH0645 wiring/SEL and microphone gain.
  The default wake threshold is 0.6; tune only after checking audio quality.
- No reply audio: check MAX98357A power, its enabled state, speaker wiring and
  the separate speaker I²S pins.
- Unexpected wake/commands: test away from TV/music. The initial silence detector
  uses audio energy and still needs room-specific hardware testing.
- Pi reboot: diagnostics recorded a previous low-voltage event during development.
  Check the Pi supply/cable if this recurs. The voice service is limited to one
  CPU core; the final software tests passed without another observed reboot.

## Verified on the Pi

Generated audio successfully exercised wake detection, English recognition,
the authenticated hub endpoint, speaker PCM delivery, and return to listening.
The network test used **check voice connection** and did not switch any lamp.
Unauthenticated audio connections were rejected. Firmware build, 65 server
regression tests and 3 utterance-capture tests passed.

Deployment backup:
`/home/khaled/smarthome/voice-backup-20261022-154806.tar.gz`.
To stop voice control: `sudo systemctl disable --now smarthome-voice`.
