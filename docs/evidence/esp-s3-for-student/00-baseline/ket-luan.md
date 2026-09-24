# W1.1 — Kết luận chẩn đoán baseline (bằng chứng chạy thật, không suy đoán)

Ngày: 2026-09-25 · Board: ESP32-S3 (BT MAC 90:da:72:49:4d:4c, tên BLE `FALLSAFE-4D4D`) · Cổng: /dev/cu.usbmodem1101
Công cụ: arduino-cli 1.5.1 (esp32 core 3.3.12), esptool 5.3.1, Python 3.11 + bleak (CoreBluetooth).

## 1. Phát hiện lớn: firmware ĐANG chạy trên board KHÔNG phải esp-s3.ino

`00-baseline/serial-boot.log` (trước khi Hermes nạp lại) cho thấy:
- `Device ID: FALLSAFE-4D4D | Build: Sep 25 2026 01:23:06`
- `[BOOT] IMU MPU6050 on I2C0 (SDA 7, SCL 6): FAIL (addr: 0x69)` — chân **7/6**, không phải 8/9.
- `[BOOT] Barometer MS5611 on I2C1 (SDA 3, SCL 2)` — chân **3/2**.
- In ra tiếng Việt không dấu: `=== DU LIEU CAM BIEN ===`, `Trang thai : TRANG_THAI_SUY_GIAM`.
- KHÔNG có dòng WiFi nào.

Đối chiếu build cache của Arduino IDE (`~/Library/Caches/arduino/sketches`):
- build `7E6CECB0...` (01:23) → `build.options.json` ghi `sketchLocation = .../esp-s3-hocsinh` → **bản học sinh CŨ** đã được nạp lúc 01:23 hôm nay.
- build `3F0F77C8...` → `sketchLocation = .../esp-s3` (bản chính thức).

⇒ Báo cáo "esp-s3.ino nạp vào không dò được BLE" CHƯA được tái hiện với đúng bản chính thức: thời điểm điều tra, board đang chạy `esp-s3-hocsinh.ino` (bản cũ: chân I2C khác, không WiFi).

## 2. BLE quảng cáo TỐT ở cả hai firmware (khi chip thực sự chạy app)

| Trạng thái board | Firmware | Quét CoreBluetooth từ Mac | Kết quả |
|---|---|---|---|
| Chạy app | esp-s3-hocsinh (Build 01:23) | `00-baseline/ble-scan-mac-running.txt` | **THẤY** `FALLSAFE-4D4D` RSSI -55, service 7d2a0001-…0001 |
| Chạy app | esp-s3 chính thức (Build 01:34, Hermes biên dịch + nạp) | `00-baseline/official-flash/ble-scan-mac.txt` | **THẤY** `FALLSAFE-4D4D` RSSI -52, service 7d2a0001-…0001 |
| Ở chế độ DOWNLOAD | (app không chạy) | `00-baseline/ble-scan-mac.txt` | KHÔNG thấy thiết bị nào của dự án |

Bằng chứng phụ (quan trọng): máy Mac quét được các thiết bị khác (`TY`, `BG0268598`) trong cả hai lần quét ⇒ quyền Bluetooth của macOS và adapter hoạt động; kết quả "không thấy FALLSAFE" không phải do lỗi công cụ quét.

## 3. Loại trừ giả thuyết

- **H1 (WiFi STA/BLE coexist làm nghẹt quảng cáo): LOẠI TRỪ.** Bản chính thức có `WiFi.mode(WIFI_STA)` + `WiFi.begin("Pdmq", …)` chạy song song, log ghi `[WIFI] Connecting to SSID: Pdmq (non-blocking, BLE unaffected)` và trong suốt 30 s đó BLE vẫn được Mac nhìn thấy (RSSI -52). Lưu ý: hotspot `Pdmq` KHÔNG bật trong lúc đo nên WiFi chưa từng nối được (không có dòng `[WIFI] Connected!`), vì vậy trạng thái "WiFi ĐÃ nối + BLE quảng cáo" chưa được đo — xem mục 5.
- **H2/H3 (thiếu tham số quảng cáo / không khôi phục quảng cáo): KHÔNG có bằng chứng gây lỗi.** Cả hai bản dùng cùng khối cấu hình quảng cáo (`addServiceUUID`, `setScanResponse(true)`, `setMinPreferred(0x06/0x12)`) và đều dò được ở khoảng cách bàn làm việc (RSSI -52…-55).
- **H5 (phần cứng/ăng-ten): LOẠI TRỪ** — thiết bị phát quảng cáo mạnh, đúng UUID service.

