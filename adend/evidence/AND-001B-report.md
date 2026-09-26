# AND-001B — bàn giao source và pure tests

Ngày 2026-09-14. Không agent con, không network, không chạy Gradle. Chỉ ghi `android/app/src/`, `android/README.md`, `android/evidence/AND-001B-*`. Không sửa core hoặc Gradle; CORE-002 độc lập.

## Source hoàn thành

- AndroidManifest không permissions, launcher MainActivity, Application giữ phiên.
- MainActivity Compose bốn tab có nội dung và tương tác: banner thử nghiệm, nguồn thật/giả, trạng thái thiếu dữ liệu, countdown/SAFE/HELP, SOS giữ 2 giây và hủy khi rời Activity, replay, hoàn tất/thử lại, công tắc sink lỗi, người nhận giả, nhật ký bounded, thông số sensor. Câu hỏi xác minh hiển thị ở mọi tab.
- PhoneSensorCollector thật và normalizer pure: đúng tên/schema PhoneSensorPacket; timestamp sensor monotonic; chuẩn hóa gyro/pressure, optional null, freshness 500 ms, không giả sensor bằng zero. Bước/độ cao/vị trí chưa tích hợp; trạng thái mang theo người UNKNOWN, confidence/quality 0 chưa đánh giá.
- Detector DEMO nhiều mẫu có cửa sổ thời gian; replay có lịch mẫu và tiêu thụ một lần, cùng đường accept → AlertCore. Clock elapsedRealtime, Handler main; tick tiếp tục qua pause nếu process sống, resume tick bắt kịp. Không hứa foreground service hoặc persistence.
- Sink synchronous tại máy: SENT không đồng nghĩa người thật nhận. SENDING có trong lịch sử vì chuyển trạng thái tức thời; FAILED riêng, ACKNOWLEDGED chỉ hoàn tất tại máy.

## Kiểm chứng thật

Lệnh: `bash android/evidence/AND-001B-test.sh` (từ root).

- `AND-001B-red.log`: compiler thất bại trước khi thêm DemoLogic.kt, 6 test đầu đã viết. Đây là RED do thiếu implementation, không phải RED hành vi trên scaffold.
- `AND-001B-green.log`: JUnit 4.13.2, **OK (6 tests)** sau implementation.
- `AND-001B-final-tests.log`: **OK (9 tests)**, thêm 3 test biên sau triển khai; không tuyên bố test-first cho 3 test bổ sung hoặc adapter Android/Compose.
- Suite test normalization/unit conversions/missing/NaN/Infinity/stale/future/short arrays, detector một mẫu/gap/duplicate/quiet/window/invalid, replay/countdown/no-response/idempotence, SAFE trước/tại hạn, HELP, sink fail/complete/retry/bounded, SOS 1999/2000 ms/cancel. Phần dùng core là integration/regression API hiện có; không viết lại core.
- `AND-001B-static-checks.log`: parse manifest, kiểm tra không có permissions, rà đường gọi ngoài và số test; không thay thế Android build.

## Bàn giao host

Hermes chạy `./gradlew --no-daemon --max-workers=2 testDebugUnitTest assembleDebug` trong android, rồi kịch bản emulator trong README. Chưa có kết quả Android compiler, APK, UI/TalkBack, phần cứng hoặc background thực tế; không bịa output. Không dừng task source vì blocker Gradle sandbox đã biết.

Giới hạn bổ sung: log chỉ lưu ID/state/status, chưa thời gian/nguồn từng sự kiện; không pin/rung/âm báo/khóa màn hình. Các ngưỡng demo chưa được kiểm chứng lâm sàng hay độ chính xác. Đã đọc các mục plan được phiếu chỉ định, core API/README và app Gradle; lệnh tìm AGENTS ban đầu vô tình quét tên ngoài workspace (permission errors), không đọc nội dung các tài liệu đó, sau đó giới hạn lại workspace.
