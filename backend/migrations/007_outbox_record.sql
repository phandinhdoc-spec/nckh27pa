ALTER TABLE safety_events ADD COLUMN recorded_at_ms INTEGER;
ALTER TABLE safety_events ADD COLUMN trigger_source TEXT;
ALTER TABLE alert_outbox ADD COLUMN recipients_json TEXT NOT NULL DEFAULT '[]';
CREATE UNIQUE INDEX idx_outbox_event ON alert_outbox(event_id);
