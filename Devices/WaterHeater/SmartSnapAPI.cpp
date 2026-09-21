#include "SmartSnapAPI.h"

// ESP8266-only header removed:
// #include "core_esp8266_features.h"

#define USE_SERIAL Serial
#define WAIT_FOR_ACK_DELAY_SEC  (10 * 10)

#if defined(ESP8266)
ESP8266WiFiMulti WiFiMulti;
#else
WiFiMulti WiFiMulti;
#endif

SocketIoClient webSocket;

static bool isDeviceConnected = false;
static bool isAckReceived = false;

static void EventDeviceConnect(const char * payload, size_t length);
static void EventMessage(const char * payload, size_t length);
static void EventConnect(const char * payload, size_t length);
static void EventDisconnect(const char * payload, size_t length);
static void EventWriteVariable(const char * payload, size_t length);
static void EventGetVariableValueFromDevice(const char * payload, size_t length);
static void EventDeviceWriteVariable(const char * payload, size_t length);
static void EventGetVariableValueFromServer(const char * payload, size_t length);

SmartSnap::SmartSnap()
{
}

int SmartSnap::Initialize(const int deviceId, const char* wifiSsid, const char* constPass)
{
  _deviceId = deviceId;
  int retVal = E_NOK;

  // Some cores support this, some don't — safe guard:
  #if defined(ESP8266)
    USE_SERIAL.setDebugOutput(true);
  #endif

  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  WiFiMulti.addAP(wifiSsid, constPass);

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  webSocket.on("DeviceConnect", EventDeviceConnect);
  webSocket.on("message", EventMessage);
  webSocket.on("connect", EventConnect);
  webSocket.on("disconnect", EventDisconnect);
  webSocket.on("PhoneWriteVariable", EventWriteVariable);
  webSocket.on("GetVariableValueFromDevice", EventGetVariableValueFromDevice);
  webSocket.on("DeviceWriteVariable", EventDeviceWriteVariable);
  webSocket.on("GetVariableValueFromServer", EventGetVariableValueFromServer);

  // If your SocketIoClient version requires port/url, see notes below.
  webSocket.begin("smarthome.herokuapp.com");

  delay(10000);

  for (uint8_t t = WAIT_FOR_ACK_DELAY_SEC; t > 0; t--) {
    webSocket.loop();
    if (isAckReceived) break;
    delay(100);
  }

  if (isAckReceived && isDeviceConnected) {
    retVal = E_OK;
  } else {
    retVal = E_NOK;
    webSocket.disconnect();
  }

  isAckReceived = false;
  return retVal;
}

void SmartSnap::Run(void)
{
  webSocket.loop();
}

void SmartSnap::WriteVariableValue(String VarName, String VarType, String Value)
{
  StaticJsonDocument<200> doc;
  String output;
  JsonObject object = doc.to<JsonObject>();

  object["homeName"] = "Home_Germany";
  object["deviceID"] = (int)_deviceId;
  object["varName"]  = VarName;
  object["varType"]  = VarType;

  if (VarType == "int") {
    object["varValue"] = (int)Value.toInt();
  } else if (VarType == "float") {
    object["varValue"] = (float)Value.toFloat();
  } else { // "string" or anything else
    object["varValue"] = Value;
  }

  serializeJson(doc, output);
  webSocket.emit("DeviceWriteVariable", output.c_str());
}

void SmartSnap::RequestVariableValueFromServer(String VarName)
{
  String output;
  StaticJsonDocument<200> doc;
  JsonObject object = doc.to<JsonObject>();

  object["homeName"] = "Home_Germany";
  object["deviceID"] = (int)_deviceId;
  object["varName"]  = VarName;

  serializeJson(doc, output);
  webSocket.emit("GetVariableValueFromServer", output.c_str());
}

