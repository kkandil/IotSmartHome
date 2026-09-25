#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "SmartSnapHubConfig.h"

SmartSnap smartSnap;

int led1State = 0;
float potValPrev = 0.0;
void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{
  if (varName == "V1")
  { 
    if(value.toInt() == 1)
    {
      led1State = 1;
      digitalWrite(D5, HIGH);
    }
    else
    {
      led1State = 0;
      digitalWrite(D5, LOW);
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
    value = String(potValPrev, 2);
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
unsigned long timerStart = 0;
void setup()
{
  Serial.begin(115200);
  
  pinMode(D5, OUTPUT); 

  digitalWrite(D5, LOW); 

  smartSnap.onWriteVariable(HandleWriteVariable);
  smartSnap.onReadVariable(HandleReadVariable);
  smartSnap.onServerValue(HandleServerValue);
  smartSnap.onConnectionStatus(HandleConnectionStatus);

  smartSnap.SetFirmwareVersion("TestDev2_1.0.0");
  int ret = smartSnap.Initialize( "Home_Germany",1012,"KS_DSL", "2wad@dsl", SMARTSNAP_HUB_HOST, SMARTSNAP_HUB_PORT);

  if (ret == E_OK) {
    Serial.println("SmartSnap initialized successfully");
  } else {
    Serial.println("SmartSnap initialization failed");
  }
  timerStart = millis();
}

void loop()
{
  float potVal = analogRead(A0);
  if((potVal > potValPrev + 20 || potVal < potValPrev - 20) && millis()-timerStart>1000)
  {
    potValPrev = potVal;
    if(potVal>800)
    {
      String msg = "New potVal recieved: "+String(potVal, 2);
      smartSnap.SendAlarmToPhone(msg);
    }
    smartSnap.WriteVariableValue("V2", "float", String(potVal, 2));
    timerStart = millis(); 
  }
  delay(10);
  smartSnap.Run();

    //smartSnap.WriteVariableValue("temperature", "float", String(temperatureValue, 2));
    //smartSnap.RequestVariableValueFromServer("deviceLabel");
  
}