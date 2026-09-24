# T20 — BLE GATT ESP32 <-> Android · bằng chứng nghiệm thu (run_20260923_144714_b990d2)

## Phạm vi
Kết nối BLE GATT 2 chiều: ESP32 (peripheral/server) <-> Android FallSafe (central/client), framing IF-003 + payload JSON v1. Firmware MỚI trong `esp32-test/`, Android BLE client + UI trong `android/`.

## 1. Firmware — esp32-test/esp32-ble/esp32-ble.ino (T1, W1.1+W1.2+W1.3)
- Worker: commandcode/glm-5.3 ban đầu bị QUOTA-BLOCKED (weekly limit, reset 2026-09-25) -> re-route sang Codex theo AGENTS.md L2.
- Đã biên dịch bằng arduino-cli (`esp32:esp32:esp32`, core 3.3.12):
  `Sketch uses 1141831 bytes (87%) ... Global variables use 45620 bytes (13%)` — 0 lỗi.
- Đặt trong thư mục riêng `esp32-test/esp32-ble/` (Arduino biên dịch MỌI .ino trong 1 folder -> cần tách khỏi esp32-test.ino để tránh redefinition).
- Lỗi đã sửa (Codex để lại, Hermes tự sửa):
  1. `enum DetectionState` được chuyển lên SỚM trước các hàm BLE, vì arduino-cli tự chèn prototype `fallStateToString(DetectionState)` TRƯỚC định nghĩa enum -> lỗi 'DetectionState not declared'. Đổi tên stateToString->fallStateToString.
  2. telemetry JSON thiếu key bắt buộc `batteryPercent`, `batteryVoltageMv`, `isCharging`, `sosButtonPressed` -> Esp32PacketDecoder trả null. Đã thêm.
- IF-003 encoder: header byte-exact khớp golden protocol-lab/fixtures/golden.json (rebuild `465301017856341200000600150000007b227072` -> MATCH=True; chunk=MTU-19=4, count=ceil(total/chunk)).
- UUID: service `7d2a0001-…0001`; telemetry `…0002`; event `…0003`; request `…0005`; profile `…0006`.
- `esp32-test.ino` KHÔNG bị sửa (hash giữ nguyên 98e358c…).

## 2. Android BLE client + UI (T2, W2.1+W2.2+W2.3)
- Worker: AGY gemini-3.8-flash-high (core) + gemini-3.8-flash-low (UI). commandcode GLM-5.3 flash re-route vì quota.
- Files mới (package vn.nckh27pa.fallsafe.bluetooth): If003Framing.kt (port reassembler IF-003), FallSafeBleClient.kt, BleGattUuids.kt (UUID khớp firmware), BleProfilePayload.kt (14 field profile JSON), BleEventPacket.kt, BleTestScreen.kt (UI: scan/connect, Request telemetry, form FallProfile, event list).
- MainActivity.kt gắn FallSafeBleClient + overlay navigation từ tab Settings.
- AndroidManifest.xml: BLUETOOTH_SCAN/CONNECT + uses-feature bluetooth_le.
- Esp32PacketDecoder.kt KHÔNG bị sửa.
- Nghiệm thu độc lập (Hermes tự chạy): `./gradlew --no-daemon --max-workers=2 testDebugUnitTest assembleDebug lintDebug` = BUILD SUCCESSFUL. 34 suites, 232 tests, failures=0 errors=0.

## 3. e2e contract (T3, W3.1)
- Host-level: BleProtocolVerificationTest chứng minh golden IF-003 frame -> Reassembler -> Esp32PacketDecoder.decodeSensor -> Esp32SensorPacket đúng; profile JSON round-trip qua framing.
- On-device GATT (+pair & real ESP32) KHÔNG chạy trên host này: không có emulator binary, chưa có quy trình flash bo thật (policy repo: chỉ emulator-*). -> mục (c) để giai đoạn sau.

## Blockers & caveat
- commandcode quota-blocked trong run này; đã fallback Codex/AGY theo policy.
- Không có emulator trên host hiện tại -> W2.3 S2.3.2 và W3.1 on-device chỉ nghiệm thu host-level, ghi rõ pending cho phần cứng thật.

---

# T21/T20 — AUDIT LẠI + ĐÓNG GÓI APK (run_20260923_164442_6529fc)

## 1. Audit mã nguồn (T1, mimo-v2.6-pro theo Q5b)
- Android: package `vn.nckh27pa.fallsafe.bluetooth` đủ 6 tệp; MainActivity import + khởi tạo lazy `FallSafeBleClient(this)` (dòng 41/76), gọi `requestBluetoothPermissions()` trong onCreate (dòng 79), điều hướng overlay `BleTestScreen` (dòng 269); manifest có BLUETOOTH/ADMIN (max30), BLUETOOTH_SCAN, BLUETOOTH_CONNECT, uses-feature bluetooth_le required=true (dòng 16-20). UUID khớp 100% firmware. `Esp32PacketDecoder.kt` KHÔNG đổi.
- ESP32: `esp32-test/esp32-ble/esp32-ble.ino` trong thư mục con riêng; `enum DetectionState` + `fallStateToString` khai báo SỚM ở đầu (dòng 121-140) trước mọi hàm -> tránh lỗi auto-prototype; telemetry JSON đủ key required của decoder (batteryPercent/batteryVoltageMv/isCharging/sosButtonPressed/sensorQuality — dòng 332); IF-003 header 16 byte khớp `Framing.kt`; `applyProfileJson` đủ 14 trường.
- **Kết luận audit: 2 phía ĐÃ CẬP NHẬT ĐẦY ĐỦ, KHÔNG phát hiện lỗ hổng** -> W1.3 không phải sửa gì.

## 2. Build/verify local (T2, hermes/self theo Q5b)
- Android: `./gradlew --no-daemon --max-workers=2 testDebugUnitTest assembleDebug lintDebug` -> **BUILD SUCCESSFUL**, 34 suites / 232 tests / 0 fail / 0 err.
- ESP32: `arduino-cli compile --fqbn esp32:esp32:esp32 --warnings none esp32-test/esp32-ble` -> **0 lỗi** (Flash 87%, RAM 13%).

## 3. Bàn giao cài đặt (T3)
- APK: `android/app/build/outputs/apk/debug/app-debug.apk` — debug cert, versionCode 3, versionName 0.3-permission.
- Hướng dẫn đầy đủ + xử lý `INSTALL_FAILED_UPDATE_INCOMPATIBLE`: `.ai/T21-apk-install.md`.
- Người dùng TỰ cài (Q2b); không tự ý cài lên máy cá nhân.

## 4. Trạng thái phần cứng (Q4b)
- `adb devices` TRỐNG -> test BLE end-to-end trên điện thoại thật: **PENDING**. Cổng nạp ESP32 sẵn sàng: `/dev/cu.usbserial-0001`.