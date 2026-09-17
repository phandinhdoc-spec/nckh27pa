ALTER TABLE safety_events ADD COLUMN cancelled_at_ms INTEGER;
CREATE TABLE alert_actions (
 event_id TEXT NOT NULL REFERENCES safety_events(event_id),
 action TEXT NOT NULL,
 performed_at_ms INTEGER NOT NULL,
 PRIMARY KEY(event_id,action)
);
