#include "SmartSnapAPI.h"
#if defined(ESP32)
#include <esp_system.h>
#include <Preferences.h>
#else
extern "C" {
#include <user_interface.h>
}
#endif
#if defined(ESP8266)
#include <ESP8266httpUpdate.h>
#include <LittleFS.h>
#include "SmartSnapOTAKey.h"
#ifdef SMARTSNAP_FAST_WIFI
#include "FastWiFi.h"
static FastWiFi fastWiFi;
#endif
#endif

#ifdef SMARTSNAP_RADAR_UART
#define USE_SERIAL Serial1
#else
#define USE_SERIAL Serial
#endif
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
    if(apply && (receipt["replace"].as<bool>() || receipt["deviceID"].as<int>()>0))LittleFS.remove("/smartsnap-assignment.json");
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
  if(_discovery) {
#if defined(ESP8266)
    if(LittleFS.begin()) {File f=LittleFS.open("/smartsnap-assignment.json","r");DynamicJsonDocument a(256);if(f && deserializeJson(a,f)==DeserializationError::Ok && a["home"].as<String>()==_homeName && a["id"].as<int>()>0)_deviceId=a["id"];f.close();}
#else
    Preferences prefs;prefs.begin("smartsnap",true);if(prefs.getString("home","")==_homeName)_deviceId=prefs.getInt("id",_deviceId);prefs.end();
#endif
  }
  _isDeviceConnected = false;
  _isAckReceived = false;

  if (_homeName.length() == 0 || (_deviceId <= 0 && !_discovery) ) {
    USE_SERIAL.println("SmartSnap Initialize: invalid parameters");
    return E_NOK;
  }

  _instance = this;

#if defined(ESP8266)
  USE_SERIAL.setDebugOutput(true);
#endif

  USE_SERIAL.println();
  USE_SERIAL.println("SmartSnap Initialize...");

  // Set DHCP hostname after resolving the saved/OTA identity, before connecting.
  const String hostname = String("ESP-") + String(_deviceId);
#if defined(ESP8266)
  WiFi.hostname(hostname);
#else
  WiFi.setHostname(hostname.c_str());
#endif

  WiFiMulti.addAP(savedSsid.c_str(), savedPass.c_str());

  USE_SERIAL.print("Connecting WiFi");
  if (_nonBlockingConnect) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
#if defined(ESP8266) && defined(SMARTSNAP_FAST_WIFI)
    fastWiFi.begin(savedSsid, savedPass);
#else
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
#endif
  } else {
    while (WiFiMulti.run() != WL_CONNECTED) {
      delay(250);
      USE_SERIAL.print(".");
    }
    USE_SERIAL.println();
    USE_SERIAL.println("WiFi connected");
  }

  _webSocket.on("DeviceConnect", EventDeviceConnect);
  _webSocket.on("RequestDeviceDefinition", EventDefinition);
  _webSocket.on("DeviceAssigned", EventAssigned);
  _webSocket.on("message", EventMessage);
  _webSocket.on("connect", EventConnect);
  _webSocket.on("disconnect", EventDisconnect);
  _webSocket.on("PhoneWriteVariable", EventWriteVariable);
  _webSocket.on("GetVariableValueFromDevice", EventGetVariableValueFromDevice);
  _webSocket.on("DeviceWriteVariable", EventDeviceWriteVariable);
  _webSocket.on("GetVariableValueFromServer", EventGetVariableValueFromServer);

  _webSocket.on("PresenceConfigure",EventPresence);
  _webSocket.on("RelayConfigure",EventRelayConfigure);
  _webSocket.on("DeviceOTA", EventOTA);
  _webSocket.begin(_serverHost.c_str(), _serverPort);

  if (_nonBlockingConnect) return E_OK; // Started; IsConnected reports readiness.

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
  if(_discovery) return E_OK; // Keep servicing pairing while waiting for the user.
  _webSocket.disconnect();
  return E_NOK;
}

