const fs=require('fs');
const env=Object.fromEntries(fs.readFileSync('/home/khaled/smarthome/hub.env','utf8').split(/\r?\n/).filter(l=>/^[A-Z_]+=/.test(l)).map(l=>{const i=l.indexOf('=');return [l.slice(0,i),l.slice(i+1).replace(/^['"]|['"]$/g,'')];}));
const io=require('/home/khaled/smarthome/current/node_modules/socket.io-client');
const s=io('http://127.0.0.1:3000',{transports:['websocket'],query:{syncToken:env.HUB_TOKEN},reconnection:false});
const ask=action=>new Promise((resolve,reject)=>s.timeout(15000).emit('PresenceCommand',{home:env.HUB_HOME,deviceID:1014,action},(e,r)=>e?reject(e):resolve(r)));
setTimeout(()=>{s.close();process.exit(1);},55000).unref();
s.on('connect',async()=>{try{
 const first=await ask('read');console.log(JSON.stringify({read:first}));
 const cfg=first.config, peaks=Array(16).fill(0), crossings=Array(16).fill(0);let count=0,present=0,belowSince=Date.now(),longestBelow=0;const began=Date.now();
 while(Date.now()-began<40000){await new Promise(r=>setTimeout(r,150));const r=await ask('status');if(!r.live)continue;count++;if(r.presence)present++;let over=false;
 for(let g=0;g<16;g++){peaks[g]=Math.max(peaks[g],r.energy[g]);if(g>=cfg.minGate&&g<=cfg.maxGate&&r.energy[g]>=cfg.still[g]){crossings[g]++;over=true;}}
 if(over)belowSince=Date.now();else longestBelow=Math.max(longestBelow,Date.now()-belowSince);
 if(count%20===0)console.log(JSON.stringify({seconds:Math.round((Date.now()-began)/1000),samples:count,present,peaks,crossings,longestBelowMs:longestBelow}));
 }
 console.log(JSON.stringify({final:true,samples:count,present,peaks,crossings,longestBelowMs:longestBelow}));

}catch(e){console.log('Diagnostic failed:',e.message);}finally{s.close();}});
