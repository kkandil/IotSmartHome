#pragma once
#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "LD2420.h"
#include "../../shared/RadarEnergyWindow.h"

// Same radar behavior as MotionSensor_Office, sharing MultiSens' connection.
extern SmartSnap smartSnap;
namespace PresenceSensor {
#ifndef RADAR_BAUD
#define RADAR_BAUD 115200
#endif
// Zero disables periodic distance reports. Live tuning still reads on demand.
constexpr uint32_t DISTANCE_REPORT_INTERVAL_MS = 0;
constexpr int DISTANCE_CHANGE_CM = 20;
constexpr uint32_t RADAR_FRESHNESS_MS = 3000;

LD2420 radar(Serial);
String pendingRequest;
DynamicJsonDocument currentConfig(2048);
bool configValid = false;
bool reportNeeded = true;
bool lastPresence = false;
int lastDistance = 0;
uint32_t lastDistanceReport = 0;

// Bounded RAM history; no flash writes or extra network messages.
struct EnergyFrame {
    uint32_t time;
    uint16_t energy[16];
};
EnergyFrame recentFrames[16];
uint8_t recentCount = 0, recentNext = 0;
uint32_t previousFrameTime = 0, detectionTime = 0, detectionSequence = 0;
bool observedFrame = false, previousPresence = false;
DynamicJsonDocument lastDetection(3072);
RadarEnergyWindow energyWindow;
uint32_t quietSince = 0;
bool quietTracked = false;

void captureDetectionFrame() {
    const uint32_t now = millis();
    const bool continuous = observedFrame && now - previousFrameTime < RADAR_FRESHNESS_MS;
    if (!continuous) { recentCount = recentNext = 0; energyWindow.clear(); quietTracked = false; }
    energyWindow.add(now, radar.energy);
    if (configValid) {
        bool below = true;
        for (int g = currentConfig["minGate"].as<int>(); g <= currentConfig["maxGate"].as<int>(); ++g)
            if (radar.energy[g] >= currentConfig["still"][g].as<int>()) below = false;
        if (!below || !quietTracked) quietSince = now;
        quietTracked = true;
    }
    if (continuous && !previousPresence && radar.presence && configValid) {
        lastDetection.clear();
        lastDetection["id"] = ++detectionSequence;
        detectionTime = now;
        lastDetection["distanceCm"] = radar.distance;
        lastDetection["config"] = currentConfig.as<JsonObject>();
        JsonArray energy = lastDetection.createNestedArray("energy");
        JsonArray peaks = lastDetection.createNestedArray("peaks");
        JsonArray candidates = lastDetection.createNestedArray("candidates");
        uint32_t windowMs = 0;
        for (int gate = 0; gate < 16; ++gate) {
            uint16_t peak = radar.energy[gate];
            for (int i = 0; i < recentCount; ++i) {
                const uint32_t age = now - recentFrames[i].time;
                if (age <= 1500) {
                    peak = max(peak, recentFrames[i].energy[gate]);
                    windowMs = max(windowMs, age);
                }
            }
            energy.add(radar.energy[gate]);
            peaks.add(peak);
            if (gate >= currentConfig["minGate"].as<int>() &&
                gate <= currentConfig["maxGate"].as<int>() &&
                peak > currentConfig["move"][gate].as<int>()) candidates.add(gate);
        }
        lastDetection["windowMs"] = windowMs;
    }
    EnergyFrame& frame = recentFrames[recentNext];
    frame.time = now;
    memcpy(frame.energy, radar.energy, sizeof(frame.energy));
    recentNext = (recentNext + 1) % 16;
    recentCount = min(16, int(recentCount) + 1);
    previousPresence = radar.presence;
    observedFrame = true;
    previousFrameTime = now;
}

bool hasFreshReading() {
    return radar.hasFrame && millis() - radar.lastFrame < RADAR_FRESHNESS_MS;
}

void handleConnection(bool connected) {
    // Publish a fresh snapshot once after every successful reconnection.
    if (connected) {
        reportNeeded = true;
    }
}

void queueRequest(const String& json) {
    if (pendingRequest.isEmpty()) {
        pendingRequest = json;
    }
}

bool validConfig(JsonObject config) {
    if (!config["minGate"].is<int>() || !config["maxGate"].is<int>() ||
        !config["delay"].is<int>()) {
        return false;
    }

    const int minimum = config["minGate"];
    const int maximum = config["maxGate"];
    const int delaySeconds = config["delay"];
    if (minimum < 0 || maximum > 15 || minimum > maximum ||
        delaySeconds < 0 || delaySeconds > 65535) {
        return false;
    }

    for (const char* key : {"move", "still"}) {
        JsonArray thresholds = config[key];
        if (thresholds.size() != 16) {
            return false;
        }
        for (JsonVariant value : thresholds) {
            if (!value.is<int>() || value.as<int>() < 0 || value.as<int>() > 65535) {
                return false;
            }
        }
    }
    return true;
}

bool sameConfig(JsonObject requested, JsonObject verified) {
    for (const char* key : {"minGate", "maxGate", "delay"}) {
        if (requested[key].as<int>() != verified[key].as<int>()) {
            return false;
        }
    }
    for (int gate = 0; gate < 16; ++gate) {
        for (const char* key : {"move", "still"}) {
            if (requested[key][gate].as<int>() != verified[key][gate].as<int>()) {
                return false;
            }
        }
    }
    return true;
}

void processRequest() {
    if (pendingRequest.isEmpty()) {
        return;
    }

    DynamicJsonDocument input(3072);
    DynamicJsonDocument output(8192);
    const String json = pendingRequest;
    pendingRequest = "";
    if (deserializeJson(input, json)) {
        return;
    }

    output["requestId"] = input["requestId"];
    const String action = input["action"].as<String>();
    bool ok = true;

    if (action == "read" || action == "save") {
        configValid = false;
        observedFrame = false;
        energyWindow.clear(); quietTracked = false;
        recentCount = recentNext = 0;
    }
    if (action == "read") {
        currentConfig.clear();
        configValid = radar.read(currentConfig.to<JsonObject>()) &&
                      validConfig(currentConfig.as<JsonObject>());
        ok = configValid;
    } else if (action == "save") {
        DynamicJsonDocument verified(2048);
        JsonObject requested = input["config"];
        ok = validConfig(requested) &&
             radar.save(requested, verified.to<JsonObject>()) &&
             validConfig(verified.as<JsonObject>()) &&
             sameConfig(requested, verified.as<JsonObject>());
        configValid = ok;
        if (ok) {
            currentConfig.set(verified);
        }
    } else if (action != "status") {
        ok = false;
    }

    if (action == "read" || action == "save") {
        observedFrame = false;
        energyWindow.clear(); quietTracked = false;
        recentCount = recentNext = 0;
    }
    output["status"] = ok ? "OK" : "Error";
    if (!ok) {
        output["message"] = "LD2420 did not confirm settings. Check wiring/firmware; "
                            "read settings before retrying.";
    }
    output["configured"] = configValid;
    if (configValid) {
        output["config"] = currentConfig.as<JsonObject>();
    }

    const bool fresh = hasFreshReading();
    output["live"] = fresh;
    if (fresh) {
        output["presence"] = radar.presence;
        output["distanceCm"] = radar.distance;
        JsonArray energies = output.createNestedArray("energy");
        for (auto value : radar.energy) {
            energies.add(value);
        }
    }

    if (fresh && configValid) {
        uint16_t peaks[16], lows[16]; uint32_t span;
        const uint32_t frames = energyWindow.read(millis(), peaks, lows, span);
        JsonObject window = output.createNestedObject("energyWindow");
        window["frames"] = frames; window["spanMs"] = span;
        window["quietMs"] = quietTracked ? uint32_t(millis() - quietSince) : 0;
        JsonArray high = window.createNestedArray("peaks");
        JsonArray low = window.createNestedArray("lows");
        for (int g = 0; g < 16; ++g) { high.add(peaks[g]); low.add(lows[g]); }
    }
    output["detectionSnapshots"] = true;
    if (!lastDetection.isNull()) {
        output["lastDetection"] = lastDetection.as<JsonObject>();
        output["lastDetection"]["ageMs"] = uint32_t(millis() - detectionTime);
    }

    String result;
    serializeJson(output, result);
    smartSnap.SendPresenceResult(result);
}

bool readVariable(const String& name, String& type, String& value) {
    if (!hasFreshReading()) {
        return false;
    }
    type = "int";
    if (name == "Presence") {
        value = radar.presence ? "1" : "0";
        return true;
    }
    if (name == "Distance") {
        value = String(radar.distance);
        return true;
    }
    return false;
}

void publishChanges() {
    if (!hasFreshReading()) {
        // Resynchronize once fresh data returns; never invent an absence reading.
        reportNeeded = true;
        return;
    }
    if (!smartSnap.IsConnected()) {
        return;
    }

    const uint32_t now = millis();
    if (reportNeeded || radar.presence != lastPresence) {
        smartSnap.WriteVariableValue("Presence", "int", radar.presence ? "1" : "0");
        smartSnap.WriteVariableValue("Distance", "int", String(radar.distance));
        reportNeeded = false;
        lastPresence = radar.presence;
        lastDistance = radar.distance;
        lastDistanceReport = now;
    } else if (DISTANCE_REPORT_INTERVAL_MS > 0 && radar.presence &&
               now - lastDistanceReport >= DISTANCE_REPORT_INTERVAL_MS &&
               abs(int(radar.distance) - lastDistance) >= DISTANCE_CHANGE_CM) {
        smartSnap.WriteVariableValue("Distance", "int", String(radar.distance));
        lastDistance = radar.distance;
        lastDistanceReport = now;
    }
}


void begin() {
    // UART0 is reserved for radar; debug output uses Serial1 / GPIO2.
    Serial1.begin(115200);
    Serial.setRxBufferSize(2048);
    Serial.begin(RADAR_BAUD);
    Serial.swap();
    Serial.setDebugOutput(false);
    delay(500);
    radar.onFrame = captureDetectionFrame;
    smartSnap.onPresenceConfigure(queueRequest);
    smartSnap.SetCapability(SmartSnap::Capability::LD2420);
    configValid = radar.read(currentConfig.to<JsonObject>()) &&
                  validConfig(currentConfig.as<JsonObject>());
}

void run() {
    radar.poll();
    processRequest();
    publishChanges();
}
} // namespace PresenceSensor
