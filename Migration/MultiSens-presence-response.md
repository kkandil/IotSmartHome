# MultiSens presence response update

Firmware: MultiSens_1.4.3

The VEML7700 automatic lux routine restarted gain/integration selection on every sample and blocked the application while waiting for conversions. This prevented buffered LD2420 readings and socket traffic from being serviced promptly.

This update uses a retained adaptive gain at 100 ms integration, reads without waiting, and allows conversion settling between loop iterations. Invalid/saturated light readings are not published. Presence change reporting remains independent of ReportInterval. Existing threshold-crossing and delta reporting behavior is retained.

Flash MultiSens_1.4.3-presence-response.bin through the existing OTA workflow or build PlatformIO/MultiSens. No APK, Pi, or Heroku update is required. Do not erase stored settings.

Validation: PlatformIO ESP8266 build passed. Physical entry-to-light latency is not yet measured. At inspection, LivingMotion_On and LivingMotion_Off were disabled; neither was changed. Re-enable the desired event for testing. ON had holdSeconds=0; OFF had holdSeconds=60.

Test: leave until Presence becomes 0, then enter several times, comparing Presence in the app with physical lamp response. Confirm Light still responds to covering/uncovering and scheduled reporting. Avoid having the presence tuning read/save operation active during timing measurements, since sensor configuration temporarily interrupts measurement.
