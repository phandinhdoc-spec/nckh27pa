ALTER TABLE sensor_readings ADD COLUMN location_timestamp_ms INTEGER;
ALTER TABLE sensor_readings ADD COLUMN motion_state TEXT;
ALTER TABLE sensor_readings ADD COLUMN fall_risk TEXT;
CREATE INDEX idx_sensor_latest ON sensor_readings(device_id,timestamp_ms DESC,id DESC);
