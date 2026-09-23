#include "SmartSnapAPI.h"

#define USE_SERIAL Serial
#define WAIT_FOR_ACK_DELAY_TICKS 100   // 100 * 100ms = 10 sec

#if defined(ESP8266)
ESP8266WiFiMulti WiFiMulti;
#else
WiFiMulti WiFiMulti;
#endif

SmartSnap* SmartSnap::_instance = nullptr;
SocketIoClient SmartSnap::_webSocket;

SmartSnap::SmartSnap()
  : _homeName(""),
    _deviceId(0),
    _isDeviceConnected(false),
    _isAckReceived(false),
    _writeHandler(nullptr),
    _readHandler(nullptr),
    _serverValueHandler(nullptr),
    _connectionStatusHandler(nullptr)
{
}

int SmartSnap::Initialize(const char* homeName,
                          int deviceId,
                          const char* wifiSsid,
                          const char* wifiPass,
                          const char* serverHost,
                          uint16_t serverPort)
{
  _homeName = homeName ? homeName : "";
  _deviceId = deviceId;
  _isDeviceConnected = false;
  _isAckReceived = false;

  if (_homeName.length() == 0 || _deviceId <= 0 ) {
    USE_SERIAL.println("SmartSnap Initialize: invalid parameters");
    return E_NOK;
  }

  _instance = this;

#if defined(ESP8266)
  USE_SERIAL.setDebugOutput(true);
#endif

  USE_SERIAL.println();
  USE_SERIAL.println("SmartSnap Initialize...");

  WiFiMulti.addAP(wifiSsid, wifiPass);

  USE_SERIAL.print("Connecting WiFi");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(250);
    USE_SERIAL.print(".");
  }
  USE_SERIAL.println();
  USE_SERIAL.println("WiFi connected");

  _webSocket.on("DeviceConnect", EventDeviceConnect);
  _webSocket.on("message", EventMessage);
  _webSocket.on("connect", EventConnect);
  _webSocket.on("disconnect", EventDisconnect);
  _webSocket.on("PhoneWriteVariable", EventWriteVariable);
  _webSocket.on("GetVariableValueFromDevice", EventGetVariableValueFromDevice);
  _webSocket.on("DeviceWriteVariable", EventDeviceWriteVariable);
  _webSocket.on("GetVariableValueFromServer", EventGetVariableValueFromServer);

  _webSocket.begin(serverHost, serverPort);

  for (uint8_t t = WAIT_FOR_ACK_DELAY_TICKS; t > 0; t--) {
    _webSocket.loop();
    if (_isAckReceived) break;
    delay(100);
  }

  if (_isAckReceived && _isDeviceConnected) {
    USE_SERIAL.println("SmartSnap connected and acknowledged");
    return E_OK;
  }

  USE_SERIAL.println("SmartSnap failed to connect");
  _webSocket.disconnect();
  return E_NOK;
}

void SmartSnap::Run()
{
  _webSocket.loop();
}

void SmartSnap::Disconnect()
{
  _webSocket.disconnect();
}

void SmartSnap::WriteVariableValue(const String& varName, const String& varType, const String& value)
{
  EmitDeviceWriteVariable(varName, varType, value);
}

void SmartSnap::RequestVariableValueFromServer(const String& varName)
{
  StaticJsonDocument<200> doc;
  String output;

  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
  doc["varName"] = varName;

  serializeJson(doc, output);
  _webSocket.emit("GetVariableValueFromServer", output.c_str());
}

void SmartSnap::onWriteVariable(WriteVariableHandler handler)
{
  _writeHandler = handler;
}

void SmartSnap::onReadVariable(ReadVariableHandler handler)
{
  _readHandler = handler;
}

void SmartSnap::onServerValue(ServerValueHandler handler)
{
  _serverValueHandler = handler;
}

void SmartSnap::onConnectionStatus(ConnectionStatusHandler handler)
{
  _connectionStatusHandler = handler;
}

bool SmartSnap::IsConnected() const
{
  return _isDeviceConnected;
}

int SmartSnap::GetDeviceId() const
{
  return _deviceId;
}

String SmartSnap::GetHomeName() const
{
  return _homeName;
}

void SmartSnap::EmitDeviceConnect()
{
  StaticJsonDocument<128> doc;
  String output;

  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;

  serializeJson(doc, output);
  _webSocket.emit("DeviceConnect", output.c_str());
}

void SmartSnap::EmitDeviceWriteVariable(const String& varName, const String& varType, const String& varValue)
{
  StaticJsonDocument<200> doc;
  String output;

  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
  doc["varName"] = varName;
  doc["varType"] = varType;

  // if (varType == "int") {
  //   doc["varValue"] = varValue.toInt();
  // } else if (varType == "float") {
  //   doc["varValue"] = varValue.toFloat();
  // } else {
    doc["varValue"] = varValue;
  // }

  serializeJson(doc, output);
  _webSocket.emit("DeviceWriteVariable", output.c_str());
}

void SmartSnap::EventDeviceConnect(const char* payload, size_t length)
{
  if (_instance) _instance->HandleDeviceConnect(payload, length);
}

void SmartSnap::EventMessage(const char* payload, size_t length)
{
  if (_instance) _instance->HandleMessage(payload, length);
}

void SmartSnap::EventConnect(const char* payload, size_t length)
{
  if (_instance) _instance->HandleConnect(payload, length);
}

void SmartSnap::EventDisconnect(const char* payload, size_t length)
{
  if (_instance) _instance->HandleDisconnect(payload, length);
}

