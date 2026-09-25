#include <SmartSnapAPI.h>

SmartSnap api;

int ledState = 0;
float temperatureValue = 24.5f;
String deviceLabel = "LivingRoomNode";
#define LED_BUILTIN 2


void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{
  if (varName == "led") {
    ledState = value.toInt();
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
  } else if (varName == "deviceLabel") {
    deviceLabel = value;
  }
}

bool HandleReadVariable(const String& varName, String& varType, String& value)
{
  if (varName == "led") {
    varType = "int";
    value = String(ledState);
    return true;
  }

  if (varName == "temperature") {
    varType = "float";
    value = String(temperatureValue, 2);
    return true;
  }

  if (varName == "deviceLabel") {
    varType = "string";
    value = deviceLabel;
    return true;
  }

  return false;
}

void HandleServerValue(const String& varName, const String& value)
{
  Serial.printf("Server value: %s = %s\n", varName.c_str(), value.c_str());
}

void HandleConnectionStatus(bool connected)
{
  Serial.printf("Connection status: %s\n", connected ? "connected" : "disconnected");
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  api.onWriteVariable(HandleWriteVariable);
  api.onReadVariable(HandleReadVariable);
  api.onServerValue(HandleServerValue);
  api.onConnectionStatus(HandleConnectionStatus);

  int ret = api.Initialize(
    "Home_Egypt",
    1006,
    "YOUR_WIFI_SSID",
    "YOUR_WIFI_PASSWORD",
  );

  if (ret == E_OK) {
    Serial.println("SmartSnap initialized successfully");
  } else {
    Serial.println("SmartSnap initialization failed");
  }
}

void loop()
{
  api.Run();
}