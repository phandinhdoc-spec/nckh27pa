export function deviceRepository(db) {
 return {
  get: id => db.prepare('SELECT * FROM devices WHERE device_id = ?').get(id),
  save(id,b,now) {
   db.prepare(`INSERT INTO devices(device_id,device_type,firmware_version,timestamp_ms,uptime_seconds,battery_percent,battery_voltage_mv,is_charging,imu_status,barometer_status,gnss_status,buffer_usage_percent,last_error_code,last_heartbeat_ms)
   VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(device_id) DO UPDATE SET device_type=excluded.device_type,firmware_version=excluded.firmware_version,timestamp_ms=excluded.timestamp_ms,uptime_seconds=excluded.uptime_seconds,battery_percent=excluded.battery_percent,battery_voltage_mv=excluded.battery_voltage_mv,is_charging=excluded.is_charging,imu_status=excluded.imu_status,barometer_status=excluded.barometer_status,gnss_status=excluded.gnss_status,buffer_usage_percent=excluded.buffer_usage_percent,last_error_code=excluded.last_error_code,last_heartbeat_ms=excluded.last_heartbeat_ms`).run(id,b.deviceType ?? this.get(id)?.device_type ?? 'ESP32',b.firmwareVersion,b.timestampMs,b.uptimeSeconds,b.batteryPercent,b.batteryVoltageMv,+b.isCharging,b.imuStatus,b.barometerStatus,b.gnssStatus,b.bufferUsagePercent,b.lastErrorCode,now);
  },
  lowBattery(id,userId,b,now,eventId) {
   db.prepare(`INSERT INTO safety_events(event_id,device_id,user_id,sequence_number,timestamp_ms,event_type,severity,alert_state,sensor_source,sos_button_pressed,confidence_percent,created_at_ms,updated_at_ms) VALUES(?,?,?,0,?,'LOW_BATTERY','WARNING','MONITORING',?,0,100,?,?)`).run(eventId,id,userId,b.timestampMs,this.get(id).device_type,now,now);
  },
 };
}
