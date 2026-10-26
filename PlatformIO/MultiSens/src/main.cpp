#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <SmartSnapAPI.h>
#include <math.h>
#include <LittleFS.h>
#include <stdlib.h>
#include "DeviceConfig.h"
#include "ReportBaseline.h"

SmartSnap smartSnap;
Adafruit_VEML7700 light;
Adafruit_AHTX0 climate;
Adafruit_BMP280 barometer;
bool lightReady = false, climateReady = false, pressureReady = false;
bool reportRequested = true, resetUpdate = false;
uint32_t lastReport = 0, lastSample = 0;
const char* const SETTINGS[] = {"ReportInterval", "LightThreshold", "LightUpdateThr", "TempUpdateThreshold"};
double settings[] = {15, 13, 50, 0.5};
bool waitingSettings[4] = {false, false, false, false};
ReportBaseline lightBaseline, temperatureBaseline;
LightThresholdState lightThresholdState;
bool flashReady = false;

int settingIndex(const String& name) {
    for (int i = 0; i < 4; ++i) if (name == SETTINGS[i]) return i;
    return -1;
}

bool validSetting(int i, double value) {
    if (!isfinite(value) || value < 0) return false;
    if (i == 0) return value >= 1 && value <= 86400 && floor(value) == value;
    return value <= (i == 3 ? 100 : 200000);
}

bool saveSettings(const double* values) {
    if (!flashReady) return false;
    StaticJsonDocument<256> doc;
    for (int i = 0; i < 4; ++i) doc[SETTINGS[i]] = values[i];
    File file = LittleFS.open("/multisens-settings.tmp", "w");
    if (!file) return false;
    bool ok = serializeJson(doc, file) > 0;
    file.flush();
    file.close();
    return ok && LittleFS.rename("/multisens-settings.tmp", "/multisens-settings.json");
}

void loadSettings() {
    flashReady = LittleFS.begin();
    if (!flashReady) return;
    StaticJsonDocument<256> doc;
    File file = LittleFS.open("/multisens-settings.json", "r");
    if (!file || deserializeJson(doc, file)) return;
    for (int i = 0; i < 4; ++i) {
        if (doc[SETTINGS[i]].is<double>() && validSetting(i, doc[SETTINGS[i]].as<double>()))
            settings[i] = doc[SETTINGS[i]].as<double>();
    }
}

void receiveSetting(int i, const String& text) {
    char* end;
    double value = strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0' || !validSetting(i, value)) {
        Serial.printf("Rejected invalid %s\n", SETTINGS[i]);
        return;
    }
    if (settings[i] != value) {
        double next[4];
        for (int j = 0; j < 4; ++j) next[j] = settings[j];
        next[i] = value;
        if (!saveSettings(next)) {
            Serial.println("Settings save failed; retaining previous settings");
            return;
        }
        settings[i] = value;
    }
    waitingSettings[i] = false;
}

// One request per setting after connection; subsequent edits arrive via onWrite.
void requestSettings() {
    for (int i = 0; i < 4; ++i)
        if (waitingSettings[i]) smartSnap.RequestVariableValueFromServer(SETTINGS[i]);
}
uint8_t pressureAddress = 0;
String luxValue, temperatureValue, humidityValue, pressureValue, bmpTemperatureValue;

