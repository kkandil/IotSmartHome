#include "SmartSnapAPI.h"
#if defined(ESP8266)
#include <ESP8266httpUpdate.h>
#include <LittleFS.h>
#include "SmartSnapOTAKey.h"
#endif

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

int SmartSnap::Initialize(const char* homeName, const char* wifiSsid, const char* wifiPass, const char* serverHost, uint16_t serverPort) {
  return Initialize(homeName, 0, wifiSsid, wifiPass, serverHost, serverPort);
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
  _serverHost=serverHost; _serverPort=serverPort;
  String savedSsid=wifiSsid, savedPass=wifiPass;
#if defined(ESP8266)
  // Sketch OTA preserves LittleFS. Keep identity and credentials independent of the new sketch defaults.
  if(LittleFS.begin()) {
    DynamicJsonDocument config(1024), receipt(512);
    File input=LittleFS.open("/smartsnap-connection.json","r");
    bool hasConfig=input && deserializeJson(config,input)==DeserializationError::Ok && config["id"].as<int>()>0;
    input.close();
    File receiptFile=LittleFS.open("/smartsnap-ota.json","r");
    bool verified=receiptFile && deserializeJson(receipt,receiptFile)==DeserializationError::Ok &&
                  receipt["md5"].as<String>()==ESP.getSketchMD5();
    receiptFile.close();
    bool apply=verified && !receipt["applied"].as<bool>();
    int savedId=hasConfig?config["id"].as<int>():_deviceId;
    if(!hasConfig || (apply && receipt["replace"].as<bool>())) {
      config.clear();config["home"]=_homeName;config["id"]=_deviceId>0?_deviceId:savedId;
      config["host"]=_serverHost;config["port"]=_serverPort;
      config["ssid"]=savedSsid;config["pass"]=savedPass;
    }
    if(apply && receipt["deviceID"].as<int>()>0) config["id"]=receipt["deviceID"].as<int>();
    bool stored=hasConfig && !apply;
    if(!stored && config["id"].as<int>()>0) {
      File output=LittleFS.open("/smartsnap-connection.tmp","w");
      if(output) {bool written=serializeJson(config,output)>0;output.close();stored=written && LittleFS.rename("/smartsnap-connection.tmp","/smartsnap-connection.json");}
    }
    if(config["id"].as<int>()>0) {
      _homeName=config["home"].as<String>();_deviceId=config["id"];
      _serverHost=config["host"].as<String>();_serverPort=config["port"];
      savedSsid=config["ssid"].as<String>();savedPass=config["pass"].as<String>();
    }
    if(verified && stored) {
      _otaCompletedJob=receipt["jobId"].as<String>();
      if(apply) {
        receipt["applied"]=true;
        File completed=LittleFS.open("/smartsnap-ota.tmp","w");
        if(completed){serializeJson(receipt,completed);completed.close();LittleFS.rename("/smartsnap-ota.tmp","/smartsnap-ota.json");}
      }
    }
  }
#endif
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

  WiFiMulti.addAP(savedSsid.c_str(), savedPass.c_str());

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

  _webSocket.on("DeviceOTA", EventOTA);
  _webSocket.begin(_serverHost.c_str(), _serverPort);

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
#if defined(ESP8266)
  if(_otaPath.length() && WiFi.status()==WL_CONNECTED) {
    String path=_otaPath;_otaPath="";
    BearSSL::PublicKey publicKey(SMARTSNAP_OTA_PUBLIC_KEY);
    BearSSL::HashSHA256 hash;
    BearSSL::SigningVerifier verifier(&publicKey);
    Update.installSignature(&hash,&verifier);
    ESPhttpUpdate.rebootOnUpdate(false);
    WiFiClient client;
    USE_SERIAL.println("SmartSnap OTA: downloading signed firmware");
    t_httpUpdate_return result=ESPhttpUpdate.update(client,_serverHost,_serverPort,path);
    Update.installSignature(nullptr,nullptr);
    if(result==HTTP_UPDATE_OK) {
      DynamicJsonDocument receipt(512);receipt["jobId"]=_otaJob;receipt["md5"]=_otaExpectedMD5;
      receipt["deviceID"]=_otaDeviceId;receipt["replace"]=_otaReplaceConfiguration;receipt["applied"]=false;
      if(LittleFS.begin()) {
        File pending=LittleFS.open("/smartsnap-ota.tmp","w");
        if(pending){serializeJson(receipt,pending);pending.close();LittleFS.rename("/smartsnap-ota.tmp","/smartsnap-ota.json");}
      }
      USE_SERIAL.println("SmartSnap OTA: verified; rebooting");delay(100);ESP.restart();
    }
    else {
      USE_SERIAL.println("SmartSnap OTA failed: "+ESPhttpUpdate.getLastErrorString());
      // Restore the socket first so the error can reach the hub.
      _webSocket.begin(_serverHost.c_str(),_serverPort);
      for(int i=0;i<30;i++){_webSocket.loop();delay(100);}
      DynamicJsonDocument status(512);status["jobId"]=_otaJob;status["error"]=ESPhttpUpdate.getLastErrorString();
      String payload;serializeJson(status,payload);_webSocket.emit("DeviceOTAStatus",payload.c_str());
    }
  }
#endif
}

void SmartSnap::SetFirmwareVersion(const String& version){_firmwareVersion=version;}
bool SmartSnap::ResetConnectionConfiguration(){
#if defined(ESP8266)
  return LittleFS.begin() && LittleFS.remove("/smartsnap-connection.json");
#else
  return false;
#endif
}
void SmartSnap::EventOTA(const char* payload,size_t length){
#if defined(ESP8266)
  if(!_instance||!_instance->_isDeviceConnected||_instance->_otaPath.length())return;
  DynamicJsonDocument data(512);
  if(deserializeJson(data,payload,length))return;
  String path=data["path"].as<String>();
  if(!path.startsWith("/ota/download/") || path.length()!=62)return;
  for(unsigned int i=14;i<path.length();i++)if(!isxdigit(path[i]))return;
  _instance->_otaPath=path;_instance->_otaJob=data["jobId"].as<String>();
  _instance->_otaReplaceConfiguration=data["replaceConfiguration"]==true;
  _instance->_otaExpectedMD5=data["sketchMD5"].as<String>();
  _instance->_otaDeviceId=data["newDeviceID"].as<int>();
#endif
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
  StaticJsonDocument<768> doc;
  String output;

  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
#if defined(ESP8266)
  doc["otaDeviceId"]=true;doc["otaReplaceConfiguration"]=true;doc["hardwareId"]=WiFi.macAddress();doc["otaCompletedJob"]=_otaCompletedJob;
  doc["ota"]=true;doc["firmwareVersion"]=_firmwareVersion;doc["sketchMD5"]=ESP.getSketchMD5();
#endif


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
