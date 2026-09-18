import { createApp } from '../src/app.js';
export async function setup(t, overrides = {}, initialTime = 1000000, appOptions = {}) {
  let time = initialTime;
  const app = createApp({ ...appOptions, config: { DB_PATH: ':memory:', HOST: '127.0.0.1', PORT: 0, ...overrides }, now: () => time });
  t.after(() => app.close());
  await app.listen();
  const request = async (path, method = 'GET', body, headers = {}) => {
    const response = await fetch(`http://127.0.0.1:${app.server.address().port}/api/v1${path}`, {
      method, headers: { 'content-type': 'application/json', ...headers },
      ...(body === undefined ? {} : { body: typeof body === 'string' ? body : JSON.stringify(body) }),
    });
    return { status: response.status, ...(await response.json()) };
  };
  const requestRaw = async (path, method = 'POST', body = '', headers = {}) => {
    const response = await fetch(`http://127.0.0.1:${app.server.address().port}/api/v1${path}`, { method, headers, body });
    return { status: response.status, contentType: response.headers.get('content-type'), text: await response.text() };
  };
  return { ...app, request, requestRaw, advance: ms => { time += ms; } };
}
export const heartbeat = { firmwareVersion: '1.0.0', timestampMs: 1, uptimeSeconds: 42, batteryPercent: 80, batteryVoltageMv: null, isCharging: false, imuStatus: 'OK', barometerStatus: 'OK', gnssStatus: 'UNAVAILABLE', bufferUsagePercent: 0, lastErrorCode: null };
export const reading = { sensorSource: 'ESP32', deviceId: 'd1', sequenceNumber: 1, timestampMs: 1, accelXMs2: 0, accelYMs2: 0, accelZMs2: 9.8, gyroXDps: 0, gyroYDps: 0, gyroZDps: 0, pressurePa: null, temperatureC: null, altitudeDeltaM: null, batteryPercent: 80, batteryVoltageMv: null, isCharging: false, sosButtonPressed: false, sensorQuality: 95, location: null };
export const event = { eventId: 'e1', deviceId: 'd1', userId: 'u1', sequenceNumber: 2, timestampMs: 1, eventType: 'IMPACT_DETECTED', severity: 'CRITICAL', alertState: 'VERIFYING', sensorSource: 'ESP32', peakAccelerationMs2: 26, orientationChangeDeg: 68, altitudeDeltaM: null, sosButtonPressed: false, confidencePercent: 92, location: null };
export const contact = { name: 'Mai', relationship: 'Con gái', phone: '090 123-45.67', receiveSos: true, isPrimary: false };
