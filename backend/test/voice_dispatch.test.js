import test from 'node:test';
import assert from 'node:assert/strict';
import { setup, event, contact } from './helpers.js';
import { voiceDispatchService } from '../src/services/voiceDispatchService.js';
import { createHmac } from 'node:crypto';
const sos={eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:2,triggerSource:'COUNTDOWN_TIMEOUT',response:'NO_RESPONSE',location:null};

test('disabled provider reports truthful pending dispatch without calling a network provider', async t => {
  const a = await setup(t);
  await a.request('/users/u1/contacts', 'POST', contact);
  await a.request('/events', 'POST', event);
  await a.request('/alerts/sos', 'POST', sos);

  const result = await a.request('/alerts/e1/dispatch');

  assert.equal(result.status, 200);
  assert.equal(result.data.provider, 'DISABLED');
  assert.equal(result.data.providerConfigured, false);
  assert.equal(result.data.status, 'PENDING');
  assert.equal(result.data.acknowledged, false);
  assert.deepEqual(result.data.attempts, []);
});

test('configured provider starts only the highest-priority contact once across SOS retries', async t => {
  const calls = [];
  const provider = { name: 'FAKE', configured: true, createCall: attempt => {
    calls.push(attempt);
    return { providerCallId: 'call-1' };
  }};
  const a = await setup(t, { VOICE_PROVIDER: 'TWILIO' }, 1000000, { voiceProvider: provider });
  await a.request('/users/u1/contacts', 'POST', { ...contact, id: '11111111-1111-4111-8111-111111111111', callPriority: 20 });
  await a.request('/users/u1/contacts', 'POST', { ...contact, id: '22222222-2222-4222-8222-222222222222', name: 'First', phone: '0912345678', callPriority: 1 });
  await a.request('/events', 'POST', event);

  await a.request('/alerts/sos', 'POST', sos);
  await a.request('/alerts/sos', 'POST', sos);

  assert.equal(calls.length, 1);
  assert.equal(calls[0].contactId, '22222222-2222-4222-8222-222222222222');
  const state = (await a.request('/alerts/e1/dispatch')).data;
  assert.equal(state.status, 'CALLING');
  assert.equal(state.attempts.length, 1);
  assert.equal(state.attempts[0].providerCallId, 'call-1');
});

test('terminal call status advances in priority order while DTMF 1 stops later contacts without resolving incident', async t => {
  const calls = [];
  const provider = { name: 'FAKE', configured: true, createCall: attempt => {
    calls.push(attempt);
    return { providerCallId: `call-${calls.length}` };
  }};
  const a = await setup(t, { VOICE_PROVIDER: 'TWILIO', VOICE_MAX_ATTEMPTS_PER_CONTACT: 1 }, 1000000, { voiceProvider: provider });
  for (const [id, priority, phone] of [
    ['11111111-1111-4111-8111-111111111111', 1, '0901234567'],
    ['22222222-2222-4222-8222-222222222222', 2, '0912345678'],
    ['33333333-3333-4333-8333-333333333333', 3, '0934567890'],
  ]) await a.request('/users/u1/contacts', 'POST', { ...contact, id, phone, callPriority: priority });
  await a.request('/events', 'POST', event);
  await a.request('/alerts/sos', 'POST', sos);
  const voice = voiceDispatchService({ db: a.db, config: a.config, now: () => 1000000, voiceProvider: provider });

  voice.handleStatus('e1', calls[0].contactId, 1, 'busy');
  assert.equal(calls.length, 2);
  voice.handleGather('e1', calls[1].contactId, 1, '1');
  voice.handleStatus('e1', calls[1].contactId, 1, 'completed');
  voice.handleStatus('e1', calls[0].contactId, 1, 'failed');

  assert.equal(calls.length, 2);
  const state = voice.status('e1');
  assert.equal(state.status, 'ACKNOWLEDGED');
  assert.equal(state.acknowledged, true);
  assert.equal(a.db.prepare('SELECT alert_state FROM safety_events WHERE event_id=?').get('e1').alert_state, 'AWAITING_HELP');
});

