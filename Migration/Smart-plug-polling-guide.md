# Smart plug polling and confirmed commands

Background reads now default to every 30 seconds. In Smart Life plugs, use the Background updates button beneath a linked plug to select Off, 15 seconds, 30 seconds, 1 minute, or 5 minutes. Settings are saved on the Pi per plug.

Commands are independent of that interval. A command waits only for an already running operation on the same plug; pending commands prevent new background reads. The worker bounds background operations to 4 seconds and commands to 8 seconds. Different plugs can operate independently.

Events and schedules wait for Tuya's read-back confirmation before recording an action as sent. A failed action does not stop remaining actions; the event records partial failure. Ordinary ESP devices retain their existing transport-level result.

Two consecutive failed background reads retain the plug connection. A third marks it offline; the next successful read restores it. Explicit commands can still attempt the plug even when a previous poll marked it offline.

Off disables periodic reads after the initial connection check. It does not disable commands or explicit refreshes. Changes made in Smart Life or with the physical button are not continuously detected, and continuous energy estimation stops. Slower intervals reduce the detail and accuracy of energy estimates. Failed reads break energy integration, avoiding interpolation across known outages.

Verification: 70 server regression tests passed, including delayed and rejected confirmation, continuation after a failed event action, polling failure tolerance, disabled polling, saved polling preferences, and slower energy sampling. No real appliances were switched during automated tests.
