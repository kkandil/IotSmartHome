#include "DeviceDefinition.h"
#include <Arduino.h>
#include <SmartSnapAPI.h>
#include "SmartSnapHubConfig.h"
#include <Servo.h>

Servo myservo;

//#define USE_SERIAL Serial
SmartSnap smartSnap;

bool isHeaterActive = false;

bool isServoAttached = false;
unsigned long servoDetachStart = 0;
#define SERVO_DETACH_DELAY_MS 1500
#define HEATER_SERVO_ON_POS 12 
#define posHallSensorPin D5

int posHallSensorStatePrev = 0;
unsigned long PosHallSensorUpdateTimeStart = 0;
#define HALL_SENSOR_DEBOUNCE_DELAY_MS 500
bool isHallSensorPosUpdated = false;

void setup() {
  int retVal = E_NOK;
  Serial.begin(115200); 
  pinMode(posHallSensorPin, INPUT);
  myservo.attach(D2); 
   myservo.write(12); 
  delay(1000);
  isServoAttached = true;
  servoDetachStart = millis(); 

  smartSnap.EnableDiscovery("WaterHeater", "WaterHeater", "gas_water_heater");
  smartSnap.SetVariableDefinitions(DEVICE_VARIABLES, sizeof(DEVICE_VARIABLES)/sizeof(DEVICE_VARIABLES[0]));
  smartSnap.onWriteVariable(HandleWriteVariable);
  smartSnap.onReadVariable(HandleReadVariable);
  smartSnap.onServerValue(HandleServerValue);
  smartSnap.onConnectionStatus(HandleConnectionStatus);

  smartSnap.SetFirmwareVersion("WaterHeater_1.1.3");
  int ret = smartSnap.Initialize( "Home_Germany",1003,"KS_DSL", "2wad@dsl", SMARTSNAP_HUB_HOST, SMARTSNAP_HUB_PORT);
  if (ret == E_OK) {
    Serial.println("SmartSnap initialized successfully");
  } else {
    Serial.println("SmartSnap initialization failed");
  }

}

void loop() {
  int posHallSensorStateCur = 0;

  smartSnap.Run();
  if( isServoAttached == true && millis() >= (servoDetachStart+SERVO_DETACH_DELAY_MS))
  {
    isServoAttached = false;
    myservo.detach();
  }

  posHallSensorStateCur = digitalRead(posHallSensorPin);
  if( posHallSensorStateCur != posHallSensorStatePrev)
  {
    PosHallSensorUpdateTimeStart = millis(); 
    isHallSensorPosUpdated = true;
  }
// Serial.println(posHallSensorStateCur);
  if( (millis()-PosHallSensorUpdateTimeStart) > HALL_SENSOR_DEBOUNCE_DELAY_MS && isHallSensorPosUpdated == true)
  {
    Serial.print(isHeaterActive);
    Serial.print(", ");
    Serial.println(posHallSensorStateCur);
    if( posHallSensorStateCur == 0 && isHeaterActive == false)
    {
      smartSnap.WriteVariableValue("V1", "int", "1");
      isHeaterActive = true;
    } 
    else if( posHallSensorStateCur == 1 && isHeaterActive == true)
    {
      smartSnap.WriteVariableValue("V1", "int", "0");
      isHeaterActive = false;
    }
    PosHallSensorUpdateTimeStart = 0;
    isHallSensorPosUpdated = false;
  }
  posHallSensorStatePrev = posHallSensorStateCur;
  // if( posHallSensorStateCur != posHallSensorStatePrev && millis() >= (PosHallSensorUpdateTimeStart + HALL_SENSOR_DEBOUNCE_DELAY_MS))
  // {
  //   Serial.print("HAL: ");
  //   Serial.println(posHallSensorStateCur);
  //   if( posHallSensorStateCur == 0)
  //   {
  //     SmartSnap.WriteVariableValue("V1", "int", "1");
  //   } 
  //   else
  //   {
  //     SmartSnap.WriteVariableValue("V1", "int", "0");
  //   }
  //   posHallSensorStatePrev = posHallSensorStateCur;
  //   PosHallSensorUpdateTimeStart = millis(); 
  // }
}

 
void HandleWriteVariable(const String& varName, const String& varType, const String& value)
{
  
   if( varName == "V1" )
   {
      if (value != "0" && value != "1") return;
      int val = value.toInt();
      if( val == 1)
      {
        isHeaterActive = true;
        if( isServoAttached == false)
        {
          myservo.attach(D2);
        } 
        myservo.write(83);
        isServoAttached = true;
        servoDetachStart = millis(); 
      }
      else
      {
        isHeaterActive = false;
        if( isServoAttached == false)
        {
          myservo.attach(D2); 
        }
        myservo.write(12);
        servoDetachStart = millis(); 
        isServoAttached = true;
      }
      // Report after setting the servo output. This confirms software state, not physical movement.
      smartSnap.WriteVariableValue("V1", "int", isHeaterActive ? "1" : "0");
      Serial.print("V1 updated with: ");
      Serial.println(String(val));
   }
   else
   {
    Serial.println("Unkown Variable");
   }
}

bool HandleReadVariable(const String& varName, String& varType, String& value)
{
  if( varName == "V1" )
  {
    varType = "int" ;
    if( isHeaterActive == true )
    {
      value = "1";
    }
    else
    {
      value = "0";
    }
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