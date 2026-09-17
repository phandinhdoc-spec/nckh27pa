CREATE UNIQUE INDEX idx_one_primary_per_user ON emergency_contacts(user_id) WHERE is_primary=1;
