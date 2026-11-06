const {DatabaseSync}=require('node:sqlite');
const db=new DatabaseSync('/home/khaled/smarthome/data/smarthome.sqlite',{readOnly:true});
for(const row of db.prepare('SELECT body,last_run FROM conditional_events').all()) {const r=JSON.parse(row.body);if(r.conditions.some(c=>c.varName==='Presence'))console.log(JSON.stringify({name:r.name,enabled:r.enabled,conditions:r.conditions,groups:r.conditionGroups,actions:r.actions,lastRun:row.last_run}));}
console.log(db.prepare("SELECT name,sql FROM sqlite_master WHERE type='table' AND (name LIKE '%activity%' OR name LIKE '%history%')").all());
