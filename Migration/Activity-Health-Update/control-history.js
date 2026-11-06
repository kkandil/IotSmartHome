'use strict';
const types=new Set(['BUTTON','BUTTON_CARD','SWITCH_CARD','CHECKBOX','CHECKBOX_CARD','SLIDER','SLIDER_CARD_HORIZONTAL','SLIDER_CARD_VERTICAL','TEXT_INPUT','TEXT_INPUT_CARD']);
class ControlHistory {
 constructor(sql){this.sql=sql;sql.exec(`CREATE TABLE IF NOT EXISTS control_targets(home TEXT,device INTEGER,variable TEXT,PRIMARY KEY(home,device,variable));
 CREATE TABLE IF NOT EXISTS control_changes(id INTEGER PRIMARY KEY AUTOINCREMENT,home TEXT,device INTEGER,variable TEXT,time INTEGER,value TEXT);
 CREATE INDEX IF NOT EXISTS control_changes_target ON control_changes(home,device,variable,id);`);
 if(!sql.prepare('PRAGMA table_info(control_changes)').all().some(c=>c.name==='confirmed'))sql.exec('ALTER TABLE control_changes ADD COLUMN confirmed INTEGER NOT NULL DEFAULT 0');
 if(!sql.prepare('PRAGMA table_info(control_changes)').all().some(c=>c.name==='source')){
  sql.exec("ALTER TABLE control_changes ADD COLUMN source TEXT NOT NULL DEFAULT 'legacy'");
  // Existing pre-telemetry records can only provide an estimate. Never upgrade them to confirmation.
  sql.exec("UPDATE control_changes SET source=CASE WHEN confirmed=1 THEN 'reported' ELSE 'unconfirmed' END WHERE confirmed=1 OR time >= (SELECT MIN(time) FROM control_changes WHERE confirmed=1)");
 }
 sql.exec('CREATE INDEX IF NOT EXISTS control_changes_time ON control_changes(home,device,variable,time,id)');}
 configure(home,widgets,devices){
  this.sql.prepare('DELETE FROM control_targets WHERE home=?').run(home);
  for(const w of widgets||[]){const b=w.binding,d=devices.find(d=>d.id===Number(b?.deviceId));
   if(types.has(w.type)&&d?.variables.some(v=>v.VarName===b.variable))this.sql.prepare('INSERT OR IGNORE INTO control_targets VALUES(?,?,?)').run(home,d.id,b.variable);}
  this.sql.prepare('DELETE FROM control_changes WHERE home=? AND NOT EXISTS(SELECT 1 FROM control_targets t WHERE t.home=control_changes.home AND t.device=control_changes.device AND t.variable=control_changes.variable)').run(home);
 }
 record(home,device,variable,old,value,type,now=Date.now(),confirmed=false){
  const same=['int','float'].includes(type)?Number(old)===Number(value):String(old)===String(value);
  if(!this.sql.prepare('SELECT 1 FROM control_targets WHERE home=? AND device=? AND variable=?').get(home,device,variable))return;
  const last=this.sql.prepare('SELECT value,confirmed FROM control_changes WHERE home=? AND device=? AND variable=? ORDER BY id DESC LIMIT 1').get(home,device,variable);
  if(same&&(!confirmed||(last?.confirmed&&last.value===String(value))))return;
  this.sql.prepare('INSERT INTO control_changes(home,device,variable,time,value,confirmed,source) VALUES(?,?,?,?,?,?,?)').run(home,device,variable,now,String(value),confirmed?1:0,confirmed?'reported':'unconfirmed');
 }
 prune(){this.sql.prepare('DELETE FROM control_changes WHERE time<?').run(Date.now()-365*86400000);}
 read(input){
  if(!input||typeof input.homeName!=='string'||!Number.isSafeInteger(input.deviceID)||typeof input.varName!=='string')throw Error('Invalid control');
  if(input.start!==undefined){
   const {start,end}=input;
   if(!Number.isSafeInteger(start)||!Number.isSafeInteger(end)||start<0||end<=start||end-start>32*86400000)throw Error('Invalid chart range');
   const until=Math.min(end,Date.now()),args=[input.homeName,input.deviceID,input.varName];
   const previous=this.sql.prepare('SELECT id,time,value,confirmed,source FROM control_changes WHERE home=? AND device=? AND variable=? AND time<? ORDER BY time DESC,id DESC LIMIT 1').get(...args,start);
   const entries=this.sql.prepare('SELECT id,time,value,confirmed,source FROM control_changes WHERE home=? AND device=? AND variable=? AND time>=? AND time<? ORDER BY time,id LIMIT 100001').all(...args,start,until);
   if(entries.length>100000)throw Error('Too many state changes; select a shorter range');
   return {start,end:until,previous:previous||null,entries};
  }
  const before=input.beforeId===undefined?Number.MAX_SAFE_INTEGER:input.beforeId;
  if(!Number.isSafeInteger(before)||before<1)throw Error('Invalid cursor');
  const rows=this.sql.prepare('SELECT id,time,value FROM control_changes WHERE home=? AND device=? AND variable=? AND id<? ORDER BY id DESC LIMIT 101').all(input.homeName,input.deviceID,input.varName,before);
  return {entries:rows.slice(0,100),nextBefore:rows.length>100?rows[99].id:null};
 }
}
module.exports={ControlHistory};
