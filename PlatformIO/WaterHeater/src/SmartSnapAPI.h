#ifndef SMARTSNAPAPI_H
#define SMARTSNAPAPI_H

#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiMulti.h>
#else
  #error "This library supports only ESP8266 and ESP32."
#endif

#include <SocketIoClient.h>
#include <ArduinoJson.h>

#define E_OK  0
#define E_NOK 1

class SmartSnap {
  public:
    SmartSnap();

    int Initialize(const int deviceId, const char* wifiSsid, const char* constPass);

    void Run();

    void WriteVariableValue(String VarName, String VarType, String Value);
    void RequestVariableValueFromServer(String VarName);

  private:
    int _deviceId = 0;
};

extern void SmartSnapWriteVariable(String VarName, String VarType, String Value);
extern void SmartSnapGetVariable(String VarName, String& VarType, String* Value);
extern void SmartSnapServerWriteVariable(String VarName, String Value);

#endif
