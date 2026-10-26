#pragma once

// Opt-in ESP8266 connection hint. DHCP and normal Wi-Fi authentication remain enabled.
class FastWiFi {
    String ssid;
    String password;
    String cached;
    uint32_t started = 0;
    bool hinted = false;
    bool saved = false;

public:
    void begin(const String& name, const String& pass) {
        ssid = name;
        password = pass;
        StaticJsonDocument<256> doc;
        File file = LittleFS.open("/wifi-hint.json", "r");
        bool valid = file && !deserializeJson(doc, file);
        file.close();
        uint8_t bssid[6];
        int channel = doc["channel"] | 0;
        valid = valid && doc["ssid"].as<String>() == ssid &&
                channel >= 1 && channel <= 14 && doc["bssid"].size() == 6;
        if (valid) {
            for (int i = 0; i < 6; ++i) bssid[i] = doc["bssid"][i].as<uint8_t>();
            serializeJson(doc, cached);
        }
        WiFi.persistent(false);
        started = millis();
        hinted = valid;
        if (valid) WiFi.begin(ssid.c_str(), password.c_str(), channel, bssid);
        else WiFi.begin(ssid.c_str(), password.c_str());
    }

    void run() {
        if (WiFi.status() != WL_CONNECTED) {
            if (hinted && uint32_t(millis() - started) >= 4000) {
                hinted = false;
                WiFi.begin(ssid.c_str(), password.c_str());
            }
            return;
        }
        if (saved) return;
        saved = true;
        Serial.printf("Startup: WiFi ready at %lu ms\n", (unsigned long)millis());
        StaticJsonDocument<256> doc;
        doc["ssid"] = ssid;
        doc["channel"] = WiFi.channel();
        JsonArray address = doc.createNestedArray("bssid");
        for (int i = 0; i < 6; ++i) address.add(WiFi.BSSID()[i]);
        String next;
        serializeJson(doc, next);
        if (next == cached) return; // Avoid flash writes on every boot.
        File file = LittleFS.open("/wifi-hint.tmp", "w");
        if (!file) return;
        bool written = file.print(next) == next.length();
        file.close();
        if (written) LittleFS.rename("/wifi-hint.tmp", "/wifi-hint.json");
    }
};
