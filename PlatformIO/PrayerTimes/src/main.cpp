#include "DeviceConfig.h"
#include "DeviceDefinition.h"
#include "PrayerSettings.h"
#include <EEPROM.h>
#include <SmartSnapAPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

#if (defined(ARDUINO_AVR_UNO) || defined(ESP8266))   // Using a soft serial port
#include <SoftwareSerial.h>
SoftwareSerial softSerial(/*rx =*/12, /*tx =*/13);
#define FPSerial softSerial
#else
#define FPSerial softSerial
#endif

DFRobotDFPlayerMini myDFPlayer;
int soundVolume =15;

// LCD pins: CLK, DIN, DC, CE, RST
Adafruit_PCD8544 display = Adafruit_PCD8544(D5, D1, D2, D0, D8);


SmartSnap smartSnap;
PrayerSettings settings = defaultSettings();
bool waiting[7] = {};
bool playerReady = false;
bool scheduleReady = false;
bool forceFetch = true;
bool fetching = false;
int scheduleDate = -1;
uint32_t lastFetchAttempt = 0;
const char* const settingNames[] = {"Volume", "DataSourceUrl", "Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
void serviceNetworkAndButton();
void checkStopButton();
void updateVolumeByButton();
int dateKey(const tm& t) { return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday; }
const char* TZ_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";

String prayerTimes[5];
const char* prayerNames[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
const char* shortNames[5] = {"F", "D", "A", "M", "I"};

bool alreadyPlayed[5] = {false, false, false, false, false};

bool prayerScreenActive = false;
int activePrayerIndex = -1;
unsigned long prayerScreenStart = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBlink = 0;
bool blinkState = true;

int lastFetchDay = -1;
int lastCheckedMinute = -1;
int listPage = 0;
unsigned long lastPageSwitch = 0;
int prayerTimesUpdateCycles = 0;
int prayerTimesUpdateTrials = 0;

const int STOP_BUTTON_PIN = D3;

void playPrayerSound(int prayerIndex) {
  Serial.print("PLAY SOUND FOR: ");
  Serial.println(prayerNames[prayerIndex]);

  // Add your DFPlayer code here
  if (playerReady && settings.enabled[prayerIndex]) myDFPlayer.play(1);
}

void stopPrayerSound() {

  Serial.println("Sound stopped by button");
  if (playerReady) myDFPlayer.stop();
}

// ---------- Time helpers ----------

String currentHHMM() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  char buf[6];
  sprintf(buf, "%02d:%02d", t->tm_hour, t->tm_min);
  return String(buf);
}

int timeToMinutes(String t) {
  return t.substring(0, 2).toInt() * 60 + t.substring(3, 5).toInt();
}

int currentMinutes() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  return t->tm_hour * 60 + t->tm_min;
}

int getNextPrayerIndex() {
  int nowMin = currentMinutes();

  for (int i = 0; i < 5; i++) {
    if (timeToMinutes(prayerTimes[i]) > nowMin) {
      return i;
    }
  }

  return 0; // tomorrow Fajr
}

String getCountdownToNextPrayer() {
  int nowMin = currentMinutes();
  int nextIndex = getNextPrayerIndex();
  int target = timeToMinutes(prayerTimes[nextIndex]);

  if (target <= nowMin) {
    target += 24 * 60;
  }

  int diff = target - nowMin;
  int h = diff / 60;
  int m = diff % 60;

  return String(h) + "h " + String(m) + "m";
}

// ---------- Mawaqit stream parser ----------

bool readUntilPattern(WiFiClient* stream, const char* pattern, unsigned long timeoutMs) {
  int matched = 0;
  int len = strlen(pattern);
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (stream->available()) {
      char c = stream->read();

      if (c == pattern[matched]) {
        matched++;
        if (matched == len) return true;
      } else {
        matched = (c == pattern[0]) ? 1 : 0;
      }

      serviceNetworkAndButton();
    }
    serviceNetworkAndButton();
    delay(1);
  }

  return false;
}

bool readNextTime(WiFiClient* stream, String &out, unsigned long timeoutMs) {
  unsigned long start = millis();
  char buf[6];
  int idx = 0;

  while (millis() - start < timeoutMs) {
    while (stream->available()) {
      char c = stream->read();

      if ((c >= '0' && c <= '9') || c == ':') {
        if (idx < 5) buf[idx++] = c;

        if (idx == 5) {
          buf[5] = '\0';

          if (buf[0] >= '0' && buf[0] <= '2' &&
              buf[1] >= '0' && buf[1] <= '9' && (buf[0] != '2' || buf[1] <= '3') &&
              buf[2] == ':' &&
              buf[3] >= '0' && buf[3] <= '5' &&
              buf[4] >= '0' && buf[4] <= '9') {
            out = String(buf);
            return true;
          }

          idx = 0;
        }
      } else {
        idx = 0;
      }

      serviceNetworkAndButton();
    }
    serviceNetworkAndButton();
    delay(1);
  }

  return false;
}

