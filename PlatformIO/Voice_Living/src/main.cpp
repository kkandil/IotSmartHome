#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>

#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#include "Secrets.example.h"
#endif

// SPH0645: SEL/LR to GND (left), powered from 3.3 V.
constexpr int MIC_BCLK = 26;
constexpr int MIC_LRCLK = 25;
constexpr int MIC_DATA = 32;
// MAX98357A uses a separate I2S controller/clock pair.
constexpr int SPEAKER_BCLK = 27;
constexpr int SPEAKER_LRCLK = 14;
constexpr int SPEAKER_DATA = 22;
constexpr int STATUS_LED = 2;
constexpr int SAMPLE_RATE = 16000;
constexpr size_t FRAME_SAMPLES = 1280; // 80 ms
constexpr int MIC_SHIFT = 14;         // Adjustable microphone gain; inspect clipping first.
constexpr int SPEAKER_VOLUME_DIVISOR = 16; // Start at 6.25% digital amplitude.
int16_t speakerStereo[4096];

WebSocketsClient socket;
bool connected = false;
bool capture = false;
bool speaking = false;
int16_t frame[FRAME_SAMPLES];
size_t frameUsed = 0;
int32_t microphone[256];
String authorization;
uint32_t lastServerMessage = 0;
float dcOffset = 0;
uint32_t micReportAt = 0;
int32_t micMin = INT32_MAX, micMax = INT32_MIN;

void checkAudio(esp_err_t result) {
    if (result != ESP_OK) {
        Serial.printf("I2S setup failed: %d\n", result);
        while (true) delay(1000);
    }
}

void configureAudio(i2s_port_t port, bool input, int bclk, int lrclk, int data) {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | (input ? I2S_MODE_RX : I2S_MODE_TX));
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = input ? I2S_BITS_PER_SAMPLE_32BIT : I2S_BITS_PER_SAMPLE_16BIT;
    // The hardware test on this ESP32/SPH0645 reports mic data in stereo
    // slot 0; mono ONLY_LEFT captured the silent slot. Keep both RX slots
    // and explicitly select the measured slot below (one sample per WS cycle).
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;
    checkAudio(i2s_driver_install(port, &config, 0, nullptr));
    if (!input) {
        checkAudio(i2s_zero_dma_buffer(port));
        checkAudio(i2s_stop(port));
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = bclk;
    pins.ws_io_num = lrclk;
    pins.data_out_num = input ? I2S_PIN_NO_CHANGE : data;
    pins.data_in_num = input ? data : I2S_PIN_NO_CHANGE;
    checkAudio(i2s_set_pin(port, &pins));
}

void silenceSpeaker() {
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_stop(I2S_NUM_0);
}

void socketEvent(WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_DISCONNECTED) {
        connected = capture = speaking = false;
        frameUsed = 0;
        digitalWrite(STATUS_LED, LOW);
        silenceSpeaker();
        Serial.println("Voice server disconnected; reconnecting.");
    } else if (type == WStype_CONNECTED) {
        connected = true;
        lastServerMessage = millis();
        Serial.println("Voice server connected.");
    } else if (type == WStype_TEXT) {
        lastServerMessage = millis();
        StaticJsonDocument<256> message;
        if (deserializeJson(message, payload, length)) return;
        const String state = message["state"] | "";
        Serial.println("Voice: " + state);
        if (state == "idle" || state == "listening") {
            silenceSpeaker();
            capture = true;
            speaking = false;
            frameUsed = 0;
            digitalWrite(STATUS_LED, state == "listening" ? HIGH : LOW);
        } else if (state == "processing" || state == "speaking") {
            capture = false;
            speaking = state == "speaking";
            silenceSpeaker();
            if (speaking) checkAudio(i2s_start(I2S_NUM_0));
            frameUsed = 0;
            digitalWrite(STATUS_LED, HIGH);
        } else if (state == "finished") {
            // Drain queued speaker samples before allowing the wake word again.
            delay(150);
            silenceSpeaker();
            speaking = false;
            socket.sendTXT("played");
        }
    } else if (type == WStype_BIN && speaking && length <= 4096 && length % 2 == 0) {
        lastServerMessage = millis();
        size_t written = 0;
        // Pi sends little-endian mono PCM; duplicate into both amplifier slots.
        for (size_t i = 0; i < length / 2; ++i) {
            int16_t mono = static_cast<int16_t>(static_cast<uint16_t>(payload[2*i]) |
                                              (static_cast<uint16_t>(payload[2*i+1]) << 8));
            speakerStereo[2*i] = speakerStereo[2*i+1] = mono / SPEAKER_VOLUME_DIVISOR;
        }
        esp_err_t result = i2s_write(I2S_NUM_0, speakerStereo, length * 2, &written, pdMS_TO_TICKS(250));
        if (result != ESP_OK || written != length * 2) {
            Serial.println("Speaker write incomplete; stopping playback.");
            silenceSpeaker(); speaking = false;
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
    configureAudio(I2S_NUM_1, true, MIC_BCLK, MIC_LRCLK, MIC_DATA);
    configureAudio(I2S_NUM_0, false, SPEAKER_BCLK, SPEAKER_LRCLK, SPEAKER_DATA);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname((String("ESP_") + VOICE_DEVICE_ID).c_str());
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(VOICE_WIFI_SSID, VOICE_WIFI_PASSWORD);
    authorization = String("Authorization: Bearer ") + VOICE_TOKEN;
    socket.setExtraHeaders(authorization.c_str());
    socket.onEvent(socketEvent);
    socket.setReconnectInterval(3000);
    socket.enableHeartbeat(15000, 3000, 2);
    socket.begin(VOICE_PI_HOST, VOICE_PI_PORT, String("/audio/") + VOICE_SATELLITE_ID);
    Serial.println("Voice_Living: waiting for Wi-Fi and local voice server.");
}

void loop() {
    socket.loop();
    size_t received = 0;
    // Always drain RX, including while speaking, so stale audio is never replayed to recognition.
    i2s_read(I2S_NUM_1, microphone, sizeof(microphone), &received, pdMS_TO_TICKS(20));
    for (size_t i = 0; i + 1 < received / sizeof(int32_t); i += 2) {
        micMin = min(micMin, microphone[i]);
        micMax = max(micMax, microphone[i]);
    }
    if (millis() - micReportAt >= 2000) {
        Serial.printf("Mic raw min=%ld max=%ld bytes=%u capture=%d\n", (long)micMin, (long)micMax, (unsigned)received, capture);
        micMin = INT32_MAX; micMax = INT32_MIN; micReportAt = millis();
    }
    if (connected && capture) {
        for (size_t i = 0; i + 1 < received / sizeof(int32_t); i += 2) {
            const int32_t raw = microphone[i] >> MIC_SHIFT;
            dcOffset += 0.002f * (raw - dcOffset);
            const int32_t sample = raw - dcOffset;
            frame[frameUsed++] = static_cast<int16_t>(constrain(sample, -32768, 32767));
            if (frameUsed == FRAME_SAMPLES) {
                socket.sendBIN(reinterpret_cast<uint8_t*>(frame), sizeof(frame));
                frameUsed = 0;
            }
        }
    }
    if (connected && !capture && millis() - lastServerMessage > 40000) {
        socket.disconnect(); // Recover from an interrupted processing/playback response.
    }
    delay(1);
}
