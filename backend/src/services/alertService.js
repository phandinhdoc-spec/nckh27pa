import { alertOutboxService } from './alertOutboxService.js';
import * as v from './validation.js';
import { ApiError, invalid, missing, conflict } from './errors.js';
import { transaction } from '../repositories/db.js';
import { eventRepository } from '../repositories/eventRepository.js';
import { alertRepository } from '../repositories/alertRepository.js';

export function alertService({ db, config, now, watchdog }) {
  const events = eventRepository(db);
  const repo = alertRepository(db);
  const outbox = alertOutboxService(db);

  function result(id) {
    const event = events.get(id);
    return {
      ...event,
      acknowledgedAtMs: event.acknowledgement.acknowledgedAtMs,
      caregiverAcknowledged: event.acknowledgement.acknowledged,
    };
  }

  function manualEvent(b, time) {
    return {
      eventId: b.eventId, deviceId: b.deviceId, userId: b.userId,
      sequenceNumber: 0, timestampMs: b.timestampMs, eventType: 'MANUAL_SOS',
      severity: 'CRITICAL', alertState: 'MONITORING',
      sensorSource: b.triggerSource === 'MANUAL_APP_BUTTON' ? 'PHONE' : 'ESP32',
      peakAccelerationMs2: null, orientationChangeDeg: null, altitudeDeltaM: null,
      sosButtonPressed: true, confidencePercent: 100, outcome: 'PENDING', response: 'UNKNOWN',
      watchdogDeadlineMs: null, watchdogActive: false, dispatchStatus: 'NONE',
      eligibleContactsCount: 0, location: v.location(b.location),
      createdAtMs: time, lastUpdatedMs: time,
    };
  }

  return {
    active(query) {
      const filters = {};
      for (const key of ['deviceId', 'userId']) {
        if (query.has(key)) filters[key] = v.string(query.get(key), key);
      }
      if (!Object.keys(filters).length) invalid('deviceId or userId is required');
      const row = repo.latest(filters);
      if (!row && !repo.known(filters)) missing();
      const active = row && row.alert_state !== 'MONITORING';
      return {
        userId: row?.user_id ?? filters.userId ?? null,
        deviceId: row?.device_id ?? filters.deviceId ?? null,
        alertState: active ? row.alert_state : 'MONITORING',
        activeEventId: active ? row.event_id : null,
        remainingMs: row?.alert_state === 'VERIFYING'
          ? Math.max(0, row.watchdog_deadline_ms - config.WATCHDOG_GRACE_MS - now()) : 0,
        response: row?.response ?? 'UNKNOWN',
        caregiverAcknowledged: !!row?.acknowledged,
        lastUpdatedMs: row?.updated_at_ms ?? now(),
      };
    },

    execute(action, b) {
      v.string(b.eventId, 'eventId', 64);
      v.integer(b.timestampMs, 'timestampMs');
      if (action === 'acknowledge' || action === 'resolve') {
        const actor = action === 'acknowledge' ? 'acknowledgedBy' : 'resolvedBy';
        v.string(b[actor], actor);
        const note = action === 'acknowledge' ? b.note : b.resolutionNote;
        if (note !== undefined) v.string(note, 'note', 2000);
        return transaction(db, () => {
          const event = events.get(b.eventId);
          if (!event) missing();
          if (repo.action(b.eventId, action)) return result(b.eventId);
          if (event.alertState !== 'AWAITING_HELP') conflict();
          const time = now();
          if (action === 'acknowledge') repo.acknowledge(b.eventId, b, time);
          else repo.resolve(b.eventId, b, time);
          repo.recordAction(b.eventId, action, time);
          return result(b.eventId);
        });
      }

      v.string(b.deviceId, 'deviceId');
      v.string(b.userId, 'userId');
      if (action === 'cancel') {
        v.enumeration(b.reason, 'reason', ['SAFE']);
        if (b.note !== undefined) v.string(b.note, 'note', 2000);
      } else {
        v.enumeration(b.triggerSource, 'triggerSource', ['COUNTDOWN_TIMEOUT', 'MANUAL_APP_BUTTON', 'MANUAL_HARDWARE_BUTTON']);
        v.enumeration(b.response, 'response', b.triggerSource === 'COUNTDOWN_TIMEOUT' ? ['NO_RESPONSE'] : ['NEED_HELP']);
        v.location(b.location);
      }

      // Commit overdue escalation before considering a late cancel. An invalid
      // cancel must not roll back the watchdog's independently due transaction.
      watchdog.scan();
      return transaction(db, () => {
        let event = events.get(b.eventId);
        if (!event && action === 'sos' && b.triggerSource !== 'COUNTDOWN_TIMEOUT') {
          events.insert(manualEvent(b, now()), null);
          event = events.get(b.eventId);
        }
        if (!event || event.deviceId !== b.deviceId || event.userId !== b.userId) missing();
        if (repo.action(b.eventId, action)) return result(b.eventId);
        const time = now();
        if (event.watchdogActive && time >= event.watchdogDeadlineMs) {
          throw new ApiError(503, 'SERVICE_UNAVAILABLE', 'Watchdog escalation pending; retry required');
        }
        if (action === 'cancel') {
          if (!['SUSPECTED', 'VERIFYING'].includes(event.alertState)) conflict();
          repo.cancel(b.eventId, time);
        } else {
          if (event.outcome !== 'PENDING' || event.dispatchStatus !== 'NONE') conflict();
          if (b.triggerSource === 'COUNTDOWN_TIMEOUT' && !['VERIFYING', 'ALERTING', 'AWAITING_HELP'].includes(event.alertState)) conflict();
          const location = v.location(b.location) ?? event.location;
          const count = outbox.record({ ...event, response: b.response, location }, time);
          repo.sos(b.eventId, time, count, b.response, b.triggerSource, location);
        }
        repo.recordAction(b.eventId, action, time);
        return result(b.eventId);
      });
    },
  };
}
