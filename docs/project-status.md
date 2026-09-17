# Trạng thái dự án
## Chính sách điều phối hiện hành
AGENTS.md lưu nguyên văn QUOTA-FIRST của chủ dự án. Hermes làm PM, không primary implementer; một task một worker, tối đa một reviewer khi cần. Routine/docs/tests dùng AGY Gemini phù hợp; triển khai nhiều file/build-fix dùng Codex; review khó/safety dùng AGY Claude; Level4 chỉ khi có lý do escalation. Không chạy lại phần đã hoàn tất để hợp thức hóa policy mới. DOC-CLOSE-001 do AGY gemini-3.8-flash-low viết từ dữ kiện đã kiểm chứng, Hermes đối chiếu và tích hợp; không reviewer thứ hai.
## Mốc khảo sát M0
Đã đọc đầy đủ android/android-plan.md (825 dòng), esp/esp32-plan.md (491 dòng) và setup-nckh27pa-26.04.sh. Ban đầu chỉ có 3 tệp, chưa Git/source/build. Đã git init main, chưa commit, không remote; hash nguồn gốc ở docs/evidence/baseline.json. Không chạy lại installer.
Máy quan sát: Linux x86_64, RAM15Gi hiển thị, khoảng12Gi available lúc khảo sát. Không có adb device hoặc Arduino board/ttyUSB/ttyACM. KVM usable; emulator API36 đã khởi động ở lượt tiếp tục (emulator-5554); chưa thấy điện thoại/ESP32 thật.

## Kết quả mốc tiếp tục
CORE-002: 12 nhóm regression và demo Hermes chạy exit0; reviewer PASS, đề xuất sửa assertion trong test lỗi sink đã áp dụng và chạy lại. Tests bổ sung sau triển khai, không phải TDD hồi tố.
ENV-002: host DNS/HTTP hoạt động; Gradle8.13 checksum chính thức + Wrapper + dependencies thành công với JDK21. Android build-1 testDebugUnitTest/assembleDebug PASS, có APK. Không nới sandbox Codex. Lỗi cũ không còn chặn host.
AND-001B/AND-002A: source UI Compose4tab, SensorManager, replay/detector DEMO, countdown/sink giả và FGS thử nghiệm đã qua build/lint. APK qua smoke6nhóm, background2nhóm, SensorManager+scroll1nhóm trên emulator. Review/fix lịch sử giữ nguyên trong evidence; không gọi emulator là điện thoại thật. FGS/permission/ownership và scroll fixes đã kiểm chứng. Chi tiết docs/evidence/final-verification.json.
AND-002B: Codex gpt-5.6-sol triển khai độ cao tương đối khí áp trong history bounded 5 giây/64 mẫu, không dùng làm detector. Hermes xác nhận RED2fail trước GREEN; clean test toàn app 24/24, lint7warnings/0errors và assembleDebug PASS. Chưa thử barometer thật.
UX-002A-R1: sau khi AGY Flash Low bị headless auto-deny read_file hai lượt, Codex gpt-5.6-sol hoàn thành patch UI một tệp: cảnh báo phải mang điện thoại trong PHONE_ONLY và hiển thị độ cao tương đối/null. Hermes chạy 24tests+lint+assemble PASS. Chưa emulator/TalkBack cho thay đổi text này; blocker AGY được giữ riêng, không che giấu.
ESP-CORE-001: C++17 host11nhóm test + demo và ASan/UBSan (kể cả leak check mặc định host) PASS do Hermes chạy; reviewer PASS static,2 đề xuất coverage không chặn. Chưa Arduino build/FQBN/bo thật.
IF-003: lab Kotlin/C++ framing với golden vectors,52scenarios mỗi ngôn ngữ và56cross-language PASS, sanitizer host PASS, review static PASS. DEC-001 sensor JSON decoder có6tests PASS và review PASS; chưa event/status decoder, live BLE/ACK. Frame draft chưa approved.

## Mốc nhỏ M1 — đạt demo trong phạm vi emulator
PHONE_ONLY nhận cảm biến thật khi Activity hoạt động + replay dữ liệu giả → xử lý/máy trạng thái → UI → cảnh báo vào bộ nhận thử nghiệm. Không phản hồi 10 giây vẫn tự cảnh báo; SAFE hủy trước hạn; SOS/NEED_HELP lập tức; lỗi gửi không hiện thành công. Không gửi SMS/cuộc gọi thật. M1 không phải nghiệm thu phiên bản đầu theo §12.
Cổng nghiệm thu: test JVM + Gradle Wrapper assembleDebug; demo emulator riêng nếu tài nguyên/SDK cho phép. Nếu chưa chạy thiết bị phải ghi rõ. Không nghiệm thu độ chính xác ngã.
M2: chốt frame BLE + bo/FQBN thực → Arduino build → thiết bị/SOS → Android → bộ nhận thử nghiệm. Không chọn bo giả để tuyên bố build thực tế.
M3: cảm biến đầy đủ/chạy nền/quyền; fusion, luật đa giai đoạn; sự kiện bền vững; UI bốn mục. M4: cảnh báo thực tế có phép và nghiên cứu an toàn theo hai kế hoạch.

