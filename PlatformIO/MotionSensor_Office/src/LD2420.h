#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
// LD2420 binary UART protocol; 16 distance gates. No ESPHome runtime dependency.
class LD2420 {
 HardwareSerial& port; uint8_t frame[256];size_t used=0;uint16_t awaited=0;bool ack=false,success=false;uint8_t response[128];size_t responseSize=0;uint32_t deadline=0;
 static uint16_t u16(const uint8_t* p){return p[0]|uint16_t(p[1])<<8;}
 static uint32_t u32(const uint8_t* p){return u16(p)|uint32_t(u16(p+2))<<16;}
 static void put16(uint8_t* p,uint16_t v){p[0]=v;p[1]=v>>8;}
 static void put32(uint8_t* p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
 bool command(uint16_t cmd,const uint8_t* data=nullptr,size_t n=0){
  if(int32_t(millis()-deadline)>=0)return false;
  uint8_t packet[80]={0xFD,0xFC,0xFB,0xFA};put16(packet+4,n+2);put16(packet+6,cmd);if(n)memcpy(packet+8,data,n);const uint8_t tail[]={4,3,2,1};memcpy(packet+8+n,tail,4);
  poll();awaited=cmd|0x0100;ack=false;success=false;responseSize=0;port.write(packet,n+12);uint32_t start=millis();
  while(!ack && millis()-start<650 && int32_t(millis()-deadline)<0){poll();delay(1);}return ack&&success;
 }
 bool configMode(bool enable){uint8_t version[]={2,0};return command(enable?0xFF:0xFE,enable?version:nullptr,enable?2:0);}
 bool readValues(const uint16_t* regs,uint32_t* values,size_t count){uint8_t data[12];for(size_t i=0;i<count;i++)put16(data+i*2,regs[i]);if(!command(8,data,count*2)||responseSize<count*4)return false;for(size_t i=0;i<count;i++)values[i]=u32(response+i*4);return true;}
 bool writeValues(const uint16_t* regs,const uint32_t* values,size_t count){uint8_t data[36];for(size_t i=0;i<count;i++){put16(data+i*6,regs[i]);put32(data+i*6+2,values[i]);}return command(7,data,count*6);}
 bool transfer(JsonObject cfg,bool write){
  uint16_t regs[]={0,1,4};uint32_t values[3];
  if(write){values[0]=cfg["minGate"];values[1]=cfg["maxGate"];values[2]=cfg["delay"];if(!writeValues(regs,values,3))return false;}
  else{if(!readValues(regs,values,3))return false;cfg["minGate"]=values[0];cfg["maxGate"]=values[1];cfg["delay"]=values[2];cfg.createNestedArray("move");cfg.createNestedArray("still");}
  for(int g=0;g<16;g++){uint16_t pair[]={uint16_t(0x10+g),uint16_t(0x20+g)};uint32_t v[2];if(write){v[0]=cfg["move"][g];v[1]=cfg["still"][g];if(!writeValues(pair,v,2))return false;}else{if(!readValues(pair,v,2))return false;cfg["move"].as<JsonArray>().add(v[0]);cfg["still"].as<JsonArray>().add(v[1]);}}
  return true;
 }
 public:
 void (*onFrame)() = nullptr;
 uint16_t energy[16]={};bool presence=false;uint16_t distance=0;uint32_t lastFrame=0;bool hasFrame=false;
 explicit LD2420(HardwareSerial& serial):port(serial){}
 void poll(){while(port.available()){
  uint8_t b=port.read();if(used==sizeof(frame))used=0;frame[used++]=b;
  while(used>=4 && !(u32(frame)==0xFAFBFCFD || u32(frame)==0xF1F2F3F4)){memmove(frame,frame+1,--used);}
  if(used<6)continue;size_t total=u16(frame+4)+10;if(total>sizeof(frame)||total<12){used=0;continue;}if(used<total)continue;
  bool control=u32(frame)==0xFAFBFCFD;uint32_t footer=u32(frame+total-4);
  if(control && footer==0x01020304 && total>=14 && u16(frame+6)==awaited){ack=true;success=u16(frame+8)==0;responseSize=total-14;if(responseSize>sizeof(response))responseSize=0;memcpy(response,frame+10,responseSize);}
  if(!control && footer==0xF5F6F7F8 && total==45){presence=frame[6]!=0;distance=u16(frame+7);for(int g=0;g<16;g++)energy[g]=u16(frame+9+2*g);lastFrame=millis();hasFrame=true;if(onFrame)onFrame();}
  used=0;
 }}
 bool read(JsonObject cfg){deadline=millis()+10000;bool ok=configMode(true);if(ok)ok=transfer(cfg,false);uint8_t mode[]={0,0,4,0,0,0};if(ok)ok=command(0x12,mode,6);deadline=millis()+700;bool exited=configMode(false);return ok&&exited;}
 bool save(JsonObject cfg,JsonObject verified){deadline=millis()+10000;bool ok=configMode(true);if(ok)ok=transfer(cfg,true);if(ok)ok=transfer(verified,false);deadline=millis()+700;bool exited=configMode(false);return ok&&exited;}
};
