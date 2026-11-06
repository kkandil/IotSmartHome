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
  // Declare supported features before Initialize(). Add future features here.
  enum class Capability : uint8_t { LD2420 = 0, StartupBehavior = 1, RelayStartup = StartupBehavior, CustomConfiguration = 2 };
  void SetCapability(Capability capability, bool enabled = true);

  typedef void (*WriteVariableHandler)(const String& varName, const String& varType, const String& value);
  typedef bool (*ReadVariableHandler)(const String& varName, String& varType, String& value);
  typedef void (*ServerValueHandler)(const String& varName, const String& value);
  typedef void (*ConnectionStatusHandler)(bool connected);

  typedef void (*PresenceHandler)(const String& payload);
  void onPresenceConfigure(PresenceHandler handler){_presenceHandler=handler;}
  void SendPresenceResult(const String& json){_webSocket.emit("PresenceResult",json.c_str());}
  void onStartupConfigure(PresenceHandler handler){_relayHandler=handler;}
  void onRelayConfigure(PresenceHandler handler){onStartupConfigure(handler);} // Legacy alias
  // Keep the transport event compatible with existing hubs.
  void SendStartupResult(const String& json){_webSocket.emit("RelaySettingsResult",json.c_str());}
  void SendRelayResult(const String& json){SendStartupResult(json);} // Legacy alias
  struct VariableDefinition { const char* name; const char* type; const char* value; bool configuration; };
  void EnableDiscovery(const char* name,const char* model,const char* icon="smartsnap") { _discovery=true;_suggestedName=name;_model=model;_icon=icon; }
  void SetVariableDefinitions(const VariableDefinition* variables,size_t count) { _definitions=variables;_definitionCount=count>32?32:count; }
  SmartSnap();

  int Initialize(const char* homeName,
                 int deviceId,
                 const char* wifiSsid,
                 const char* wifiPass,
                 const char* serverHost = "smarthome.herokuapp.com",
                 uint16_t serverPort = 80);

  // Uses the identity already provisioned on the device or assigned by OTA.
  int Initialize(const char* homeName, const char* wifiSsid, const char* wifiPass,
                 const char* serverHost = "smarthome.herokuapp.com", uint16_t serverPort = 80);

  void Run();
  void SetFirmwareVersion(const String& version);
  // Opt in before Initialize: return while Wi-Fi/hub connect in the background.
  void SetNonBlockingConnect(bool enabled = true) { _nonBlockingConnect = enabled; }
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
  bool _discovery=false;
  String _suggestedName, _model, _icon;
  const VariableDefinition* _definitions=nullptr;
  size_t _definitionCount=0;
  uint32_t _discoveryAt=0;
  static void EventDefinition(const char*,size_t);
  static void EventAssigned(const char*,size_t);
  struct CommandReceipt { String variable, value, id; };
  CommandReceipt _commandReceipts[8];
  String _bootId;
  uint32_t _healthAt = 0, _uptimeTick = 0;
  uint64_t _uptimeMs = 0;
  bool _healthSent = false;
  void EmitHealth();
  void TrackCommand(const String& variable, const String& value, const String& id);
  PresenceHandler _relayHandler = nullptr;
  static void EventRelayConfigure(const char* payload, size_t length);
  bool _nonBlockingConnect = false;
  PresenceHandler _presenceHandler=nullptr;
  static void EventPresence(const char* payload,size_t length);
  uint32_t _capabilities = 0;
  String _homeName;
  String _serverHost, _otaPath, _otaJob, _otaExpectedMD5, _otaCompletedJob;
  bool _otaReplaceConfiguration = false;
  int _otaDeviceId = 0;
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