bool skipToCalendarMonth(WiFiClient* stream, int targetMonthIndex, unsigned long timeoutMs) {
  int depth = 0;
  int monthIndex = -1;
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (stream->available()) {
      char c = stream->read();

      if (c == '{') {
        if (depth == 0) {
          monthIndex++;
          if (monthIndex == targetMonthIndex) {
            return true;
          }
        }
        depth++;
      } else if (c == '}') {
        depth--;
      }

      serviceNetworkAndButton();
    }
    serviceNetworkAndButton();
    delay(1);
  }

  return false;
}

bool fetchPrayerTimes() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  const int requestedDate = dateKey(*t);
  int monthIndex = t->tm_mon;   // 0 = Jan, 5 = June
  int day = t->tm_mday;

  Serial.println("Downloading prayer times from calendar...");
  Serial.print("Month index: ");
  Serial.println(monthIndex);
  Serial.print("Day: ");
  Serial.println(day);

  BearSSL::WiFiClientSecure client;
  client.setInsecure(); // Same TLS policy as the original sketch; see README.
  client.setTimeout(5000);


  HTTPClient https;
  https.setTimeout(5000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.useHTTP10(true);

  const String requestedUrl(settings.url);
  if (!https.begin(client, requestedUrl)) {
    Serial.println("HTTPS begin failed");
    return false;
  }

  https.setUserAgent("Mozilla/5.0");
  https.addHeader("Accept-Encoding", "identity");
  https.addHeader("Accept", "text/html");

  int code = https.GET();
  Serial.print("HTTP: ");
  Serial.println(code);

  if (code != 200) {
    https.end();
    return false;
  }

  WiFiClient* stream = https.getStreamPtr();

  if (!readUntilPattern(stream, "\"calendar\":[", 30000)) {
    Serial.println("calendar not found");
    https.end();
    return false;
  }

  if (!skipToCalendarMonth(stream, monthIndex, 30000)) {
    Serial.println("month not found");
    https.end();
    return false;
  }

  char dayPattern[12];
  sprintf(dayPattern, "\"%d\":[", day);

  if (!readUntilPattern(stream, dayPattern, 30000)) {
    Serial.println("day not found");
    https.end();
    return false;
  }

  String dayTimes[6];

  for (int i = 0; i < 6; i++) {
    if (!readNextTime(stream, dayTimes[i], 5000)) {
      Serial.println("failed reading calendar time");
      https.end();
      return false;
    }
  }

  https.end();

  if (requestedUrl != settings.url) return false; // URL edited during download.
  const int fetchedDate = requestedDate;
  if (scheduleDate != fetchedDate) for (bool& played : alreadyPlayed) played = false;
  scheduleDate = fetchedDate;
  scheduleReady = true;
  prayerTimes[0] = dayTimes[0]; // Fajr
  prayerTimes[1] = dayTimes[2]; // Dhuhr
  prayerTimes[2] = dayTimes[3]; // Asr
  prayerTimes[3] = dayTimes[4]; // Maghrib
  prayerTimes[4] = dayTimes[5]; // Isha

  Serial.println("Correct prayer times from calendar:");
  for (int i = 0; i < 5; i++) {
    Serial.print(prayerNames[i]);
    Serial.print(": ");
    Serial.println(prayerTimes[i]);
  }



  return true;
}

// ---------- LCD screens ----------

void drawNormalScreen() {
  int nextIndex = getNextPrayerIndex();

  display.clearDisplay();
  display.setTextColor(BLACK);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(currentHHMM());

  display.setCursor(50, 0);
  display.print(String(soundVolume));

  display.setCursor(72, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "OK" : "X");

  display.drawLine(0, 9, 83, 9, BLACK);

  display.setCursor(0, 12);
  display.print("Next ");
  display.print(shortNames[nextIndex]);
  display.print(" ");
  display.print(prayerTimes[nextIndex]);

  display.setCursor(0, 22);
  display.print("In ");
  display.print(getCountdownToNextPrayer());

  display.drawLine(0, 31, 83, 31, BLACK);

  if (millis() - lastPageSwitch > 5000) {
    lastPageSwitch = millis();
    listPage = !listPage;
  }

  if (listPage == 0) {
    display.setCursor(0, 33);
    display.print("D");
    display.print(prayerTimes[1]);

    display.setCursor(42, 33);
    display.print(" A");
    display.print(prayerTimes[2]);

    display.setCursor(0, 40);
    display.print("M");
    display.print(prayerTimes[3]);

    display.setCursor(42, 40);
    display.print(" I");
    display.print(prayerTimes[4]);
  } else {
    display.setCursor(0, 33);
    display.print("F");
    display.print(prayerTimes[0]);

    display.setCursor(0, 40);
    display.print("DU: ");
    display.print(String(lastFetchDay));
  }

  display.display();
}

