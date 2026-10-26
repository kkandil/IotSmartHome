#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "BoardConfig.h"
#include "ConnectionConfig.h"
#include "RelayLogic.h"
#include "RelayPersistence.h"
#include "ServerRestore.h"

SmartSnap smartSnap;
SwitchInput switches[3];
bool relayStates[3] = {false, false, false};
bool reportPending[3] = {true, true, true};
const char* const VARIABLES[] = {"Relay1", "Relay2", "Relay3"};
RelayPersistence persistence;
StartupState startup;
bool storageHealthy = false;
ServerRestore serverRestore;

uint8_t currentStates() {
    uint8_t bits = 0;
    for (uint8_t i = 0; i < 3; ++i) if (relayStates[i]) bits |= 1U << i;
    return bits;
}

void onStartupConfigure(const String& payload) {
    StaticJsonDocument<1024> input;
    if (deserializeJson(input, payload)) return;
    const String requestId = input["requestId"] | "";
    if (requestId.isEmpty() || requestId.length() > 64) return;
    String error;
    const String action = input["action"] | "";
    if (action == "save") {
        const String policy = input["policy"] | "";
        if (policy != "off" && policy != "preserve") error = "Invalid startup policy";
        else {
            StartupState next;
            next.preserve = policy == "preserve";
            next.states = currentStates();
            if (persistence.save(next)) { startup = next; storageHealthy = true; }
            else { storageHealthy = false; error = "Could not verify saved settings. Read settings before retrying."; }
        }
    } else if (action == "read") {
        StartupState stored;
        if (persistence.load(stored)) {
            startup = stored;
            storageHealthy = !stored.pending;
        } else {
            storageHealthy = false;
            error = "Could not read saved startup settings";
        }
    } else error = "Unknown settings action";
    StaticJsonDocument<512> reply;
    reply["requestId"] = requestId;
    reply["status"] = error.isEmpty() ? "OK" : "Error";
    reply["message"] = error;
    reply["policy"] = startup.preserve ? "preserve" : "off";
    reply["storageHealthy"] = storageHealthy;
    String output;
    serializeJson(reply, output);
    smartSnap.SendStartupResult(output);
}

void latchOutputs(uint8_t outputs) {
    digitalWrite(LATCH_PIN, LOW);
    shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, outputs);
    digitalWrite(LATCH_PIN, HIGH);
}

void pulseRelay(uint8_t relay, bool on) {
    // Blank before loading: never energize SET and RESET together.
    digitalWrite(OE_PIN, HIGH);
    latchOutputs(coilMask(relay, on));
    digitalWrite(OE_PIN, LOW);
    // Bounded pulse: no network/OTA/flash operations while a coil is active.
    // delay yields to the ESP watchdog, but does not call SmartSnap.Run().
    delay(COIL_PULSE_MS);
    digitalWrite(OE_PIN, HIGH);
    latchOutputs(0);
    relayStates[relay] = on;
    reportPending[relay] = true;
}

void setRelay(uint8_t relay, bool on) {
    if (relayStates[relay] != on) {
        if (startup.preserve) {
            // Commit intent BEFORE pulsing. A power failure during the operation
            // leaves pending=true; next boot resets all contacts to known OFF.
            StartupState next = startup;
            next.states = currentStates();
            if (on) next.states |= 1U << relay;
            else next.states &= ~(1U << relay);
            next.pending = true;
            if (!persistence.save(next)) {
                storageHealthy = false;
                Serial.println("Relay not changed: unable to save recovery journal");
                reportPending[relay] = true;
                return;
            }
            startup = next;
        }
        pulseRelay(relay, on);
        if (startup.preserve) {
            StartupState next = startup;
            next.states = currentStates();
            next.pending = false;
            storageHealthy = persistence.save(next);
            if (storageHealthy) startup = next;
            else Serial.println("Relay changed; recovery journal pending, next boot will reset OFF");
        }
    }
    reportPending[relay] = true; // Also acknowledge an idempotent write.
}

void onWrite(const String& name, const String& type, const String& value) {
    for (uint8_t i = 0; i < 3; ++i) {
        if (name != VARIABLES[i]) continue;
        if (value == "0" || value == "1") {
            serverRestore.accept(i); // A newer live command wins over an outstanding read.
            setRelay(i, value == "1");
        } else {
            Serial.println("Rejected relay value: use 0 or 1");
            reportPending[i] = true;
        }
        return;
    }
}

