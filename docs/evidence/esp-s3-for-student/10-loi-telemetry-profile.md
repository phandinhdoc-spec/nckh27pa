# Lỗi Telemetry + Profile khi Android đã kết nối ESP32-S3: điều tra và kết luận

Ngày: 2026-09-25. Board: ESP32-S3 `FALLSAFE-4D4D`, chạy firmware chính thức `esp-s3/esp-s3.ino` (Build Sep 25 2026 01:34:21).
Hiện tượng người dùng báo: app Android kết nối BLE thành công, nhưng nút **"Yêu cầu Telemetry"** và nút **"Gửi Profile"** đều thất bại.

## LỖI 1 — "Yêu cầu Telemetry" thất bại

| Phía | Bằng chứng |
|------|-----------|
| App | `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/BleTestScreen.kt:409` → `bleClient.writeRequest("REQ_TELEMETRY")` — gửi **chuỗi thô**, không phải JSON |
| Firmware | `esp-s3/esp-s3.ino`, hàm `dispatchBleCommand`: chỉ tìm `"commandType":"..."` hoặc `"command":"..."` rồi `sscanf`. Chuỗi thô ⇒ `cmdName` rỗng ⇒ trả `sendBleAck(cmdId="", "REJECTED", "UNKNOWN_COMMAND", "Command not recognized")` |
| Nguồn gốc | Bản test-rig `esp32-test/esp32-ble/esp32-ble.ino:347` — `RequestCallbacks::onWrite` nhận chuỗi bất kỳ và đẩy ngay telemetry. App được viết theo test-rig nên vẫn gửi `REQ_TELEMETRY`. Tài liệu `esp/esp32-plan.md` không định nghĩa lệnh này cho firmware chính thức |

Kết luận: không phải lỗi kết nối hay MTU. Firmware từ chối vì sai định dạng lệnh.

## LỖI 2 — "Gửi Profile" thất bại

| Phía | Bằng chứng |
|------|-----------|
| App | `BleGattUuids.kt:13` `PROFILE_WRITE_UUID = 7d2a0006-…`; `BleTestScreen.kt:532` gọi `bleClient.writeProfile(profile)`; `FallSafeBleClient.kt:527-531` ghi JSON vào chính UUID đó |
| Firmware | `esp-s3/esp-s3.ino:83` `#define UUID_CHAR_ACK "7d2a0006-…" // Notify` → `PROPERTY_NOTIFY` duy nhất (`esp-s3.ino:1540`), **không ghi được** ⇒ `writeCharacteristic` trả về thất bại ngay ở tầng Android |
| Kế hoạch | `esp/esp32-plan.md:270` ghi `COMMAND_ACK_CHARACTERISTIC_UUID = 7d2a0006 …, Notify` — firmware chính thức ĐÚNG kế hoạch, app lệch |
| Nơi `7d2a0006` là ghi được | Chỉ có ở test-rig: `esp32-test/esp32-ble/esp32-ble.ino:148` (`BLE_PROFILE_UUID`) và `:251` (`createCharacteristic(..., PROPERTY_WRITE)`), kèm `applyProfileJson` 14 trường |
| Khoảng trống tính năng | Firmware chính thức KHÔNG có đặc trưng profile và KHÔNG có lệnh `SET_PROFILE`/`GET_PROFILE` trong `dispatchBleCommand` — tính năng sửa 14 tham số FallProfile mà app quảng cáo chưa tồn tại ở phía ESP |
| Tự mâu thuẫn trong app | `FallSafeBleClient.kt:335` ánh xạ `7d2a0006` → `KIND_ACK`, tức app vừa coi đó là kênh ACK vừa dùng để ghi profile |

Kết luận: ghi thất bại ngay ở tầng GATT (đặc trưng chỉ Notify), và kể cả ghi được thì firmware cũng chưa có lệnh xử lý profile.

## Quyết định của chủ dự án (2026-09-25)

Chọn **sửa phía APP cho đúng hợp đồng chính thức**, KHÔNG đụng firmware:
1. Nút "Yêu cầu Telemetry" → gửi JSON đúng hợp đồng qua `sendStartStream()` (`START_STREAM`).
2. Nút "Gửi Profile" → khoá lại (`enabled = false`) kèm lời giải thích trung thực; `writeProfile` trả `false` ngay, không ghi vào `7d2a0006`.
3. Thứ tự thi công: hoàn tất bản firmware học sinh trước, rồi mới sửa app (vì fix ở phía app không làm bản học sinh phải dịch lại).

## Mức độ kiểm chứng của điều tra này

- Đã có: bằng chứng tĩnh hai phía (đường dẫn + số dòng + UUID + thuộc tính đặc trưng) và tài liệu kế hoạch đối chiếu.
- CHƯA có: log phần cứng ghi lại lệnh của app. Bốn cửa sổ bắt log serial (150/180/300/240 giây) không ghi được lệnh BLE nào vì điện thoại chưa kết nối lại trong cửa sổ, và hai cửa sổ đầu còn bị lỗi công cụ (pyserial không nhận `dtr/rts` trong constructor).
- Ghi chú công cụ QUAN TRỌNG (đã sửa): mở cổng `/dev/cu.usbmodem1101` bằng pyserial với DTR/RTS mặc định sẽ đẩy ESP32-S3 vào chế độ DOWNLOAD (`boot:0x23`, "waiting for download") — firmware ngừng chạy và BLE ngừng quảng cáo. Đây chính là trạng thái "không dò được thiết bị" từng gặp. Cách mở đúng: gán `dtr=False`, `rts=False` TRƯỚC khi `open()` (đặt trên đối tượng `Serial()` đang đóng), khi đó chip chỉ khởi động lại bình thường (`boot:0x2b SPI_FAST_FLASH_BOOT`).
- Hệ quả vận hành: mỗi lần mở cổng serial, chip reset ⇒ điện thoại mất kết nối BLE. Muốn bắt log lệnh của app thì phải mở cổng TRƯỚC, rồi mới kết nối app và bấm nút trong lúc cửa sổ ghi còn mở.