test('Twilio callback endpoints require a valid signature and serve truthful XML', async t => {
  const base='https://public.example';
  const token='test-token';
  const provider={name:'FAKE',configured:true,createCall:()=>({providerCallId:'CA1'})};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO',TWILIO_AUTH_TOKEN:token,PUBLIC_CALLBACK_BASE_URL:base},1000000,{voiceProvider:provider});
  const id='11111111-1111-4111-8111-111111111111';
  await a.request('/users/u1/contacts','POST',{...contact,id});
  await a.request('/events','POST',event);await a.request('/alerts/sos','POST',sos);
  const path=`/voice/twilio/e1/${id}/1/twiml`;
  const url=`${base}/api/v1${path}`;
  const signed=createHmac('sha1',token).update(url).digest('base64');

  assert.equal((await a.requestRaw(path,'POST','',{'content-type':'application/x-www-form-urlencoded','x-twilio-signature':'wrong'})).status,403);
  const valid=await a.requestRaw(path,'POST','',{'content-type':'application/x-www-form-urlencoded','x-twilio-signature':signed});
  assert.equal(valid.status,200);assert.match(valid.contentType,/application\/xml/);assert.match(valid.text,/Vị trí hiện chưa xác định|Vị trí đang chờ gửi/);
});

test('watchdog escalation starts configured voice dispatch independently', async t => {
  const calls=[];const provider={name:'FAKE',configured:true,createCall:a=>{calls.push(a);return {providerCallId:'CA1'};}};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO',WATCHDOG_SCAN_INTERVAL_MS:60000},1000000,{voiceProvider:provider});
  await a.request('/users/u1/contacts','POST',{...contact,id:'11111111-1111-4111-8111-111111111111'});
  await a.request('/events','POST',event);a.advance(15000);
  await a.request('/alerts/cancel','POST',{eventId:'e1',deviceId:'d1',userId:'u1',timestampMs:1,reason:'SAFE'});
  assert.equal(calls.length,1);
});

test('default retries are round-robin by attempt round', async t => {
  const calls=[];const provider={name:'FAKE',configured:true,createCall:a=>{calls.push(a);return {providerCallId:`CA${calls.length}`};}};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO'},1000000,{voiceProvider:provider});
  for (const [id,priority,phone] of [['11111111-1111-4111-8111-111111111111',1,'0901234567'],['22222222-2222-4222-8222-222222222222',2,'0912345678']])
    await a.request('/users/u1/contacts','POST',{...contact,id,phone,callPriority:priority});
  await a.request('/events','POST',event);await a.request('/alerts/sos','POST',sos);
  const voice=voiceDispatchService({db:a.db,config:a.config,now:()=>1000000,voiceProvider:provider});
  voice.handleStatus('e1',calls[0].contactId,1,'busy');
  assert.deepEqual(calls.map(x=>[x.contactId,x.attempt]),[
    ['11111111-1111-4111-8111-111111111111',1],
    ['22222222-2222-4222-8222-222222222222',1],
  ]);
  voice.handleStatus('e1',calls[1].contactId,1,'no-answer');
  assert.deepEqual([calls[2].contactId,calls[2].attempt],['11111111-1111-4111-8111-111111111111',2]);
});

