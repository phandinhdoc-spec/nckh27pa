import test from 'node:test';
import assert from 'node:assert/strict';
import { setup } from './helpers.js';
test('empty SQLite, real HTTP health and error envelopes', async t => {
  const app = await setup(t);
  for (const table of ['devices', 'sensor_readings', 'safety_events', 'emergency_contacts', 'alert_outbox']) assert.equal(app.db.prepare(`SELECT count(*) AS n FROM ${table}`).get().n, 0);
  const health = await app.request('/system/status');
  assert.equal(health.status, 200); assert.equal(health.success, true);
  assert.equal(health.data.status, 'HEALTHY'); assert.equal(health.data.activeAlertsCount, 0);
  assert.equal(typeof health.timestamp, 'number');
  for (const [path, method, body, status] of [['/missing', 'GET', undefined, 404], ['/devices/d1/status', 'POST', '{', 400], ['/devices/d1/status', 'POST', 'x'.repeat(65537), 400]]) {
    const r = await app.request(path, method, body);
    assert.equal(r.status, status); assert.equal(r.success, false); assert.equal(typeof r.error.code, 'string'); assert.equal(typeof r.timestamp, 'number');
  }
});
