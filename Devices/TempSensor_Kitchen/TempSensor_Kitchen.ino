#include <Arduino.h>
#include "SmartSnapAPI.h"
#include "DHTesp.h" // Click here to get the library: http://librarymanager/All#DHTesp
#include "SmartSnapHubConfig.h"

// #ifdef ESP32
// #pragma message(THIS EXAMPLE IS FOR ESP8266 ONLY!)
// #error Select ESP8266 board.
// #endif

DHTesp dht;

SmartSnap smartSnap;

 
#define SLEEP_TIME_IN_MIN 1
unsigned long timer =0;

void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{ 
  Serial.printf("varName: %s =  %s, int: %d\n", varName, value, value.toInt());
  if (varName == "update") 
  {
    if ( value.toInt() == 1)
    {
      Serial.printf("test: %s =  %s, int: %d\n", varName, value, value.toInt());
      float humidity = dht.getHumidity();
      float temperature = dht.getTemperature();
      smartSnap.WriteVariableValue("Temp", "float", String(temperature, 2)); 
      smartSnap.WriteVariableValue("Humid", "float", String(humidity, 2));
      smartSnap.WriteVariableValue("status", "string", dht.getStatusString());
    }
  }
  if (varName == "test") 
  {
  }
}

bool HandleReadVariable(const String& varName, String& varType, String& value)
{
  // Serial.printf("HandleReadVariable: %s\n", varName.c_str());
  if (varName == "Temp") {
    float temperature = dht.getTemperature();
    varType = "float";
    value = String(temperature,2);
    return true;
  }
  else if (varName == "Humid") {
    float humidity = dht.getHumidity();
    varType = "float";
    value = String(humidity,2);
    return true;
  }
  else if (varName == "status") {
    varType = "float";
    value = dht.getStatusString();
    return true;
  }

  // if (varName == "temperature") {
  //   varType = "float";
  //   value = String(temperatureValue, 2);
  //   return true;
  // }

  // if (varName == "deviceLabel") {
  //   varType = "string";
  //   value = deviceLabel;
  //   return true;
  // }

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

  delay(500);

  smartSnap.onWriteVariable(HandleWriteVariable);
  smartSnap.onReadVariable(HandleReadVariable);
  smartSnap.onServerValue(HandleServerValue);
  smartSnap.onConnectionStatus(HandleConnectionStatus);

  int ret = smartSnap.Initialize( "Home_Germany", 1011, "KS_DSL", "2wad@dsl", SMARTSNAP_HUB_HOST, SMARTSNAP_HUB_PORT);

  if (ret == E_OK) {
    Serial.println("SmartSnap initialized successfully");
  } else {
    Serial.println("SmartSnap initialization failed");
  }

  dht.setup(0, DHTesp::DHT22); // Connect DHT sensor to GPIO 17
 
  
  // timer = millis();
  // while(millis()-timer < 1000)
  // {
  //   smartSnap.Run();
  // }
  smartSnap.Run();

  smartSnap.WriteVariableValue("Temp", "float", String(dht.getTemperature(), 2)); 
  smartSnap.WriteVariableValue("Humid", "float", String(dht.getHumidity(), 2));
  smartSnap.WriteVariableValue("status", "string", dht.getStatusString());
  timer = millis();
  while(millis()-timer < 1000)
  {
    smartSnap.Run();
  }
  smartSnap.Disconnect();
  delay(500);

  ESP.deepSleep(SLEEP_TIME_IN_MIN * 60 * 1000000ULL);
  delay(500);
}


void loop()
{
  // smartSnap.Run(); 
}