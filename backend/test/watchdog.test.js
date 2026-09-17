import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,rm} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {setup,event} from './helpers.js';
async function waitFor(check) {for(let i=0;i<100;i++){if(await check())return;await delay(10);}assert.fail('watchdog did not reach expected state');}
test('startup recovery escalates overdue persisted events once and preserves future deadlines',async t=>{
 const dir=await mkdtemp(new URL('./recovery-',import.meta.url));t.after(()=>rm(dir,{recursive:true,force:true}));
 const cfg={DB_PATH:join(dir,'state.db'),WATCHDOG_SCAN_INTERVAL_MS:10};
 const first=await setup(t,cfg);await first.request('/events','POST',event);
 first.advance(10000);await first.request('/events','POST',{...event,eventId:'future'});await first.close();
 const second=await setup(t,cfg,1015000);
 let e=(await second.request('/events/e1')).data;assert.equal(e.alertState,'AWAITING_HELP');assert.equal(e.outcome,'TIMEOUT_ESCALATED');assert.equal(e.watchdogActive,false);assert.equal(e.dispatchStatus,'RECORDED');
 assert.equal((await second.request('/events/future')).data.alertState,'VERIFYING');
 assert.equal(second.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
 second.advance(10000);await waitFor(async()=> (await second.request('/events/future')).data.alertState==='AWAITING_HELP');
 await second.close();
 const third=await setup(t,cfg,1040000);assert.equal(third.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,2);
 const retry=await third.request('/alerts/sos','POST',{eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,triggerSource:'COUNTDOWN_TIMEOUT',response:'NO_RESPONSE'});
 assert.equal(retry.status,200);assert.equal(third.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,2);
});
test('periodic watchdog respects deadline, ignores cancelled events and never duplicates',async t=>{
 const a=await setup(t,{COUNTDOWN_TIMEOUT_MS:10,WATCHDOG_GRACE_MS:5,WATCHDOG_SCAN_INTERVAL_MS:5});
 await a.request('/events','POST',event);await a.request('/events','POST',{...event,eventId:'cancelled'});
 await a.request('/alerts/cancel','POST',{eventId:'cancelled',deviceId:'d1',userId:'u1',timestampMs:1,reason:'SAFE'});
 a.advance(14);await delay(20);assert.equal((await a.request('/events/e1')).data.alertState,'VERIFYING');
 a.advance(1);await waitFor(async()=> (await a.request('/events/e1')).data.alertState==='AWAITING_HELP');
 await delay(30);assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
 assert.equal((await a.request('/events/cancelled')).data.outcome,'CANCELLED_SAFE');
});
test('watchdog storage failure rolls back, reports unavailability and retries successfully',async t=>{
 const a=await setup(t,{WATCHDOG_SCAN_INTERVAL_MS:5});await a.request('/events','POST',event);
 a.db.exec("CREATE TRIGGER fail_watchdog BEFORE INSERT ON alert_outbox BEGIN SELECT RAISE(ABORT, 'test failure'); END");
 a.advance(15000);await waitFor(async()=> (await a.request('/system/status')).status===503);
 assert.equal((await a.request('/events/e1')).data.watchdogActive,true);assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,0);
 a.db.exec('DROP TRIGGER fail_watchdog');await waitFor(async()=> (await a.request('/events/e1')).data.alertState==='AWAITING_HELP');
 assert.equal((await a.request('/system/status')).status,200);
});
test('a late cancel cannot disable an already overdue watchdog between scans',async t=>{
 const a=await setup(t,{WATCHDOG_SCAN_INTERVAL_MS:60000});await a.request('/events','POST',event);
 a.advance(15000);
 const r=await a.request('/alerts/cancel','POST',{eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:1,reason:'SAFE'});
 assert.equal(r.status,409);assert.equal((await a.request('/events/e1')).data.outcome,'TIMEOUT_ESCALATED');
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
});