## 4. Trạng thái KHÔNG dò được BLE đã tái hiện được — nhưng không do code

Sau lệnh reset của esptool qua chân RTS, board đứng ở chế độ nạp:
`rst:0x15 (USB_UART_CHIP_RESET), boot:0x23 (DOWNLOAD(USB/UART0))` / `waiting for download` — ở trạng thái này app KHÔNG chạy và BLE KHÔNG quảng cáo (khớp với lần quét đầu tiên không thấy thiết bị).
Cơ chế: khi cổng USB bị Serial Monitor của Arduino IDE giữ (đã quan sát: tiến trình `serial-monitor` giữ `/dev/cu.usbmodem1101`), thao tác nạp có thể không kết thúc bằng reset mềm; board nằm lại ở chế độ nạp ⇒ điện thoại không thể dò thấy thiết bị.

## 5. Phần CHƯA kiểm chứng (không được coi là đã xong)

1. Chưa đo trạng thái **WiFi Pdmq đã nối thành công** + BLE quảng cáo cùng lúc (cần bật hotspot điện thoại).
2. Chưa tái hiện bằng **app Android FallSafe trên điện thoại** — đây là tiêu chí nghiệm thu cuối của kế hoạch, chỉ chủ dự án thực hiện được.
3. Lần quét đầu tiên (trước mọi can thiệp) không thấy thiết bị; nguyên nhân chính xác của lần đó chưa xác định dứt khoát (ứng viên: board ở chế độ nạp, hoặc host vừa đóng Serial Monitor). Ghi nhận trung thực là "chưa xác định", không quy kết cho code.

## 6. BỔ SUNG (đo sau khi chủ dự án tự nạp lại esp32-s3.ino + bật hotspot Pdmq)

File: `00-baseline/after-user-upload/serial.log` (198 dòng, 40 s) và `after-user-upload/ble-scan-mac.txt`.
- Board chạy `Build: Sep 25 2026 01:34:21` (chính là bản chính thức đã biên dịch; việc nạp lại không đổi dấu thời gian vì Arduino IDE tái dùng bản biên dịch trong cache).
- `[I2C] Scan bus0/Wire: 0x68`, `[BOOT] IMU MPU6050 on I2C0 (SDA 8, SCL 9): PASS (addr: 0x68)` → đúng chân chính thức.
- dòng 46: `[WIFI] Connecting to SSID: Pdmq (non-blocking, BLE unaffected)`
- dòng 65 (8.0 s): `[WIFI] Connected! IP: 10.139.245.190 (RSSI: -45 dBm)` → **WiFi Pdmq ĐÃ nối thành công.**
- Quét CoreBluetooth ngay sau đó (25 s): **THẤY** `FALLSAFE-4D4D` addr 796FA933-… rssi **-59**, services = `['7d2a0001-6f45-4c2b-9a1e-38a8f5c10001']`.

⇒ Kết luận dứt điểm: trong điều kiện WiFi Pdmq đã nối (RSSI -45) thiết bị VẪN quảng cáo BLE và Mac vẫn dò thấy. **Giả thuyết H1 (coexist WiFi/BLE làm nghẹt quảng cáo) bị LOẠI TRỪ hoàn toàn.** Phía thiết bị (esp-s3.ino) không có lỗi dò được.
Điểm còn lại duy nhất: xác nhận bằng app Android FallSafe trên điện thoại (người dùng) → xem `06-nguoi-dung-xac-nhan.md`.