bool responds(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void initializeSensors() {
    if (!lightReady && responds(0x10)) lightReady = light.begin(&Wire);
    if (!climateReady && responds(0x38)) climateReady = climate.begin(&Wire);
    if (!pressureReady) {
        for (uint8_t address : {uint8_t(0x76), uint8_t(0x77)}) {
            if (responds(address) && barometer.begin(address)) {
                pressureReady = true;
                pressureAddress = address;
                break;
            }
        }
    }
}

void publishNumber(const char* name, float value, String& cached, bool valid) {
    if (!valid || !isfinite(value)) {
        cached = ""; // Do not return an old reading as if it were fresh.
        return;
    }
    cached = String(value, 2);
    smartSnap.WriteVariableValue(name, "float", cached);
}

void sampleAndPublish(bool fullReport) {
    initializeSensors();
    String errors;
    float lux = NAN, temperature = NAN, humidity = NAN, pressure = NAN, bmpTemp = NAN;
    if (lightReady && responds(0x10)) lux = light.readLux(VEML_LUX_AUTO);
    if (!isfinite(lux) || lux < 0) { lightReady = false; errors += "VEML7700 "; }

    sensors_event_t humidityEvent{}, temperatureEvent{};
    if (climateReady && responds(0x38) && climate.getEvent(&humidityEvent, &temperatureEvent)) {
        temperature = temperatureEvent.temperature;
        humidity = humidityEvent.relative_humidity;
    }
    bool climateValid = isfinite(temperature) && isfinite(humidity) && humidity >= 0 && humidity <= 100;
    if (!climateValid) { climateReady = false; errors += "AHT20 "; }
    if (pressureReady && responds(pressureAddress)) {
        bmpTemp = barometer.readTemperature();
        pressure = barometer.readPressure() / 100.0f; // Pa -> hPa (not sea-level corrected).
    }
    bool pressureValid = isfinite(pressure) && pressure > 0 && isfinite(bmpTemp);
    if (!pressureValid) { pressureReady = false; errors += "BMP280 "; }

    // Every published reading becomes the comparison baseline. Extra updates
    // still leave the full-report timer unchanged.
    bool lightCrossed = lightThresholdState.update(lux, settings[1]);
    bool lightTrigger = lightBaseline.exceeds(lux, settings[2]);
    bool tempTrigger = climateValid && temperatureBaseline.exceeds(temperature, settings[3]);
    if (fullReport || lightCrossed || (lightTrigger && String(lux, 2) != luxValue)) {
        publishNumber("Light", lux, luxValue, lightReady);
        if (lightReady && isfinite(lux)) lightBaseline.reported(luxValue.toFloat());
    }
    if (fullReport || (tempTrigger && String(temperature, 2) != temperatureValue)) {
        publishNumber("Temp", temperature, temperatureValue, climateValid);
        if (climateValid) temperatureBaseline.reported(temperatureValue.toFloat());
    }
    if (!fullReport) return;
    publishNumber("Humd", humidity, humidityValue, climateValid);
    publishNumber("Pressure", pressure, pressureValue, pressureValid);
    publishNumber("BMPTemp", bmpTemp, bmpTemperatureValue, pressureValid);
    smartSnap.WriteVariableValue("status", "string", errors.isEmpty() ? "OK" : "Sensor error: " + errors);
    if (resetUpdate) {
        smartSnap.WriteVariableValue("Update", "int", "0");
        resetUpdate = false;
    }
    lastReport = millis();
}

void requestUpdate(const String& name, const String& value) {
    if (name == "Update" && value == "1") {
        reportRequested = true;
        resetUpdate = true;
    }
}

void onWrite(const String& name, const String& type, const String& value) {
    int i = settingIndex(name);
    if (i >= 0) receiveSetting(i, value);
    else requestUpdate(name, value); // Queue sensor work outside the callback.
}

void onServerValue(const String& name, const String& value) {
    int i = settingIndex(name);
    if (i >= 0) {
        if (waitingSettings[i]) receiveSetting(i, value);
    } else requestUpdate(name, value);
}

void onConnected(bool connected) {
    if (connected) {
        reportRequested = true;
        for (bool& waiting : waitingSettings) waiting = true;
        requestSettings();
        // Recover an Update command saved while this device was offline.
        smartSnap.RequestVariableValueFromServer("Update");
    }
}

bool onRead(const String& name, String& type, String& value) {
    int i = settingIndex(name);
    if (i >= 0) {
        // Never overwrite a saved server setting before it has been read.
        if (waitingSettings[i]) return false;
        type = i == 0 ? "int" : "float";
        value = String(settings[i], i == 0 ? 0 : 3);
        return true;
    }
    type = "float";
    if (name == "Light") value = luxValue;
    else if (name == "Temp") value = temperatureValue;
    else if (name == "Humd") value = humidityValue;
    else if (name == "Pressure") value = pressureValue;
    else if (name == "BMPTemp") value = bmpTemperatureValue;
    else return false;
    return !value.isEmpty(); // For a new measurement, write Update=1.
}

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    loadSettings();
    smartSnap.SetFirmwareVersion("MultiSens_1.2.2");
    smartSnap.SetCapability(SmartSnap::Capability::CustomConfiguration);
    smartSnap.SetNonBlockingConnect();
    smartSnap.onWriteVariable(onWrite);
    smartSnap.onServerValue(onServerValue);
    smartSnap.onReadVariable(onRead);
    smartSnap.onConnectionStatus(onConnected);
    smartSnap.Initialize(HOME_NAME, DEVICE_ID, WIFI_SSID, WIFI_PASSWORD, HUB_HOST, HUB_PORT);
}

void loop() {
    smartSnap.Run();
    if (smartSnap.IsConnected()) {
        bool full = reportRequested || uint32_t(millis() - lastReport) >= uint32_t(settings[0] * 1000);
        if (full || uint32_t(millis() - lastSample) >= 1000) {
            reportRequested = false;
            sampleAndPublish(full);
            lastSample = millis();
        }
    }
    delay(1);
}
