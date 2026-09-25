#include "SmartSnapAPI.h"
#include <Ultrasonic.h>
#include "SmartSnapHubConfig.h"
 
// TrigPin = D3, EchoPin = D2 
Ultrasonic ultrasonic(D6, D7);
 

//#define USE_SERIAL Serial
SmartSnap SmartSnap;
 
#define PUMP1_PIN D0
#define PUMP2_PIN D5
#define LED_PIN D2
#define WATER_SENSOR_PIN D1
#define MOISTURE_SENSOR_PIN A0

#define PUMP1_CONTROL_VAR "V1"
#define PUMP2_CONTROL_VAR "V2"
#define PUMP1_STATUS_VAR "V3"
#define PUMP2_STATUS_VAR "V4"
#define MOISTUR_VALUE_VAR "V5"

#define NUMBER_OF_PUMPS 2
#define PUMP1_ACTIVE_TIMEOUT_SEC 60
#define PUMP2_ACTIVE_TIMEOUT_SEC 60
#define ACTIVATION_BLOCK_TIMER_RESOLUTION 60  // (1min)
#define ACTIVATION_BLOCK_COUNTER_MAX  1*24*ACTIVATION_BLOCK_TIMER_RESOLUTION // 1 days 

#define MOISTURE_SENSOR_UPDATE_TIMER_RESOLUTION 60  // (1min)
#define MOISTURE_SENSOR_UPDATE_COUNTER_MAX  1*MOISTURE_SENSOR_UPDATE_TIMER_RESOLUTION  // 1 hour

#define WATER_SENSOR_DEBOUNCE_DELAY_MS 500

int moistureSensorTimerCounts = 0;
unsigned long moistureSensorTimerStart = 0;

typedef enum{
  PUMP_STATE_OFF=0,
  PUMP_STATE_ON=1
}enPumpState;

typedef enum{
 WATER_SENSOR_STATE_NOTDETECTED=0,
  WATER_SENSOR_STATE_DEBOUNCING_ON,
  WATER_SENSOR_STATE_DETECTED,
  WATER_SENSOR_STATE_DEBOUNCING_OFF
}enWaterSensorStates;

#define REQ_STATE_ON 1
#define REQ_STATE_OFF 0

typedef struct{
  String statusVarName;
  enPumpState currentState;
  enPumpState prevState;
  int outputPin;
  unsigned long activationStartTime;
  bool isActivationBlocked;
  unsigned long activationBlockTimerStart;
  int activationBlockCounter;
  int activeTimeoutSec;
}strPumps;

strPumps Pumps[NUMBER_OF_PUMPS] = { 
  {PUMP1_STATUS_VAR,PUMP_STATE_OFF,PUMP_STATE_OFF,PUMP1_PIN,0,false,0,0,PUMP1_ACTIVE_TIMEOUT_SEC},
  {PUMP2_STATUS_VAR,PUMP_STATE_OFF,PUMP_STATE_OFF,PUMP2_PIN,0,false,0,0,PUMP2_ACTIVE_TIMEOUT_SEC}
};


unsigned long waterSensorDebounceTimer=0;
enWaterSensorStates waterSensorState = WATER_SENSOR_STATE_NOTDETECTED;
bool isWaterSensorActive = true;
unsigned long waterLevelSensorUpdateTimer=0; 
int waterSensorValueCM = 0;

void SetPumpOutputState(int pumpIndex, int state);
void PumpTimersHandler(int index);
void MoistureSensorHandler(void);
void WaterSensorHandler(void);
void WaterLevelSensorHandler(void);


void HandleWriteVariable(const String& varName, const String& VarType, const String& value)
{
  int outputState = 0;

  if( varName == PUMP1_CONTROL_VAR )
  {
      outputState = value.toInt(); 
      SetPumpOutputState(0, outputState);
  }
  else if( varName == PUMP2_CONTROL_VAR )
  {
      outputState = value.toInt(); 
      SetPumpOutputState(1, outputState);
  }
  else if( varName == "V7")
  {
    outputState = value.toInt(); 
    if( outputState == 1)
      isWaterSensorActive = true;
    else
      isWaterSensorActive = false;
    SmartSnap.WriteVariableValue("V7", "int", String(isWaterSensorActive));
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
    value = String(Pumps[0].currentState) ; 
    return true;
  }
  else if( varName == "V2" )
  {
    varType = "int" ;
    value = String(Pumps[1].currentState) ; 
    return true;
  }
  else if( varName == "V5" )
  {
    varType = "int" ; 
    value = String(analogRead(MOISTURE_SENSOR_PIN));
    return true;
  }
  else if( varName == "V6" )
  {
    varType = "int" ; 
    if( waterSensorState == WATER_SENSOR_STATE_DETECTED)
      value = "1";
    else
      value = "0";
   return true;   
  }
  else if( varName == "V7" )
  {
    varType = "int" ; 
    value = String(isWaterSensorActive); 
    return true;
  } 
  else if( varName == "V8" )
  {
    WaterLevelSensorHandler();
    varType = "int" ; 
    value = String(waterSensorValueCM); 
    return true;
  }  
  return false;
}


