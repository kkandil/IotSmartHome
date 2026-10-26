#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <limits.h>

// Standalone test: no Wi-Fi, credentials, recognition, or device commands.
// Capture both slots so a channel-selection mismatch cannot hide the signal.
constexpr i2s_port_t MIC_PORT = I2S_NUM_1;
constexpr int SAMPLE_RATE = 16000;
int32_t samples[512];
uint32_t reportAt;
uint32_t readErrors = 0;

struct Levels {
    uint32_t count = 0;
    uint32_t nonzero = 0;
    int32_t minimum = INT32_MAX;
    int32_t maximum = INT32_MIN;
    double sum = 0;
    double squares = 0;

    void add(int32_t raw) {
        ++count;
        if (raw != 0) ++nonzero;
        if (raw < minimum) minimum = raw;
        if (raw > maximum) maximum = raw;
        const double scaled = raw / 65536.0;
        sum += scaled;
        squares += scaled * scaled;
    }

    void print(const char* name) const {
        const double mean = count ? sum / count : 0;
        const double variance = count ? squares / count - mean * mean : 0;
        Serial.printf("%s samples=%lu nonzero=%lu rawMin=%ld rawMax=%ld AC_RMS=%.2f\n",
                      name, (unsigned long)count, (unsigned long)nonzero,
                      (long)(count ? minimum : 0), (long)(count ? maximum : 0),
                      sqrt(variance > 0 ? variance : 0));
    }
} slots[2];

void check(esp_err_t result) {
    if (result == ESP_OK) return;
    Serial.printf("I2S error: %d\n", result);
    while (true) delay(1000);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("MIC TEST: BCLK=26 WS=25 DATA=32; 3.3V; SEL=GND.");
    Serial.println("Both slots captured. Stay quiet, then speak or clap near the mic.");
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    check(i2s_driver_install(MIC_PORT, &config, 0, nullptr));
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = 26;
    pins.ws_io_num = 25;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = 32;
    check(i2s_set_pin(MIC_PORT, &pins));
    reportAt = millis();
}

void loop() {
    size_t bytes = 0;
    esp_err_t result = i2s_read(MIC_PORT, samples, sizeof(samples), &bytes, pdMS_TO_TICKS(100));
    if (result != ESP_OK) ++readErrors;
    for (size_t i = 0; i + 1 < bytes / sizeof(int32_t); i += 2) {
        slots[0].add(samples[i]);
        slots[1].add(samples[i + 1]);
    }
    if (millis() - reportAt >= 2000) {
        Serial.printf("Window=%lu ms readErrors=%lu\n", (unsigned long)(millis() - reportAt), (unsigned long)readErrors);
        slots[0].print("Slot A");
        slots[1].print("Slot B");
        slots[0] = Levels{};
        slots[1] = Levels{};
        readErrors = 0;
        reportAt = millis();
    }
}
