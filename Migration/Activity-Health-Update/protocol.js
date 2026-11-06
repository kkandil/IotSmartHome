'use strict';

// Keep the legacy Android/ESP event contract. Devices connect only to the Pi.
const requests = [
  'PhoneConnect', 'AddNewHome', 'GetAllHomes', 'DeleteHome', 'AddDevice','RenameDevice',
  'DeleteDevice', 'AddVariable', 'DeleteDeviceVariable', 'PhoneWriteVariable',
  'RequestVariableValue', 'GetVariableValueFromServer', 'GetVariableSnapshot', 'GetVariableHistory', 'GetControlHistory', 'GetPlugEnergy', 'ClearVariableHistory', 'GetDeviceStatus',
  'GetAllDevices', 'GetDeviceVariables', 'GetSchedules', 'SaveSchedule', 'DeleteSchedule',
  'GetAutomationConditions', 'GetAutomationHistory', 'GetNotificationHistory', 'GetDeviceHealth', 'GetEvents', 'SaveEvent', 'DeleteEvent', 'SetEventEnabled', 'GetEventVariables'
];
const responses = [...requests, 'DeviceWriteNotification', 'HistoryPoint', 'HistoryCleared', 'DeviceStatus', 'DeviceDeleted', 'DeviceWriteVariable', 'SchedulesChanged', 'EventsChanged'];
module.exports = { requests, responses };
