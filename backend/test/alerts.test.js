import test from 'node:test';
import assert from 'node:assert/strict';
import {setup,event,heartbeat} from './helpers.js';
const cancel={eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,reason:'SAFE',note:'Safe'};
test('VERIFYING cancel returns MONITORING/CANCELLED_SAFE and is idempotent',async t=>{
 const a=await setup(t);
 await a.request('/events','POST',event);
 let active=(await a.request('/alerts/active?deviceId=d1')).data;
 assert.equal(active.alertState,'VERIFYING');assert.equal(active.remainingMs,10000);assert.equal(active.activeEventId,'e1');
 a.advance(4000);assert.equal((await a.request('/alerts/active?deviceId=d1')).data.remainingMs,6000);
 const r=await a.request('/alerts/cancel','POST',cancel);
 assert.equal(r.status,200);assert.equal(r.data.alertState,'MONITORING');assert.equal(r.data.outcome,'CANCELLED_SAFE');
 assert.equal((await a.request('/alerts/cancel','POST',cancel)).data.cancelledAtMs,r.data.cancelledAtMs);
 const detail=(await a.request('/events/e1')).data;assert.equal(detail.response,'SAFE');assert.equal(detail.watchdogActive,false);
 active=(await a.request('/alerts/active?deviceId=d1')).data;assert.equal(active.alertState,'MONITORING');assert.equal(active.activeEventId,null);
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,0);
});
test('cancel rejects unknown events, mismatched ownership and invalid states',async t=>{
 const a=await setup(t);
 assert.equal((await a.request('/alerts/cancel','POST',cancel)).status,404);
 await a.request('/events','POST',event);
 assert.equal((await a.request('/alerts/cancel','POST',{...cancel,userId:'u2'})).status,404);
 assert.equal((await a.request('/alerts/cancel','POST',{...cancel,reason:'HELP'})).status,400);
 await a.request('/events','POST',{...event,eventId:'e2',alertState:'MONITORING'});
 assert.equal((await a.request('/alerts/cancel','POST',{...cancel,eventId:'e2'})).status,409);
 assert.equal((await a.request('/alerts/active?deviceId=missing')).status,404);
 await a.request('/devices/d2/status','POST',heartbeat);
 assert.equal((await a.request('/alerts/active?deviceId=d2')).data.alertState,'MONITORING');
});
