"use strict";
const fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process'),{randomUUID}=require('node:crypto');
const connect=require('socket.io-client');
function worker(input){return new Promise((resolve,reject)=>{
 const child=spawn(process.env.TUYA_PYTHON||'python3',[path.join(__dirname,'tuya-worker.py')],{stdio:['pipe','pipe','ignore']});
 let output='';const timer=setTimeout(()=>{child.kill();reject(Error('Tuya operation timed out'));},60000);
 child.stdout.on('data',b=>{output+=b;if(output.length>500000){child.kill();reject(Error('Too many Tuya devices'));}});
 child.on('error',()=>{clearTimeout(timer);reject(Error('Install the Pi Tuya worker dependencies first'));});
 child.on('close',()=>{clearTimeout(timer);try{const result=JSON.parse(output);if(result.status!=='OK')throw Error(result.message);resolve(result);}catch(e){reject(Error(e.message.startsWith('Unexpected')?'Tuya worker failed':e.message));}});
 child.stdin.on('error',()=>{});child.stdin.end(JSON.stringify(input));
});}
function start({home,localUrl,exists,energy,file=process.env.TUYA_CONFIG_FILE||path.join(__dirname,'data','tuya.json'),run=worker}){
 let config={devices:[],selected:[]};if(fs.existsSync(file))config=JSON.parse(fs.readFileSync(file,'utf8'));
 const jobs=new Map(),sessions=new Map(),queues=new Map();
 const execute=(entry,write)=>{const next=(queues.get(entry.id)||Promise.resolve()).catch(()=>{}).then(()=>run({action:'local',entry,devices:config.devices,...(write===undefined?{}:{write})}));queues.set(entry.id,next);return next;};let setupBusy=false,closed=false;
 const save=()=>{fs.mkdirSync(path.dirname(file),{recursive:true,mode:0o700});fs.writeFileSync(file+'.tmp',JSON.stringify(config),{mode:0o600});fs.chmodSync(file+'.tmp',0o600);fs.renameSync(file+'.tmp',file);};
 const publicList=()=>config.devices.map(d=>({id:d.id,name:d.name,deviceID:config.selected.find(s=>s.id===d.id)?.deviceID}));
 const publish=(state,power,force=false)=>{state.power=power;if(state.ready&&(force||state.publishedPower!==power)){state.publishedPower=power;state.socket.emit('DeviceWriteVariable',{homeName:home,deviceID:state.entry.deviceID,varName:'Power',varType:'int',varValue:power?'1':'0'});}};
 const metering=()=>config.selected.filter(e=>Object.keys(e.metrics||{}).length).map(e=>({deviceID:e.deviceID,tuyaId:e.id,variables:[...Object.keys(e.metrics),...(e.metrics.PowerWatts?['EnergyToday','EnergyTotal']:[])]}));
 function measure(entry,result){
  const readings=result.measurements||{};if(energy&&Number.isFinite(readings.PowerWatts))energy.record(home,entry.deviceID,readings.PowerWatts);
  const state=sessions.get(entry.id);if(!state?.ready||Date.now()-(state.measuredAt||0)<30000)return;
  state.measuredAt=Date.now();const all={...readings,...(energy&&Number.isFinite(readings.PowerWatts)?energy.totals(home,entry.deviceID):{})};
  for(const [varName,value]of Object.entries(all))if(Number.isFinite(value))state.socket.emit('DeviceWriteVariable',{homeName:home,deviceID:entry.deviceID,varName,varType:'float',varValue:String(Math.round(value*1000000)/1000000)});
 }
 function ensure(entry,power){
  let state=sessions.get(entry.id);if(state){publish(state,power);return;}
  state={entry,power,ready:false,busy:false};sessions.set(entry.id,state);
  const socket=state.socket=connect(localUrl,{transports:['websocket'],forceNew:true,reconnection:false});
  socket.on('connect',()=>socket.emit('DeviceConnect',{homeName:home,deviceID:entry.deviceID,firmwareVersion:'Tuya local bridge',capabilities:[]}));
  socket.on('DeviceConnect',result=>{if(result!=='OK'){drop(entry.id);return;}state.ready=true;publish(state,state.power);});
  socket.on('disconnect',()=>{if(sessions.get(entry.id)===state)sessions.delete(entry.id);});
  socket.on('PhoneWriteVariable',data=>{
   if(data.varName!=='Power'||!['0','1'].includes(String(data.varValue)))return;
   // Serialize commands per plug, then publish the device-confirmed value.
   state.queue=(state.queue||Promise.resolve()).then(async()=>{
    if(closed||!exists(entry.deviceID))return;
    state.busy=true;
    try{const result=await execute(entry,String(data.varValue)==='1');publish(state,result.power,true);}
    catch(e){publish(state,state.power,true);drop(entry.id);}
    finally{state.busy=false;}
   });
  });
  socket.on('GetVariableValueFromDevice',()=>poll(entry));
 }
 function drop(id){const state=sessions.get(id);sessions.delete(id);state?.socket.disconnect();}
 const polling=new Set();
 async function poll(entry){
  if(closed||setupBusy||polling.has(entry.id)||sessions.get(entry.id)?.busy)return;
  if(!exists(entry.deviceID)){drop(entry.id);return;}
  polling.add(entry.id);
  try{const r=await execute(entry);if(!closed&&exists(entry.deviceID)){ensure(entry,r.power);measure(entry,r);}}
  catch(e){energy?.missing(home,entry.deviceID);drop(entry.id);}finally{polling.delete(entry.id);}
 }
 function job(task){if(setupBusy)throw Error('Another plug setup is in progress');setupBusy=true;
  for(const [id,j]of jobs)if(Date.now()-j.created>600000)jobs.delete(id);
  const id=randomUUID();jobs.set(id,{status:'Pending',created:Date.now()});
  Promise.resolve().then(task).then(result=>jobs.set(id,{status:'OK',created:Date.now(),...result})).catch(e=>jobs.set(id,{status:'Error',created:Date.now(),message:e.message})).finally(()=>setupBusy=false);
  return {status:'OK',jobId:id};
 }
 const timer=setInterval(()=>{for(const e of config.selected)poll(e);},5000);timer.unref();
 return {metering,async command(input){
  if(input.home!==home)throw Error('Wrong home');
  if(input.action==='status')return {status:'OK',configured:!!config.project,preparedId:config.prepared?.id,devices:publicList()};
  if(input.action==='job'){const j=jobs.get(input.jobId);if(!j)throw Error('Setup expired. Try again');return {status:'OK',job:j};}
  if(input.action==='discover')return job(async()=>{
   const project=input.project||config.project;
   if(!project||typeof project.clientId!=='string'||typeof project.secret!=='string'||project.clientId.length>128||project.secret.length>256)throw Error('Enter project credentials');
   const result=await run({action:'discover',project});config.project=project;config.devices=result.devices;for(const selected of config.selected){const d=result.devices.find(d=>d.id===selected.id);if(d)selected.metrics=d.metrics||{};}save();return {devices:publicList()};
  });
  if(input.action==='test')return job(async()=>{
   const device=config.devices.find(d=>d.id===input.id);if(!device)throw Error('Reload the plug list');
   const saved=config.selected.find(d=>d.id===input.id);
   await Promise.allSettled([...queues.values()]);
   let entry={...device,ip:input.ip||saved?.ip||''};let result;
   try{result=await execute(entry);}catch(error){
    if(input.ip||!saved?.ip)throw error;
    entry={...device,ip:''};result=await execute(entry);
   }
   config.prepared={...entry,ip:result.ip||entry.ip,version:result.version};save();return {id:entry.id,power:result.power,ip:config.prepared.ip};
  });
  if(input.action==='attach'){
   const prepared=config.prepared;if(!prepared||prepared.id!==input.id||!Number.isSafeInteger(input.deviceID)||input.deviceID<1)throw Error('Test the local connection first');
   drop(input.id);config.selected=config.selected.filter(d=>d.id!==input.id);config.selected.push({...prepared,deviceID:input.deviceID});save();return {status:'OK'};
  }
  throw Error('Unknown plug operation');
 },close(){closed=true;clearInterval(timer);for(const id of sessions.keys())drop(id);}};
}
module.exports={start,worker};
