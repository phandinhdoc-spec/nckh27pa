import test from 'node:test';
import assert from 'node:assert/strict';
import { setup } from './helpers.js';
import { openAiCompatibleTextProvider } from '../src/providers/openAiCompatibleTextProvider.js';
import { loadConfig } from '../src/config.js';
import { aiTextService } from '../src/services/aiTextService.js';
import { route } from '../src/routes/router.js';

test('incomplete optional AI configuration degrades to NOT_CONFIGURED without blocking backend boot', () => {
  const config = loadConfig({ AI_PROVIDER: 'OPENAI_COMPATIBLE' });
  const provider = openAiCompatibleTextProvider(config);
  assert.equal(provider.configured, false);
  assert.equal(provider.name, 'DISABLED');
});

test('whitespace provider configuration is never treated as configured', () => {
  const provider = openAiCompatibleTextProvider({
    AI_PROVIDER:'OPENAI_COMPATIBLE',AI_BASE_URL:'   ',AI_MODEL:' ',AI_API_KEY:'\t',
  });
  assert.equal(provider.configured,false);
  assert.equal(provider.name,'DISABLED');
});

test('AI text is disabled by default with a truthful NOT_CONFIGURED response', async t => {
  const app = await setup(t);
  const result = await app.request('/ai/text', 'POST', { text: 'Xin chào' });
  assert.equal(result.status, 503);
  assert.equal(result.success, false);
  assert.equal(result.error.code, 'NOT_CONFIGURED');
});

test('AI endpoint accepts only one bounded text field and never reflects sensitive input', async t => {
  const calls = [];
  const provider = { name: 'FAKE', configured: true, requestText: async text => {
    calls.push(text);
    return { status: 'COMPLETED', text: 'Phản hồi an toàn' };
  }};
  const app = await setup(t, {}, 1000000, { aiProvider: provider });
  const secret = '0901234567-sensitive';

  for (const body of [
    {},
    { text: '' },
    { text: 'x'.repeat(2001) },
    { text: 'hello', contacts: [secret] },
    { text: 'hello', location: { latitude: 10.1, longitude: 106.1 } },
    { text: 'hello', unknown: secret },
  ]) {
    const result = await app.request('/ai/text', 'POST', body);
    assert.equal(result.status, 400);
    assert.equal(result.error.code, 'VALIDATION_ERROR');
    assert.doesNotMatch(result.error.message, /0901234567-sensitive|10\.1|106\.1/);
  }
  assert.deepEqual(calls, []);

  const success = await app.request('/ai/text', 'POST', { text: 'Chỉ văn bản này' });
  assert.equal(success.status, 200);
  assert.deepEqual(success.data, { status: 'COMPLETED', text: 'Phản hồi an toàn' });
  assert.deepEqual(calls, ['Chỉ văn bản này']);
});

test('AI service bounds provider time and output and hides provider failures', async t => {
  const timeoutApp = await setup(t, { AI_REQUEST_TIMEOUT_MS: 20 }, 1000000, {
    aiProvider: { name: 'FAKE', configured: true, requestText: () => new Promise(() => {}) },
  });
  const started = Date.now();
  const timedOut = await timeoutApp.request('/ai/text', 'POST', { text: 'timeout' });
  assert.equal(timedOut.status, 503);
  assert.equal(timedOut.error.code, 'SERVICE_UNAVAILABLE');
  assert.ok(Date.now() - started < 500);

  const longApp = await setup(t, { AI_MAX_OUTPUT_CHARS: 40 }, 1000000, {
    aiProvider: { name: 'FAKE', configured: true, requestText: async () => ({ status: 'COMPLETED', text: 'a'.repeat(100) }) },
  });
  const bounded = await longApp.request('/ai/text', 'POST', { text: 'bounded' });
  assert.equal(bounded.status, 200);
  assert.equal(bounded.data.text.length, 40);

  const failedApp = await setup(t, {}, 1000000, {
    aiProvider: { name: 'FAKE', configured: true, requestText: async () => { throw new Error('provider-secret-detail'); } },
  });
  const failed = await failedApp.request('/ai/text', 'POST', { text: 'failure' });
  assert.equal(failed.status, 503);
  assert.equal(failed.error.code, 'SERVICE_UNAVAILABLE');
  assert.doesNotMatch(failed.error.message, /provider-secret-detail/);
});

