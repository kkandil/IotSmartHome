#pragma once

// Set these before the first USB flash. OTA-provisioned identity takes priority.
constexpr int DEVICE_ID = 0; // Assigned by SmartSnap discovery.
constexpr char HOME_NAME[] = "Home_Germany";
constexpr char WIFI_SSID[] = "CHANGE_ME";
constexpr char WIFI_PASSWORD[] = "CHANGE_ME";
constexpr char HUB_HOST[] = "192.168.178.89";
constexpr unsigned short HUB_PORT = 3000;
