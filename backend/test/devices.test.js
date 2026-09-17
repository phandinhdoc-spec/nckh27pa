import test from 'node:test';
import assert from 'node:assert/strict';
import { setup, heartbeat } from './helpers.js';
test('heartbeat registers real device and derives staleness from server receipt', async t => {
 const a = await setup(t, { HEARTBEAT_STALE_THRESHOLD_MS: 100 });
 assert.equal((await a.request('/devices/d1/status')).status,404);
 assert.equal((await a.request('/devices/d1/status','POST',heartbeat)).data.recorded,true);
 let d=(await a.request('/devices/d1/status')).data;
 assert.equal(d.lastHeartbeatMs,1000000); assert.equal(d.uptimeSeconds,42); assert.equal(d.isConnected,true);
 a.advance(100); assert.equal((await a.request('/devices/d1/status')).data.isConnected,true);
 a.advance(1); assert.equal((await a.request('/devices/d1/status')).data.isConnected,false);
 await a.request('/devices/d1/status','POST',heartbeat);
 assert.equal((await a.request('/devices/d1/status')).data.isConnected,true);
});
test('heartbeat validates required types and ranges and records low battery', async t => {
 const a=await setup(t);
 for (const patch of [{batteryPercent:101},{isCharging:1},{imuStatus:'BAD'},{timestampMs:-1},{batteryVoltageMv:-1},{uptimeSeconds:1.2}]) assert.equal((await a.request('/devices/d1/status','POST',{...heartbeat,...patch})).status,400);
 assert.equal((await a.request('/devices/d1/status','POST',{})).status,400);
 assert.equal((await a.request('/devices/d1/status','POST',{...heartbeat,batteryPercent:14})).status,200);
 const row=a.db.prepare('SELECT * FROM safety_events').get();
 assert.equal(row.event_type,'LOW_BATTERY'); assert.equal(row.severity,'WARNING'); assert.equal(row.alert_state,'MONITORING');
});