extern void HandleServerValue(const String& varName, const String& value)
{
  if( varName == "V6" )
  {
    Serial.println("Received Value for V6 = " + value);
    if(value.toInt() == 1 )
    {
      waterSensorState = WATER_SENSOR_STATE_DETECTED;
      
    }
    else
    {
      waterSensorState = WATER_SENSOR_STATE_NOTDETECTED;
    }
  }
}

void HandleConnectionStatus(bool connected)
{
  Serial.printf("Connection status: %s\n", connected ? "connected" : "disconnected");
}

void setup() {
  int retVal = E_NOK;
  Serial.begin(115200); 

  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(WATER_SENSOR_PIN, INPUT); 

  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  
   
  delay(1000);  
  
  SmartSnap.onWriteVariable(HandleWriteVariable);
  SmartSnap.onReadVariable(HandleReadVariable);
  SmartSnap.onServerValue(HandleServerValue);
  SmartSnap.onConnectionStatus(HandleConnectionStatus);
  
//  while( retVal == E_NOK)
 {
    retVal = SmartSnap.Initialize("Home_Germany", 111222, "KS_DSL", "2wad@dsl", SMARTSNAP_HUB_HOST, SMARTSNAP_HUB_PORT);
   if( retVal == E_NOK)
     delay(5000);
 }
 
 SmartSnap.RequestVariableValueFromServer("V6");
   WaterLevelSensorHandler();

  moistureSensorTimerStart = millis();
  waterLevelSensorUpdateTimer = millis(); 
}


void loop() { 
  SmartSnap.Run();
 
  for(int index=0; index<NUMBER_OF_PUMPS ; index++)
  {
    PumpTimersHandler(index);
  }

  MoistureSensorHandler();

  WaterSensorHandler();
 
  // WaterLevelSensorHandler();
}

void SetPumpOutputState(int pumpIndex, int state)
{  
  String msg = "";
  if( state == REQ_STATE_ON && Pumps[pumpIndex].isActivationBlocked == false && waterSensorState != WATER_SENSOR_STATE_DETECTED)
  { 
    Pumps[pumpIndex].currentState = (enPumpState) state;
    digitalWrite(Pumps[pumpIndex].outputPin, state);
    Pumps[pumpIndex].activationStartTime = millis();
    Pumps[pumpIndex].isActivationBlocked = true;
    Pumps[pumpIndex].activationBlockTimerStart = millis();

    SmartSnap.WriteVariableValue(Pumps[pumpIndex].statusVarName, "string", "Activated");
    msg = "Pump_" + String(pumpIndex) + "Activated";
    Serial.println(msg);
  }
  else if(state == REQ_STATE_ON && Pumps[pumpIndex].isActivationBlocked == true)
  { 
    SmartSnap.WriteVariableValue(Pumps[pumpIndex].statusVarName, "string", "Couldn't act: Act Blocked [" + String(Pumps[pumpIndex].activationBlockCounter) + "/" + String(ACTIVATION_BLOCK_COUNTER_MAX) + "]");
    msg = "Pump_" + String(pumpIndex) + " Couldn't activate: Act Blocked[" + String(Pumps[pumpIndex].activationBlockCounter) + "]/" + String(ACTIVATION_BLOCK_COUNTER_MAX);
     Serial.println(msg);
  }
  else if( state == REQ_STATE_ON && waterSensorState == WATER_SENSOR_STATE_DETECTED)
  {
    SmartSnap.WriteVariableValue(Pumps[pumpIndex].statusVarName, "string", "Couldn't act: Water detected");
    msg = "Pump_" + String(pumpIndex) + " Couldn't activate: Water detected";
     Serial.println(msg);
  }
  else if( state == REQ_STATE_OFF )
  {
    Pumps[pumpIndex].currentState = (enPumpState) state;
    digitalWrite(Pumps[pumpIndex].outputPin, state);
     SmartSnap.WriteVariableValue(Pumps[pumpIndex].statusVarName, "string", "Deactivated");
    msg = "Pump_" + String(pumpIndex) + "Deactivated";
    Serial.println(msg);
  }
}
 
