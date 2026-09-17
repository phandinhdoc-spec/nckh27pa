import { ApiError } from './errors.js';
import { activeAlertsCount } from '../repositories/systemRepository.js';
export function systemStatus(db, config, now, started, watchdog) {
  if (!watchdog.isHealthy()) throw new ApiError(503, 'SERVICE_UNAVAILABLE', 'Watchdog storage scan failed; retry pending');
  return {
    status: 'HEALTHY', version: '1.0.0', serverTimeMs: now,
    uptimeSeconds: Math.max(0, Math.floor((now - started) / 1000)),
    heartbeatStaleThresholdMs: config.HEARTBEAT_STALE_THRESHOLD_MS,
    activeAlertsCount: activeAlertsCount(db),
  };
}