void SmartSnap::EventWriteVariable(const char* payload, size_t length)
{
  if (_instance) _instance->HandleWriteVariable(payload, length);
}

void SmartSnap::EventGetVariableValueFromDevice(const char* payload, size_t length)
{
  if (_instance) _instance->HandleGetVariableValueFromDevice(payload, length);
}

void SmartSnap::EventDeviceWriteVariable(const char* payload, size_t length)
{
  if (_instance) _instance->HandleDeviceWriteVariable(payload, length);
}

void SmartSnap::EventGetVariableValueFromServer(const char* payload, size_t length)
{
  if (_instance) _instance->HandleGetVariableValueFromServer(payload, length);
}

void SmartSnap::HandleDeviceConnect(const char* payload, size_t length)
{
  USE_SERIAL.printf("DeviceConnect: %.*s\n", (int)length, payload);

  if (strstr(payload, "OK") != nullptr) {
    _isDeviceConnected = true;
    USE_SERIAL.println("Device connection ACK = OK");
  } else {
    _isDeviceConnected = false;
    USE_SERIAL.println("Device connection ACK = NOK");
  }

  _isAckReceived = true;

  if (_connectionStatusHandler) {
    _connectionStatusHandler(_isDeviceConnected);
  }
}

void SmartSnap::HandleMessage(const char* payload, size_t length)
{
  USE_SERIAL.printf("Message: %.*s\n", (int)length, payload);
}

void SmartSnap::HandleConnect(const char* payload, size_t length)
{
  USE_SERIAL.println("Connected to server");
  EmitDeviceConnect();
}

void SmartSnap::HandleDisconnect(const char* payload, size_t length)
{
  USE_SERIAL.println("Disconnected from server");
  _isDeviceConnected = false;

  if (_connectionStatusHandler) {
    _connectionStatusHandler(false);
  }
}

void SmartSnap::HandleWriteVariable(const char* payload, size_t length)
{
  USE_SERIAL.printf("PhoneWriteVariable: %.*s\n", (int)length, payload);

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (PhoneWriteVariable): %s\n", err.c_str());
    return;
  }

  String varName = doc["varName"] | "";
  String varType = doc["varType"] | "";
  String varValue;

  JsonVariant v = doc["varValue"];

  if (v.isNull()) {
    varValue = "";
  // } else if (varType == "int") {
  //   varValue = String(v.as<long>());
  // } else if (varType == "float") {
  //   varValue = String(v.as<float>(), 6);
  } else {
    const char* s = v.as<const char*>();
    varValue = s ? String(s) : String("");
  }

  if (_writeHandler) {
    _writeHandler(varName, varType, varValue);
  }
}

void SmartSnap::HandleGetVariableValueFromDevice(const char* payload, size_t length)
{
  USE_SERIAL.printf("GetVariableValueFromDevice: %.*s\n", (int)length, payload);

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (GetVariableValueFromDevice): %s\n", err.c_str());
    return;
  }

  String varName = doc["varName"] | "";
  String varType = "";
  String varValue = "";

  if (_readHandler) {
    bool ok = _readHandler(varName, varType, varValue);
    if (ok) {
      EmitDeviceWriteVariable(varName, varType, varValue);
    } else {
      USE_SERIAL.printf("Variable not found: %s\n", varName.c_str());
    }
  }
}

void SmartSnap::HandleDeviceWriteVariable(const char* payload, size_t length)
{
  USE_SERIAL.printf("DeviceWriteVariable ACK/Event: %.*s\n", (int)length, payload);
}

void SmartSnap::HandleGetVariableValueFromServer(const char* payload, size_t length)
{
  USE_SERIAL.printf("GetVariableValueFromServer: %.*s\n", (int)length, payload);

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (GetVariableValueFromServer): %s\n", err.c_str());
    return;
  }

  String varName = doc["varName"] | "";
  String varValue;

  JsonVariant v = doc["Value"];
  // if (v.isNull()) {
  //   varValue = "";
  // } else if (v.is<const char*>()) {
  //   const char* s = v.as<const char*>();
  //   varValue = s ? String(s) : String("");
  // } else if (v.is<long>()) {
  //   varValue = String(v.as<long>());
  // } else if (v.is<float>()) {
  //   varValue = String(v.as<float>(), 6);
  // } else {
  //   serializeJson(v, varValue);
  // }

  if (v.isNull()) {
    varValue = "";
  // } else if (varType == "int") {
  //   varValue = String(v.as<long>());
  // } else if (varType == "float") {
  //   varValue = String(v.as<float>(), 6);
  } else {
    const char* s = v.as<const char*>();
    varValue = s ? String(s) : String("");
  }


  if (_serverValueHandler) {
    _serverValueHandler(varName, varValue);
  }
}

// Returns whether the notification was emitted, not a delivery receipt.
bool SmartSnap::SendNotificationToPhone(const String& message) {
  if (!IsConnected() || message.length() == 0 || message.length() > 512) return false;
  DynamicJsonDocument doc(1024);
  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
  doc["message"] = message;
  String output;
  serializeJson(doc, output);
  _webSocket.emit("DeviceWriteNotification", output.c_str());
  return true;
}

// Send on an alarm transition, not repeatedly from every Run/loop iteration.
bool SmartSnap::SendAlarmToPhone(const String& message) {
  if (!IsConnected() || message.length() == 0 || message.length() > 512) return false;
  DynamicJsonDocument doc(1024);
  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
  doc["message"] = message;
  String output;
  serializeJson(doc, output);
  _webSocket.emit("DeviceWriteAlarm", output.c_str());
  return true;
}
