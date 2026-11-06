# Event duplication and action groups

## Duplicate an event

Long-press an event and choose Duplicate, or open its editor and tap Duplicate event.
The new draft is named "original name (copy)" and starts disabled. It copies the conditions, trigger durations, and actions but not the event identity or run history. Rename/edit it and Save event. Until you save it, no new event exists. Enable it when ready.

## Create a reusable action group

1. Select the intended home.
2. Open Events and the toolbar overflow menu, then Action groups.
3. Tap New action group, enter a name, and add actions with the + button.
4. Pick each device, variable, and value just as in an event.
5. Tap Save action group.

Groups belong to the selected home and are stored by the gateway. They are available to that home's authorized users; editing requires Events permission. Maximum: 100 groups per home, 32 actions per group.

## Use a group

In a new or existing event, tap Use action group beneath Then, and select the group. Its actions are appended as editable copies. Add the conditions and save the event.

Editing or deleting a group does not modify events that already copied its actions. There are no live group links or nested groups. If a device/variable referenced by a group has been deleted, remove or replace that action before saving/using it. The server validates targets, types, and read-only measurements when saving; event cycle checks still apply.

## Installation and checks

Install SmartHome-action-groups-debug.apk. The gateway support is deployed. No Pi or firmware update is needed for these features; events still execute on the Pi through the existing event engine.

123 backend tests passed, including group persistence, isolation, invalid values/targets, duplicate names, revision conflicts, idempotency, detached copies, and event duplication. Android debug build and JVM unit tests passed. Physical-device visual interaction was not tested. No existing live events or devices were changed by the tests.
