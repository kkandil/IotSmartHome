#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "StartupState.h"

class RelayPersistence {
public:
    bool begin() { ready = LittleFS.begin(); return ready; }

    bool load(StartupState& state) {
        if (!ready) return false;
        File file = LittleFS.open("/relay-startup.json", "r");
        if (!file || file.size() > 256) return false;
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, file)) return false;
        if (doc["version"] != 1 || !doc["preserve"].is<bool>() ||
            !doc["states"].is<int>() || !doc["pending"].is<bool>()) return false;
        const int bits = doc["states"];
        if (bits < 0 || bits > 7) return false;
        state.preserve = doc["preserve"];
        state.states = bits;
        state.pending = doc["pending"];
        return true;
    }

    bool save(const StartupState& state) {
        if (!ready) return false;
        StaticJsonDocument<256> doc;
        doc["version"] = 1;
        doc["preserve"] = state.preserve;
        doc["states"] = state.states;
        doc["pending"] = state.pending;
        File file = LittleFS.open("/relay-startup.tmp", "w");
        if (!file) return false;
        const size_t written = serializeJson(doc, file);
        file.flush();
        file.close();
        if (written != measureJson(doc) ||
            !LittleFS.rename("/relay-startup.tmp", "/relay-startup.json")) return false;
        StartupState check;
        return load(check) && check.preserve == state.preserve &&
               check.states == state.states && check.pending == state.pending;
    }

private:
    bool ready = false;
};
