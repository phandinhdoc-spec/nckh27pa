export function loadConfig(env = process.env, overrides = {}) {
  const defaults = { HOST: '0.0.0.0', PORT: 3000, DB_PATH: './data/fallsafe.db', COUNTDOWN_TIMEOUT_MS: 10000, WATCHDOG_GRACE_MS: 5000, HEARTBEAT_STALE_THRESHOLD_MS: 120000, WATCHDOG_SCAN_INTERVAL_MS: 1000 };
  const config = {};
  for (const [key, fallback] of Object.entries(defaults)) {
    const raw = overrides[key] ?? env[key] ?? fallback;
    if (typeof fallback === 'number' && (typeof raw === 'string' && !/^[0-9]+$/.test(raw))) throw new Error(`Invalid configuration: ${key}`);
    config[key] = typeof fallback === 'number' ? Number(raw) : raw;
    if (typeof fallback === 'number' && (!Number.isSafeInteger(config[key]) || config[key] < (key === 'WATCHDOG_SCAN_INTERVAL_MS' ? 1 : 0) || (key === 'PORT' && config[key] > 65535))) throw new Error(`Invalid configuration: ${key}`);
    if (typeof fallback === 'string' && (typeof raw !== 'string' || !raw.trim())) throw new Error(`Invalid configuration: ${key}`);
  }
  return Object.freeze(config);
}
export const BODY_LIMIT_BYTES = 65536;
