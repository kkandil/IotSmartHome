# Event date/time conditions

Install `SmartHome-home-location-debug.apk`, then open **Events**, create or edit a routine, and tap **+** in **If**.

## Choose a condition

- **Variable** keeps the existing device/variable comparison.
- **Date / time → Exact time** lets you pick a time, weekdays, and timezone.
- **Date / time → Daytime / Nighttime** offers two definitions:
  - **Sunrise and sunset – home location:** the Pi detects its approximate city and region automatically using its internet connection. The app displays the city, region and country, without coordinate fields. New conditions use the detected home timezone. The Pi caches the location and refreshes it daily; cached data remains available during internet outages. If no location is available yet, reconnect the Pi and reopen the condition, or select custom hours.
  - **My own start and end times:** define daytime (for example 07:00–20:00). Nighttime is the remaining hours. A daytime range may cross midnight.

Select the weekdays on which the condition should apply. Optionally enable **Only on a specific date** and choose the date. The selected date must also match one of the selected weekdays. Days refer to the current local date, including after midnight.

Save the condition, add the **Then** actions, enable the event, and tap **Save event**.

## Examples

- Every weekday at 08:00: Exact time 08:00, Mon–Fri; action `Power = 1`.
- Motion only at night: Variable `Presence = 1` AND Nighttime; action `Power = 1`.
- An evening action at sunset: Nighttime using sunrise/sunset, all weekdays; add the desired action. This becomes true at sunset.

AND conditions form a group; OR starts another group. Time and variable conditions can be mixed.

## Execution details

The Pi evaluates clock conditions once per second even with the phone closed. It must be powered on and have an accurate clock. Solar calculations are local and require no paid weather service. They use [SunCalc](https://github.com/mourner/suncalc/tree/v1.9.0).

Exact-time conditions match during the selected minute. A group that also contains variable conditions can trigger when those conditions become true during that minute. Each exact-time occurrence runs at most once, including during the repeated hour when daylight saving ends. Missed times while the Pi is offline are not replayed. The nonexistent hour when daylight saving begins is skipped.

Day/night conditions remain true for their whole period. As with existing variable events, actions run when the overall condition changes from false to true, not continuously throughout the period. Saving, enabling, syncing an edited event, or restarting the Pi does not immediately run a condition already true. Choose a future minute when testing an exact-time event.

Actions for disconnected devices retain the existing behavior: skipped, rather than queued.

Location comes from the Pi, never from your travelling phone or the Heroku server. IP geolocation may identify a nearby city or your ISP/VPN location rather than your actual neighbourhood. The approved lookup uses https://ipwhois.io/documentation. Reopen and save existing sunlight conditions to use the detected home location; existing saved rules are not silently rewritten.

## Delivered and verified

- Automatic city display deployed after v46; server commit 0674257.
- Pi server updated with a backup of the previous files.
- 58 server tests passed, including the new clock tests; 17 event tests rerun after the final performance adjustment.
- Android debug build passed. Emulator test verified reopening all time modes and preserving selected values.
- No test actions were sent to your real devices. Install the APK on your phone to try the new editor.
