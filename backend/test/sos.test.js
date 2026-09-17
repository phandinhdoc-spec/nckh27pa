import test from 'node:test';
import assert from 'node:assert/strict';
import {setup,event,contact} from './helpers.js';
export const sos={eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,triggerSource:'COUNTDOWN_TIMEOUT',response:'NO_RESPONSE',location:null};
test('VERIFYING SOS records one durable outbox, snapshots eligible contacts and retries safely',async t=>{
 const a=await setup(t);
 await a.request('/users/u1/contacts','POST',contact);
 await a.request('/users/u1/contacts','POST',{...contact,name:'Two'});
 await a.request('/users/u1/contacts','POST',{...contact,name:'Disabled',receiveSos:false});
 await a.request('/users/u2/contacts','POST',contact);
 await a.request('/events','POST',event);
 const r=await a.request('/alerts/sos','POST',sos);
 assert.equal(r.status,200);assert.equal(r.data.alertState,'AWAITING_HELP');assert.equal(r.data.dispatchStatus,'RECORDED');assert.equal(r.data.eligibleContactsCount,2);
 const rows=a.db.prepare('SELECT * FROM alert_outbox').all();assert.equal(rows.length,1);assert.equal(rows[0].status,'RECORDED');assert.equal(rows[0].dispatched_at_ms,null);
 assert.equal(JSON.parse(rows[0].recipients_json).length,2);
 for(let i=0;i<3;i++)assert.equal((await a.request('/alerts/sos','POST',sos)).data.recordedAtMs,r.data.recordedAtMs);
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
 assert.equal((await a.request('/events/e1')).data.watchdogActive,false);
 assert.equal((await a.request('/alerts/cancel','POST',{...sos,reason:'SAFE'})).status,409);
});
test('manual SOS creates event without a prior detection or eligible contacts',async t=>{
 const a=await setup(t);
 const r=await a.request('/alerts/sos','POST',{...sos,triggerSource:'MANUAL_APP_BUTTON',response:'NEED_HELP'});
 assert.equal(r.status,200);assert.equal(r.data.eligibleContactsCount,0);assert.equal(r.data.dispatchStatus,'RECORDED');
 assert.equal((await a.request('/events/e1')).data.eventType,'MANUAL_SOS');
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
 assert.equal((await a.request('/alerts/sos','POST',{...sos,userId:'u2',triggerSource:'MANUAL_APP_BUTTON',response:'NEED_HELP'})).status,404);
});
test('SOS rejects unknown timeout, invalid triggers and resolved/cancelled states',async t=>{
 const a=await setup(t);
 assert.equal((await a.request('/alerts/sos','POST',sos)).status,404);
 for(const patch of [{triggerSource:'FAKE'},{response:'SAFE'},{timestampMs:-1},{triggerSource:'MANUAL_APP_BUTTON',response:'NO_RESPONSE'}])assert.equal((await a.request('/alerts/sos','POST',{...sos,...patch})).status,400);
 await a.request('/events','POST',event);
 await a.request('/alerts/cancel','POST',{...sos,reason:'SAFE'});
 assert.equal((await a.request('/alerts/sos','POST',sos)).status,409);
});
test('concurrent SOS retries serialize into one outbox record',async t=>{
 const a=await setup(t);await a.request('/events','POST',event);
 const results=await Promise.all(Array.from({length:12},()=>a.request('/alerts/sos','POST',sos)));
 for(const r of results)assert.equal(r.status,200);
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
});
