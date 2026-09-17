import * as v from './validation.js';
import { missing, invalid } from './errors.js';
import { sensorRepository } from '../repositories/sensorRepository.js';
export function sensorService({db}) {
 const repo=sensorRepository(db);
 return {
  ingest(b) {
   v.string(b.deviceId,'deviceId'); v.source(b.sensorSource); v.integer(b.sequenceNumber,'sequenceNumber'); v.integer(b.timestampMs,'timestampMs');
   for(const k of ['accelXMs2','accelYMs2','accelZMs2']) v.number(b[k],k);
   const gyroAxes=['gyroXDps','gyroYDps','gyroZDps'];
   for(const k of gyroAxes) v.nullable(b[k],k);
   const nullAxes=gyroAxes.filter(k=>b[k]===null).length;
   if(nullAxes!==0 && nullAxes!==3) invalid('Gyroscope axes must be three numbers or three nulls');
   for(const k of ['pressurePa','temperatureC','altitudeDeltaM']) v.nullable(b[k],k);
   v.nullable(b.batteryVoltageMv,'batteryVoltageMv',v.integer); v.integer(b.batteryPercent,'batteryPercent',100); v.integer(b.sensorQuality,'sensorQuality',100);
   v.boolean(b.isCharging,'isCharging'); v.boolean(b.sosButtonPressed,'sosButtonPressed');
   if(b.motionState!==undefined) v.enumeration(b.motionState,'motionState',['STATIONARY','WALKING','RUNNING','FALLING','UNKNOWN']);
   if(b.fallRisk!==undefined) v.enumeration(b.fallRisk,'fallRisk',['LOW','MEDIUM','HIGH','CRITICAL']);
   repo.insert({...b,location:v.location(b.location)});
   return {deviceId:b.deviceId,sequenceNumber:b.sequenceNumber,stored:true};
  },
  latest(id) { v.string(id,'deviceId'); return repo.latest(id)??missing(); },
 };
}