bool onRead(const String& name, String& type, String& value) {
    for (uint8_t i = 0; i < 3; ++i) {
        if (name != VARIABLES[i]) continue;
        if (serverRestore.waiting[i]) return false; // Do not overwrite pending server state.
        type = "int";
        value = relayStates[i] ? "1" : "0";
        return true;
    }
    return false;
}

void onServerValue(const String& name, const String& value) {
    for (uint8_t i = 0; i < 3; ++i) {
        if (name != VARIABLES[i] || !serverRestore.waiting[i]) continue;
        if (value != "0" && value != "1") {
            Serial.println("Ignoring invalid saved relay value; expected 0 or 1");
            return;
        }
        serverRestore.accept(i);
        setRelay(i, value == "1");
        Serial.printf("Startup: %s restored at %lu ms\n", VARIABLES[i], (unsigned long)millis());
        return;
    }
}

void requestSavedRelayValues() {
    for (uint8_t i = 0; i < 3; ++i) {
        if (serverRestore.waiting[i]) smartSnap.RequestVariableValueFromServer(VARIABLES[i]);
    }
}

void onConnected(bool connected) {
    if (connected) {
        Serial.printf("Startup: hub ready at %lu ms\n", (unsigned long)millis());
        serverRestore.connect(REQUEST_SERVER_STATE_ON_CONNECT, millis());
        for (bool& pending : reportPending) pending = true;
        requestSavedRelayValues();
    }
}

void setup() {
    // GPIO3 belongs to OE, not UART RX. TX logging remains on GPIO1.
    digitalWrite(OE_PIN, HIGH);
    pinMode(OE_PIN, OUTPUT);
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    // Reassert pin ownership after UART initialization, including future changes.
    digitalWrite(OE_PIN, HIGH);
    pinMode(OE_PIN, OUTPUT);
    for (uint8_t pin : {DATA_PIN, CLOCK_PIN, LATCH_PIN}) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    latchOutputs(0);

    for (uint8_t i = 0; i < 3; ++i) {
        // GPIO16 has no pull-up: fit an external 10k to 3.3V on Button3.
        pinMode(SWITCH_PINS[i], SWITCH_PINS[i] == 16 ? INPUT : INPUT_PULLUP);
    }
    persistence.begin();
    const bool validStartup = persistence.load(startup);
    if (startup.canKeepContacts(validStartup)) {
        // Latching relays physically retain their state; no startup pulse needed.
        for (uint8_t i = 0; i < 3; ++i) relayStates[i] = (startup.states & (1U << i)) != 0;
        storageHealthy = true;
    } else {
        for (uint8_t i = 0; i < 3; ++i) pulseRelay(i, false);
        startup.states = 0;
        startup.pending = false;
        storageHealthy = persistence.save(startup);
    }
    for (uint8_t i = 0; i < 3; ++i) {
        switches[i].begin(digitalRead(SWITCH_PINS[i]), millis());
    }

    smartSnap.SetFirmwareVersion("3IOSwitch_1.2.2");
    smartSnap.SetCapability(SmartSnap::Capability::StartupBehavior);
    smartSnap.onStartupConfigure(onStartupConfigure);
    smartSnap.SetNonBlockingConnect();
    smartSnap.onWriteVariable(onWrite);
    smartSnap.onServerValue(onServerValue);
    smartSnap.onReadVariable(onRead);
    smartSnap.onConnectionStatus(onConnected);
    smartSnap.Initialize(HOME_NAME, DEVICE_ID, WIFI_SSID, WIFI_PASSWORD, HUB_HOST, HUB_PORT);
}

void loop() {
    for (uint8_t i = 0; i < 3; ++i) {
        if (switches[i].update(digitalRead(SWITCH_PINS[i]), millis(),
                               DEBOUNCE_MS, MOMENTARY_SWITCHES)) {
            serverRestore.accept(i); // A wall-switch action during sync wins over a delayed read.
            setRelay(i, !relayStates[i]);
        }
    }
    smartSnap.Run();
    if (smartSnap.IsConnected()) {
        if (serverRestore.retryDue(millis())) requestSavedRelayValues();
        for (uint8_t i = 0; i < 3; ++i) {
            if (!reportPending[i] || serverRestore.waiting[i]) continue;
            reportPending[i] = false;
            smartSnap.WriteVariableValue(VARIABLES[i], "int", relayStates[i] ? "1" : "0");
        }
    }
    delay(1);
}
