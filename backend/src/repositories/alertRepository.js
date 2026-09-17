export function alertRepository(db) {
 return {
  due: time=>db.prepare("SELECT event_id FROM safety_events WHERE alert_state='VERIFYING' AND watchdog_active=1 AND watchdog_deadline_ms<=?").all(time),
  timeout: id=>db.prepare("UPDATE safety_events SET outcome='TIMEOUT_ESCALATED' WHERE event_id=?").run(id),
  action: (id,action)=>db.prepare('SELECT * FROM alert_actions WHERE event_id=? AND action=?').get(id,action),
  recordAction: (id,action,time)=>db.prepare('INSERT INTO alert_actions(event_id,action,performed_at_ms) VALUES(?,?,?)').run(id,action,time),
  cancel: (id,time)=>db.prepare("UPDATE safety_events SET alert_state='MONITORING',outcome='CANCELLED_SAFE',response='SAFE',watchdog_active=0,cancelled_at_ms=?,updated_at_ms=? WHERE event_id=?").run(time,time,id),
  acknowledge(id,b,time) {db.prepare('UPDATE safety_events SET acknowledged=1,acknowledged_by=?,acknowledged_at_ms=?,acknowledgement_note=?,updated_at_ms=? WHERE event_id=?').run(b.acknowledgedBy,b.timestampMs,b.note??null,time,id);},
  resolve(id,b,time) {db.prepare("UPDATE safety_events SET alert_state='MONITORING',outcome='RESOLVED_ACKNOWLEDGED',watchdog_active=0,resolved_at_ms=?,resolved_by=?,resolution_note=?,updated_at_ms=? WHERE event_id=?").run(b.timestampMs,b.resolvedBy,b.resolutionNote??null,time,id);},
  sos(id,time,count,response,trigger,location) {
   db.prepare("UPDATE safety_events SET alert_state='AWAITING_HELP',watchdog_active=0,dispatch_status='RECORDED',eligible_contacts_count=?,recorded_at_ms=?,response=?,trigger_source=?,updated_at_ms=? WHERE event_id=?").run(count,time,response,trigger,time,id);
   if(location) db.prepare('UPDATE safety_events SET latitude=?,longitude=?,accuracy_m=?,location_timestamp_ms=?,location_message=? WHERE event_id=?').run(location.latitude,location.longitude,location.accuracyM,location.timestampMs,location.locationMessage,id);
  },
  latest(filters) {
   const where=[],values=[];for(const [key,col] of [['deviceId','device_id'],['userId','user_id']]) if(filters[key]!==undefined){where.push(`${col}=?`);values.push(filters[key]);}
   return db.prepare(`SELECT * FROM safety_events WHERE ${where.join(' AND ')} ORDER BY (alert_state!='MONITORING') DESC,updated_at_ms DESC,rowid DESC LIMIT 1`).get(...values);
  },
  known(filters) {
   if(filters.deviceId) return !!db.prepare('SELECT 1 FROM devices WHERE device_id=?').get(filters.deviceId);
   return !!db.prepare('SELECT 1 FROM emergency_contacts WHERE user_id=? LIMIT 1').get(filters.userId);
  },
 };
}