## Ma trận yêu cầu → nghiệm thu
|ID|Nguồn|Chức năng / bằng chứng cần đạt|Nhiệm vụ|
|---|---|---|---|
|R01|Android §2,4.5,10.1|PHONE_ONLY không cần ESP; packet đúng đơn vị, cảm biến thiếu null; test + điện thoại|AND-001,AND-002|
|R02|Android §3|4 mục, chữ lớn/TalkBack, trạng thái nguồn/quyền/thiếu dữ liệu/lỗi; kiểm UI|UX-001,UX-002|
|R03|Android §4.4,6.7; ESP §7|NO_RESPONSE tự cảnh báo, SOS ngay, SAFE; monotonic, chống lặp, ACK không hủy; unit + thiết bị|AND-001,ESP-002|
|R04|Android §4.5.4|3 chế độ, đồng bộ/quality, nguồn mâu thuẫn không kết luận an toàn; test dropout/clock|AND-003|
|R05|Android §5; ESP §6–7|Ngã mạnh/thấp/loạng choạng và hoạt động thường; đa bằng chứng, lý do; dữ liệu có nhãn|DET-001|
|R06|Android §6,8; ESP §8,11|UUID/API v1 chính xác, parser sai/thiếu/trùng, MTU nhỏ/ACK/reboot/reconnect/bonding|IF-001,IF-002,LINK-001|
|R07|Android §7,9|Vị trí tốt nhất, thiếu GPS không chặn, quyền/kênh/thứ tự người thân, nhật ký bền vững|ALERT-002|
|R08|Android §8,9,11|Foreground service đúng quyền, offline, khóa màn hình, retention hữu hạn, log không PII|AND-002|
|R09|ESP §3,5–6|Bo/GPIO/nguồn đúng, IMU50–100Hz, pin/sạc/barometer optional; Arduino CLI=IDE|ESP-001,ESP-002|
|R10|ESP §7,11|Mất BLE vẫn SOS/báo cục bộ, lỗi IMU không chặn SOS, queue bền vững hữu hạn|ESP-002|
|R11|ESP §10,12; Android §10|Đo pin48–72h, false alarm, độ nhạy/bỏ sót/độ trễ; tách tập và vị trí mang máy|VAL-001|
|R12|ESP §2,9|GNSS/4G mở rộng sau lõi; không tự mua/SIM/backend/phát hành|OWNER-GATE|

## Công cụ và điều phối đã quan sát
- Codex CLI0.154.0: login status=ChatGPT; exec, --model, workspace-write, review có trong help. Cache: gpt-6-astra, gpt-reserve, gpt-5.6-sol, gpt-5.6-terra, gpt-5.6-luna, gpt-5.5, codex-auto-review. Cache không chứng minh model dùng được. AND-001 gọi thành công gpt-6-astra theo header CLI và lượt trả thực, nhưng hạ tầng DNS sandbox chặn tải Gradle. CORE-001 giữ model này vì lỗi hạ tầng không phải lý do đổi model. Chưa chốt model nhẹ khác cho tới khi thử.
- AGY có --print, --model, --mode plan/accept-edits, --sandbox. agy models trả gemini-3.8-flash-{high,medium,low}, gemini-3.7-flash-{high,medium,low}, gemini-3.6-flash-{high,medium,low}, gemini-3.1-pro-{high,low}, claude-sonnet-4-6, claude-opus-4-6-thinking, gpt-oss-120b-medium. UX-001 gọi gemini-3.8-flash-low: lần đọc file bị headless permission từ chối; lần dùng nội dung inline không tool trả được checklist. Hermes hiệu chỉnh tại docs/evidence/UX-001-hermes-review.md. Những model khác chưa được probe inference.
- Không thấy số quota còn, giới hạn đồng thời phía nhà cung cấp, token/chi phí trong lượt kiểm kê; ghi KHÔNG BIẾT, không giả định miễn phí/vô hạn. Không dùng khả năng agent con ngầm. Chính sách cục bộ: tối đa2 tác vụ triển khai,1 Gradle,1 emulator; ngừng hướng sau2 lần không tiến triển.
- Java môi trường=25.0.3; SDK36/build-tools36.0.0; Arduino CLI1.5.1/core esp32:esp323.3.11. Wrapper nckh-arduino dùng ~/.arduinoIDE/arduino-cli.yaml như IDE. JDK/Gradle phải xác minh tương thích lúc build.

## Rủi ro và tiếp theo
Codex báo usage limit trong FIX-AND-001B (exit1); không thử lại cùng đường. Thời điểm CLI gợi ý reset giữ nguyên trong log, không đoán timezone hoặc số quota. AGY gemini-3.1-pro-high được gọi review qua inline source (model trong danh sách), chưa coi thành công inference cho tới có kết quả. Hermes làm fix/build/QA, không nới sandbox. Chưa có số tiền/tổng token dự án.
Tiếp tục review và emulator, sau đó mới tích hợp mốc demo. Thông tin bo/IMU/nguồn/GPIO thực thiếu ghi docs/hardware-needed.md; không chặn host logic. C01–C07 và framing draft còn cần review/chốt trước live BLE. Chưa phát hành/cảnh báo người thật.

