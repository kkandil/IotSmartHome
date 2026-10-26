# Voice_Living — English voice prototype

ESP32-WROOM-32E + GY-SPH0645 + MAX98357A. Say **Hey Jarvis**, pause briefly,
then say a configured command. No push-to-talk button is needed.

The Pi detects the wake phrase and runs Speech-to-Phrase locally. The ESP32
streams mono 16 kHz, 16-bit audio (about 32 kB/s, plus network overhead) while
listening. It stops sending microphone audio during processing and replies.
Audio is not sent to Heroku and recordings are not stored. The Pi's journal
contains recognized command text for troubleshooting.

## Wiring

Disconnect power while wiring. These pin assignments are for this project.

| GY-SPH0645 | ESP32 |
|---|---|
| VDD / 3V | 3.3 V |
| GND | GND |
| BCLK / SCK | GPIO26 |
| LRCLK / WS | GPIO25 |
| DOUT / SD | GPIO32 |
| SEL / L-R | GND (left channel) |

| MAX98357A | Connection |
|---|---|
| VIN | Regulated 5 V supply |
| GND | Common ESP32/supply ground |
| BCLK | GPIO27 |
| LRC / LRCLK | GPIO14 |
| DIN | GPIO22 |
| SD / shutdown | Leave at the breakout's enabled default; follow its datasheet |
| Speaker + and − | Directly to the speaker terminals |

Use a 4–8 ohm speaker suitable for the amplifier. Neither speaker terminal goes
to ground. Do not power the amplifier from the ESP32's 3.3 V regulator. Use a
stable supply with enough current for Wi-Fi and the speaker. A bare WROOM module
needs its own regulated 3.3 V supply; 5 V is only for the amplifier or a development
board's documented 5 V input. Keep microphone wires short and separate the mic
from the speaker. GPIO2 is an optional status LED (board polarity may vary).

## Build and upload

1. Open this folder in VS Code with PlatformIO.
2. Copy `include/Secrets.example.h` to `include/Secrets.h`.
3. Enter Wi-Fi credentials, Pi address, and the **satellite** token from the Pi's
   private `data/voice.json`. Do not use the hub token. Keep this file private.
4. Confirm `VOICE_DEVICE_ID` is free before assigning it. It currently controls
   the router hostname; the audio endpoint uses `VOICE_SATELLITE_ID`.
5. Build the `esp32dev` environment, then upload over USB. Serial monitor: 115200.
6. The binary is `.pio/build/esp32dev/firmware.bin`. For an erased ESP32, use
   PlatformIO Upload so bootloader and partition table are written too.

This prototype uses a dedicated audio connection, not SmartSnap DeviceConnect.
It does not replace or impersonate an existing device in the app's device list.
The Pi uses the existing SmartSnap write path for the target lamps, so the normal
widgets and their control logs receive the resulting value updates.

## First test

Wait for `Voice: idle`. Say **Hey Jarvis**, wait for `Voice: listening` (LED on),
then **check voice connection**. The speaker should say **Voice control is ready**.
This test does not change a device value.

Then try one of these exact phrases:

- Turn on the right lamp / Turn off the right lamp
- Turn on the left lamp / Turn off the left lamp
- Turn on the right living room lamp / Turn off the right living room lamp
- Turn on the left living room lamp / Turn off the left living room lamp

The initial mappings target the existing living-room smart plugs' `Power`
variables: right 1015 and left 1016, ON=1 and OFF=0. Check the Pi configuration
before using a plug for a different appliance.

`Command sent` means SmartSnap dispatched the command, not an independently
verified physical state. Offline devices produce an error reply, not a queued
future action. Unknown commands are rejected, but speech recognition can still
mishear speech as a supported command. This initial command set is for lamps.

## Tuning and limitations

- Start 0.5–1 m from the microphone in a quiet room.
- If recognition fails, inspect microphone wiring, left-channel selection and
  audio levels before changing the wake threshold.
- `MIC_SHIFT` in `src/main.cpp` controls digital gain: a larger number reduces it.
- The microphone captures stereo I2S and uses slot 0, verified on the connected
  ESP32/SPH0645. Speaker replies duplicate mono audio into both I2S slots.
- `SPEAKER_VOLUME_DIVISOR` starts at 16 (6.25% digital amplitude). Larger values
  are quieter. Speaker buffers are cleared and transmission stops while idle.
- `wakeThreshold` in the Pi config defaults to 0.6. Lower increases sensitivity
  and false wakes; higher reduces both. Restart the voice service after changes.
- The initial endpoint detector uses audio energy and a silence timeout; TV/music
  and room noise need real-hardware tuning. A command has an 8-second maximum.
- Listening pauses during replies; interrupting a spoken reply is not supported.
- English only. No custom wake-phrase training or Arabic recognition in this build.
- The prototype supports at most two simultaneous satellites; test one first.
- Connections are authenticated but **unencrypted on the trusted LAN**. Do not
  expose port 10400 to the internet. Production deployment should add TLS.

See `HubServer/voice/README.md` for Pi installation and command configuration.
