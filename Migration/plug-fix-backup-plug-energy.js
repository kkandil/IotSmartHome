'use strict';
const {localTime}=require('./event-clock');
class PlugEnergy {
 constructor(sql,zone=()=> 'Europe/Berlin'){this.sql=sql;this.zone=zone;this.previous=new Map();sql.exec(`CREATE TABLE IF NOT EXISTS plug_energy(home TEXT,device INTEGER,time INTEGER,kwh REAL,seconds REAL,PRIMARY KEY(home,device,time)); CREATE TABLE IF NOT EXISTS plug_energy_totals(home TEXT,device INTEGER,kwh REAL,PRIMARY KEY(home,device));`);}
 missing(home,device){this.previous.delete(JSON.stringify([home,device]));}
 record(home,device,watts,now=Date.now()){
  if(!Number.isFinite(watts)||watts<0)return;
  const key=JSON.stringify([home,device]),prev=this.previous.get(key);this.previous.set(key,{watts,now});
  if(prev&&now>prev.now&&now-prev.now<=30000){
   // Bounded integration only across consecutive successful samples, split at minute boundaries.
   this.sql.exec('SAVEPOINT integrate_energy');
   try{let from=prev.now;while(from<now){const to=Math.min(now,(Math.floor(from/60000)+1)*60000),duration=to-from;
    const a=prev.watts+(watts-prev.watts)*(from-prev.now)/(now-prev.now),b=prev.watts+(watts-prev.watts)*(to-prev.now)/(now-prev.now);
    this.sql.prepare('INSERT INTO plug_energy VALUES(?,?,?,?,?) ON CONFLICT(home,device,time) DO UPDATE SET kwh=kwh+excluded.kwh,seconds=seconds+excluded.seconds').run(home,device,Math.floor(from/60000)*60000,(a+b)/2*duration/3600000000,duration/1000);this.sql.prepare('INSERT INTO plug_energy_totals VALUES(?,?,?) ON CONFLICT(home,device) DO UPDATE SET kwh=kwh+excluded.kwh').run(home,device,(a+b)/2*duration/3600000000);from=to;
   }this.sql.exec('RELEASE integrate_energy');}catch(e){this.sql.exec('ROLLBACK TO integrate_energy; RELEASE integrate_energy');this.previous.delete(key);throw e;}
  }
 }
 totals(home,device,now=Date.now()){
  const zone=this.zone(),day=localTime(new Date(now),zone).date;
  const rows=this.sql.prepare('SELECT time,kwh FROM plug_energy WHERE home=? AND device=? AND time>=?').all(home,device,now-2*86400000);
  return {EnergyTotal:(this.sql.prepare('SELECT kwh FROM plug_energy_totals WHERE home=? AND device=?').get(home,device)?.kwh||0),
   EnergyToday:rows.filter(r=>localTime(new Date(r.time),zone).date===day).reduce((s,r)=>s+r.kwh,0)};
 }
 read(input){
  const {homeName,deviceID,start,end}=input,zone=this.zone();
  if(typeof homeName!=='string'||!Number.isSafeInteger(deviceID)||!Number.isSafeInteger(start)||!Number.isSafeInteger(end)||start<0||end<=start||end-start>32*86400000)throw Error('Invalid energy range');
  localTime(new Date(),zone);
  const rows=this.sql.prepare('SELECT time,kwh,seconds FROM plug_energy WHERE home=? AND device=? AND time>=? AND time<? ORDER BY time').all(homeName,deviceID,start,end);
  const groups=new Map();const hourly=end-start<=25*3600000;
  for(const r of rows){const l=localTime(new Date(r.time),zone),key=hourly?Math.floor(r.time/3600000):l.date;
   const g=groups.get(key)||{time:r.time,first:r.time,last:r.time,value:0,count:1,seconds:0};g.value+=r.kwh;g.seconds+=r.seconds;g.last=r.time;groups.set(key,g);}
  return {points:[...groups.values()],estimated:true,total:rows.reduce((s,r)=>s+r.kwh,0),coverageSeconds:rows.reduce((s,r)=>s+r.seconds,0),timeZone:zone,start,end,mode:'energy',through:Date.now()};
 }
}
module.exports={PlugEnergy};
