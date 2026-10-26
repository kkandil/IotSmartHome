# Smart plug electrical readings and energy

Install SmartHome-plug-energy-debug.apk, then refresh the app's devices.

The four existing linked plugs are upgraded automatically. For newly linked plugs, supported electrical measurements are discovered during Smart Life setup; only mapped measurements are exposed.

## Sensor cards

Add or edit a Sensor Card. Select a smart plug and one of these variables:

| Variable | Unit | Meaning |
|---|---|---|
| PowerWatts | W | Current measured load power |
| Voltage | V | Measured supply voltage |
| Current | mA | Measured current |
| EnergyToday | kWh | Estimated consumption during the home's current day |
| EnergyTotal | kWh | Estimated consumption since this feature was enabled |

The existing Power variable still controls ON/OFF. Do not bind the watts display to Power.

Under Unit choose W, kW, V, A, mA, Wh, kWh or Hz. Existing units remain available. Custom unit opens a text field; enter any suffix up to 16 characters, or leave it blank for no unit. Unit selection changes the displayed suffix only; it does not convert values. For example Current is in mA, so selecting A alone does not divide it by 1000.

## Charts

Tap a watts/voltage/current Sensor Card to open its normal reading history. Recording starts when that variable is used by a sensor card.

Tap an EnergyToday or EnergyTotal Sensor Card to open estimated consumption: Day uses hourly bars; Week and Month use daily bars. Use the arrows, date picker and Today button to navigate. The large number is the sum for the selected period. Tap a bar for its value and time. Charts use the Pi home's timezone and support light/dark mode. Energy records are kept on the Pi independently of whether an energy card exists. The energy chart does not offer the sensor-history clear action.

## Updates and accuracy

The Pi polls locally approximately every five seconds and publishes electrical variables every 30 seconds, including unchanged readings. No phone connection is required for collection. Measurement variables are read-only.

Energy is estimated by integrating measured watts over consecutive successful samples. Failed polls, restarts and gaps longer than 30 seconds are not filled with assumed usage. The chart reports measured minutes; incomplete coverage means the total can be lower than actual consumption. A partial current day is expected when first enabling this feature.

The plug's add_ele field is not used for totals because its reporting/reset behavior has not been verified; repeated status reads are not blindly added to consumption. These totals do not import Smart Life history or reproduce its lifetime total. They begin when this integration is enabled, and remain saved through Pi restarts.

Electrical mapping lookup uses the linked Tuya project during setup. Ongoing readings and energy calculations run locally over Wi-Fi.
