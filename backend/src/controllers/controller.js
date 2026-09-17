import { alertService } from '../services/alertService.js';
import { contactService } from '../services/contactService.js';
import { eventService } from '../services/eventService.js';
import { sensorService } from '../services/sensorService.js';
import { deviceService } from '../services/deviceService.js';
import { systemStatus } from '../services/systemService.js';

export function controller(context) {
  const devices = deviceService(context);
  const sensors = sensorService(context);
  const events = eventService(context);
  const contacts = contactService(context);
  const alerts = alertService(context);
  return {
    alert: (action, body, query) => ({ data: action === 'active' ? alerts.active(query) : alerts.execute(action, body) }),
    contact: (...args) => contacts.execute(...args),
    event: (method, id, body, query) => method === 'POST'
      ? events.create(body) : { data: id ? events.get(id) : events.list(query) },
    sensor: (method, id, body) => ({ data: method === 'GET' ? sensors.latest(id) : sensors.ingest(body) }),
    device: (method, id, body, headers) => ({ data: method === 'GET'
      ? devices.get(id) : devices.heartbeat(id, body, headers['x-user-id']) }),
    system: () => ({ data: systemStatus(context.db, context.config, context.now(), context.started, context.watchdog) }),
  };
}

export function respond(res, status, data, timestamp, error = false) {
  res.writeHead(status, { 'content-type': 'application/json; charset=utf-8' });
  res.end(JSON.stringify(error ? { success: false, error: data, timestamp } : { success: true, data, timestamp }));
}
