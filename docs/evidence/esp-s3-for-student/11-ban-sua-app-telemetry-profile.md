# Bản sửa phía APP cho lỗi Telemetry/Profile — hồ sơ thi công và kiểm chứng

Ngày 2026-09-25. Quyết định của chủ dự án: sửa phía APP theo hợp đồng chính thức, KHÔNG đụng firmware.
Worker: openai-codex `gpt-5.6-sol` (đề bài: `/Users/phananh/.hermes/cache/scratch/w6-app-fix-prompt.md`, nhật ký: `w6-app-fix.log`).

## Thay đổi đã áp

| Tệp | Thay đổi |
|-----|----------|
| `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/BleTestScreen.kt` | Nút "Yêu cầu Telemetry" gọi `bleClient.sendStartStream()` (JSON `commandType: START_STREAM`) thay cho việc gửi chuỗi thô `REQ_TELEMETRY` (dòng 409 và dòng 429) |
| cùng tệp | Mục cấu hình Fall Profile: nút gửi đặt `enabled = false`, nhãn "Gửi Profile (chưa hỗ trợ)" (dòng 542–545); giữ nguyên 14 ô nhập liệu |
| `FallSafeBleClient.kt` | `writeProfile(...)` trả `false` ngay ở cả hai overload (dòng 530, 533) — không còn ghi GATT vào `7d2a0006`; thêm comment nêu rõ `PROFILE_WRITE_UUID` là tên cũ, `0006` chỉ là ACK Notify |

## Kiểm chứng Hermes tự chạy (không dùng model)

| Phép kiểm | Lệnh / cách làm | Kết quả |
|-----------|-----------------|---------|
| Hết chuỗi thô | `grep -rn 'REQ_TELEMETRY' android/app/src/main` | không còn kết quả |
| Không ghi vào đặc trưng ACK | `grep -rn 'PROFILE_WRITE_UUID' android/app/src/main` | 3 tham chiếu còn lại đều là NHẬN/ĐỊNH TUYẾN ACK (`getCharacteristic` cho subscribe, ánh xạ `KIND_ACK`, comment) — không có `writeCharacteristic` nào trỏ vào `7d2a0006` |
| Hàm gửi profile bị vô hiệu | đọc `FallSafeBleClient.kt:530,533` | cả hai overload `writeProfile(...) = false` |
| Biên dịch | `./gradlew :app:assembleDebug` (JDK 17 Temurin, SDK tại `~/Library/Android/sdk`) | `BUILD SUCCESSFUL` |
| Kiểm thử đơn vị | `./gradlew :app:testDebugUnitTest` | chạy qua, không lỗi |
| Lint | `./gradlew :app:lintDebug` | `BUILD SUCCESSFUL`. Báo cáo có 53 vấn đề (39 Warning, 8 Error, 4 Fatal, 2 Hint) — kiểm tra từng vấn đề mức Error/Fatal: tất cả đều nằm NGOÀI vùng sửa (`MissingPermission` ở `BleTestScreen.kt:193/198/214` là phần xin quyền quét, `FallSafeBleClient.kt:607` trong `writeCharacteristicCompat`, cùng `MainActivity.kt`, `AndroidPlatformLocationSource.kt`, `AndroidSmsManagerGateway.kt`) ⇒ KHÔNG phát sinh vấn đề mới. Báo cáo: `android/app/build/reports/lint-results-debug.html` |
| Firmware KHÔNG bị đụng | `shasum -a 256 esp-s3/esp-s3.ino` + `git status --short esp-s3/esp-s3.ino` | hash `8954eae25ae581848f60ece649876250b6ab9286e5270afc1c8df6fbf792a995` (đúng bằng hash gốc của bản chính thức) và git không ghi nhận thay đổi |
| Phạm vi sửa gọn | `git status --short android/` | chỉ 2 tệp Kotlin nêu trên bị sửa |

## Sản phẩm giao cho người dùng

APK debug: `/Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/build/outputs/apk/debug/app-debug.apk`
(build 03:07 ngày 2026-09-25, 13.039.470 byte)

## Còn thiếu (ghi nhận trung thực)

Chưa có bằng chứng phần cứng rằng sau khi cài APK mới thì app nhận được dữ liệu telemetry: chưa có thiết bị nào kết nối qua `adb` (danh sách rỗng), và bốn cửa sổ bắt log serial trước đó không thu được lệnh BLE nào vì điện thoại chưa kết nối lại trong cửa sổ. Muốn có bằng chứng này: mở cổng serial TRƯỚC (mở cổng làm chip reset nên phải kết nối app SAU), rồi bấm nút trên app và đọc dòng `[BLE_CMD] ... START_STREAM` + ACK.
