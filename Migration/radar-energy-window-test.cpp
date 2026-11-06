#include "RadarEnergyWindow.h"
#include <assert.h>
int main() {
 RadarEnergyWindow w; uint16_t e[16]={},p[16],l[16]; uint32_t span;
 assert(w.read(0,p,l,span)==0);
 e[4]=146; w.add(100,e); e[4]=70; w.add(200,e);
 assert(w.read(300,p,l,span)==2 && p[4]==146 && l[4]==70);
 assert(w.read(400,p,l,span)==2 && p[4]==146); // non-destructive multiple readers
 e[4]=50;w.add(4100,e);
 assert(w.read(4200,p,l,span)==1 && p[4]==50);
 assert(w.read(9000,p,l,span)==0);
 w.clear(); e[4]=99;w.add(0xfffffff0u,e);
 assert(w.read(20,p,l,span)==1 && p[4]==99); // uint32 clock wrap
 w.clear();assert(w.read(25,p,l,span)==0);
}

