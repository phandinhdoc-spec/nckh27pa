import test from 'node:test';
import assert from 'node:assert/strict';
import {setup,reading} from './helpers.js';
test('sensor ingest/latest round trip and natural sequence idempotency',async t=>{
 const a=await setup(t);
 assert.equal((await a.request('/sensors/latest?deviceId=d1')).status,404);
 const sample={...reading,motionState:'WALKING',fallRisk:'LOW',location:{latitude:10,longitude:106,accuracyM:5,timestampMs:1,locationMessage:'Home'}};
 assert.equal((await a.request('/sensors/ingest','POST',sample)).data.stored,true);
 assert.deepEqual((await a.request('/sensors/latest?deviceId=d1')).data,sample);
 assert.equal((await a.request('/sensors/ingest','POST',{...sample,accelXMs2:99})).status,200);
 assert.equal(a.db.prepare('SELECT count(*) n FROM sensor_readings').get().n,1);
 assert.equal((await a.request('/sensors/latest?deviceId=d1')).data.accelXMs2,0);
 await a.request('/sensors/ingest','POST',{...reading,sequenceNumber:2,timestampMs:2});
 await a.request('/sensors/ingest','POST',{...reading,sequenceNumber:3,timestampMs:0});
 assert.equal((await a.request('/sensors/latest?deviceId=d1')).data.sequenceNumber,2);
});
test('sensor rejects invalid fields and missing required nullable fields',async t=>{
 const a=await setup(t);
 for(const patch of [{sequenceNumber:-1},{sensorSource:'BLE'},{sensorQuality:101},{sosButtonPressed:0},{motionState:'BAD'},{fallRisk:'BAD'},{location:{latitude:91}},{accelXMs2:'0'},{pressurePa:undefined}]) assert.equal((await a.request('/sensors/ingest','POST',{...reading,...patch})).status,400);
 assert.equal((await a.request('/sensors/latest')).status,400);
 assert.equal(a.db.prepare('SELECT count(*) n FROM sensor_readings').get().n,0);
});
test('PHONE without gyroscope preserves all three explicit null axes over HTTP and SQLite',async t=>{
 const a=await setup(t);
 const sample={...reading,sensorSource:'PHONE',gyroXDps:null,gyroYDps:null,gyroZDps:null};
 const result=await a.request('/sensors/ingest','POST',sample);
 assert.equal(result.status,200,JSON.stringify(result));
 assert.deepEqual((await a.request('/sensors/latest?deviceId=d1')).data,sample);
 const row=a.db.prepare('SELECT gyro_x_dps,gyro_y_dps,gyro_z_dps FROM sensor_readings').get();
 assert.deepEqual({...row},{gyro_x_dps:null,gyro_y_dps:null,gyro_z_dps:null});
});
test('gyroscope group requires all axes present and either three numbers or three nulls',async t=>{
 const a=await setup(t);
 for(const axis of ['gyroXDps','gyroYDps','gyroZDps']) {
  for(const value of [undefined,'0',true,null]) {
   const r=await a.request('/sensors/ingest','POST',{...reading,[axis]:value});
   assert.equal(r.status,400,`${axis}=${value}`);assert.equal(r.error.code,'VALIDATION_ERROR');
  }
 }
 for(const field of ['deviceId','sensorSource','sequenceNumber','timestampMs','accelXMs2','accelYMs2','accelZMs2']) {
  assert.equal((await a.request('/sensors/ingest','POST',{...reading,[field]:null})).status,400);
 }
 assert.equal(a.db.prepare('SELECT count(*) n FROM sensor_readings').get().n,0);
});
