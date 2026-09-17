const fields={deviceId:'device_id',sensorSource:'sensor_source',sequenceNumber:'sequence_number',timestampMs:'timestamp_ms',accelXMs2:'accel_x_ms2',accelYMs2:'accel_y_ms2',accelZMs2:'accel_z_ms2',gyroXDps:'gyro_x_dps',gyroYDps:'gyro_y_dps',gyroZDps:'gyro_z_dps',pressurePa:'pressure_pa',temperatureC:'temperature_c',altitudeDeltaM:'altitude_delta_m',batteryPercent:'battery_percent',batteryVoltageMv:'battery_voltage_mv',isCharging:'is_charging',sosButtonPressed:'sos_button_pressed',sensorQuality:'sensor_quality',motionState:'motion_state',fallRisk:'fall_risk'};
const locationFields={latitude:'latitude',longitude:'longitude',accuracyM:'accuracy_m',timestampMs:'location_timestamp_ms',locationMessage:'location_message'};
export function sensorRepository(db) {
 return {
  insert(b) {
   const entries=[...Object.entries(fields).map(([k,c])=>[c,typeof b[k]==='boolean'?+b[k]:b[k]??null]),...Object.entries(locationFields).map(([k,c])=>[c,b.location?.[k]??null])];
   db.prepare(`INSERT INTO sensor_readings(${entries.map(e=>e[0])}) VALUES(${entries.map(()=>'?')}) ON CONFLICT(device_id,sequence_number) DO NOTHING`).run(...entries.map(e=>e[1]));
  },
  latest(id) {
   const row=db.prepare('SELECT * FROM sensor_readings WHERE device_id=? ORDER BY timestamp_ms DESC,id DESC LIMIT 1').get(id);
   if(!row) return null;
   const result=Object.fromEntries(Object.entries(fields).map(([k,c])=>[k,row[c]]));
   result.isCharging=!!result.isCharging; result.sosButtonPressed=!!result.sosButtonPressed;
   for(const key of ['motionState','fallRisk']) if(result[key]===null) delete result[key];
   result.location=row.latitude===null?null:Object.fromEntries(Object.entries(locationFields).map(([k,c])=>[k,row[c]]));
   return result;
  },
 };
}
