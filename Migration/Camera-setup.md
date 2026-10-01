# Imou camera integration

The IPC-A22E-D can be viewed through the home Pi. No firmware change is needed on the camera. This first version is a video-only preview, resized to 640 pixels wide with selectable 1, 5, 10, 15 or 20 fps (default 5). It uses the camera main profile because the tested secondary profile did not produce video. Audio, pan/tilt, snapshots and recording are not included in this version.

## After deployment

1. Run the updated Android app from Android Studio.
2. Open Home, enable Edit Mode, then Add Widget > Display > Camera Card.
3. Open its settings and tap Choose camera. Select the configured Imou camera and save widget settings.
4. Return to Live Mode and tap Play. Fullscreen opens a larger viewer and keeps the current playback state. Android Back returns to Home.
5. The green play icon starts video and becomes a red pause icon. Navigation and fullscreen share playback state. Backgrounding suspends streaming; returning resumes only cameras you had played. Closing the app starts a fresh paused session next time. Paused footage is marked Last frame.

For another Imou camera, use Add / update camera in widget settings. Supply its local IPv4 address, camera username and camera password. These are camera credentials, not your Imou cloud login. Selecting an existing camera first updates it; an empty password preserves its existing password. Device-edit permission is required to configure cameras; home access is required to view them.

## Frame rate

Choose Frame rate in the Camera widget settings. This is a target, not a guaranteed rate. Higher rates increase data usage, Pi CPU work, and phone battery use. Cards for the same camera share playback in the app; the latest opened card sets that session’s rate. Across multiple phones the Pi produces the highest requested rate, while the gateway limits each viewer to its requested rate.

## Network and storage

Reserve the camera IP (currently 192.168.178.40) in the router so it does not change. The Pi must remain powered on and able to reach the camera. Remote viewing needs the Pi's outbound internet connection; do not expose RTSP port 554 on the router.

The Pi stores camera credentials in cameras.json alongside its DATA_FILE, with owner-only permissions. They are not included in shared widget layouts or cloud database records. Setup requests pass over the authenticated HTTPS connection and the authenticated Pi bridge. Frames are relayed in memory and not recorded. Camera traffic uses a separate Socket.IO connection from device controls.

## Troubleshooting

- No cameras listed: check that the updated Pi bridge is connected to Heroku.
- Camera unavailable: verify the local IP and camera password, privacy mode and stream availability.
- Paused last frame: tap Play to open a fresh connection.
- Slow stream: this is a capped JPEG preview; internet bandwidth, Wi-Fi and Pi CPU load affect the achieved frame rate.
