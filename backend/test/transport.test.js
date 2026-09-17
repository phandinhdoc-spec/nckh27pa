import test from 'node:test';
import assert from 'node:assert/strict';
import {connect} from 'node:net';
import {setup,heartbeat,event} from './helpers.js';
async function raw(port,message) {return new Promise((resolve,reject)=>{const socket=connect(port,'127.0.0.1',()=>socket.write(message));let data='';socket.setTimeout(2000,()=>socket.destroy(new Error('socket timeout')));socket.on('data',chunk=>data+=chunk);socket.on('error',reject);socket.on('end',()=>resolve(data));});}
test('invalid encoded paths and invalid HTTP produce standard validation envelopes',async t=>{
 const a=await setup(t);
 const r=await a.request('/devices/%ZZ/status');assert.equal(r.status,400);assert.equal(r.error.code,'VALIDATION_ERROR');
 const response=await raw(a.server.address().port,'BAD METHOD / HTTP/1.1\r\nHost: localhost\r\n\r\n');
 const json=JSON.parse(response.split('\r\n\r\n')[1]);assert.equal(json.success,false);assert.equal(json.error.code,'VALIDATION_ERROR');assert.equal(typeof json.timestamp,'number');
});
test('JSON primitives, missing fields, bad routes, oversized bodies and invalid filters are enveloped',async t=>{
 const a=await setup(t);
 for(const body of ['null','[]','1','true','"text"','{"timestampMs":']) {
  const r=await a.request('/devices/d1/status','POST',body);assert.equal(r.status,400);assert.equal(r.success,false);assert.equal(r.error.code,'VALIDATION_ERROR');
 }
 for(const path of ['/alerts/active','/events?deviceId=','/alerts/active?userId=','/devices/'+ 'x'.repeat(129)+'/status'])assert.equal((await a.request(path)).status,400);
 assert.equal((await a.request('/users/u1/contacts/no/action','POST')).status,404);
 assert.equal((await a.request('/devices/d1/status','PUT',heartbeat)).status,404);
});
test('event input cannot claim delivery or invalid server-managed enum values',async t=>{
 const a=await setup(t);
 for(const patch of [{dispatchStatus:'SENT'},{outcome:'RESOLVED'},{response:'BAD'},{watchdogActive:1},{dispatchStatus:'DISPATCHED'}])assert.equal((await a.request('/events','POST',{...event,...patch})).status,400);
 assert.equal(a.db.prepare('SELECT count(*) n FROM safety_events').get().n,0);
});