void PumpTimersHandler(int index)
{ 
  String msg = "";

  if( Pumps[index].currentState == PUMP_STATE_ON )
  {
    if( ((millis()-Pumps[index].activationStartTime) >= (Pumps[index].activeTimeoutSec*1000)) )
    {
      Pumps[index].currentState = PUMP_STATE_OFF;
      digitalWrite(Pumps[index].outputPin, LOW);

      SmartSnap.WriteVariableValue(Pumps[index].statusVarName, "string", "Pump_" + String(index) + " Auto shutoff");
      msg = "Pump_" + String(index) + " Auto shutoff";
      Serial.println(msg);
    }
  }
  if( Pumps[index].isActivationBlocked == true )
  {
    if( (millis()-Pumps[index].activationBlockTimerStart) >= ACTIVATION_BLOCK_TIMER_RESOLUTION*1000 )
    {
      Pumps[index].activationBlockCounter++;
      Pumps[index].activationBlockTimerStart = millis();
    }

    if( Pumps[index].activationBlockCounter >= ACTIVATION_BLOCK_COUNTER_MAX)
    {
      Pumps[index].activationBlockCounter = 0;
      Pumps[index].isActivationBlocked = false; 

      SmartSnap.WriteVariableValue(Pumps[index].statusVarName, "string", "Pump_" + String(index) + " Act Block Removed");
      msg = "Pump_" + String(index) + " Activation Block Removed";
      Serial.println(msg); 
    }
  } 
}

void MoistureSensorHandler(void)
{
  int sensorValue = 0;
  String msg = "";

  if( millis()-moistureSensorTimerStart >= MOISTURE_SENSOR_UPDATE_TIMER_RESOLUTION*1000 )
  {
    moistureSensorTimerStart = millis();
    moistureSensorTimerCounts++;
  }

  if( moistureSensorTimerCounts >= MOISTURE_SENSOR_UPDATE_COUNTER_MAX)
  {
    moistureSensorTimerCounts = 0;
    sensorValue = analogRead(MOISTURE_SENSOR_PIN);

    SmartSnap.WriteVariableValue(MOISTUR_VALUE_VAR, "int", String(sensorValue));
    msg = "Moisture=" + String(sensorValue);
    Serial.println(msg); 

    WaterLevelSensorHandler();
    SmartSnap.WriteVariableValue("V8", "int", String(waterSensorValueCM));  
  } 
}

void WaterSensorHandler(void)
{
  int waterSensorInput = digitalRead(WATER_SENSOR_PIN);

  if( isWaterSensorActive == true)
  {
    if( waterSensorInput == 0 && waterSensorState == WATER_SENSOR_STATE_NOTDETECTED)
    {
      Serial.println("WATER_SENSOR_STATE_DEBOUNCING_ON");
      waterSensorState = WATER_SENSOR_STATE_DEBOUNCING_ON;
      waterSensorDebounceTimer = millis();  
    }
    else if( waterSensorState == WATER_SENSOR_STATE_DEBOUNCING_ON && waterSensorInput == 1)
    {
      Serial.println("WATER_SENSOR_STATE_NOTDETECTED");
      waterSensorState = WATER_SENSOR_STATE_NOTDETECTED; 
    }
    else if( waterSensorState == WATER_SENSOR_STATE_DEBOUNCING_ON && waterSensorInput == 0 && (millis()-waterSensorDebounceTimer)>=WATER_SENSOR_DEBOUNCE_DELAY_MS)
    {
      Serial.println("WATER_SENSOR_STATE_DETECTED");
      waterSensorState = WATER_SENSOR_STATE_DETECTED;
      digitalWrite(LED_PIN, HIGH);
      SmartSnap.WriteVariableValue("V6", "int", "1");

      for(int index=0 ; index<2 ; index++)
      {
        digitalWrite(Pumps[index].outputPin, LOW); 
        SmartSnap.WriteVariableValue(Pumps[index].statusVarName, "string", "WaterDetected: Auto shutoff");
      }
    } 
    else if( waterSensorState == WATER_SENSOR_STATE_DETECTED && waterSensorInput == 1)
    {
      Serial.println("WATER_SENSOR_STATE_DEBOUNCING_OFF");
      waterSensorState = WATER_SENSOR_STATE_DEBOUNCING_OFF;
      waterSensorDebounceTimer = millis();
    }
    else if( waterSensorState == WATER_SENSOR_STATE_DEBOUNCING_OFF && waterSensorInput == 0)
    {
      Serial.println("WATER_SENSOR_STATE_DETECTED");
      waterSensorState = WATER_SENSOR_STATE_DETECTED; 
    }
    else if( waterSensorState == WATER_SENSOR_STATE_DEBOUNCING_OFF && waterSensorInput == 1 && (millis()-waterSensorDebounceTimer)>=WATER_SENSOR_DEBOUNCE_DELAY_MS)
    {
      Serial.println("WATER_SENSOR_STATE_NOTDETECTED");
      waterSensorState = WATER_SENSOR_STATE_NOTDETECTED; 
      digitalWrite(LED_PIN, LOW);
      SmartSnap.WriteVariableValue("V6", "int", "0");
    }
  }
  else
  {
    waterSensorState = WATER_SENSOR_STATE_NOTDETECTED; 
  }
}

void WaterLevelSensorHandler(void)
{
  waterSensorValueCM = ultrasonic.read();
  
  // Serial.print("Distance in CM: ");
  // Serial.println(waterSensorValueCM);
}