void SmartSnap::Run()
{
#if defined(ESP8266) && defined(SMARTSNAP_FAST_WIFI)
  if (_nonBlockingConnect) fastWiFi.run();
#endif
  if (_nonBlockingConnect && WiFi.status() != WL_CONNECTED) {
    if (_isDeviceConnected) {
      _webSocket.disconnect();
      _isDeviceConnected = false;
    }
    return;
  }
  const uint32_t healthNow = millis();
  _uptimeMs += uint32_t(healthNow - _uptimeTick);
  _uptimeTick = healthNow;
  _webSocket.loop();
  if(_discovery && !_isDeviceConnected && uint32_t(millis()-_discoveryAt)>10000UL && WiFi.status()==WL_CONNECTED){_discoveryAt=millis();EmitDeviceConnect();}
  if (_isDeviceConnected && (!_healthSent || uint32_t(healthNow - _healthAt) >= 300000UL)) {
    EmitHealth();
    _healthAt = healthNow;
    _healthSent = true;
  }
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
  StaticJsonDocument<768> doc;
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
  StaticJsonDocument<1024> doc;
  String output;

  doc["homeName"] = _homeName;
  doc["deviceID"] = _deviceId;
  if(_discovery){doc["discovery"]=true;doc["hardwareId"]=WiFi.macAddress();doc["suggestedName"]=_suggestedName;doc["model"]=_model;doc["icon"]=_icon;}
  JsonArray capabilities = doc.createNestedArray("capabilities");
  if (_capabilities & 1U) capabilities.add("ld2420");
  if (_capabilities & 4U) capabilities.add("custom_configuration");
  if (_capabilities & 2U) {
    capabilities.add("startup_behavior");
    capabilities.add("relay_startup"); // Compatibility with older apps and hubs.
  }
#if defined(ESP8266)
  doc["otaDeviceId"]=true;doc["otaReplaceConfiguration"]=true;doc["hardwareId"]=WiFi.macAddress();doc["otaCompletedJob"]=_otaCompletedJob;
  doc["ota"]=true;doc["firmwareVersion"]=_firmwareVersion;doc["sketchMD5"]=ESP.getSketchMD5();
#endif


  serializeJson(doc, output);
  _webSocket.emit("DeviceConnect", output.c_str());
}

void SmartSnap::EmitDeviceWriteVariable(const String& varName, const String& varType, const String& varValue)
{
  StaticJsonDocument<768> doc;
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

  doc["reportKind"] = "state";
  for (auto& receipt : _commandReceipts) {
    if (receipt.variable == varName && receipt.id.length() &&
        (receipt.value == varValue || ((varType == "int" || varType == "float") && [&]() {
          char *expectedEnd, *reportedEnd;
          double expected = strtod(receipt.value.c_str(), &expectedEnd);
          double reported = strtod(varValue.c_str(), &reportedEnd);
          return expectedEnd != receipt.value.c_str() && reportedEnd != varValue.c_str() &&
                 *expectedEnd == '\0' && *reportedEnd == '\0' &&
                 isfinite(expected) && isfinite(reported) && expected == reported;
        }()))) {
      doc["commandId"] = receipt.id;
      doc["reportKind"] = "command_result";
      receipt.id = "";
      break;
    }
  }
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
    _healthSent = false;
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

  StaticJsonDocument<768> doc;
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

  TrackCommand(varName, varValue, doc["commandId"] | "");
  if (_writeHandler) {
    _writeHandler(varName, varType, varValue);
  }
}

void SmartSnap::HandleGetVariableValueFromDevice(const char* payload, size_t length)
{
  USE_SERIAL.printf("GetVariableValueFromDevice: %.*s\n", (int)length, payload);

  StaticJsonDocument<768> doc;
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

  StaticJsonDocument<768> doc;
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
    if (v.is<const char*>()) varValue = v.as<const char*>();
    else serializeJson(v, varValue); // Server may store JSON numeric values.
  }


  TrackCommand(varName, varValue, doc["commandId"] | "");
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

void SmartSnap::EventPresence(const char* payload,size_t length){if(_instance && _instance->_presenceHandler && length<=4096)_instance->_presenceHandler(String(payload).substring(0,length));}
void SmartSnap::EventRelayConfigure(const char* payload,size_t length){if(_instance && _instance->_relayHandler && length<=1024)_instance->_relayHandler(String(payload).substring(0,length));}

