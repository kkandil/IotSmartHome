#include <SmartSnapAPI.h>

SmartSnap api;

void setup() {
  Serial.begin(115200);
  api.SetFirmwareVersion("TempSens_1.0.1");
  // No device ID in this application. OTA assigns it, or keeps the provisioned ID.
  // Initial USB provisioning still uses the legacy Initialize(home, id, ...) form.
  api.Initialize("Home_Germany", "YOUR_WIFI", "YOUR_PASSWORD", "SmartHubEgypt", 3000);
}

void loop() {
  api.Run();
  // Add your sensor/controller logic here without blocking api.Run().
}
