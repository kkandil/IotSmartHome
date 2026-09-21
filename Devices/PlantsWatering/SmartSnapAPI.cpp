#include "core_esp8266_features.h"
#include "SmartSnapAPI.h"

#define USE_SERIAL Serial
#define WAIT_FOR_ACK_DELAY_SEC  10*10


ESP8266WiFiMulti WiFiMulti;
SocketIoClient webSocket;

bool isDeviceConnected = false;
bool isAckReceived = false;

void EventDeviceConnect(const char * payload, size_t length);
void EventMessage(const char * payload, size_t length);
void EventConnect(const char * payload, size_t length);
void EventDisconnect(const char * payload, size_t length);
void EventWriteVariable(const char * payload, size_t length);
void EventGetVariableValueFromDevice(const char * payload, size_t length);
void EventDeviceWriteVariable(const char * payload, size_t length);
void EventGetVariableValueFromServer(const char * payload, size_t length);

SmartSnap::SmartSnap()
{ 
}

int SmartSnap::Initialize (const int deviceId, const char* wifiSsid, const char* constPass)
{
  int retVal = E_NOK;
  
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  for(uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }
  WiFiMulti.addAP(wifiSsid, constPass);
  while(WiFiMulti.run() != WL_CONNECTED) {
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

  webSocket.begin("smarthome.herokuapp.com"); 

  delay(10000);
  
//  const size_t CAPACITY = JSON_OBJECT_SIZE(2);
//  StaticJsonDocument<CAPACITY> doc;
//  
//  // create an object
//  String output; 
//  JsonObject object = doc.to<JsonObject>();
//  object["deviceID"] = (int)123456;  
//  serializeJson(doc, output); 
//  webSocket.emit("DeviceConnect", output.c_str()); 
  for(uint8_t t = WAIT_FOR_ACK_DELAY_SEC; t > 0; t--) {
    webSocket.loop();
    if( isAckReceived == true)
      break;
    delay(100);
  }
  if( isAckReceived == true && isDeviceConnected == true)
  {
    retVal = E_OK;
  }
  else if( isAckReceived == false || isDeviceConnected == false )
  {
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
  StaticJsonDocument<200> doc2;
    String output; 
    JsonObject object = doc2.to<JsonObject>();
    object["deviceID"] = (int)111222;  
    object["varName"] = VarName;  
    object["varType"] = VarType;  
    if( VarType == "int")
    {
      object["varValue"] = (int)Value.toInt();  
    }
    else if( VarType == "string")
    {
      object["varValue"] = Value;  
    }
    serializeJson(doc2, output); 
    webSocket.emit("DeviceWriteVariable", output.c_str()); 
}
 
void SmartSnap::RequestVariableValueFromServer(String VarName)
{
  String output; 
  StaticJsonDocument<200> doc2;
  JsonObject object = doc2.to<JsonObject>();
  object["deviceID"] = (int)111222;  
  object["varName"] = VarName;   
  serializeJson(doc2, output);  
  webSocket.emit("GetVariableValueFromServer", output.c_str());  
}

void EventGetVariableValueFromServer(const char * payload, size_t length) { 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  String deviceId = doc["deviceID"];
  String varName = doc["varName"]; 
  String varValue = doc["Value"];   
  USE_SERIAL.printf("GetVariableValueFromServer deviceId=%s,varName=%s,varValue=%s\n", deviceId,varName,varValue);
  
  SmartSnapServerWriteVariable(varName, varValue);
  
}

 
void EventDeviceConnect(const char * payload, size_t length) {
  USE_SERIAL.printf("DeviceConnect: %s\n", payload);
  if( strstr(payload,"OK") != NULL )
  {
    USE_SERIAL.printf("OK\n");
    isDeviceConnected = true; 
  }
  else
  {
    USE_SERIAL.printf("NOK\n");
    isDeviceConnected = false;
  }
  isAckReceived = true;
}

void EventMessage(const char * payload, size_t length) {
  USE_SERIAL.printf("Message: %s\n", payload);
}

void EventConnect(const char * payload, size_t length)
{
  USE_SERIAL.printf("connected to server\n");
  const size_t CAPACITY = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<CAPACITY> doc;
  String output; 
  JsonObject object = doc.to<JsonObject>();
  object["deviceID"] = (int)111222;  
  serializeJson(doc, output); 
  webSocket.emit("DeviceConnect", output.c_str()); 
}

void EventDisconnect(const char * payload, size_t length) {
  USE_SERIAL.printf("Disconnected from server\n");
}

void EventWriteVariable(const char * payload, size_t length) {
  USE_SERIAL.printf("WriteVariable: %s\n", payload); 
  StaticJsonDocument<200> doc;
   deserializeJson(doc, payload);
   String varName = doc["varName"];
   String varType = doc["varType"];
   String varValue = String(doc["varValue"]);
   USE_SERIAL.printf("varName=%s , varType=%s , varValue=%f\n", varName, varType, varValue);

//   unWriteData value;
//   switch( varType){
//    case "string":
//      value.Str = doc["varValue"];
//    break;
//    case "int":
//      value.Int = (int) doc["varValue"];
//    break;
//    case "float":
//      value.Double = (double) doc["varValue"];
//    break;
//   } 
 
    SmartSnapWriteVariable(varName, varType, varValue);  
   
}

void EventGetVariableValueFromDevice(const char * payload, size_t length) { 
  StaticJsonDocument<200> doc;
   deserializeJson(doc, payload);
   String varName = doc["varName"];  
   USE_SERIAL.printf("GetVariableValueFromDevice varName=%s\n", varName);

    String varType = "" ;
    String varValue = "" ;
   SmartSnapGetVariable(varName, varType, &varValue); 
   USE_SERIAL.printf("varType=%s, varValue=%s\n", varType, varValue);
 
    StaticJsonDocument<200> doc2;
    String output; 
    JsonObject object = doc2.to<JsonObject>();
    object["deviceID"] = (int)111222;  
    object["varName"] = varName;  
    object["varType"] = varType;  
    if( varType == "int")
    {
      object["varValue"] = (int)varValue.toInt();  
    }
    else if( varType == "float")
    {
      object["varValue"] = (float)varValue.toFloat();
    }
    else if( varType == "string")
    {
      object["varValue"] = varValue;
    }
    serializeJson(doc2, output); 
    webSocket.emit("DeviceWriteVariable", output.c_str()); 
}

void EventDeviceWriteVariable(const char * payload, size_t length) { 
  USE_SERIAL.printf("DeviceWriteVariable: %s\n", payload);
}

