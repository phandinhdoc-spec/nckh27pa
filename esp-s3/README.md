# NCKH27PA — ESP32-S3 Super Mini Firmware (Official Product)

Firmware chính thức cho thiết bị đeo phát hiện ngã (fall detection product) dựa trên vi điều khiển **ESP32-S3 Super Mini**.

---

## 1. Phần cứng & Board FQBN

- **MCU**: ESP32-S3 Super Mini (Dual-core Xtensa LX7, 4MB Flash, không PSRAM).
- **Cảm biến IMU**: MPU6050 (GY-521) trên I2C Bus 0.
- **Cảm biến khí áp / độ cao**: MS5611-01BA03 (GY-63) trên I2C Bus 1.
- **Arduino Core**: `esp32:esp32` (core version 3.3.x).
- **Board FQBN**:
  - Cấu hình (a) USB-CDC native (khuyên dùng khi nạp qua cổng USB Super Mini):
    `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled`
  - Cấu hình (b) USB-UART tiêu chuẩn (không CDC):
    `esp32:esp32:esp32s3`

---

## 2. Sơ lược chân kết nối (Pinout)

| Ngoại vi | Chân ESP32-S3 | Trạng thái xác thực | Ghi chú |
| :--- | :--- | :--- | :--- |
| **MPU6050 SDA** | `GPIO 8` | **CONFIRMED** | Bus 0 (`Wire`), 400 kHz |
| **MPU6050 SCL** | `GPIO 9` | **CONFIRMED** | Bus 0 (`Wire`), 400 kHz |
| **MS5611 SDA** | `GPIO 7` | **CONFIRMED** | Bus 1 (`Wire1`), 400 kHz |
| **MS5611 SCL** | `GPIO 6` | **CONFIRMED** | Bus 1 (`Wire1`), 400 kHz |
| **Nút SOS** | `GPIO 4` | `TODO(HW)` | Active LOW, giữ >= 2s để báo động tức thì |
| **Nút CANCEL** | `GPIO 5` | `TODO(HW)` | Active LOW, nhấn giữ >= 300ms để hủy báo động |
| **Còi / Rung** | `GPIO 1` | `TODO(HW)` | PWM / High level output |
| **LED Status** | `GPIO 8` | `TODO(HW)` | On-board LED ESP32-S3 Super Mini |
| **LED Pin 1..3**| `GPIO 9, 10, 11` | `TODO(HW)` | Dải LED hiển thị pin |
| **Battery ADC** | `GPIO 12` | `TODO(HW)` | Cầu phân áp đo điện áp pin LiPo |

*Chi tiết bảng chân và nguồn gốc: xem [PINMAP.md](PINMAP.md).*

---

## 3. Hướng dẫn Build & Flash

Dùng `arduino-cli`:

```bash
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"

# Build cấu hình (a) - USB-CDC:
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" \
  esp-s3

# Build cấu hình (b) - USB-UART:
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 \
  esp-s3

# Nạp qua cổng serial (thay /dev/cu.usbmodem... bằng cổng thực tế):
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml upload \
  -p /dev/cu.usbmodem1101 \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" \
  esp-s3
```

---

## 4. Các chế độ Serial Console

Baudrate mặc định: `115200`.

- **Human Mode (Mặc định)**: In dòng trạng thái cô đọng tốc độ 4 dòng/giây (250 ms), không làm nghẽn terminal:
  `[T=012345 ms] State: MONITORING     | |a|:  9.81 m/s^2 | AltDelta:   0.45 m | BLE: ADV`
- **CSV Mode**: In dòng dữ liệu phân cách bởi dấu phẩy phù hợp vẽ đồ thị / phân tích log:
  `timestamp_ms,ax,ay,az,mag,gx,gy,gz,pressure_pa,altitudeDeltaM,state,event`

### Các phím lệnh tương tác (gõ vào Serial Monitor):
- `r` / `R`: Bật / tắt chế độ Raw CSV.
- `t` / `T`: In bảng Profile và ngưỡng phát hiện ngã (nguồn Android vs suy luận).
- `c` / `C`: Chạy lại quy trình tự hiệu chuẩn (Calibration) & Self-test.
- `s` / `S`: In trạng thái hệ thống, bộ đếm lỗi, mẫu đọc và tài nguyên Heap.
- `h` / `H`: In trợ giúp danh sách lệnh.

---

## 5. Giao thức BLE GATT (FALLSAFE-xxxx) — Chuẩn hóa §8 & §6.2

