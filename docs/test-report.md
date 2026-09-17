# Bằng chứng kiểm thử — mốc tiếp tục
Không dùng CLI exit0 thay nghiệm thu task. Bằng chứng cũ giữ nguyên trong evidence.

|Hạng mục|Kết quả do Hermes kiểm chứng|Bằng chứng|
|---|---|---|
|CORE-002 Kotlin host|12 nhóm regression PASS, demo PASS; review PASS, assertion test lỗi sink đã đưa ra ngoài callback|CORE-002-hermes.log, CORE-002-hermes-final.log, REV-CORE-002.txt|
|Host/toolchain|DNS resolve, official Gradle/Google Maven/Maven Central HTTP200; Gradle8.13 checksum + wrapper generation + dependency task PASS|android-env/host-network.log, diagnosis.md, wrapper-generation.log, dependencies.log|
|Android APK ban đầu|Gradle Wrapper testDebugUnitTest/assembleDebug BUILD SUCCESSFUL;9 JUnit tests|android-env/build-1.log; app/build/test-results/testDebugUnitTest/*.xml|
|Android fixes|Review2P2; exact seam suite RED2fail/13tests, sau Hermes sửa GREEN13tests|REV-AND-001B.txt; FIX-AND-001B-hermes-red.log, -green.log|
|Android build/lint cuối|PASS,21JUnit tests,7warnings/0errors|final-android-build.log; final-verification.json|
|Emulator API36|Smoke6nhóm,background2nhóm,SensorManager+scroll1nhóm PASS trên APK cuối|final-smoke-verified.log; AND-002A-runtime-final.log; sensor-scroll-verified.log; final-verification.json|
|AND-002B khí áp tương đối|RED host:3 test/2fail trước implementation; sau GREEN clean test toàn app24/24, lint7warnings/0errors, APK build PASS|AND-002B-codex.log; AND-002B-hermes-clean-test.log; AND-002B-hermes-final-build.log; app/build/test-results/testDebugUnitTest/*.xml|
|UX-002A-R1 UI PHONE_ONLY/độ cao|Static region đúng3 thay đổi; Gradle24tests, lint7warnings/0errors, APK PASS. Chưa emulator/TalkBack|UX-002A-agy.log (2 blocker); UX-002A-R1-codex.log; UX-002A-R1-hermes-gate.log|
|ESP local C++ host|11 nhóm+demo PASS; ASan/UBSan mặc định host PASS (không tắt leak check)|esp-core-hermes.log, esp-core-sanitizer-hermes.log, REV-ESP-CORE-001.txt|
|Framing lab Kotlin/C++|52 shared scenarios mỗi ngôn ngữ và56 cross-language exchanges PASS; host ASan/UBSan detect_leaks=1 PASS|IF-003-hermes.log, IF-003-hermes-sanitizer.log|
|Arduino đúng bo, BLE, điện thoại thật|CHƯA CHẠY — thiếu bo; frame vẫn draft|hardware-needed.md, interface-framing-draft.md|

Các đường evidence trong bảng tương đối docs/evidence, trừ app/build thuộc android. Không cộng các nhóm test khác loại thành một tổng “độ bao phủ”. Các test XML và scripts được đọc/parse khi tổng kết cuối.

## Thất bại không che giấu
- Lượt đầu Codex sandbox curl exit6 và gradlew exit127 vì Wrapper chưa có. Host Hermes sau đó build được; không kết luận DNS Linux hỏng.
- Codex FIX-AND-001B exit1 vì usage limit sau khi đã viết seam/tests RED. Hermes đọc/tiếp quản; không nhận task đã hoàn thành từ lời agent.
- REVIEW Android ban đầu FAIL2P2; sau sửa đã qua actual adapter build/emulator và rereview. Các lỗi compiler/callback/ownership/scroll và lỗi harness phát hiện sau đó vẫn giữ log RED/failure riêng; không biến chúng thành pass hồi tố.
- Sanitizer trong sandbox Codex có lỗi LeakSanitizer ptrace; host Hermes chạy lại ESP và protocol với leak check không tắt đã PASS.

## Phân biệt TDD và regression
CORE-002 là kiểm thử bổ sung sau triển khai. CORE-001 thiếu RED riêng ownership/wallclock/bounded scaffold; không tạo lịch sử giả. ESP và protocol agent viết nhiều test trước implementation theo nhóm: có RED/GREEN thật, không phải strict từng hành vi. Source fix Android có2bug được tái hiện RED trước sửa. Review recommendations không chặn được bổ sung như regression, không test-first claim.

## Giới hạn sản phẩm
Android có FGS thử nghiệm do người dùng bật, notification rõ và bounded wake lock trong VERIFYING; chưa phục hồi process death, vị trí/bước/pin/âm-rung. Screen-off test kiểm đếm ngược đang chạy, không chứng minh phát hiện mọi sự kiện khi Doze trên điện thoại thật. Detector là ngưỡng DEMO, chưa dữ liệu hiệu chỉnh/độ chính xác y khoa. Các nguồn invalid/thiếu không giả thành0. Sink chỉ bộ nhớ, không SMS/cuộc gọi/network; không có cảnh báo thật.
ESP host core caller-owned tick/clock, một active+last record, chưa GPIO/driver/flash/queue bền vững/Arduino firmware. Framing lab chỉ reassembly, không event ACK/retry/dedupe/live BLE/security bonding hoặc JSON decoder. Không đo pin48–72h hay độ nhạy/báo giả.

## Tài nguyên/quota
Tối đa2 triển khai đồng thời,1Gradle,1emulator. Codex usage limit đã quan sát; không suy đoán tổng quota/cost. AGY read-only inline review để tránh headless file permission, không bypass. Không secrets, không sudo/flash/push.
