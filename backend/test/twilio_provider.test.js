import test from 'node:test';
import assert from 'node:assert/strict';
import { createHmac } from 'node:crypto';
import { validateTwilioSignature, buildVoiceTwiml } from '../src/providers/twilioVoiceProvider.js';

function signature(token, url, params) {
  const payload = url + Object.keys(params).sort().map(key => key + params[key]).join('');
  return createHmac('sha1', token).update(payload).digest('base64');
}

test('Twilio callback signature validation rejects tampering', () => {
  const url = 'https://public.example/api/v1/voice/twilio/e1/c1/1/status';
  const params = { CallSid: 'CA1', CallStatus: 'busy' };
  const signed = signature('secret', url, params);
  assert.equal(validateTwilioSignature('secret', url, params, signed), true);
  assert.equal(validateTwilioSignature('secret', url, { ...params, CallStatus: 'completed' }, signed), false);
  assert.equal(validateTwilioSignature('secret', url, params, ''), false);
});

test('voice TwiML repeats the full bounded message and never claims an unreported SMS was sent', () => {
  const pending = buildVoiceTwiml({ displayName: 'Người dùng FallSafe', smsStatus: 'FAILED', hasLocation: false, gatherUrl: 'https://public.example/gather' });
  assert.match(pending, /<Gather[^>]+numDigits="1"/);
  assert.doesNotMatch(pending, /đã gửi liên kết/i);
  assert.doesNotMatch(pending, /loop="0"/);
  assert.equal((pending.match(/cần trợ giúp/g) ?? []).length, 4);
  assert.equal((pending.match(/<Pause length="3"\/>/g) ?? []).length, 4);

  const sent = buildVoiceTwiml({ displayName: 'Người dùng FallSafe', smsStatus: 'SENT', hasLocation: true, gatherUrl: 'https://public.example/gather' });
  assert.match(sent, /đã gửi liên kết/i);
});

test('acknowledged TwiML keeps bounded emergency speech without gathering or hanging up', () => {
  const output = buildVoiceTwiml({ displayName: 'Bà An', smsStatus: 'DELIVERED', hasLocation: true,
    gatherUrl: 'https://public.example/gather', acknowledged: true });
  assert.equal((output.match(/Bà An/g) ?? []).length, 3);
  assert.equal((output.match(/<Pause length="3"\/>/g) ?? []).length, 3);
  assert.doesNotMatch(output, /<Gather/);
  assert.doesNotMatch(output, /<Hangup/);
});
