import test from 'node:test';
import assert from 'node:assert/strict';
import {connect} from 'node:net';
import {once} from 'node:events';
import {spawn} from 'node:child_process';
import {mkdtemp,rm} from 'node:fs/promises';
import {join} from 'node:path';
import {DatabaseSync} from 'node:sqlite';
import {setTimeout as delay} from 'node:timers/promises';
import {setup,heartbeat,reading,contact,event} from './helpers.js';
import {loadConfig} from '../src/config.js';
test('shutdown clears watchdog and terminates incomplete HTTP connections',async t=>{
 const a=await setup(t,{WATCHDOG_SCAN_INTERVAL_MS:5});
 const socket=connect(a.server.address().port,'127.0.0.1');t.after(()=>socket.destroy());await once(socket,'connect');
 socket.write('POST /api/v1/events HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000\r\n\r\n{');
 await delay(10);
 const closing=a.close();
 assert.equal(await Promise.race([closing.then(()=>true),delay(200).then(()=>false)]),true);
 await a.close();assert.equal(a.server.listening,false);
});
test('oversize streaming HTTP body is rejected before client finishes uploading',async t=>{
 let socket;t.after(()=>socket?.destroy());
 const a=await setup(t);
 socket=connect(a.server.address().port,'127.0.0.1');await once(socket,'connect');
 const received=once(socket,'data');
 socket.write('POST /api/v1/events HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1000000\r\n\r\n'+'x'.repeat(70000));
 const data=await Promise.race([received.then(([chunk])=>chunk.toString()),delay(300).then(()=>null)]);
 assert.ok(data?.includes('400 Bad Request'));assert.ok(data.includes('VALIDATION_ERROR'));
});
test('configuration rejects empty numeric values and preserves environment overrides',()=>{
 assert.throws(()=>loadConfig({PORT:''}));assert.throws(()=>loadConfig({WATCHDOG_SCAN_INTERVAL_MS:'0'}));
 const c=loadConfig({PORT:'0',COUNTDOWN_TIMEOUT_MS:'20',WATCHDOG_GRACE_MS:'5',HOST:'127.0.0.1',DB_PATH:':memory:',HEARTBEAT_STALE_THRESHOLD_MS:'30',WATCHDOG_SCAN_INTERVAL_MS:'4'});
 assert.equal(c.PORT,0);assert.equal(c.COUNTDOWN_TIMEOUT_MS,20);assert.equal(c.HEARTBEAT_STALE_THRESHOLD_MS,30);
});
test('file database retains API data and lifecycle retries across restart',async t=>{
 const dir=await mkdtemp(new URL('./persist-',import.meta.url));t.after(()=>rm(dir,{recursive:true,force:true}));const cfg={DB_PATH:join(dir,'state.db')};
 const a=await setup(t,cfg);await a.request('/devices/d1/status','POST',heartbeat);await a.request('/sensors/ingest','POST',reading);
 const c=(await a.request('/users/u1/contacts','POST',contact)).data.contact;
 await a.request('/events','POST',event);
 const cancel={eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,reason:'SAFE'};await a.request('/alerts/cancel','POST',cancel);await a.close();
 const b=await setup(t,cfg);
 assert.equal((await b.request('/devices/d1/status')).data.uptimeSeconds,42);assert.deepEqual((await b.request('/sensors/latest?deviceId=d1')).data,reading);
 assert.equal((await b.request('/users/u1/contacts')).data.contacts[0].id,c.id);
 assert.equal((await b.request('/alerts/cancel','POST',cancel)).data.outcome,'CANCELLED_SAFE');
 assert.equal((await b.request('/events','POST',event)).status,200);assert.equal(b.db.prepare('SELECT count(*) n FROM safety_events').get().n,1);
});
test('npm start serves real HTTP with empty file SQLite and exits on SIGTERM',async t=>{
 const dir=await mkdtemp(new URL('./runtime-',import.meta.url));t.after(()=>rm(dir,{recursive:true,force:true}));
 const path=join(dir,'runtime.db');
 const child=spawn('npm',['start'],{cwd:new URL('../',import.meta.url),env:{...process.env,HOST:'127.0.0.1',PORT:'0',DB_PATH:path},detached:true,stdio:['ignore','pipe','pipe']});
 const exited=once(child,'exit');let stopped=false;
 t.after(async()=>{if(!stopped){try{process.kill(-child.pid,'SIGKILL');}catch{}await exited;}});
 let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
 let address;
 for(let i=0;i<100;i++){address=output.match(/FallSafe listening on (http:\/\/[^\s]+)/)?.[1];if(address)break;await delay(20);}
 assert.ok(address,output);
 const r=await fetch(address+'/api/v1/system/status');assert.equal(r.status,200);assert.equal((await r.json()).data.activeAlertsCount,0);
 const db=new DatabaseSync(path);for(const table of ['devices','emergency_contacts','safety_events'])assert.equal(db.prepare(`SELECT count(*) n FROM ${table}`).get().n,0);db.close();
 process.kill(-child.pid,'SIGTERM');await exited;stopped=true;
 await assert.rejects(fetch(address+'/api/v1/system/status'));
});
