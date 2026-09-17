import { randomUUID } from 'node:crypto';
import * as v from './validation.js';
import { missing } from './errors.js';
import { deviceRepository } from '../repositories/deviceRepository.js';
import { transaction } from '../repositories/db.js';
export function deviceService({db,config,now}) {
 const repo=deviceRepository(db);
 return {
  get(id) {
   v.string(id,'deviceId'); const d=repo.get(id); if(!d) missing();
   return {deviceId:id,deviceType:d.device_type,firmwareVersion:d.firmware_version,timestampMs:d.timestamp_ms,uptimeSeconds:d.uptime_seconds,batteryPercent:d.battery_percent,batteryVoltageMv:d.battery_voltage_mv,isCharging:!!d.is_charging,isConnected:now()-d.last_heartbeat_ms<=config.HEARTBEAT_STALE_THRESHOLD_MS,imuStatus:d.imu_status,barometerStatus:d.barometer_status,gnssStatus:d.gnss_status,bufferUsagePercent:d.buffer_usage_percent,lastErrorCode:d.last_error_code,lastHeartbeatMs:d.last_heartbeat_ms};
  },
  heartbeat(id,b,userId) {
   v.string(id,'deviceId'); if(userId!==undefined) v.string(userId,'userId');
   v.string(b.firmwareVersion,'firmwareVersion'); v.integer(b.timestampMs,'timestampMs'); v.integer(b.uptimeSeconds,'uptimeSeconds'); v.integer(b.batteryPercent,'batteryPercent',100); v.nullable(b.batteryVoltageMv,'batteryVoltageMv',v.integer); v.boolean(b.isCharging,'isCharging');
   for(const k of ['imuStatus','barometerStatus','gnssStatus']) v.enumeration(b[k],k,['OK','CALIBRATING','UNAVAILABLE','ERROR']);
   v.integer(b.bufferUsagePercent,'bufferUsagePercent',100); v.nullable(b.lastErrorCode,'lastErrorCode',v.string); if(b.deviceType!==undefined) v.source(b.deviceType);
   const time=now(); transaction(db,()=>{repo.save(id,b,time); if(b.batteryPercent<15) repo.lowBattery(id,userId ?? id,b,time,randomUUID());});
   return {deviceId:id,recorded:true,serverTimeMs:time};
  },
 };
}
