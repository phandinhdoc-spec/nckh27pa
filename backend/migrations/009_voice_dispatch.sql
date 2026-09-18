ALTER TABLE emergency_contacts ADD COLUMN call_priority INTEGER NOT NULL DEFAULT 1000;

CREATE TABLE voice_dispatches (
  event_id TEXT PRIMARY KEY,
  provider TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'PENDING',
  acknowledged INTEGER NOT NULL DEFAULT 0,
  acknowledged_contact_id TEXT,
  acknowledged_at_ms INTEGER,
  created_at_ms INTEGER NOT NULL,
  updated_at_ms INTEGER NOT NULL
);

CREATE TABLE voice_attempts (
  event_id TEXT NOT NULL,
  contact_id TEXT NOT NULL,
  contact_name TEXT NOT NULL,
  phone TEXT NOT NULL,
  call_priority INTEGER NOT NULL,
  attempt INTEGER NOT NULL,
  status TEXT NOT NULL,
  provider_call_id TEXT,
  created_at_ms INTEGER NOT NULL,
  updated_at_ms INTEGER NOT NULL,
  PRIMARY KEY(event_id, contact_id, attempt)
);
CREATE UNIQUE INDEX idx_voice_provider_call ON voice_attempts(provider_call_id) WHERE provider_call_id IS NOT NULL;
