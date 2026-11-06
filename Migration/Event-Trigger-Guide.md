# Condition trigger duration

Install SmartHome-event-trigger-debug.apk. The Pi and Heroku updates were deployed on 28 October 2026. No firmware change is required.

## Configure the lights

Create or edit these two events, selecting the actual Presence sensor and light variables:

1. Light ON: Presence = 1, Trigger = Immediately, action Power = 1.
2. Light OFF: Presence = 0, Trigger = After remaining true for, Hours 00 / Minutes 15 / Seconds 00, action Power = 0.

Tap the duration numbers to type, or use the + / minus controls. Save the condition, then save the event. Existing conditions remain Immediate until edited.

The duration belongs to each condition, not the whole event. AND requires every condition to be ready; OR permits a ready alternative group. Each condition's duration is tracked independently even while another condition is false. An event runs on the transition from not matching to matching, not repeatedly while matching.

Repeated matching reports do not reset the timer. A false reading resets it. The Pi's one-second event clock finishes the countdown even if the sensor sends no further readings. The phone can be closed.

A detected source-device disconnect cancels its delayed conditions and clears their cached evaluation values. A fresh report after reconnecting starts a new duration. Saving/editing/enabling an event or restarting the Pi starts a full new duration if its condition is currently known to match; downtime is not counted. Timers are intentionally not restored across Pi restarts. An already matching Immediate condition retains existing behavior: saving/enabling does not immediately execute it.

The configured LD2420 disappearance delay occurs before Presence becomes 0, so it adds to the 15 minutes.

For a date/time condition, its time window must remain true for the duration. An exact-time condition is true only during its selected minute, so a delay of 60 seconds or more cannot complete in that window. Use Immediately for exact times, and delayed triggers for continuous variable or daytime/nighttime conditions.

## Checks

- Backend: 122 tests passed, including timer cancellation, duplicate reports, AND/OR, offline/reconnect, restart, edit/disable/delete, validation, time windows, and gateway save/reload.
- Android: debug build and JVM unit tests passed.
- Pi deployment: service health check passed, backup at /home/khaled/event-trigger-backup-f2JWv0.
- Device UI/hardware interaction was not exercised; the Android emulator was unavailable in this session. Existing live events were not changed and real lights were not toggled by tests.