test('AI service pure boundary enforces validation, deadline, and output bound without HTTP', async () => {
  const base={AI_MAX_INPUT_CHARS:20,AI_MAX_OUTPUT_CHARS:4,AI_REQUEST_TIMEOUT_MS:10};
  const calls=[];
  const service=aiTextService({config:base,aiProvider:{configured:true,requestText:async text=>{calls.push(text);return {status:'COMPLETED',text:'abcdef'};}}});
  assert.deepEqual(await service.request({text:'hello'}),{status:'COMPLETED',text:'abcd'});
  assert.deepEqual(calls,['hello']);
  await assert.rejects(service.request({text:'hello',contacts:['private']}),error=>error.code==='VALIDATION_ERROR');
  assert.deepEqual(calls,['hello']);
  const timeout=aiTextService({config:base,aiProvider:{configured:true,requestText:()=>new Promise(()=>{})}});
  await assert.rejects(timeout.request({text:'hello'}),error=>error.code==='SERVICE_UNAVAILABLE');
  const disabled=aiTextService({config:base,aiProvider:{configured:false}});
  await assert.rejects(disabled.request({text:'hello'}),error=>error.code==='NOT_CONFIGURED');
});

test('router exposes only POST /api/v1/ai/text to the AI action', async () => {
  const calls=[];
  const actions={aiText:async body=>{calls.push(body);return {data:{status:'COMPLETED',text:'ok'}};}};
  const body={text:'hello'};
  assert.deepEqual(await route('POST',new URL('http://localhost/api/v1/ai/text'),actions,body,{}),{data:{status:'COMPLETED',text:'ok'}});
  assert.deepEqual(calls,[body]);
  assert.throws(()=>route('GET',new URL('http://localhost/api/v1/ai/text'),actions,{},{}),error=>error.code==='RESOURCE_NOT_FOUND');
});

test('OpenAI-compatible provider sends the minimal Responses payload with env-supplied configuration', async () => {
  let captured;
  const provider = openAiCompatibleTextProvider({
    AI_PROVIDER: 'OPENAI_COMPATIBLE',
    AI_BASE_URL: 'https://provider.invalid/v1',
    AI_MODEL: 'test-model',
    AI_API_KEY: 'test-placeholder-not-a-secret',
    AI_MAX_OUTPUT_TOKENS: 64,
    AI_PROVIDER_RESPONSE_LIMIT_BYTES: 4096,
  }, {
    fetchImpl: async (url, init) => {
      captured = { url, init };
      return new Response(JSON.stringify({
        output: [{ type: 'message', content: [{ type: 'output_text', text: 'Kết quả' }] }],
      }), { status: 200, headers: { 'content-type': 'application/json' } });
    },
  });

  const result = await provider.requestText('Nội dung', { signal: new AbortController().signal });

  assert.deepEqual(result, { status: 'COMPLETED', text: 'Kết quả' });
  assert.equal(captured.url, 'https://provider.invalid/v1/responses');
  assert.equal(captured.init.method, 'POST');
  assert.equal(captured.init.headers.authorization, 'Bearer test-placeholder-not-a-secret');
  assert.deepEqual(JSON.parse(captured.init.body), {
    model: 'test-model', input: 'Nội dung', max_output_tokens: 64, store: false,
  });
});

test('OpenAI-compatible provider cancels ignored or oversized upstream response bodies', async () => {
  const config = {
    AI_PROVIDER: 'OPENAI_COMPATIBLE', AI_BASE_URL: 'https://provider.invalid/v1', AI_MODEL: 'test-model',
    AI_API_KEY: 'test-placeholder-not-a-secret', AI_MAX_OUTPUT_TOKENS: 64, AI_PROVIDER_RESPONSE_LIMIT_BYTES: 8,
  };
  let rejectedCancelled = false;
  const rejected = openAiCompatibleTextProvider(config, { fetchImpl: async () => ({
    ok: false,
    headers: new Headers(),
    body: { cancel: async () => { rejectedCancelled = true; } },
  }) });
  assert.deepEqual(await rejected.requestText('text'), { status: 'UNAVAILABLE' });
  assert.equal(rejectedCancelled, true);

  let oversizedCancelled = false;
  const oversized = openAiCompatibleTextProvider(config, { fetchImpl: async () => ({
    ok: true,
    headers: new Headers({ 'content-length': '9' }),
    body: { cancel: async () => { oversizedCancelled = true; } },
  }) });
  await assert.rejects(oversized.requestText('text'));
  assert.equal(oversizedCancelled, true);
});
