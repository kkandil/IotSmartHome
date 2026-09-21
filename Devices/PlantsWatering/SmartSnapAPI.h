
#ifndef SMARTSNAPAPI_H
#define SMARTSNAPAPI_H
/*
 *  Mandatory includes
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <SocketIoClient.h> 
#include <ArduinoJson.h>

#define E_OK  0
#define E_NOK 1

//typedef union unWriteDataType{
//  String Str;
//  int Int;
//  double Double;
//}unWriteData;

/*
 *  Class
 */
class SmartSnap {
  public:
    // Constructor
    SmartSnap();
    
    // Setup method
    int Initialize (const int deviceId, const char* wifiSsid, const char* constPass);

    void Run();

    void WriteVariableValue(String VarName, String VarType, String Value);
    void RequestVariableValueFromServer(String VarName);

  private:
};

extern void SmartSnapWriteVariable(String VarName, String VarType, String Value);
extern void SmartSnapGetVariable(String VarName, String& VarType, String* Value);
extern void SmartSnapServerWriteVariable(String VarName, String Value);
#endif