test('late provider completion cannot overwrite a terminal callback or duplicate the next attempt', async t => {
  const pending=[];const calls=[];
  const provider={name:'FAKE',configured:true,createCall:a=>{calls.push(a);return new Promise(resolve=>pending.push(resolve));}};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO',VOICE_MAX_ATTEMPTS_PER_CONTACT:1},1000000,{voiceProvider:provider});
  for (const [id,priority,phone] of [['11111111-1111-4111-8111-111111111111',1,'0901234567'],['22222222-2222-4222-8222-222222222222',2,'0912345678']])
    await a.request('/users/u1/contacts','POST',{...contact,id,phone,callPriority:priority});
  await a.request('/events','POST',event);await a.request('/alerts/sos','POST',sos);
  const voice=voiceDispatchService({db:a.db,config:a.config,now:()=>1000000,voiceProvider:provider});
  voice.handleStatus('e1',calls[0].contactId,1,'busy');
  pending[0]({providerCallId:'LATE'}); await Promise.resolve(); await Promise.resolve();
  assert.equal(calls.length,2);
  assert.equal(voice.status('e1').attempts.find(x=>x.contactId===calls[0].contactId).status,'busy');
});

test('authenticated SMS reports are idempotent, reject READ, and update later TwiML with display name', async t => {
  const base='https://public.example',token='test-token';
  const provider={name:'FAKE',configured:true,createCall:()=>({providerCallId:'CA1'})};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO',TWILIO_AUTH_TOKEN:token,PUBLIC_CALLBACK_BASE_URL:base},1000000,{voiceProvider:provider});
  const id='11111111-1111-4111-8111-111111111111';
  await a.request('/users/u1/contacts','POST',{...contact,id});
  await a.request('/events','POST',{...event,displayName:'Bà An'});
  await a.request('/alerts/sos','POST',{...sos,location:{latitude:10.5,longitude:106.5,accuracyM:5,timestampMs:2,locationMessage:'chưa xác định được địa chỉ'}});
  const path=`/voice/twilio/e1/${id}/1/twiml`,url=`${base}/api/v1${path}`;
  const signed=createHmac('sha1',token).update(url).digest('base64');
  let xml=await a.requestRaw(path,'POST','',{'content-type':'application/x-www-form-urlencoded','x-twilio-signature':signed});
  assert.match(xml.text,/Bà An/);assert.doesNotMatch(xml.text,/Đã gửi liên kết/);
  const auth={'X-Device-Id':'d1','X-User-Id':'u1'};
  const report={contactId:id,channel:'SMS',status:'SENT',timestampMs:10,detail:null};
  assert.equal((await a.request('/alerts/e1/transport-status','POST',report,auth)).status,200);
  assert.equal((await a.request('/alerts/e1/transport-status','POST',report,auth)).status,200);
  assert.equal((await a.request('/alerts/e1/transport-status','POST',{...report,status:'READ'},auth)).status,400);
  xml=await a.requestRaw(path,'POST','',{'content-type':'application/x-www-form-urlencoded','x-twilio-signature':signed});
  assert.match(xml.text,/Đã gửi liên kết vị trí/);
  assert.equal(a.db.prepare('SELECT count(*) n FROM transport_status_reports').get().n,1);
});

test('DTMF acknowledgement response repeats current emergency speech without launching contacts', async t => {
  const base='https://public.example',token='test-token',calls=[];
  const provider={name:'FAKE',configured:true,createCall:a=>{calls.push(a);return {providerCallId:'CA1'};}};
  const a=await setup(t,{VOICE_PROVIDER:'TWILIO',TWILIO_AUTH_TOKEN:token,PUBLIC_CALLBACK_BASE_URL:base},1000000,{voiceProvider:provider});
  const id='11111111-1111-4111-8111-111111111111';
  await a.request('/users/u1/contacts','POST',{...contact,id});await a.request('/events','POST',{...event,displayName:'Ông Bình'});await a.request('/alerts/sos','POST',sos);
  const path=`/voice/twilio/e1/${id}/1/gather`,url=`${base}/api/v1${path}`;
  const signed=createHmac('sha1',token).update(url+'Digits1').digest('base64');
  const response=await a.requestRaw(path,'POST','Digits=1',{'content-type':'application/x-www-form-urlencoded','x-twilio-signature':signed});
  assert.match(response.text,/Ông Bình/);assert.doesNotMatch(response.text,/<Gather|<Hangup/);assert.equal(calls.length,1);
});
