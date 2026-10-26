# Device templates

1. Install SmartHome-device-templates-debug.apk.
2. Open Me > Device Template > New template.
3. Enter a name, add variables, and expand each variable card to edit its name,
   Int/float/String type, default value (initially 0), and Configuration flag.
4. Tap Save template once for the entire template.
5. Open Devices > Add device > SmartSnap device. Enter a device name and select
   a template below it. None is selected by default.
6. Review the compact variable preview and tap Create device.

Templates are private to the signed-in account and reusable across its homes.
Creating a device copies the variables in one server configuration revision.
Editing or deleting a template never modifies devices already created from it.
Device firmware must still implement the variables and advertise its own
capabilities; a template does not flash or provision firmware.

Validation includes unique variable names, supported types, numeric defaults,
50 templates/account and 100 variables/template. Conflicting edits from another
phone require reopening the editor. Defaults and configuration flags are retained.

Verification: Android build and unit tests; 96 server tests passed, including
account isolation, revision conflicts, default validation, no-template creation,
atomic creation, and duplicate-request handling. Physical phone UI verification
has not been performed for this update.
