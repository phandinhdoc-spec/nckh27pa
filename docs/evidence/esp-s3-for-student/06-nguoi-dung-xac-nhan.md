# Nghiệm thu cuối bằng app Android FallSafe (tiêu chí (e) của kế hoạch)

Người thực hiện: chủ dự án (Hermes không thể chạy app trên điện thoại thật).
Thời điểm: 2026-09-25, ~01:40 (+07).

## Điều kiện đo
- Board ESP32-S3 (tên BLE `FALLSAFE-4D4D`, BT MAC 90:da:72:49:4d:4c) nạp bản chính thức `esp-s3.ino`.
- Hotspot điện thoại `Pdmq` ĐÃ bật; log serial thiết bị ghi `[WIFI] Connected! IP: 10.139.245.190 (RSSI: -45 dBm)`.
- Quét độc lập từ Mac (CoreBluetooth/bleak) cùng lúc: thấy `FALLSAFE-4D4D` rssi -59, service `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001`
  (bằng chứng: `00-baseline/after-user-upload/ble-scan-mac.txt`).

## Kết quả do chủ dự án báo
- App Android **FallSafe ĐÃ dò thấy** thiết bị `FALLSAFE-4D4D`.

## Kết luận
Tiêu chí nghiệm thu BLE cho bản chính thức `esp-s3.ino`: **ĐẠT**.
Chuỗi bằng chứng đầy đủ: phần cứng quảng cáo → Mac dò thấy → app dự án dò thấy, trong điều kiện WiFi hotspot đã nối.

## Còn lại (chưa nghiệm thu)
- Bản học sinh `esp-s3-for-student.ino` phải được nạp và dò lại bằng app (bước S4.1.3/S4.1.4) trước khi kết luận.
