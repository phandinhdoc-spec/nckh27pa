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

## Lượt sửa luồng SOS (2026-09-18, T6/T7) — bằng chứng thật
|Hạng mục|Kết quả quan sát|Đường dẫn evidence|
|---|---|---|
|Test đơn vị Android|170 PASS, 0 fail/error/skip sau `--rerun-tasks`; Hermes tự đọc XML, không tin báo cáo worker|android/app/build/test-results/testDebugUnitTest/|
|APK|`assembleDebug` PASS (`app-debug.apk`, 12.566.848 byte)|android/app/build/outputs/apk/debug/|
|Vị trí: fused, không cần satellite fix|`FallSafe/Location: source=FUSED accuracy=5.0 cause=none` ngay trong lúc đếm ngược; thẻ UI "Đã xác định ± 5 m • vừa xong"|docs/evidence/sos-location/logcat-location-during-countdown.txt, location-card.txt|
|Vị trí: fallback cache|`source=CACHED accuracy=5.0 age=117559 cause=TIMEOUT` → SOS VẪN gửi|docs/evidence/sos-location/logcat-dispatch.txt|
|SMS thật|`content://sms/sent` trả về row: `address=0901234567` + nội dung có `https://maps.google.com/?q=10.8231,106.62969833333334`, độ chính xác 5 m|docs/evidence/sos-location/sent-sms-provider.txt|
|CALL thật|`FallSafe/CALL: status=STARTED`, `SIM_CALL:SUCCESS`; màn hình gọi điện của emulator hiện "Calling… 0901234567"|docs/evidence/sos-location/sos-state.txt, logcat-dispatch.txt|
|Báo cáo SOS 5 bước|`Vị trí: Thành công • Tin nhắn • Cuộc gọi trợ giúp • Cuộc gọi SIM • Liên kết bản đồ`|docs/evidence/sos-location/sos-state.txt|
|Review L3 (AGY claude-sonnet-4-6, plan mode)|0 BLOCKER, 4 MAJOR (F-01/F-02/F-04/F-09), 3 MINOR (F-05/F-06/F-07); đã sửa 5, từ chối 3 có lý do|.ai/review-prompt-T6.txt + .ai/T6-fixes-round2.txt|
|Harness cũ vẫn xanh|`permission-flow-check.py` **11/11 PASS** sau khi cập nhật chính harness: báo cáo SOS nay 5 dòng trong hộp thoại cuộn 400dp nên check phải cuộn trước khi khẳng định đủ bước, và cách dò "chưa có fix" phải theo chữ mới của thẻ vị trí (nếu không, check cũ sẽ xanh giả)|docs/evidence/permission-flow/results.json|
|Không kiểm chứng được|Biên nhận giao SMS của nhà mạng, cuộc gọi qua mạng di động thật, GPS ngoài trời, hộp thoại quyền theo hãng máy — cần thiết bị thật|docs/next-gate.md §5|

