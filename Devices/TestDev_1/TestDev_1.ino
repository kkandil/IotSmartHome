#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "SmartSnapHubConfig.h"

SmartSnap api;

int SliderCntrInt = 0;
float SliderCntrFloat = 0.0;

void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{
  if (varName == "SliderCntrInt")
  { 
    SliderCntrInt = value.toInt();
    api.WriteVariableValue("SliderCtrlInt_State", "int", String(SliderCntrInt));
   }
   else if (varName == "SliderCntrFloat")
  { 
    SliderCntrFloat = value.toFloat();
    api.WriteVariableValue("SliderCtrlFloat_State", "float", String(SliderCntrFloat, 2));
  }
}

bool HandleReadVariable(const String& varName, String& varType, String& value)
{
  if (varName == "SliderCntrInt")
  {  
    varType = "int" ;
    value = String(SliderCntrInt) ;  
    return true;
   }
   else if (varName == "SliderCntrFloat")
  {  
    varType = "float" ;
    value = String(SliderCntrFloat, 2);
    return true;
  }
  return false;
}

void HandleServerValue(const String& varName, const String& value)
{
  Serial.printf("HandleServerValue: %s = %s\n", varName.c_str(), value.c_str());
}

void HandleConnectionStatus(bool connected)
{
  Serial.printf("Connection status: %s\n", connected ? "connected" : "disconnected");
}

void setup()
{
  Serial.begin(115200);

  api.onWriteVariable(HandleWriteVariable);
  api.onReadVariable(HandleReadVariable);
  api.onServerValue(HandleServerValue);
  api.onConnectionStatus(HandleConnectionStatus);

  int ret = api.Initialize( "Home_Germany",1007,"KS_DSL", "2wad@dsl", SMARTSNAP_HUB_HOST, SMARTSNAP_HUB_PORT);

  if (ret == E_OK) {
    Serial.println("SmartSnap initialized successfully");
  } else {
    Serial.println("SmartSnap initialization failed");
  }
}

void loop()
{
  api.Run();

    //api.WriteVariableValue("temperature", "float", String(temperatureValue, 2));
    //api.RequestVariableValueFromServer("deviceLabel");
  
}