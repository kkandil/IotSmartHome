# Event notifications and alarms

Install SmartHome-event-alerts-debug.apk.

1. Open Events and create or edit an event.
2. Set its conditions as usual.
3. Under Then, tap +. Choose Device variable, Notification, or Alarm.
4. For Notification or Alarm, enter the message and add the action.
5. Save the event. You can mix messages and device-variable actions in one event, or save them in action groups.

The Pi evaluates conditions even when the app is closed. Existing trigger timing and false-to-true behavior apply; saving an event does not immediately trigger it.

In Me > App settings, Event alerts controls receiving alerts and playing alarm sounds for the selected home on this phone. Saving your first alert event enables these defaults unless you previously chose a preference. Other phones must opt in separately. Android notification permission is also required.

Automation history shows Alert sent for these actions, not Device confirmed. This indicates handoff to the existing alert delivery path, not proof that a phone displayed or sounded the alert. Notification history retains the message and timestamp.

Local foreground delivery can work without internet. Background push delivery requires internet. Missed push alerts are not automatically replayed. Existing SendNotificationToPhone and SendAlarmToPhone remain available; no firmware update is needed.

Validation: 149 backend tests passed; Android assembleDebug and testDebugUnitTest passed. Pi and Heroku backend deployed. Physical phone display/audio has not been tested in this session.