- **Service UUID**: `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001`
- **Characteristics & Gói tin**:
  - `Stream (0002)` — `Esp32SensorPacket` (Notify): Luồng dữ liệu cảm biến thời gian thực:
    `protocolVersion` (1), `deviceId`, `sequenceNumber`, `timestampMs`, `accelXMs2`, `accelYMs2`, `accelZMs2`, `gyroXDps`, `gyroYDps`, `gyroZDps`, `pressurePa` (null nếu mất cảm biến), `temperatureC`, `altitudeDeltaM` (độ cao so mốc m), `batteryPercent` (-1 khi chưa có IC đo pin), `batteryVoltageMv` (null), `isCharging`, `sosButtonPressed`, `sensorQuality` (0–100).
  - `Event (0003)` — `Esp32EventPacket` (Indicate): Gói tin sự kiện ngã / SOS / lỗi:
    `protocolVersion` (1), `eventId` (duy nhất, giữ nguyên khi gửi lại), `deviceId`, `sequenceNumber`, `timestampMs`, `eventType`, `eventSeverity`, `sosButtonPressed`, `eventConfidence`, `peakAccelerationMs2`, `orientationChangeDeg`, `altitudeDeltaM`, `inactivityDurationMs`, `checksum` (null v1), `triggerReasons` (mở rộng telemetry).
    Các `eventType` hợp lệ: `IMPACT_DETECTED`, `FREE_FALL_SUSPECTED`, `POSTURE_CHANGED`, `INSTABILITY_DETECTED`, `INACTIVITY_DETECTED`, `SOS_PRESSED`, `SOS_CANCELLED`, `LOW_BATTERY`, `SENSOR_ERROR`.
  - `Status (0004)` — `Esp32DeviceStatus` (Read / Notify):
    `protocolVersion` (1), `deviceId`, `timestampMs`, `firmwareVersion` (`"esp-s3 1.0.0"`), `uptimeSeconds`, `batteryPercent` (-1), `batteryVoltageMv` (null), `isCharging`, `imuStatus` (`OK`/`CALIBRATING`/`ERROR`), `barometerStatus` (`OK`/`CALIBRATING`/`UNAVAILABLE`/`ERROR`), `gnssStatus` (`UNAVAILABLE`), `bufferUsagePercent`, `lastErrorCode` (`IMU_READ_FAILED`/`BAROMETER_READ_FAILED`/`BATTERY_READ_FAILED`/`TIME_NOT_SYNCED`/`BUFFER_OVERFLOW`/null).
  - `Command (0005)` — `Esp32Command` (Write): Lệnh điều khiển từ Android.
  - `Command ACK (0006)` — `Esp32CommandAck` (Notify):
    `protocolVersion` (1), `commandId`, `deviceId`, `timestampMs`, `commandStatus` (`ACCEPTED`/`COMPLETED`/`REJECTED`/`FAILED`), `errorCode` (null nếu không lỗi), `message`.

### Các lệnh BLE được hỗ trợ:
- `PING`: Kiểm tra kết nối -> `commandStatus=COMPLETED`, message="PONG".
- `GET_STATUS`: Báo cáo `Esp32DeviceStatus`.
- `START_STREAM` / `STOP_STREAM`: Bật/tắt luồng dữ liệu Notify.
- `SET_SAMPLE_RATE`: Đổi tần số đọc IMU (`sampleRateHz` = 100 hoặc 50 Hz; mức khác trả `REJECTED`).
- `SET_REFERENCE_ALTITUDE`: Đặt mốc 0m cho `altitudeDeltaM` từ áp suất ổn định hiện tại.
- `TRIGGER_BUZZER`: Kêu còi theo thời lượng `remainingMs`.
- `STOP_BUZZER`: Dập âm còi ngay lập tức.
- `ACK_EVENT`: Xác nhận đã nhận event (KHÔNG dập còi hay hủy cảnh báo tại chỗ).
- `CANCEL_ALERT`: Hủy trạng thái cảnh báo khẩn cấp, đưa về `MONITORING`.
- `START_SELF_TEST`: Kích hoạt tự kiểm tra và hiệu chuẩn lại cảm biến (phát `SENSOR_ERROR` nếu IMU lỗi).
- `SET_DEVICE_TIME`: Đồng bộ đồng hồ mốc Epoch (`unixTimeMs`).
- `REBOOT_DEVICE`: Khởi động lại vi điều khiển an toàn (từ chối `REJECTED` nếu đang cảnh báo khẩn).

---

## 6. CẦN XÁC MINH TRÊN PHẦN CỨNG (Hardware Verification Checklist)

1. **Sơ đồ nguyên lý thực tế (Schematic)**: Xác nhận lại các chân GPIO cho Nút SOS, Nút Cancel, Còi báo, dải LED pin và cầu chia áp ADC pin.
2. **Cực tính nút bấm (Button polarity)**: Hiện firmware cấu hình `INPUT_PULLUP` và tích cực mức THẤP (Active LOW). Cần đo thực tế trên mạch xem có pull-down hay pull-up ngoài.
3. **Mạch lái Còi (Buzzer driver)**: Xác minh còi là loại chủ động (Active buzzer - cấp mức 1 là kêu) hay bị động (Passive buzzer - cần tín hiệu xung PWM tần số ~2.7 kHz).
4. **Địa chỉ I2C thực tế**: MPU6050 (0x68 hoặc 0x69 qua chân AD0); MS5611 (0x77 hoặc 0x76 qua chân CSB). Firmware đã có cơ chế tự quét fallback.
5. **Nhiễu rung khi lắp vỏ (Mechanical coupling)**: Kiểm tra đệm chống rung cho MPU6050 để tránh hiện tượng va đập giả khi gõ nhẹ vào vỏ thiết bị.
6. **Mạch quản lý nguồn / đo pin**: Khi bo có IC MAX17048 hoặc cầu phân áp ADC, cần cập nhật hệ số chuyển đổi điện áp sang % pin trong hàm cập nhật pin.
