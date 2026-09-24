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
  typedef void (*WriteVariableHandler)(const String& varName, const String& varType, const String& value);
  typedef bool (*ReadVariableHandler)(const String& varName, String& varType, String& value);
  typedef void (*ServerValueHandler)(const String& varName, const String& value);
  typedef void (*ConnectionStatusHandler)(bool connected);

  SmartSnap();

  int Initialize(const char* homeName,
                 int deviceId,
                 const char* wifiSsid,
                 const char* wifiPass,
                 const char* serverHost = "smarthome.herokuapp.com",
                 uint16_t serverPort = 80);

  void Run();
  void SetFirmwareVersion(const String& version);
  // Erases only saved connection provisioning; reboot with desired Initialize defaults afterwards.
  bool ResetConnectionConfiguration();
  void Disconnect();

  void WriteVariableValue(const String& varName, const String& varType, const String& value);
  bool SendNotificationToPhone(const String& message);
  bool SendAlarmToPhone(const String& message);
  void RequestVariableValueFromServer(const String& varName);

  void onWriteVariable(WriteVariableHandler handler);
  void onReadVariable(ReadVariableHandler handler);
  void onServerValue(ServerValueHandler handler);
  void onConnectionStatus(ConnectionStatusHandler handler);

  bool IsConnected() const;
  int GetDeviceId() const;
  String GetHomeName() const;

private:
  String _homeName;
  String _serverHost, _otaPath, _otaJob;
  String _firmwareVersion = "unversioned";
  uint16_t _serverPort = 3000;
  static void EventOTA(const char* payload, size_t length);
  int _deviceId;
  bool _isDeviceConnected;
  bool _isAckReceived;

  WriteVariableHandler _writeHandler;
  ReadVariableHandler _readHandler;
  ServerValueHandler _serverValueHandler;
  ConnectionStatusHandler _connectionStatusHandler;

  static SmartSnap* _instance;
  static SocketIoClient _webSocket;

  static void EventDeviceConnect(const char* payload, size_t length);
  static void EventMessage(const char* payload, size_t length);
  static void EventConnect(const char* payload, size_t length);
  static void EventDisconnect(const char* payload, size_t length);
  static void EventWriteVariable(const char* payload, size_t length);
  static void EventGetVariableValueFromDevice(const char* payload, size_t length);
  static void EventDeviceWriteVariable(const char* payload, size_t length);
  static void EventGetVariableValueFromServer(const char* payload, size_t length);

  void HandleDeviceConnect(const char* payload, size_t length);
  void HandleMessage(const char* payload, size_t length);
  void HandleConnect(const char* payload, size_t length);
  void HandleDisconnect(const char* payload, size_t length);
  void HandleWriteVariable(const char* payload, size_t length);
  void HandleGetVariableValueFromDevice(const char* payload, size_t length);
  void HandleDeviceWriteVariable(const char* payload, size_t length);
  void HandleGetVariableValueFromServer(const char* payload, size_t length);

  void EmitDeviceConnect();
  void EmitDeviceWriteVariable(const String& varName, const String& varType, const String& varValue);
};

#endif