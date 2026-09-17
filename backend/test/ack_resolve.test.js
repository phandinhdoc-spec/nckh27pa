import test from 'node:test';
import assert from 'node:assert/strict';
import {setup,event} from './helpers.js';
const sos={eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,triggerSource:'COUNTDOWN_TIMEOUT',response:'NO_RESPONSE'};
const ack={eventId:'e1',acknowledgedBy:'Mai',timestampMs:3,note:'Coming'};
const resolve={eventId:'e1',resolvedBy:'Mai',timestampMs:4,resolutionNote:'Safe now'};
test('ACK leaves AWAITING_HELP active; resolve returns MONITORING with persistent retry results',async t=>{
 const a=await setup(t);await a.request('/events','POST',event);await a.request('/alerts/sos','POST',sos);
 const r=await a.request('/alerts/acknowledge','POST',ack);
 assert.equal(r.status,200);assert.equal(r.data.alertState,'AWAITING_HELP');assert.equal(r.data.caregiverAcknowledged,true);assert.equal(r.data.acknowledgedAtMs,3);
 assert.equal((await a.request('/alerts/active?userId=u1')).data.caregiverAcknowledged,true);
 const d=(await a.request('/events/e1')).data;assert.deepEqual(d.acknowledgement,{acknowledged:true,acknowledgedBy:'Mai',acknowledgedAtMs:3,note:'Coming'});
 const resolved=await a.request('/alerts/resolve','POST',resolve);
 assert.equal(resolved.data.alertState,'MONITORING');assert.equal(resolved.data.outcome,'RESOLVED_ACKNOWLEDGED');assert.equal(resolved.data.resolvedAtMs,4);assert.equal(resolved.data.status,'ACKNOWLEDGED');
 for(const [action,body] of [['acknowledge',ack],['resolve',resolve],['sos',sos]])assert.equal((await a.request(`/alerts/${action}`,'POST',body)).data.alertState,'MONITORING');
 assert.equal((await a.request('/events','POST',event)).data.alertState,'MONITORING');
 assert.equal((await a.request('/system/status')).data.activeAlertsCount,0);
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,1);
});
test('ACK/resolve reject unknown resources, invalid bodies and VERIFYING',async t=>{
 const a=await setup(t);
 assert.equal((await a.request('/alerts/acknowledge','POST',ack)).status,404);
 await a.request('/events','POST',event);
 for(const [action,body] of [['acknowledge',ack],['resolve',resolve]]) assert.equal((await a.request(`/alerts/${action}`,'POST',body)).status,409);
 assert.equal((await a.request('/alerts/acknowledge','POST',{...ack,acknowledgedBy:''})).status,400);
 assert.equal((await a.request('/alerts/resolve','POST',{...resolve,timestampMs:-1})).status,400);
});
test('outbox storage failure returns safe internal envelope and rolls back SOS',async t=>{
 const a=await setup(t);await a.request('/events','POST',event);
 a.db.exec("CREATE TRIGGER fail_outbox BEFORE INSERT ON alert_outbox BEGIN SELECT RAISE(ABORT, 'private storage failure'); END");
 const r=await a.request('/alerts/sos','POST',sos);
 assert.equal(r.status,500);assert.equal(r.success,false);assert.equal(r.error.code,'INTERNAL_SERVER_ERROR');assert.equal(r.error.message.includes('private'),false);
 const d=(await a.request('/events/e1')).data;assert.equal(d.alertState,'VERIFYING');assert.equal(d.watchdogActive,true);assert.equal(d.dispatchStatus,'NONE');
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_actions').get().n,0);
 a.db.exec('DROP TRIGGER fail_outbox');assert.equal((await a.request('/alerts/sos','POST',sos)).status,200);
});
test('failure after outbox insertion rolls back both sink and state',async t=>{
 const a=await setup(t);await a.request('/events','POST',event);
 a.db.exec("CREATE TRIGGER fail_state BEFORE UPDATE ON safety_events WHEN NEW.alert_state='AWAITING_HELP' BEGIN SELECT RAISE(ABORT, 'test failure'); END");
 assert.equal((await a.request('/alerts/sos','POST',sos)).status,500);
 assert.equal(a.db.prepare('SELECT count(*) n FROM alert_outbox').get().n,0);
 assert.equal((await a.request('/events/e1')).data.alertState,'VERIFYING');
 a.db.exec('DROP TRIGGER fail_state');assert.equal((await a.request('/alerts/sos','POST',sos)).status,200);
});
