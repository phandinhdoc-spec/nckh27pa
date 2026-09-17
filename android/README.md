# FallSafe — bản nghiên cứu PHONE_ONLY
Ứng dụng thật Compose4tab, APK debug; không phải thiết bị y tế hoặc bản phát hành. Không gửi SMS/cuộc gọi/network/người nhận thật. Hai kế hoạch gốc không thay đổi.

## Build và kiểm thử
Từ thư mục android, dùng JDK21 đã có (JBR25 mặc định của Studio không phải JDK đang dùng cho build này):
```sh
source ~/.config/nckh27pa/env.sh
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
./gradlew --no-daemon --max-workers=2 testDebugUnitTest assembleDebug lintDebug
```
Gradle8.13, AGP8.13.0, Kotlin2.2.10, Compose BOM2025.08.01, SDK36/build-tools36.0.0. Wrapper sinh từ distribution chính thức có SHA256 pin. APK: app/build/outputs/apk/debug/app-debug.apk.

Từ root repo:
```sh
./android/core/run-tests.sh
python3 android/scripts/emulator-smoke.py
python3 android/scripts/emulator-background.py
python3 android/scripts/emulator-sensor-scroll.py
```
Các script UI từ chối thiết bị không mang serial emulator-*, mặc định emulator-5554. Cần một emulator đã boot và APK đã cài. Script background chỉ thay quyền notification của app thử trên emulator. Không chạy scripts lên điện thoại thật. adb/emulator theo ~/.config/nckh27pa/env.sh; chưa có phép cài lên điện thoại cá nhân hoặc flash bo.

## Demo
1. Trang chủ: banner THỬ NGHIỆM, PHONE_ONLY, mẫu cảm biến nếu có; không gọi thiếu dữ liệu là an toàn.
2. Chạy dữ liệu mô phỏng: chuỗi mẫu đi qua detector DEMO, vào xác minh10s. Không bấm vẫn gửi sink bộ nhớ và hiện Đã gửi THỬ NGHIỆM. SAFE trước hạn hủy; CẦN GIÚP ĐỠ gửi ngay. SOS giữ2giây, tap ngắn không gửi.
3. Hoàn tất sự kiện rồi thử lại. Cài đặt → Giả lập lỗi gửi để test FAILED, không có SENT giả. Sự kiện/Người thân hiển thị dữ liệu phiên và người nhận giả.
4. Cài đặt → Bật giám sát nền thử: yêu cầu notification. Chỉ bật FGS khi được phép hiển thị; từ chối thì giữ cảm biến khi mở app và nói rõ chưa bật nền. Notification xuất hiện tức thì API31+, service health non-exported. Dừng nền ở trạng thái MONITORING; sự kiện đang active cần hoàn tất trước. OS task manager/force-stop luôn có thể dừng ứng dụng.
5. Activity/service bàn giao collector bằng callback, không delay tùy ý. FGS giữ sensor khi rời Activity, chỉ giữ partial wakelock tối đa11s trong VERIFYING10s. Đây là cấu hình DEMO, không phải bảo đảm mọi hãng điện thoại/Doze. START_NOT_STICKY, không tự bật sau reboot/process death.
6. Đổi tab hoặc khi state thay đổi, màn hình cuộn về đầu để không ẩn nút xác minh. Có test SensorManager trên emulator (đưa gia tốc giả vào Android sensor API) riêng với replay.

## Mô-đun và dữ liệu
- core/: AlertCore độc lập UI, owner-thread, deadline monotonic, bounded logs, fake sink, lỗi gửi không SENT. CORE-002 là regression sau triển khai; giữ công bố thiếu TDD lịch sử.
- PhoneSensorCollector / PhoneInputPipeline / PhoneNormalizer: gia tốc m/s²; gyro rad/s→độ/s; rotation vector→pitch/roll/yaw độ; pressure hPa→Pa. Optional thiếu/cũ500ms=null; thiếu accel trả packet null và reset bằng chứng detector của PHONE_ONLY. Không reset countdown đã vào VERIFYING.
- DemoDetector: va đập≥25m/s² rồi yên9.81±1 trong≥1s, ≥6mẫu, cửa sổ3s; gap>250ms/invalid/time-reversed làm mất bằng chứng. Chỉ minh họa, chưa hiệu chỉnh, không chẩn đoán ngã/đột quỵ.
- PhoneSensorPacket giữ tên theo plan; step/location/relative altitude chưa đo; UNKNOWN/confidence0/quality0 là chưa đánh giá, không chất lượng chuẩn.
- protocol/Esp32PacketDecoder: sensor JSON v1 strict/duplicate key/size/type/range; chưa event/status/command decoder, BLE, ACK hay dedupe repository. Frame lab nằm riêng protocol-lab, chưa bật trong app.

## Giới hạn và bằng chứng
21 JUnit tests ở APK build hiện tại (Demo14, Ownership1, Decoder6); core runner riêng12nhóm. docs/test-report.md là trạng thái nghiệm thu mới nhất và danh sách log/ảnh/XML thực, không lấy lời agent làm bằng chứng.
Nhật ký/sink32bản RAM; chưa Room/persistence, event IDs không bền qua restart. Chưa GNSS/location/bước/pin/âm-rung, BLE/fusion ba chế độ, gửi thật, kênh nhận thật, pin dài hạn, dữ liệu ngã có nhãn, kiểm thử điện thoại/ESP32 thật. Không nói đã hoàn thành Android §12 chỉ từ build/emulator.
