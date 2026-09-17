import test from 'node:test';
import assert from 'node:assert/strict';
import {setup,event} from './helpers.js';
test('VERIFYING event persists deadline, supports reads, filters and retries',async t=>{
 const a=await setup(t);
 assert.deepEqual((await a.request('/events')).data.events,[]);
 const r=await a.request('/events','POST',event);
 assert.equal(r.status,201); assert.equal(r.data.alertState,'VERIFYING'); assert.equal(r.data.active,true); assert.equal(r.data.watchdogDeadlineMs,1015000);
 const d=(await a.request('/events/e1')).data;
 for(const [k,value] of Object.entries(event)) assert.deepEqual(d[k],value);
 assert.equal(d.watchdogActive,true); assert.equal(d.dispatchStatus,'NONE'); assert.equal(d.acknowledgement.acknowledged,false);
 assert.equal((await a.request('/events','POST',event)).status,200);
 assert.equal((await a.request('/events','POST',{...event,deviceId:'other'})).error.code,'DUPLICATE_EVENT_ID');
 assert.equal((await a.request('/events?userId=other')).data.events.length,0);
 assert.equal((await a.request('/events?deviceId=d1&limit=1')).data.events.length,1);
 assert.equal((await a.request('/events/missing')).status,404);
 assert.equal((await a.request('/system/status')).data.activeAlertsCount,1);
});
test('event validation rejects bad enums, IDs and limits',async t=>{
 const a=await setup(t);
 for(const patch of [{eventId:'x'.repeat(65)},{userId:''},{eventType:'FAKE'},{severity:'HIGH'},{alertState:'CANCELLED'},{confidencePercent:101},{sequenceNumber:1.5},{peakAccelerationMs2:'x'}]) assert.equal((await a.request('/events','POST',{...event,...patch})).status,400);
 for(const limit of ['0','101','1.5','NaN','']) assert.equal((await a.request(`/events?limit=${limit}`)).status,400);
});
