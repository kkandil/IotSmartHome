"use strict";
const ICONS=['smartsnap','plug','triple_switch','speaker','microphone','sensor','light','motion','climate','tv_remote','gas_water_heater','fan','air_conditioner','kitchen_hood'];
function icon(value) {return ICONS.includes(value)?value:'smartsnap';}
function variables(input) {
 if(!Array.isArray(input)||input.length>100)throw Error('Use at most 100 variables');
 const names=new Set();
 return input.map(v=>{
  if(!v||typeof v.name!=='string'||!/^[A-Za-z0-9_-]{1,128}$/.test(v.name)||names.has(v.name))throw Error('Variable names must be unique and use letters, numbers, underscores or hyphens');
  names.add(v.name);
  if(!['int','float','string'].includes(v.type)||typeof v.value!=='string'||v.value.length>1024)throw Error('Invalid variable type or default');
  if(v.type!=='string'&&(!v.value.trim()||!Number.isFinite(Number(v.value))))throw Error('Enter a numeric default');
  if(v.type==='int'&&(!/^[+-]?\d+$/.test(v.value.trim())||!Number.isSafeInteger(Number(v.value))||Number(v.value)<-2147483648||Number(v.value)>2147483647))throw Error('Enter a valid Int default');
  return {name:v.name,type:v.type,value:v.value,configuration:v.configuration===true};
 });
}
class DeviceTemplates {
 constructor(collection){this.collection=collection;}
 async get(user){return await this.collection.findOne({_id:user.id})||{revision:0,templates:[]};}
 async save(user,input){
  if(!Number.isSafeInteger(input?.revision)||input.revision<0||!Array.isArray(input.templates)||input.templates.length>50)throw Error('Invalid templates');
  const ids=new Set(),names=new Set();
  const templates=input.templates.map(t=>{
   if(!t||typeof t.id!=='string'||!/^[a-zA-Z0-9-]{1,64}$/.test(t.id)||ids.has(t.id)||typeof t.name!=='string'||!t.name.trim()||t.name.trim().length>80||names.has(t.name.trim().toLowerCase()))throw Error('Use unique template names and IDs');
   ids.add(t.id);names.add(t.name.trim().toLowerCase());
   return {id:t.id,name:t.name.trim(),icon:icon(t.icon),variables:variables(t.variables)};
  });
  const doc={_id:user.id,revision:input.revision+1,templates};
  if(input.revision===0){try{await this.collection.insertOne(doc);}catch(e){if(e.code===11000)throw Error('Templates changed on another phone. Reopen before saving.');throw e;}}
  else if(!(await this.collection.replaceOne({_id:user.id,revision:input.revision},doc)).matchedCount)throw Error('Templates changed on another phone. Reopen before saving.');
  return doc;
 }
 async selected(user,id,revision){return (await this.selection(user,id,revision)).variables;}
 async selection(user,id,revision){
  const doc=await this.get(user);
  if(doc.revision!==revision)throw Error('Templates changed. Reopen Add device and select again.');
  const template=doc.templates.find(t=>t.id===id);
  if(!template)throw Error('Template not found');
  return {variables:variables(template.variables),icon:icon(template.icon)};
 }
}
module.exports={DeviceTemplates,variables,icon};
