#include "PrayerSettings.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
int main() {
    PrayerSettings s = defaultSettings();
    assert(validSettings(s)); assert(s.volume == 15);
    for (auto enabled : s.enabled) assert(enabled == 1);
    assert(!strcmp(s.url, "https://mawaqit.net/de/m/izstuttgart"));
    s.volume = 25; assert(!validSettings(s));
    s.checksum = settingsChecksum(s); assert(validSettings(s));
    s.volume = 31; s.checksum = settingsChecksum(s); assert(!validSettings(s));
    s = defaultSettings(); s.enabled[2] = 2; s.checksum = settingsChecksum(s); assert(!validSettings(s));
    s = defaultSettings(); memset(s.url, 'x', sizeof(s.url)); assert(!validSettings(s));
    int value = -1;
    assert(parseSettingInt("0", 30, value) && value == 0);
    assert(parseSettingInt("30", 30, value) && value == 30);
    for (auto text : {"31", "-1", "1.5", "", "99999", "abc", "1x"}) assert(!parseSettingInt(text, 30, value));
    assert(parseSettingInt("1", 1, value)); assert(!parseSettingInt("2", 1, value));
    assert(validUrl("https://mawaqit.net/de/m/izstuttgart"));
    assert(!validUrl("http://example.com")); assert(!validUrl("https://")); assert(!validUrl("https:///bad"));
    assert(!validUrl("https://example.com/a b"));
    s = defaultSettings(); s.magic = 0xffffffff; assert(!validSettings(s));
    puts("Prayer settings tests passed");
}
