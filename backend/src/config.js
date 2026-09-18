export function loadConfig(env = process.env, overrides = {}) {
  const defaults = { HOST: '0.0.0.0', PORT: 3000, DB_PATH: './data/fallsafe.db', COUNTDOWN_TIMEOUT_MS: 10000, WATCHDOG_GRACE_MS: 5000, HEARTBEAT_STALE_THRESHOLD_MS: 120000, WATCHDOG_SCAN_INTERVAL_MS: 1000, VOICE_PROVIDER: 'DISABLED', VOICE_MAX_ATTEMPTS_PER_CONTACT: 2, VOICE_MAX_CONTACTS: 5, AI_PROVIDER: 'DISABLED', AI_REQUEST_TIMEOUT_MS: 6000, AI_MAX_INPUT_CHARS: 2000, AI_MAX_OUTPUT_CHARS: 4000, AI_MAX_OUTPUT_TOKENS: 256, AI_PROVIDER_RESPONSE_LIMIT_BYTES: 32768 };
  const config = {};
  for (const [key, fallback] of Object.entries(defaults)) {
    const raw = overrides[key] ?? env[key] ?? fallback;
    if (typeof fallback === 'number' && (typeof raw === 'string' && !/^[0-9]+$/.test(raw))) throw new Error(`Invalid configuration: ${key}`);
    config[key] = typeof fallback === 'number' ? Number(raw) : raw;
    if (typeof fallback === 'number' && (!Number.isSafeInteger(config[key]) || config[key] < (key === 'WATCHDOG_SCAN_INTERVAL_MS' ? 1 : 0) || (key === 'PORT' && config[key] > 65535))) throw new Error(`Invalid configuration: ${key}`);
    if (typeof fallback === 'string' && (typeof raw !== 'string' || !raw.trim())) throw new Error(`Invalid configuration: ${key}`);
  }
  if (!['DISABLED','TWILIO'].includes(config.VOICE_PROVIDER)) throw new Error('Invalid configuration: VOICE_PROVIDER');
  if (!['DISABLED','OPENAI_COMPATIBLE'].includes(config.AI_PROVIDER)) throw new Error('Invalid configuration: AI_PROVIDER');
  for (const key of ['AI_REQUEST_TIMEOUT_MS','AI_MAX_INPUT_CHARS','AI_MAX_OUTPUT_CHARS','AI_MAX_OUTPUT_TOKENS','AI_PROVIDER_RESPONSE_LIMIT_BYTES']) {
    if (config[key] < 1) throw new Error(`Invalid configuration: ${key}`);
  }
  if (config.AI_REQUEST_TIMEOUT_MS > 15000 || config.AI_MAX_INPUT_CHARS > 10000 || config.AI_MAX_OUTPUT_CHARS > 16000 || config.AI_MAX_OUTPUT_TOKENS > 2048 || config.AI_PROVIDER_RESPONSE_LIMIT_BYTES > 262144) throw new Error('Invalid AI bounds configuration');
  const AI_BASE_URL=overrides.AI_BASE_URL??env.AI_BASE_URL??null;
  const AI_MODEL=overrides.AI_MODEL??env.AI_MODEL??null;
  const AI_API_KEY=overrides.AI_API_KEY??env.AI_API_KEY??null;
  if(config.AI_PROVIDER==='OPENAI_COMPATIBLE'&&[AI_BASE_URL,AI_MODEL,AI_API_KEY].every(value=>typeof value==='string'&&value.trim())) {
    let url;try{url=new URL(AI_BASE_URL);}catch{throw new Error('Invalid AI provider configuration');}
    if(url.protocol!=='https:'||url.username||url.password)throw new Error('Invalid AI provider configuration');
  }
  return Object.freeze({...config,AI_BASE_URL,AI_MODEL,AI_API_KEY,TWILIO_ACCOUNT_SID:overrides.TWILIO_ACCOUNT_SID??env.TWILIO_ACCOUNT_SID??null,TWILIO_AUTH_TOKEN:overrides.TWILIO_AUTH_TOKEN??env.TWILIO_AUTH_TOKEN??null,TWILIO_FROM_NUMBER:overrides.TWILIO_FROM_NUMBER??env.TWILIO_FROM_NUMBER??null,PUBLIC_CALLBACK_BASE_URL:overrides.PUBLIC_CALLBACK_BASE_URL??env.PUBLIC_CALLBACK_BASE_URL??null});
}
export const BODY_LIMIT_BYTES = 65536;
