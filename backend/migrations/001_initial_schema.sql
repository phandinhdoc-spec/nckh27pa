-- 1. Bảng Thiết bị
-- Lưu ý: isConnected là trường suy diễn động từ last_heartbeat_ms, không lưu cờ tĩnh trong bảng
CREATE TABLE IF NOT EXISTS devices (
    device_id TEXT PRIMARY KEY,
    device_type TEXT NOT NULL DEFAULT 'ESP32',
    firmware_version TEXT,
    battery_percent INTEGER,
    battery_voltage_mv INTEGER,
    is_charging INTEGER DEFAULT 0,
    imu_status TEXT DEFAULT 'OK',
    barometer_status TEXT DEFAULT 'OK',
    gnss_status TEXT DEFAULT 'UNAVAILABLE',
    buffer_usage_percent INTEGER DEFAULT 0,
    last_error_code TEXT,
    last_heartbeat_ms INTEGER NOT NULL
);

-- 2. Bảng Dữ liệu Cảm biến gần nhất & Lịch sử
CREATE TABLE IF NOT EXISTS sensor_readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    sensor_source TEXT NOT NULL,
    sequence_number INTEGER NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    accel_x_ms2 REAL NOT NULL,
    accel_y_ms2 REAL NOT NULL,
    accel_z_ms2 REAL NOT NULL,
    gyro_x_dps REAL,
    gyro_y_dps REAL,
    gyro_z_dps REAL,
    pressure_pa REAL,
    temperature_c REAL,
    altitude_delta_m REAL,
    battery_percent INTEGER NOT NULL,
    battery_voltage_mv INTEGER,
    is_charging INTEGER NOT NULL,
    sos_button_pressed INTEGER NOT NULL,
    sensor_quality INTEGER NOT NULL,
    latitude REAL,
    longitude REAL,
    accuracy_m REAL,
    location_message TEXT,
    UNIQUE(device_id, sequence_number)
);

-- 3. Bảng Sự kiện Cảnh báo & Vòng đời Sự cố
CREATE TABLE IF NOT EXISTS safety_events (
    event_id TEXT PRIMARY KEY,
    device_id TEXT NOT NULL,
    user_id TEXT NOT NULL,
    sequence_number INTEGER NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    severity TEXT NOT NULL,
    alert_state TEXT NOT NULL,
    sensor_source TEXT NOT NULL,
    peak_acceleration_ms2 REAL,
    orientation_change_deg REAL,
    altitude_delta_m REAL,
    sos_button_pressed INTEGER NOT NULL,
    confidence_percent INTEGER NOT NULL,
    outcome TEXT NOT NULL DEFAULT 'PENDING',
    response TEXT NOT NULL DEFAULT 'UNKNOWN',
    watchdog_deadline_ms INTEGER,
    watchdog_active INTEGER NOT NULL DEFAULT 0,
    dispatch_status TEXT NOT NULL DEFAULT 'NONE',
    eligible_contacts_count INTEGER NOT NULL DEFAULT 0,
    latitude REAL,
    longitude REAL,
    accuracy_m REAL,
    location_message TEXT,
    acknowledged INTEGER DEFAULT 0,
    acknowledged_by TEXT,
    acknowledged_at_ms INTEGER,
    acknowledgement_note TEXT,
    created_at_ms INTEGER NOT NULL,
    updated_at_ms INTEGER NOT NULL
);

-- 4. Bảng Danh bạ Khẩn cấp theo Người dùng
CREATE TABLE IF NOT EXISTS emergency_contacts (
    id TEXT NOT NULL,
    user_id TEXT NOT NULL,
    name TEXT NOT NULL,
    relationship TEXT NOT NULL,
    phone TEXT NOT NULL,
    receive_sos INTEGER NOT NULL DEFAULT 1,
    is_primary INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (user_id, id)
);
CREATE INDEX IF NOT EXISTS idx_contacts_user_id ON emergency_contacts(user_id);

-- 5. Bảng Outbox Cảnh báo An toàn Phía Máy chủ (Server-side Sink)
CREATE TABLE IF NOT EXISTS alert_outbox (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_id TEXT NOT NULL,
    user_id TEXT NOT NULL,
    contact_id TEXT NOT NULL,
    contact_name TEXT NOT NULL,
    phone TEXT NOT NULL,
    message_content TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'RECORDED',
    created_at_ms INTEGER NOT NULL,
    dispatched_at_ms INTEGER
);
