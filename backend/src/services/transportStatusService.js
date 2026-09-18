import * as v from './validation.js';
import { missing, invalid } from './errors.js';

const RANK = { FAILED: 1, SENT: 2, DELIVERED: 3 };

export function transportStatusService({ db, now }) {
  function report(eventId, body, headers) {
    v.string(eventId, 'eventId', 64);
    v.string(body.contactId, 'contactId', 128);
    v.enumeration(body.channel, 'channel', ['SMS']);
    v.enumeration(body.status, 'status', ['SENT', 'DELIVERED', 'FAILED']);
    v.integer(body.timestampMs, 'timestampMs');
    if (body.detail !== undefined && body.detail !== null) v.string(body.detail, 'detail', 1000);
    const event = db.prepare('SELECT device_id,user_id FROM safety_events WHERE event_id=?').get(eventId);
    if (!event) missing();
    if (headers['x-device-id'] !== event.device_id || headers['x-user-id'] !== event.user_id)
      invalid('Authenticated device/user does not own this event');
    const outbox = db.prepare('SELECT recipients_json FROM alert_outbox WHERE event_id=?').get(eventId);
    if (!outbox || !JSON.parse(outbox.recipients_json).some(contact => contact.id === body.contactId)) missing();
    const existing = db.prepare('SELECT * FROM transport_status_reports WHERE event_id=? AND contact_id=? AND channel=?')
      .get(eventId, body.contactId, body.channel);
    let status = body.status;
    let detail = body.detail ?? null;
    let clientTimestamp = body.timestampMs;
    if (existing) {
      if (RANK[existing.status] > RANK[status]) status = existing.status;
      if (body.timestampMs < existing.client_timestamp_ms) {
        status = existing.status; detail = existing.detail; clientTimestamp = existing.client_timestamp_ms;
      } else if (status === existing.status && detail === null) detail = existing.detail;
    }
    db.prepare(`INSERT INTO transport_status_reports(event_id,contact_id,channel,status,detail,client_timestamp_ms,updated_at_ms)
      VALUES(?,?,?,?,?,?,?) ON CONFLICT(event_id,contact_id,channel) DO UPDATE SET
      status=excluded.status,detail=excluded.detail,client_timestamp_ms=excluded.client_timestamp_ms,updated_at_ms=excluded.updated_at_ms`)
      .run(eventId, body.contactId, body.channel, status, detail, clientTimestamp, now());
    return { eventId, contactId: body.contactId, channel: body.channel, status, detail, timestampMs: clientTimestamp };
  }

  function aggregateSms(eventId) {
    const rows = db.prepare("SELECT status FROM transport_status_reports WHERE event_id=? AND channel='SMS'").all(eventId);
    if (rows.some(row => row.status === 'DELIVERED')) return 'DELIVERED';
    if (rows.some(row => row.status === 'SENT')) return 'SENT';
    if (rows.length && rows.every(row => row.status === 'FAILED')) return 'FAILED';
    return 'RECORDED';
  }

  return { report, aggregateSms };
}