void drawBlinkBorder() {
  if (!blinkState) return;

  for (int x = 0; x < 84; x += 6) {
    display.fillRect(x, 0, 3, 3, BLACK);
    display.fillRect(x, 45, 3, 3, BLACK);
  }

  for (int y = 0; y < 48; y += 6) {
    display.fillRect(0, y, 3, 3, BLACK);
    display.fillRect(81, y, 3, 3, BLACK);
  }
}

void drawPrayerScreen() {
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    blinkState = !blinkState;
  }

  display.clearDisplay();
  display.setTextColor(BLACK);

  drawBlinkBorder();

  display.setTextSize(2);

  int nameX = 0;
  if (String(prayerNames[activePrayerIndex]).length() <= 3) nameX = 24;
  else if (String(prayerNames[activePrayerIndex]).length() <= 5) nameX = 12;
  else nameX = 0;

  display.setCursor(nameX, 11);
  display.print(prayerNames[activePrayerIndex]);

  display.setCursor(12, 28);
  display.print(prayerTimes[activePrayerIndex]);

  display.display();
}

// ---------- Logic ----------

void checkPrayerTimes() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  if (!scheduleReady || dateKey(*t) != scheduleDate || now < 1700000000) return;
  if (t->tm_min == lastCheckedMinute) return;
  lastCheckedMinute = t->tm_min;

  String nowStr = currentHHMM();

  for (int i = 0; i < 5; i++) {
    if (!alreadyPlayed[i] && nowStr == prayerTimes[i]) {
      alreadyPlayed[i] = true;

      activePrayerIndex = i;
      prayerScreenActive = true;
      prayerScreenStart = millis();

      playPrayerSound(i);
    }
  }
}

// Retry a failed daily/source refresh once per minute, never in a tight loop.
void dailyUpdateCheck() {
  time_t now = time(nullptr);
  if (now < 1700000000 || WiFi.status() != WL_CONNECTED || fetching) return;
  const tm today = *localtime(&now);
  const bool needed = forceFetch || !scheduleReady || scheduleDate != dateKey(today);
  if (!needed || (lastFetchAttempt && millis() - lastFetchAttempt < 60000)) return;
  lastFetchAttempt = millis();
  forceFetch = false;
  fetching = true;
  if (fetchPrayerTimes()) lastFetchDay = today.tm_mday;
  else forceFetch = true;
  fetching = false;
}
bool ButtonState = HIGH;
void checkStopButton() {
  static bool lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;

  bool reading = digitalRead(STOP_BUTTON_PIN);

  if (reading != lastButtonState) {
    Serial.println("Debounce");
    lastDebounceTime = millis();
    lastButtonState = reading;
  }

  if ((millis() - lastDebounceTime) > 100) {
    if (reading == LOW && ButtonState == HIGH) {
      ButtonState = LOW;
      Serial.println("pressed");
      if(prayerScreenActive == true)
      {
        stopPrayerSound();
      }
      else
      {
        updateVolumeByButton();
      }
    }
    else if(reading == HIGH && ButtonState == LOW) {
      ButtonState = HIGH;
    }
  }

  lastButtonState = reading;
}


