#include "SmartSnapAPI.h"
#include <Servo.h>

Servo myservo;

//#define USE_SERIAL Serial
SmartSnap SmartSnap;

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
//  pinMode(D2, OUTPUT);
//  digitalWrite(D2, HIGH);
//  while( retVal == E_NOK)
//  {
    retVal = SmartSnap.Initialize(1003, "KS_DSL", "2wad@dsl");
//    if( retVal == E_NOK)
//      delay(5000);
//  }
}

void loop() {
  int posHallSensorStateCur = 0;

  SmartSnap.Run();
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
      SmartSnap.WriteVariableValue("V1", "int", "1");
      isHeaterActive = true;
    } 
    else if( posHallSensorStateCur == 1 && isHeaterActive == true)
    {
      SmartSnap.WriteVariableValue("V1", "int", "0");
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


extern void SmartSnapWriteVariable(String VarName, String VarType, String Value)
{
  
   if( VarName == "V1" )
   {
      int val = Value.toInt();
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
//        Serial.println("OFF") ;
      }
      Serial.print("V1 updated with: ");
      Serial.println(String(val));
   }
   else
   {
    Serial.println("Unkown Variable");
   }
}

extern void SmartSnapGetVariable(String VarName, String& VarType, String* Value)
{
  if( VarName == "V1" )
  {
    VarType = "int" ;
    if( isHeaterActive == true )
    {
      *Value = "1";
    }
    else
    {
      *Value = "0";
    }
  }
}
//void loop() {
//    webSocket.loop(); 
//    while (Serial.available()) {
//      char inChar = (char)Serial.read();
//      inputString += inChar;
//      if (inChar == '\n') {
////        DynamicJsonDocument doc(1024);
//////        JsonArray array = doc.to<JsonArray>();
//////        array.add("message");
//////        JsonObject param1 = array.createNestedObject();
//////        param1["p1"]     = (uint32_t) 10;
//////        param1["p2"]     = (uint32_t) 20;
////        JsonObject obj = doc.createNestedObject();
////        obj["p1"] = (uint32_t) 10;
////        obj["p2"] = (uint32_t) 10;
////         String output;
////        serializeJson(doc, output);
//        const size_t CAPACITY = JSON_OBJECT_SIZE(2);
//        StaticJsonDocument<CAPACITY> doc;
//        
//        // create an object
//        String output;
//        JsonObject object = doc.to<JsonObject>();
//        object["p1"] = 10;
//        object["p2"] = 20;
//        
//        serializeJson(doc, output);
////        Serial.println(output);
//        webSocket.emit("message", output.c_str()); 
////          webSocket.emit("message", "khaled");
//
////        inputString.trim();
////        String asd = "\"" + inputString + "\"";
////        webSocket.emit("message", asd.c_str());
////        inputString = "";
//      }
//    }
//}
extern void SmartSnapServerWriteVariable(String VarName, String Value)
{
}