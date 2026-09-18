ALTER TABLE safety_events ADD COLUMN display_name TEXT NOT NULL DEFAULT 'Người dùng FallSafe';

CREATE TABLE transport_status_reports (
  event_id TEXT NOT NULL,
  contact_id TEXT NOT NULL,
  channel TEXT NOT NULL,
  status TEXT NOT NULL,
  detail TEXT,
  client_timestamp_ms INTEGER NOT NULL,
  updated_at_ms INTEGER NOT NULL,
  PRIMARY KEY(event_id, contact_id, channel),
  FOREIGN KEY(event_id) REFERENCES safety_events(event_id)
);