int settingIndex(const String& name) {
    for (int i = 0; i < 7; ++i) if (name == settingNames[i]) return i;
    return -1;
}
String settingValue(int i) {
    if (i == 0) return String(settings.volume);
    if (i == 1) return String(settings.url);
    return String(settings.enabled[i - 2]);
}
bool persist(PrayerSettings next) {
    next.checksum = settingsChecksum(next);
    if (memcmp(&next, &settings, sizeof(next)) == 0) return true;
    EEPROM.put(0, next);
    if (!EEPROM.commit()) {
        EEPROM.put(0, settings);
        Serial.println("EEPROM save failed; settings retained");
        return false;
    }
    const bool urlChanged = strcmp(next.url, settings.url) != 0;
    settings = next;
    soundVolume = settings.volume;
    if (playerReady) myDFPlayer.volume(soundVolume);
    if (urlChanged) { forceFetch = true; scheduleReady = false; lastFetchAttempt = 0; }
    return true;
}
bool applySetting(int i, const String& value) {
    PrayerSettings next = settings;
    int number;
    if (i == 1) {
        if (!validUrl(value.c_str())) return false;
        strcpy(next.url, value.c_str());
    } else {
        if (!parseSettingInt(value.c_str(), i == 0 ? 30 : 1, number)) return false;
        if (i == 0) next.volume = number;
        else next.enabled[i - 2] = number;
    }
    const bool saved = persist(next);
    if (saved && i >= 2 && activePrayerIndex == i - 2 && !next.enabled[i - 2]) stopPrayerSound();
    return saved;
}
void updateVolumeByButton() {
    const int next = settings.volume + 5 > 30 ? 5 : settings.volume + 5;
    if (!applySetting(0, String(next))) return;
    waiting[0] = false; // A new local press supersedes an outstanding startup read.
    if (smartSnap.IsConnected()) smartSnap.WriteVariableValue("Volume", "int", String(next));
}
void onWrite(const String& name, const String& type, const String& value) {
    const int i = settingIndex(name);
    if (i >= 0) {
        waiting[i] = false; // Never apply an older startup reply over this live command.
        if (!applySetting(i, value)) Serial.println("Rejected setting: " + name);
        smartSnap.WriteVariableValue(name, i == 1 ? "string" : "int", settingValue(i));
    } else if (name == "Stop" && (value == "0" || value == "1")) {
        if (value == "1") stopPrayerSound();
        smartSnap.WriteVariableValue("Stop", "int", value); // Dedicated command confirmation.
        if (value == "1") smartSnap.WriteVariableValue("Stop", "int", "0");
    }
}
void onServerValue(const String& name, const String& value) {
    const int i = settingIndex(name);
    if (i < 0 || !waiting[i]) return;
    if (applySetting(i, value)) waiting[i] = false;
    else Serial.println("Invalid server setting; using EEPROM: " + name);
}
bool onRead(const String& name, String& type, String& value) {
    const int i = settingIndex(name);
    if (i >= 0) {
        if (waiting[i]) return false; // Do not overwrite a saved server setting.
        type = i == 1 ? "string" : "int";
        value = settingValue(i);
        return true;
    }
    if (name == "Stop") { type = "int"; value = "0"; return true; }
    return false;
}
void onConnected(bool connected) {
    if (!connected) return;
    for (int i = 0; i < 7; ++i) {
        waiting[i] = true;
        smartSnap.RequestVariableValueFromServer(settingNames[i]);
    }
}
void serviceNetworkAndButton() {
    static uint32_t servicedAt = 0;
    if (millis() - servicedAt < 5) { yield(); return; }
    servicedAt = millis();
    smartSnap.Run();
    checkStopButton();
    yield();
}
void setup() {
    Serial.begin(115200);
    pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
    EEPROM.begin(512);
    PrayerSettings saved;
    EEPROM.get(0, saved);
    if (validSettings(saved)) settings = saved;
    soundVolume = settings.volume;

    display.begin();
    display.setContrast(55);
    display.clearDisplay();
    display.display();
    FPSerial.begin(9600);
    playerReady = myDFPlayer.begin(FPSerial);
    Serial.println(playerReady ? "DFPlayer ready" : "DFPlayer unavailable; check wiring and SD card");
    if (playerReady) myDFPlayer.volume(soundVolume);

    smartSnap.SetFirmwareVersion("PrayerTimes_1.0.0");
    smartSnap.SetCapability(SmartSnap::Capability::CustomConfiguration);
    smartSnap.SetNonBlockingConnect();
    smartSnap.EnableDiscovery("PrayerTimes", "PrayerTimes", "speaker");
    smartSnap.SetVariableDefinitions(DEVICE_VARIABLES, sizeof(DEVICE_VARIABLES) / sizeof(DEVICE_VARIABLES[0]));
    smartSnap.onWriteVariable(onWrite);
    smartSnap.onServerValue(onServerValue);
    smartSnap.onReadVariable(onRead);
    smartSnap.onConnectionStatus(onConnected);
    smartSnap.Initialize(HOME_NAME, DEVICE_ID, WIFI_SSID, WIFI_PASSWORD, HUB_HOST, HUB_PORT);
    configTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
}
void loop() {
    serviceNetworkAndButton();
    checkPrayerTimes();
    dailyUpdateCheck();
    if (prayerScreenActive && millis() - prayerScreenStart >= 203000) {
        prayerScreenActive = false;
        activePrayerIndex = -1;
    }
    if (millis() - lastDisplayUpdate >= 500) {
        lastDisplayUpdate = millis();
        time_t now = time(nullptr);
        if (prayerScreenActive) drawPrayerScreen();
        else if (scheduleReady && now >= 1700000000 && dateKey(*localtime(&now)) == scheduleDate) drawNormalScreen();
        else {
            display.clearDisplay();display.setTextColor(BLACK);display.setTextSize(1);display.setCursor(0, 0);
            display.println("PrayerTimes");
            display.println(now < 1700000000 ? "Syncing time..." : "Loading times...");
            display.println(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi offline");
            display.display();
        }
    }
    delay(1);
}

