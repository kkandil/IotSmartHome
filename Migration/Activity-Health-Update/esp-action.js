'use strict';

// ESP automation values are durable even when there is no device socket.
// Delivery after reconnect is opt-in: firmware requests the saved variable.
async function dispatchEspAction({target,connection,save}) {
  if(target.saveOffline===false&&!connection?.Socket?.connected)return 'skipped_offline';
  const update=await save();
  if(!update || typeof update!=='object')throw Error('Could not save action value');
  if(!connection?.Socket?.connected)return 'saved_offline';
  connection.Socket.emit('PhoneWriteVariable',{
    homeName:target.homeName,deviceID:target.deviceID,
    varName:target.varName,varType:target.varType,varValue:target.varValue,
    ...(target.commandId?{commandId:target.commandId}:{})
  });
  return 'sent';
}

module.exports={dispatchEspAction};
