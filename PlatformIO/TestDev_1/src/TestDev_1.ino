#include "DeviceDefinition.h"
#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "SmartSnapHubConfig.h"

SmartSnap api;

int SliderCntrInt = 0;
float SliderCntrFloat = 0.0;
int led1State = 0;
float led2State = 0.0;
void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{
  if (varName == "V1")
  { 
    if(value.toInt() == 1)
    {
      led1State = 1;
      digitalWrite(D5, HIGH);
      api.WriteVariableValue("V1", "int", String(led1State));
    }
    else
    {
      led1State = 0;
      digitalWrite(D5, LOW);
      api.WriteVariableValue("V1", "int", String(led1State));
    }
   }
  else if (varName == "V2")
  { 
    if(value.toFloat() == 1.0)
    {
      led2State = 1.0;
      digitalWrite(D6, HIGH);
      api.WriteVariableValue("V2", "float", String(led2State));
    }
    else
    {
      led2State = 0.0;
      digitalWrite(D6, LOW);
      api.WriteVariableValue("V2", "float", String(led2State));
    }
  }
}

bool HandleReadVariable(const String& varName, String& varType, String& value)
{
  if (varName == "V1")
  {  
    varType = "int" ;
    value = String(led1State) ;  
    return true;
   }
   else if (varName == "V2")
  {  
    varType = "float" ;
    value = String(led2State, 2);
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
  api.SetFirmwareVersion("TestDev_1.4.3");
  
  pinMode(D5, OUTPUT);
  pinMode(D6, OUTPUT);

  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);

  api.EnableDiscovery("TestDev_1", "TestDev_1", "sensor");
  api.SetVariableDefinitions(DEVICE_VARIABLES, sizeof(DEVICE_VARIABLES)/sizeof(DEVICE_VARIABLES[0]));
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