static void EventGetVariableValueFromServer(const char * payload, size_t length)
{
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (GetVariableValueFromServer): %s\n", err.c_str());
    return;
  }

  
  String deviceId = doc["deviceID"] | "";
  String varName  = doc["varName"]  | "";
  String varValue = doc["Value"]    | "";

  USE_SERIAL.printf("GetVariableValueFromServer deviceId=%s, varName=%s, varValue=%s\n",
                    deviceId.c_str(), varName.c_str(), varValue.c_str());

  SmartSnapServerWriteVariable(varName, varValue);
}

static void EventDeviceConnect(const char * payload, size_t length)
{
  USE_SERIAL.printf("DeviceConnect: %.*s\n", (int)length, payload);

  if (strstr(payload, "OK") != NULL) {
    USE_SERIAL.printf("OK\n");
    isDeviceConnected = true;
  } else {
    USE_SERIAL.printf("NOK\n");
    isDeviceConnected = false;
  }

  isAckReceived = true;
}

static void EventMessage(const char * payload, size_t length)
{
  USE_SERIAL.printf("Message: %.*s\n", (int)length, payload);
}

static void EventConnect(const char * payload, size_t length)
{
  USE_SERIAL.printf("connected to server\n");

  StaticJsonDocument<64> doc;
  String output;
  JsonObject object = doc.to<JsonObject>();

  // NOTE: this uses the same hardcoded device id behavior as your original connect handler.
  // If you want this to use _deviceId too, we can refactor by storing a global pointer, or making handlers members.
  object["homeName"] = "Home_Germany";
  object["deviceID"] = (int)1004;

  serializeJson(doc, output);
  webSocket.emit("DeviceConnect", output.c_str());
}

static void EventDisconnect(const char * payload, size_t length)
{
  USE_SERIAL.printf("Disconnected from server\n");
}

static void EventWriteVariable(const char * payload, size_t length)
{
  USE_SERIAL.printf("WriteVariable: %.*s\n", (int)length, payload);

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (WriteVariable): %s\n", err.c_str());
    return;
  }

  String varName  = doc["varName"]  | "";
  String varType  = doc["varType"]  | "";
  // String varValue = String(doc["varValue"] | "");

  String varValue;
  JsonVariant v = doc["varValue"];

  if (v.isNull()) {
    varValue = "";
  } else if (varType == "int") {
    varValue = String(v.as<long>());
  } else if (varType == "float") {
    varValue = String(v.as<float>(), 6);
  } else {
    // string (or anything else)
    varValue = String(v.as<const char*>());
  }

  USE_SERIAL.printf("varName=%s , varType=%s , varValue=%s\n",
                    varName.c_str(), varType.c_str(), varValue.c_str());

  SmartSnapWriteVariable(varName, varType, varValue);
}

static void EventGetVariableValueFromDevice(const char * payload, size_t length)
{
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    USE_SERIAL.printf("JSON parse error (GetVariableValueFromDevice): %s\n", err.c_str());
    return;
  }

  String varName = doc["varName"] | "";
  USE_SERIAL.printf("GetVariableValueFromDevice varName=%s\n", varName.c_str());

  String varType = "";
  String varValue = "";
  SmartSnapGetVariable(varName, varType, &varValue);

  USE_SERIAL.printf("varType=%s, varValue=%s\n", varType.c_str(), varValue.c_str());

  StaticJsonDocument<200> doc2;
  String output;
  JsonObject object = doc2.to<JsonObject>();

  object["homeName"] = "Home_Germany";
  object["deviceID"] = (int)1004;
  object["varName"]  = varName;
  object["varType"]  = varType;

  if (varType == "int") {
    object["varValue"] = (int)varValue.toInt();
  } else if (varType == "float") {
    object["varValue"] = (float)varValue.toFloat();
  } else {
    object["varValue"] = varValue;
  }

  serializeJson(doc2, output);
  webSocket.emit("DeviceWriteVariable", output.c_str());
}

static void EventDeviceWriteVariable(const char * payload, size_t length)
{
  USE_SERIAL.printf("DeviceWriteVariable: %.*s\n", (int)length, payload);
}
