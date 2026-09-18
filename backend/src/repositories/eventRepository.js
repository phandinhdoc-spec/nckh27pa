const fields={eventId:'event_id',deviceId:'device_id',userId:'user_id',displayName:'display_name',sequenceNumber:'sequence_number',timestampMs:'timestamp_ms',eventType:'event_type',severity:'severity',alertState:'alert_state',sensorSource:'sensor_source',peakAccelerationMs2:'peak_acceleration_ms2',orientationChangeDeg:'orientation_change_deg',altitudeDeltaM:'altitude_delta_m',sosButtonPressed:'sos_button_pressed',confidencePercent:'confidence_percent',outcome:'outcome',response:'response',watchdogDeadlineMs:'watchdog_deadline_ms',watchdogActive:'watchdog_active',dispatchStatus:'dispatch_status',eligibleContactsCount:'eligible_contacts_count',createdAtMs:'created_at_ms',lastUpdatedMs:'updated_at_ms'};
export function eventFromRow(row) {
 if(!row) return null;
 const result=Object.fromEntries(Object.entries(fields).map(([key,column])=>[key,row[column]]));
 result.sosButtonPressed=!!result.sosButtonPressed; result.watchdogActive=!!result.watchdogActive; result.active=result.alertState!=='MONITORING';
 result.location=row.latitude===null?null:{latitude:row.latitude,longitude:row.longitude,accuracyM:row.accuracy_m,timestampMs:row.location_timestamp_ms,locationMessage:row.location_message};
 result.acknowledgement={acknowledged:!!row.acknowledged,acknowledgedBy:row.acknowledged_by,acknowledgedAtMs:row.acknowledged_at_ms,note:row.acknowledgement_note};
 result.cancelledAtMs=row.cancelled_at_ms;result.recordedAtMs=row.recorded_at_ms;result.resolvedAtMs=row.resolved_at_ms;result.resolvedBy=row.resolved_by;result.resolutionNote=row.resolution_note;result.status=row.outcome==='RESOLVED_ACKNOWLEDGED'||row.acknowledged?'ACKNOWLEDGED':'PENDING';
 return result;
}
export function eventRepository(db) {
 return {
  row: id=>db.prepare('SELECT * FROM safety_events WHERE event_id=?').get(id),
  get(id) {return eventFromRow(this.row(id));},
  insert(e,original) {
   const values=Object.entries(fields).map(([k,c])=>[c,typeof e[k]==='boolean'?+e[k]:e[k]??null]);
   values.push(['latitude',e.location?.latitude??null],['longitude',e.location?.longitude??null],['accuracy_m',e.location?.accuracyM??null],['location_timestamp_ms',e.location?.timestampMs??null],['location_message',e.location?.locationMessage??null],['original_json',JSON.stringify(original)]);
   db.prepare(`INSERT INTO safety_events(${values.map(v=>v[0])}) VALUES(${values.map(()=>'?')})`).run(...values.map(v=>v[1]));
  },
  list({userId,deviceId,limit}) {
   const where=[],params=[];
   if(userId!==undefined){where.push('user_id=?');params.push(userId);}
   if(deviceId!==undefined){where.push('device_id=?');params.push(deviceId);}
   return db.prepare(`SELECT * FROM safety_events ${where.length?'WHERE '+where.join(' AND '):''} ORDER BY timestamp_ms DESC,rowid DESC LIMIT ?`).all(...params,limit).map(eventFromRow);
  },
 };
}
