'use strict';
const {randomUUID}=require('node:crypto');
const DAY=86400000;
class ActivityHistory {
  constructor(sql,clock=()=>Date.now()) {
    this.sql=sql;this.clock=clock;
    sql.exec(`CREATE TABLE IF NOT EXISTS activity_history(id TEXT PRIMARY KEY,home TEXT NOT NULL,kind TEXT NOT NULL,device INTEGER,at INTEGER NOT NULL,body TEXT NOT NULL);
      CREATE INDEX IF NOT EXISTS activity_query ON activity_history(home,kind,at DESC,id);
      CREATE TABLE IF NOT EXISTS device_health(home TEXT NOT NULL,device INTEGER NOT NULL,body TEXT NOT NULL,PRIMARY KEY(home,device));
      CREATE TABLE IF NOT EXISTS action_receipts(id TEXT PRIMARY KEY,run TEXT NOT NULL,home TEXT NOT NULL,device INTEGER NOT NULL,variable TEXT NOT NULL,value TEXT NOT NULL,type TEXT NOT NULL,at INTEGER NOT NULL);`);
  }
  put(home,kind,body,device=null,id=randomUUID()) {
    const at=this.clock();
    this.sql.prepare('INSERT INTO activity_history VALUES(?,?,?,?,?,?)').run(id,home,kind,device,at,JSON.stringify(body));
    return id;
  }
  get(id) {const row=this.sql.prepare('SELECT * FROM activity_history WHERE id=?').get(id);return row?{...row,body:JSON.parse(row.body)}:null;}
  update(id,body) {this.sql.prepare('UPDATE activity_history SET body=? WHERE id=?').run(JSON.stringify(body),id);}
  list(input,kind) {
    if(!input||typeof input.homeName!=='string'||!/^[\w-]+$/.test(input.homeName))throw Error('Choose a home');
    const before=Number.isSafeInteger(input.before)?input.before:Number.MAX_SAFE_INTEGER;
    const from=Number.isSafeInteger(input.from)?input.from:0;
    const device=Number.isSafeInteger(input.deviceID)?input.deviceID:null;
    const eventId=typeof input.eventId==='string'?input.eventId:null;
    const result=typeof input.result==='string'?input.result:null;
    const rows=this.sql.prepare(`SELECT rowid AS cursor,* FROM activity_history WHERE home=? AND kind=? AND rowid<? AND at>=? AND (? IS NULL OR device=?) AND (? IS NULL OR json_extract(body,'$.eventId')=?) AND (? IS NULL OR json_extract(body,'$.status')=?) ORDER BY rowid DESC LIMIT 51`).all(input.homeName,kind,before,from,device,device,eventId,eventId,result,result);
    const more=rows.length>50;const items=rows.slice(0,50).map(r=>({id:r.id,at:r.at,...JSON.parse(r.body)}));
    return {items,next:more?rows[49].cursor:null};
  }
  begin(rule,conditions,trigger) {return this.put(rule.homeName,'automation',{name:rule.name,eventId:rule.id,conditions,trigger,actions:[],status:'running'});}
  startAction(run,action) {
    const id=randomUUID(),row=this.get(run);
    row.body.actions.push({...action,commandId:id,status:'running'});this.update(run,row.body);
    this.sql.prepare('INSERT INTO action_receipts VALUES(?,?,?,?,?,?,?,?)').run(id,run,row.home,action.deviceID,action.varName,action.varValue,action.varType,this.clock());return id;
  }
  finishAction(run,id,status) {
    const row=this.get(run);if(!row)return;
    const a=row.body.actions.find(a=>a.commandId===id);if(!a)return;
    if(!['confirmed','superseded'].includes(a.status))a.status=status;
    if(status==='confirmed'&&!a.confirmedAt)a.confirmedAt=this.clock();
    this.summarize(row);
  }
  summarize(row) {
    const a=row.body.actions;
    row.body.status=a.every(x=>x.status==='confirmed')?'confirmed':a.some(x=>x.status==='running')?'running':
      a.every(x=>x.status==='sent')?'sent':a.every(x=>x.status==='saved_offline')?'pending':
      a.some(x=>['confirmed','sent','saved_offline'].includes(x.status))?'partial':
      a.some(x=>x.status==='outcome_unknown')?'outcome_unknown':'failed';
    this.update(row.id,row.body);
  }
  supersede(home,device,variable,keep) {
    const rows=this.sql.prepare('SELECT * FROM action_receipts WHERE home=? AND device=? AND variable=?').all(home,device,variable);
    for(const r of rows)if(r.id!==keep){
      const row=this.get(r.run),a=row?.body.actions.find(x=>x.commandId===r.id);
      if(a&&a.status!=='confirmed'){a.status='superseded';this.summarize(row);}
      this.sql.prepare('DELETE FROM action_receipts WHERE id=?').run(r.id);
    }
  }
  pending(home,device,variable,value) {
    const rows=this.sql.prepare('SELECT * FROM action_receipts WHERE home=? AND device=? AND variable=? ORDER BY at DESC LIMIT 1').all(home,device,variable);
    const r=rows[0];return r&&String(value)===r.value?r.id:undefined;
  }
  confirm(data) {
    if(typeof data.commandId!=='string')return false;
    const r=this.sql.prepare('SELECT * FROM action_receipts WHERE id=? AND home=? AND device=? AND variable=?').get(data.commandId,data.homeName,data.deviceID,data.varName);
    if(!r)return false;
    const equal=['int','float'].includes(r.type)?String(data.varValue).trim()!==''&&Number.isFinite(Number(data.varValue))&&Number(data.varValue)===Number(r.value):String(data.varValue)===r.value;
    if(!equal)return false;
    const row=this.get(r.run);if(!row)return false;
    const a=row.body.actions.find(x=>x.commandId===r.id);if(!a)return false;
    a.status='confirmed';a.confirmedAt=this.clock();this.summarize(row);
    this.sql.prepare('DELETE FROM action_receipts WHERE id=?').run(r.id);return true;
  }
  health(home,device) {const row=this.sql.prepare('SELECT body FROM device_health WHERE home=? AND device=?').get(home,device);return row?JSON.parse(row.body):{};}
  report(home,device,input) {
    if(!input||typeof input.bootId!=='string'||! /^[a-zA-Z0-9-]{1,64}$/.test(input.bootId))throw Error('Invalid health report');
    const previous=this.health(home,device),now=this.clock();
    const body={...(previous.bootId===input.bootId?previous:{}),bootId:input.bootId,receivedAt:now};
    for(const k of ['uptimeMs','freeHeap','rssi'])if(Number.isFinite(input[k])&&Math.abs(input[k])<Number.MAX_SAFE_INTEGER)body[k]=input[k];
    for(const k of ['platform','resetReason','firmwareVersion'])if(typeof input[k]==='string')body[k]=input[k].slice(0,120);
    if(previous.bootId!==input.bootId) {
      body.bootSeenAt=now;
      this.put(home,'health',{message:'Device started',reason:body.resetReason||'Unknown'},device);
    }
    this.sql.prepare('INSERT INTO device_health VALUES(?,?,?) ON CONFLICT(home,device) DO UPDATE SET body=excluded.body').run(home,device,JSON.stringify(body));
  }
  connection(home,device,online) {this.put(home,'health',{message:online?'Connected to hub':'Disconnected from hub'},device);}
  prune() {
    this.sql.prepare('DELETE FROM activity_history WHERE at<?').run(this.clock()-30*DAY);
    // Bound storage per home and category even under noisy firmware.
    this.sql.exec(`DELETE FROM activity_history WHERE rowid IN (SELECT rowid FROM (SELECT rowid,ROW_NUMBER() OVER(PARTITION BY home,kind ORDER BY rowid DESC) n FROM activity_history) WHERE n>10000)`);
    this.sql.prepare('DELETE FROM action_receipts WHERE at<? OR run NOT IN (SELECT id FROM activity_history)').run(this.clock()-30*DAY);
  }
}
module.exports={ActivityHistory};
