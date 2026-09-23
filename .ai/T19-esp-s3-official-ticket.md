# T19 — Firmware CHÍNH THỨC cho sản phẩm ESP32-S3 Super Mini

STATUS: DONE (build PASS 2 FQBN; chờ xác minh phần cứng)
DATE: 2026-09-22

OWNER
- Worker: AGY `gemini-3.8-flash-high` (grill-tab, accept-edits) — đọc `esp/esp32-plan.md`, tạo `esp-s3/`, viết
  `esp-s3.ino` rồi vòng R1 fix 8 điểm lệch hợp đồng §8.
- Hermes: phát hiện + sửa lỗi biên dịch còn lại sau R1, build và xác minh toàn bộ 2 FQBN.

GOAL
Firmware chính thức (TARGET_PRODUCT) cho ESP32-S3 Super Mini + MPU6050 + GY-63/MS5611, một file `.ino` duy nhất,
không thư viện ngoài, giữ đúng hợp đồng BLE §8 và luật phát hiện ngã parity Android.

FILES (mới)
- `esp-s3/esp-s3.ino` (1667 dòng) — firmware chính thức
- `esp-s3/README.md` (122 dòng) — board/FQBN, chân, build/flash, chế độ serial, giao thức BLE, checklist xác minh
- `esp-s3/PINMAP.md` (38 dòng) — bảng chân: CONFIRMED (từ `esp/node/.../node_config.h`) vs `TODO(HW)`
- KHÔNG sửa `esp/esp32-plan.md`, `esp/node/**`, `esp32-test/**`, `android/**`, `backend/**`, `docs/**`

RESULT (đã kiểm chứng bằng build thật)
- FQBN (a) `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled`: **650253 bytes (49%)**,
  RAM 30024 bytes (9%) — exit 0
- FQBN (b) `esp32:esp32:esp32s3`: **654861 bytes (49%)**, RAM 29832 bytes (9%) — exit 0
- Hợp đồng §8 đã đúng: `Esp32SensorPacket`/`DeviceStatus`/`Event`/`CommandAck` dùng đúng tên trường v1
  (kể cả `commandStatus`, `errorCode`, `eventId`, `deviceId`, `sequenceNumber`, `altitudeDeltaM`, `sensorQuality`).
- `altitudeDeltaM` tính theo mốc `s_referencePressurePa` (§6.2); mốc chỉ đổi khi có lệnh
  `SET_REFERENCE_ALTITUDE` hoặc áp suất ổn định ±10 Pa ≥ 10 s **và** đang `MONITORING` (không đổi trong cửa sổ nghi ngã).
- Pin kiểu dữ liệu: chưa có MAX17048 → `batteryPercent = -1` + `BATTERY_READ_FAILED`, KHÔNG báo 100% giả.
- Ring buffer 10 s trước sự kiện (1000 mẫu @100 Hz), MPU6050 `SMPLRT_DIV=9` / `DLPF=1` / `±16 g` / `±2000 dps`.

FIX do Hermes thực hiện (sau khi worker R1 báo xong nhưng file không biên dịch được):
1. `enum DeviceState` được chuyển lên đầu sketch (mục "0. USER TYPES DECLARED FIRST"): prototype do Arduino builder
   tự sinh chèn TRƯỚC khai báo enum → `'DeviceState' was not declared in this scope` ở dòng 135/854/1550/1615.
2. `s_batteryPercent`, `s_isCharging`, `s_lowBatWarned`, `s_criticalBatWarned` chuyển lên khối biến toàn cục
   trước mục 5 (chúng bị dùng trong các hàm packet ở dòng 747/794/809/867 nhưng khai báo tận dòng 991).

NEXT_ACTION (phần cứng)
1. Đối chiếu `esp-s3/PINMAP.md` với sơ đồ thật của Super Mini; mọi chân `TODO(HW)` (4,5,1,8,9,10,11,12) phải
   xác minh trước khi hàn. Riêng bus I2C 0/1 (GPIO 7/6, 3/2) đã có căn cứ từ node S3 đang chạy.
2. Nạp firmware, kiểm tra self-test 2 cảm biến + BLE `FALLSAFE-xxxx` xuất hiện khi scan.
3. Kiểm thử ghép với app Android: ghép cặp, `SET_DEVICE_TIME`, `GET_STATUS`, đọc stream, gửi `TRIGGER_BUZZER`.
4. Chạy lại bộ kịch bản T0–T12 của `esp32-test/TEST_PLAN.md` trên phần cứng sản phẩm để so ngưỡng S3 vs WROOM-32.
