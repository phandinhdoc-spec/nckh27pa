# T21 — HƯỚNG DẪN CÀI ĐẶT VÀ CẬP NHẬT APK FALLSAFE (NCKH27PA)

## 1. Thông tin bản dựng APK (Artifact Specifications)
- **Đường dẫn tệp APK (tuyệt đối):**
  `/Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk`
- **Application ID:** `vn.nckh27pa.fallsafe`
- **Version Code:** `3`
- **Version Name:** `0.3-permission`
- **Dung lượng tệp:** `13,039,615 bytes` (~12.44 MB)
- **Mã băm SHA-256:** `a7065833b38ae2aeebc27c699e208ee54da7478e89659f6c6130ff74ff4605ae`
- **Yêu cầu hệ điều hành:** Android 8.0 trở lên (minSdkVersion: 26, targetSdkVersion: 36)
- **Loại chữ ký (Signing):** `Android Debug Certificate` (v2 scheme verified, keystore debug)
- **Tính năng mới trong bản dựng:**
  + Tích hợp đầy đủ BLE GATT Central Client (`vn.nckh27pa.fallsafe.bluetooth`).
  + Màn hình kiểm thử BLE chuyên dụng (`BleTestScreen`) trong Cài đặt -> KIỂM THỬ BLE ESP32: hỗ trợ quét, kết nối, đọc telemetry, gửi FallProfile (14 thông số), nhận event cảnh báo ngã tức thời.
  + Đã cấp và xử lý quyền BLE động (`BLUETOOTH_SCAN`, `BLUETOOTH_CONNECT`) cho Android 12+ (API 31+).

---

## 2. Chuẩn bị thiết bị và kết nối ADB

### Bước 1: Kích hoạt Developer Options (Tùy chọn nhà phát triển)
1. Trên điện thoại Android, vào **Cài đặt (Settings)** -> **Thông tin điện thoại (About Phone)**.
2. Tìm mục **Số bản dựng (Build Number)** và chạm liên tục 7 lần cho đến khi xuất hiện thông báo: *"Bạn hiện là nhà phát triển!"*.
3. Quay lại menu Cài đặt chính -> vào **Hệ thống (System)** -> **Tùy chọn nhà phát triển (Developer Options)**.
4. Bật công tắc **Gỡ lỗi qua USB (USB Debugging)**.

### Bước 2: Kết nối máy tính và xác thực RSA Key
1. Cắm cáp USB nối điện thoại với máy tính Mac.
2. Mở Terminal trên máy Mac và gõ:
   ```bash
   adb devices -l
   ```
3. Trên màn hình điện thoại sẽ hiện hộp thoại xác thực: *"Cho phép gỡ lỗi qua USB từ máy tính này?"*.
4. Tích chọn **"Luôn cho phép từ máy tính này" (Always allow from this computer)** rồi nhấn **OK (Cho phép)**.
5. Kiểm tra lại bằng lệnh `adb devices`, kết quả phải hiển thị mã thiết bị kèm chữ `device`:
   ```text
   List of devices attached
   RFCW20XXXXX    device product:husky model:Pixel_8_Pro device:husky
   ```
   *(Nếu hiển thị `unauthorized`, rút cáp cắm lại và xác nhận trên điện thoại. Nếu danh sách trống, đổi cáp/cổng USB hoặc bật chế độ truyền tệp MTP).*

---

## 3. Các lệnh cài đặt APK qua ADB

### Cách 1: Cài đặt cập nhật đè (Giữ lại cấu hình và dữ liệu hiện tại)
Lệnh tiêu chuẩn dùng để cập nhật bản build mới mà không làm mất cấu hình đã thiết lập:
```bash
adb install -r /Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk
```

### Cách 2: Cài đặt và tự động cấp toàn bộ quyền runtime (-g)
Nếu muốn cài đặt và tự động cấp sẵn các quyền runtime (Location, SMS, Notifications, Phone):
```bash
adb install -r -g /Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk
```

### Cách 3: Xử lý lỗi xung đột chữ ký (INSTALL_FAILED_UPDATE_INCOMPATIBLE)
- **Nguyên nhân:** Điện thoại đã cài đặt ứng dụng FallSafe từ trước nhưng được build hoặc ký bởi máy tính/keystore khác.
- **Khắc phục:** Bắt buộc gỡ bỏ phiên bản cũ trên điện thoại trước, sau đó cài bản mới:
```bash
# 1. Gỡ cài đặt phiên bản cũ
adb uninstall vn.nckh27pa.fallsafe

# 2. Cài đặt lại bản APK mới
adb install /Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk
```

### Cách 4: Xử lý các lỗi cài đặt thường gặp khác
- **Lỗi `INSTALL_FAILED_VERSION_DOWNGRADE`:** Thêm cờ `-d` để cho phép hạ cấp:
  ```bash
  adb install -r -d /Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk
  ```
- **Lỗi `INSTALL_FAILED_INSUFFICIENT_STORAGE`:** Bộ nhớ trong của điện thoại bị đầy, cần giải phóng tối thiểu 100MB dung lượng trống.
- **Lỗi `device offline`:** Khởi động lại ADB daemon trên máy tính:
  ```bash
  adb kill-server && adb start-server
  ```

---

## 4. Khởi chạy và Chẩn đoán vận hành từ xa qua ADB

### Khởi chạy ứng dụng trực tiếp bằng ADB
Không cần chạm vào màn hình điện thoại, có thể khởi động ngay FallSafe qua lệnh:
```bash
adb shell am start -n vn.nckh27pa.fallsafe/.MainActivity
```

### Kiểm tra tiến trình hoạt động (Process verification)
Kiểm tra xem ứng dụng có đang chạy trên thiết bị hay không:
```bash
adb shell pidof vn.nckh27pa.fallsafe
```
*(Nếu trả về số PID, ví dụ `18420`, ứng dụng đang chạy ổn định).*

### Theo dõi Logcat thời gian thực (Diagnostic Logs)
1. **Lọc log các thành phần chính (FallSafe, BLE, Crashes):**
   ```bash
   adb logcat -s FallSafe:V BLE:V BleManager:V AndroidRuntime:E
   ```
2. **Theo dõi toàn bộ log phát sinh bởi tiến trình FallSafe:**
   ```bash
   adb logcat --pid=$(adb shell pidof vn.nckh27pa.fallsafe)
   ```
3. **Xóa bộ đệm log cũ và bắt đầu ghi mới:**
   ```bash
   adb logcat -c && adb logcat -s FallSafe:V BLE:V AndroidRuntime:E
   ```
4. **Kiểm tra ngoại lệ hoặc lỗi sập ứng dụng (Crash / Exception):**
   ```bash
   adb logcat -b crash
   ```

---

## 5. Quy trình nghiệm thu tính năng trên điện thoại
1. Mở ứng dụng **FallSafe THỬ NGHIỆM**.
2. Trên màn hình chính, kiểm tra danh sách quyền (Vị trí, Bluetooth, SMS/Cuộc gọi, Thông báo) và cấp phép đầy đủ.
3. Chuyển sang tab **Cài đặt** -> cuộn xuống phần **CÔNG CỤ THỬ NGHIỆM PHẦN CỨNG** -> chọn **KIỂM THỬ BLE ESP32**.
4. Nhấn **QUÉT THIẾT BỊ BLE** để quét mạch ESP32 chạy firmware `ble_server` / FallSafe BLE GATT.
5. Nhấn **KẾT NỐI** để kiểm tra đồng bộ dữ liệu cảm biến đo gia tốc, áp suất, độ cao và sự kiện phát hiện ngã theo thời gian thực.