## Lượt sửa lỗi SOS runtime (2026-09-19, T8 — Terra Medium/Codex) — bằng chứng thật, KHÔNG phải PASS thiết bị thật
|Hạng mục|Kết quả quan sát (Hermes tự chạy lại, không tin báo cáo worker)|Đường dẫn evidence|
|---|---|---|
|Test đơn vị Android|**27 suite / 175 test / 0 fail / 0 error / 0 skip** sau `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`; đếm từ XML|android/app/build/test-results/testDebugUnitTest/TEST-*.xml|
|APK|`assembleDebug` PASS, 12.566.848 byte, sha256 `2c16eca3f14a7b216042e573bf17a3b8275bfd05f4997909b8f5a7a5cd1daa06`, đã cài lên emulator-5554|android/app/build/outputs/apk/debug/app-debug.apk|
|SOS → LOCATION → SMS → CALL (bình thường)|Công tắc vị trí BẬT: ngã thật bằng `adb emu sensor` → `source=FUSED accuracy=5.0`; SMS thật trong `content://sms/sent` (`address=0901234567`, `https://maps.google.com/?q=10.8231,106.6296983`, "Độ chính xác: 5 m"); `SIM_CALL=SUCCESS` + `FallSafe/CALL: status=STARTED`; 5/5 bước trong báo cáo|docs/evidence/sos-location/results.json (2/2 PASS), logcat-dispatch.txt, sent-sms-provider.txt, sos-state.txt|
|LOCATION → MAPS|Thẻ vị trí lên trạng thái "Đã xác định ± 5 m" và không còn thông báo chặn "ra chỗ thoáng"|docs/evidence/sos-location/location-card.txt|
|Nhánh suy giảm (công tắc Vị trí TẮT)|`LOCATION=UNAVAILABLE (cause=PROVIDER_DISABLED)`, nhưng `SMS=SUCCESS`, `SIM_CALL=SUCCESS`, `MAP_LINK=SKIPPED` ⇒ SMS/CALL không phụ thuộc vị trí|docs/evidence/sos-runtime/hermes-degraded-location-off.logcat.txt, .sms-sent.txt, .txt|
|Sửa lỗi thật trong lượt này|(1) `PROVIDER_DISABLED` không bao giờ được trả về khi công tắc vị trí tắt; (2) nhánh đa SIM CHẶN HẲN SMS khẩn cấp → nay dùng SIM mặc định và ghi `subscriptionId`; (3) SIM call báo "STARTED" chỉ vì post thành công → nay chờ kết quả `startActivity` thật (≤ 2 s); (4) `SmsPartAggregation.require` có thể crash khi callback muộn → nay trả kết quả cũ; (5) readiness bỏ qua phần cứng telephony → nay có `isSupported`; (6) log SOS phân mảnh → nay một dòng `FallSafe/SOS` cho mỗi bước, không log số điện thoại|.ai/task_on_progress.md §T8|
|Quyết định D09 (cập nhật 2026-09-19)|Lượt T8 từng gỡ guard "chỉ gọi SIM khi adapter thoại máy chủ chưa tiếp nhận" và T8-fix1 khôi phục `SKIPPED`. **Cập nhật 2026-09-19: chủ dự án chốt lại D09** — adapter máy chủ chỉ là nhánh best-effort PHỤ TRỢ, máy chủ `STARTED` KHÔNG suppress cuộc gọi SIM trên handset, nên guard `SKIPPED` đó KHÔNG còn hiệu lực; code luôn gọi handset là ĐÚNG. Tài liệu đã cập nhật theo: `docs/decisions.md` D09, `docs/next-gate.md` §4, `docs/permission-flow.md` §4|docs/decisions.md, docs/next-gate.md, docs/permission-flow.md (§4), .ai/T8-fix1-d09-guard.md (lịch sử)|
|Điều kiện build|Java mặc định của máy là 25.0.3 làm Gradle 8.13 fail trước khi cấu hình project; build phải dùng JDK 17 (`/usr/lib/jvm/java-17-openjdk-amd64`)|.ai/task_on_progress.md §T8 DO_NOT_REPEAT|
|Harness quyền/SOS/maps (chạy lại trên artifact cuối)|`permission-flow-check.py` **11/11 PASS** (first-run, permission-center, request-deny, permanent-denial, grant-on-resume, approximate-only, sos-degradation, gps-off, maps-intents, maps-opens-google-maps, maps-fallback-browser)|docs/evidence/permission-flow/results.json|
|Không kiểm chứng được (không được tính là PASS)|Biên nhận giao SMS của nhà mạng, cuộc gọi SIM thật qua mạng di động, đa SIM thật, GNSS ngoài trời, FGS khi khoá màn hình — **chưa có điện thoại thật** nên KHÔNG kết luận DEVICE VERIFIED|docs/next-gate.md §5, docs/permission-flow.md §6|
|Còn lệch giữa code và tài liệu (cần chủ dự án chốt)|(a) `docs/api-contract.md:1124` ghi "nhiều SIM phải dùng subscription người dùng chọn, không tự chọn ngầm", trong khi code nay tự dùng SIM mặc định của hệ thống để SMS khẩn cấp không bị chặn; (b) ghi chú ở `permissions/CapabilityAccess.kt:112` + `docs/permission-flow.md:14-16` yêu cầu người dùng cấp `READ_PHONE_STATE` nhưng app KHÔNG BAO GIỜ xin quyền này lúc chạy ⇒ ghi chú không thể thực hiện được|docs/api-contract.md, android/app/src/main/java/vn/nckh27pa/fallsafe/permissions/CapabilityAccess.kt|