void SmartSnap::SetCapability(Capability capability, bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(capability);
  if (bit > static_cast<uint8_t>(Capability::CustomConfiguration)) return;
  if (enabled) _capabilities |= (1UL << bit);
  else _capabilities &= ~(1UL << bit);
}


// A receipt is attached only to an explicit matching device value report.
// Receiving a command alone does not confirm that the application applied it.
void SmartSnap::TrackCommand(const String& variable, const String& value, const String& id) {
  for (auto& receipt : _commandReceipts) {
    if (receipt.variable == variable) { receipt.value=value; receipt.id=id; return; }
  }
  if (!id.length()) return;
  for (auto& receipt : _commandReceipts) {
    if (!receipt.id.length()) { receipt.variable=variable; receipt.value=value; receipt.id=id; return; }
  }
  // A full receipt buffer loses confirmation only; it never blocks control.
}

void SmartSnap::EmitHealth() {
  StaticJsonDocument<768> doc;
  doc["homeName"]=_homeName;
  doc["deviceID"]=_deviceId;
  if (!_bootId.length()) {
#if defined(ESP8266)
    _bootId=String(os_random(),HEX)+String(os_random(),HEX);
#else
    _bootId=String(esp_random(),HEX)+String(esp_random(),HEX);
#endif
  }
  doc["bootId"]=_bootId;
  doc["uptimeMs"]=_uptimeMs;
  doc["rssi"]=WiFi.RSSI();
  doc["freeHeap"]=ESP.getFreeHeap();
  doc["firmwareVersion"]=_firmwareVersion;
#if defined(ESP8266)
  doc["platform"]="ESP8266";
  const char* reasons[]={"Power-on / reset", "Hardware watchdog", "Firmware exception", "Software watchdog", "Software restart", "Deep-sleep wake", "External reset"};
  const uint32_t reason=ESP.getResetInfoPtr()->reason;
  doc["resetReason"]=reason<7?reasons[reason]:"Unknown";
#else
  doc["platform"]="ESP32";
  const char* reasons[]={"Unknown", "Power-on", "External reset", "Software restart", "Firmware panic", "Interrupt watchdog", "Task watchdog", "Other watchdog", "Deep-sleep wake", "Brownout", "SDIO reset", "USB reset", "JTAG reset", "eFuse error", "Power glitch", "CPU lockup"};
  const int reason=int(esp_reset_reason());
  doc["resetReason"]=reason>=0 && reason<16?reasons[reason]:"Unknown";
#endif
  String output;serializeJson(doc,output);
  _webSocket.emit("DeviceHealth",output.c_str());
}

void SmartSnap::EventDefinition(const char*,size_t) {
  if(!_instance||!_instance->_discovery)return;
  DynamicJsonDocument doc(8192);JsonArray list=doc.createNestedArray("variables");
  for(size_t i=0;i<_instance->_definitionCount;i++){const auto& v=_instance->_definitions[i];JsonObject item=list.createNestedObject();item["name"]=v.name;item["type"]=v.type;item["value"]=v.value;item["configuration"]=v.configuration;}
  String out;serializeJson(doc,out);_webSocket.emit("DeviceDefinition",out.c_str());
}
void SmartSnap::EventAssigned(const char* payload,size_t length) {
  if(!_instance||!_instance->_discovery)return;
  DynamicJsonDocument doc(512);if(deserializeJson(doc,payload,length)!=DeserializationError::Ok)return;
  int id=doc["deviceID"]|0;
  if(id<=0||doc["homeName"].as<String>()!=_instance->_homeName||doc["hardwareId"].as<String>()!=WiFi.macAddress())return;
#if defined(ESP8266)
  if(!LittleFS.begin())return;
  DynamicJsonDocument saved(256);saved["id"]=id;saved["home"]=_instance->_homeName;
  File f=LittleFS.open("/smartsnap-assignment.tmp","w");if(!f)return;
  bool ok=serializeJson(saved,f)>0;f.close();if(!ok||!LittleFS.rename("/smartsnap-assignment.tmp","/smartsnap-assignment.json"))return;
#else
  Preferences prefs;if(!prefs.begin("smartsnap",false))return;prefs.putString("home",_instance->_homeName);prefs.putInt("id",id);prefs.end();
#endif
  _instance->_deviceId=id;_instance->EmitDeviceConnect();
}
