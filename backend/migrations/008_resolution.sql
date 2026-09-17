ALTER TABLE safety_events ADD COLUMN resolved_at_ms INTEGER;
ALTER TABLE safety_events ADD COLUMN resolved_by TEXT;
ALTER TABLE safety_events ADD COLUMN resolution_note TEXT;
