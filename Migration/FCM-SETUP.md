# Firebase Cloud Messaging setup

The app uses Firebase only for messaging. Accounts, sharing, device data, schedules and events stay on the existing servers.

## Configuration needed

1. Open https://console.firebase.google.com/ and create/select a project. Analytics is optional.
2. Add an Android app with package name `com.example.smarthome`.
3. Download `google-services.json` into `SmartHome/app/google-services.json`. This file is ignored by Git. Do not use a service-account file here.
4. In Project settings > Service accounts > Firebase Admin SDK, generate a private key. Save it locally as `Migration/private/firebase-service-account.json`. This directory is ignored by Git. Do not paste the key into chat or put it in the Android app.
5. Configure the Heroku app `smarthomehub` with `FIREBASE_SERVICE_ACCOUNT_JSON` containing this file's JSON, using a private API/config upload. Never print it in logs. The Android and service-account files must belong to the same Firebase project. Ensure Firebase Cloud Messaging API (HTTP v1) is enabled.
6. Sync Gradle and run from Android Studio on each phone. The build supports working without configuration, but background push remains unavailable until the real files/credentials are installed.
7. In Device Details, enable notifications for the device and allow Android's notification permission. Open the dialog again to check registration status.

## Test

Keep the Pi online. In the device firmware call the existing `SendNotificationToPhone("Test message")`. Verify one notification with the app visible, then press Home or swipe the app from Recents, lock the phone, and trigger a different message. Repeat on a shared user's phone with that device enabled. Disable the device on one phone and verify it stops receiving alerts. Do not use Android Settings > Force stop for this test: a force-stopped application must be opened again.

The Pi now forwards a separate authenticated HubNotification to Heroku regardless of phone socket connections. Heroku checks the home/device, current membership, active account session, and installation preferences before sending a high-priority FCM data message. The receiver verifies the intended account/session and local opt-in before showing an Android notification. Socket and FCM messages share a notification ID for duplicate suppression.

MongoDB collections: `push_installations` stores per-phone registration tokens, enabled device subscriptions and session binding; `push_receipts` holds one-day deduplication receipts. FCM tokens are not passwords but should not be printed or shared. Sign-out unregisters the session; receiving code also ignores delayed messages after sign-out.

Messages are not durably queued on the Pi across an Internet outage. FCM messages expire after five minutes. Android/OEM restrictions, loss of Internet or a force-stopped app can still prevent delivery; this is not an alarm delivery guarantee.

References:
- https://firebase.google.com/docs/cloud-messaging/android/get-started
- https://firebase.google.com/docs/cloud-messaging/android/receive-messages
- https://firebase.google.com/docs/admin/setup

## Device alarms

Firmware can call `api.SendAlarmToPhone("Smoke detected in kitchen");`. Like the notification API, it returns whether the connected device emitted the message, not a delivery confirmation. The Pi receives `DeviceWriteAlarm` and sends an authenticated, typed alarm through the same bridge/FCM path. Call it when the alarm becomes active rather than on every firmware loop iteration.

On each phone, open Device Details, enable device notifications, then enable **Allow alarm sounds**. This preference is local to that user and phone and defaults off. With alarm sounds off, alarm messages are ordinary notifications. Turning off device notifications disables both kinds.

The alarm service uses the alarm audio stream and a visible **Silence** action. Playback/vibration ends after 60 seconds from the first active alert; additional alerts do not extend that time. The **Alarm sound settings** button opens Android's alarm notification channel for tone, vibration and DND settings. No app code raises the user's volume or disables DND. When Android blocks background playback, the app falls back to a notification. Delayed alarms older than two minutes do not start playback; alarm FCM messages have a one-minute TTL.

This is supplemental remote notification, not a replacement for an independent certified smoke alarm or local siren. Test on each phone model, including lock screen, volume/DND and battery settings.
