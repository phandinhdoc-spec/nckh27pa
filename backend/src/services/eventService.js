import { isDeepStrictEqual } from 'node:util';
import * as v from './validation.js';
import { missing, conflict } from './errors.js';
import { eventRepository } from '../repositories/eventRepository.js';
export const states=['MONITORING','SUSPECTED','VERIFYING','ALERTING','AWAITING_HELP'];
export function eventService({db,config,now}) {
 const repo=eventRepository(db);
 return {
  create(b) {
   v.string(b.eventId,'eventId',64); v.string(b.deviceId,'deviceId'); v.string(b.userId,'userId'); v.integer(b.sequenceNumber,'sequenceNumber'); v.integer(b.timestampMs,'timestampMs');
   v.enumeration(b.eventType,'eventType',['IMPACT_DETECTED','FREE_FALL_SUSPECTED','POSTURE_CHANGED','INSTABILITY_DETECTED','INACTIVITY_DETECTED','SOS_PRESSED','SOS_CANCELLED','LOW_BATTERY','SENSOR_ERROR','MANUAL_SOS']);
   v.enumeration(b.severity,'severity',['INFO','WARNING','CRITICAL']); v.enumeration(b.alertState,'alertState',states); v.source(b.sensorSource);
   for(const k of ['peakAccelerationMs2','orientationChangeDeg','altitudeDeltaM']) v.nullable(b[k],k);
   v.boolean(b.sosButtonPressed,'sosButtonPressed'); v.integer(b.confidencePercent,'confidencePercent',100);
   if(b.dispatchStatus!==undefined)v.enumeration(b.dispatchStatus,'dispatchStatus',['NONE']);
   if(b.outcome!==undefined)v.enumeration(b.outcome,'outcome',['PENDING','CANCELLED_SAFE','RESOLVED_ACKNOWLEDGED','TIMEOUT_ESCALATED']);
   if(b.response!==undefined)v.enumeration(b.response,'response',['UNKNOWN','SAFE','NEED_HELP','NO_RESPONSE']);
   if(b.watchdogActive!==undefined)v.boolean(b.watchdogActive,'watchdogActive');
   if(b.watchdogDeadlineMs!==undefined)v.nullable(b.watchdogDeadlineMs,'watchdogDeadlineMs',v.integer);
   if(b.eligibleContactsCount!==undefined)v.integer(b.eligibleContactsCount,'eligibleContactsCount');
   const displayName=b.displayName===undefined?'Người dùng FallSafe':v.string(b.displayName,'displayName',100).trim();
   const input=Object.fromEntries(['eventId','deviceId','userId','sequenceNumber','timestampMs','eventType','severity','alertState','sensorSource','peakAccelerationMs2','orientationChangeDeg','altitudeDeltaM','sosButtonPressed','confidencePercent'].map(k=>[k,b[k]]));
   input.displayName=displayName;
   input.location=v.location(b.location);
   const existing=repo.row(b.eventId);
   if(existing) {
    const original=JSON.parse(existing.original_json);if(original.displayName===undefined)original.displayName='Người dùng FallSafe';
    if(!isDeepStrictEqual(original,input)) conflict('Conflicting eventId','DUPLICATE_EVENT_ID');
    return {status:200,data:repo.get(b.eventId)};
   }
   const time=now(), verifying=b.alertState==='VERIFYING';
   repo.insert({...input,outcome:'PENDING',response:'UNKNOWN',watchdogActive:verifying,watchdogDeadlineMs:verifying?time+config.COUNTDOWN_TIMEOUT_MS+config.WATCHDOG_GRACE_MS:null,dispatchStatus:'NONE',eligibleContactsCount:0,createdAtMs:time,lastUpdatedMs:time},input);
   return {status:201,data:repo.get(b.eventId)};
  },
  get(id) {v.string(id,'eventId',64);return repo.get(id)??missing();},
  list(query) {
   const filters={limit:32};
   for(const key of ['deviceId','userId']) if(query.has(key)) filters[key]=v.string(query.get(key),key);
   if(query.has('limit')) {const raw=query.get('limit'); if(!/^[0-9]+$/.test(raw)) v.integer(NaN,'limit'); filters.limit=v.number(Number(raw),'limit',1,100,true);}
   return {events:repo.list(filters)};
  },
 };
}
