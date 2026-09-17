ALTER TABLE safety_events ADD COLUMN location_timestamp_ms INTEGER;
ALTER TABLE safety_events ADD COLUMN original_json TEXT;
CREATE INDEX idx_event_history ON safety_events(timestamp_ms DESC);
CREATE INDEX idx_watchdog_due ON safety_events(watchdog_active,alert_state,watchdog_deadline_ms);
