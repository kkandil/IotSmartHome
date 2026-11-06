#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
struct PrayerSettings {
    uint32_t magic;
    uint8_t volume;
    uint8_t enabled[5];
    char url[257];
    uint32_t checksum;
};
inline uint32_t settingsChecksum(const PrayerSettings& s) {
    uint32_t hash = 2166136261u;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&s);
    for (size_t i = 0; i < offsetof(PrayerSettings, checksum); ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
inline bool validUrl(const char* url) {
    const size_t n = strlen(url);
    if (n <= 8 || n > 256 || strncmp(url, "https://", 8)) return false;
    for (size_t i = 8; i < n; ++i) if (static_cast<unsigned char>(url[i]) <= 32 || url[i] == '\x7f') return false;
    return url[8] != '/' && url[8] != '?';
}
inline bool validSettings(const PrayerSettings& s) {
    if (s.magic != 0x50525931 || s.volume > 30 || !memchr(s.url, 0, sizeof(s.url))) return false;
    for (uint8_t enabled : s.enabled) if (enabled > 1) return false;
    return validUrl(s.url) && settingsChecksum(s) == s.checksum;
}
inline PrayerSettings defaultSettings() {
    PrayerSettings s{};
    s.magic = 0x50525931;
    s.volume = 15;
    for (uint8_t& enabled : s.enabled) enabled = 1;
    strcpy(s.url, "https://mawaqit.net/de/m/izstuttgart");
    s.checksum = settingsChecksum(s);
    return s;
}
inline bool parseSettingInt(const char* text, int maximum, int& value) {
    if (!text || !*text) return false;
    for (const char* p = text; *p; ++p) if (*p < '0' || *p > '9') return false;
    if (strlen(text) > 3) return false;
    value = atoi(text);
    return value <= maximum;
}
