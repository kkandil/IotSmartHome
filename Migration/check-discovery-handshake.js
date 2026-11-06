const io=require('../HubServer/node_modules/socket.io-client');
const s=io('http://192.168.178.89:3000',{transports:['websocket'],forceNew:true,reconnection:false});
const timeout=setTimeout(()=>{s.close();console.error('Discovery handshake timed out');process.exitCode=1;},7000);
s.on('connect',()=>s.emit('DeviceConnect',{homeName:'Home_Germany',deviceID:0,discovery:true,hardwareId:'02:00:00:00:FE:01',suggestedName:'Discovery_Test',model:'Transient validation'}));
s.on('RequestDeviceDefinition',()=>{s.emit('DeviceDefinition',{variables:[]});setTimeout(()=>{clearTimeout(timeout);s.close();console.log('Pi requested the definition from an unassigned device; empty manifest sent. Test connection closed.');},250);});
s.on('connect_error',e=>{clearTimeout(timeout);s.close();console.error(e.message);process.exitCode=1;});
