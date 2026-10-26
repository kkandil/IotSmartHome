#pragma once

// Set these before the first USB flash. OTA-provisioned identity takes priority.
constexpr int DEVICE_ID = 1014; // MultiSens, assigned in the app.
constexpr char HOME_NAME[] = "Home_Germany";
constexpr char WIFI_SSID[] = "KS_DSL";
constexpr char WIFI_PASSWORD[] = "2wad@dsl";
constexpr char HUB_HOST[] = "192.168.178.89";
constexpr unsigned short HUB_PORT = 3000;
constexpr int I2C_SDA = 4; // NodeMCU D2
constexpr int I2C_SCL = 5; // NodeMCU D1
// Reporting interval is configured by the ReportInterval server variable (seconds).
