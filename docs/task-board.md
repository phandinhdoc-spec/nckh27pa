# Bảng công việc
Chính sách hiện hành: AGENTS.md (QUOTA-FIRST). Hermes làm PM/giao việc/kiểm chứng/tích hợp, không primary implementer. Mỗi task đúng một worker và tối đa một reviewer khi cần; không tác vụ cạnh tranh. Các phiếu mới bắt buộc STATUS, OWNER, FILES, GOAL, RESULT, NEXT_ACTION. Không sửa chồng tệp; không tự commit/push. Các tên worker trong bảng lịch sử không phải routing cho nhiệm vụ mới. Quota hiện tại chưa tái kiểm.

|ID|Người/model|Phụ thuộc|Trạng thái|
|---|---|---|---|
|SUR-001,IF-001|Hermes|Không|DONE khảo sát/trích source; source gốc giữ nguyên|
|AND-001|Codex gpt-6-astra|SUR|Lượt cũ blocked sandbox DNS; đã thay bằng ENV-002/AND-001B|
|CORE-001/002|Codex + Hermes, Codex review|SUR|Host12nhóm PASS, review PASS, regression sau triển khai; core dùng trong app|
|ENV-002|Hermes|SUR|DONE host DNS/official checksum/Wrapper/build; không nới sandbox|
|AND-001B/FIX|Codex→Hermes sau quota; Codex/AGY review|CORE/ENV|Build/lint PASS;13tests saufix; smoke6/6 PASS trước FGS,2P2 đóng|
|DEC-001|Hermes; AGY Flash High review|ENV/IF|Sensor v1 parser6tests PASS; strict malformed/type/bounds, review PASS; chưa BLE|
|AND-002A|Hermes; AGY Flash High review (lịch sử trước policy mới)|M1|Build/lint21tests PASS;FGS/background2nhóm PASS trên emulator;review cuối PASS;chưa phần cứng thật|
|AND-002B|Codex gpt-5.6-sol;không reviewer|AND-002A|DONE altitude tương đối bounded; RED host2fail→GREEN; clean24tests+lint+APK PASS; chưa barometer thật|
|UX-002A|AGY gemini-3.8-flash-low|AND-002B|BLOCKED sau2 lượt headless auto-deny read_file; không sửa mã, không bypass|
|UX-002A-R1|Codex gpt-5.6-sol;không reviewer|UX-002A blocker|DONE 1file UI;24tests+lint+APK PASS;chưa emulator/TalkBack|
|DOC-CLOSE-001|AGY gemini-3.8-flash-low;không reviewer|final-verification.json|DONE worker trả báo cáo, Hermes đối chiếu/tích hợp, không sửa code|
|ESP-CORE-001|Codex; Hermes tests; Codex review|Không bo|DONE host11nhóm+sanitizer+demo; review PASS, P3 coverage bổ sung; không Arduino build|
|IF-002/IF-003|Hermes draft; Codex implement; AGY review|IF-001|Lab framing52scenarios mỗi phía+56crosslanguage PASS, sanitizer host PASS; draft CHƯA approved/live|
|UX-001|AGY Flash Low, Hermes hiệu chỉnh|SUR|DONE tư vấn; UI thực do AND-001B, không lấy checklist làm sản phẩm|
|ESP-001|Hermes + Codex/AGY khi khả dụng|Mã bo/linh kiện/nguồn|BLOCKED thông tin thực; xem hardware-needed.md|
|LINK-001|Hermes + reviewer|Chốt draft/bo, encoder/decoder|Chưa bật BLE; cần chốt giao thức+hardware|
|AND-003/DET-001|Nhóm logic/reviewer|LINK/cảm biến, dữ liệu nhãn|Chưa fusion/luật hiệu chỉnh; không tự xác nhận độ chính xác từdemo|
|ALERT-002|Nhóm logic/UI + chủ dự án|Kênh và người nhận thử có phép|Chưa gửi ngoài; giữ fake sink|
|VAL-001|Hermes/người dùng|Thiết bị/kịch bản an toàn|Chưa thực nghiệm người/pin/độ chính xác|
|OWNER-GATE|Chủ dự án|Lõi ổn định|GNSS/4G/mua/phát hành chưa mở|

Giới hạn TDD lịch sử vẫn ghi test-report, không cần tái viết mã chỉ để tạo RED giả. Codex usage limit khi FIX; Hermes tiếp quản sau đọc source. AGY pro timeout một số review, đổi input/model có lý do; Flash High đã trả review. Không còn triển khai Codex chạy ngầm. AND-002/UX-002 là backlog rộng: FGS đang tách AND-002A; persist/location/bước/accessibility chuyên sâu chưa xong, không gắn DONE cả giai đoạn.

## SUR-001
Mục tiêu: hiện trạng không phá thay đổi. Nguồn: chỉ thị §1,8,10. Người: Hermes. Đọc: cả hai kế hoạch, setup script, wrapper env/công cụ; Git, SDK, USB. Phạm vi sửa: docs/evidence, git init/.gitignore mới. Phụ thuộc: không. Bàn giao: hash baseline, inventory/status. Nghiệm thu: đọc đủ, Git/thiết bị và CLI từ lệnh thật, không secrets. Kiểm: so hash, git status, tool help. Retry tối đa2 mỗi hướng.

## IF-001
Mục tiêu: nguồn thống nhất không đổi API. Nguồn Android §6–9, ESP §8. Người Hermes. Đọc hai kế hoạch. Phạm vi docs/interface-contract.md, fixtures, decisions. Phụ thuộc SUR-001. Bàn giao schema nguyên văn/UUID/units/gaps/vector JSON. Nghiệm thu so ví dụ hai nguồn bằng parser, giữ tên và đánh dấu đề xuất. Kiểm JSON load + source diff/hash. Retry2. Không coi dữ liệu fixture là decoder test.

## IF-002
Mục tiêu: đặc tả frame bounded, ACK/time/reconnect/dedupe và ma trận sự kiện. Nguồn Android §6,8,10; ESP §7.3,8.5–8.8. Nhóm Codex; reviewer độc lập. Đọc contract, decisions, fixtures. Sửa docs/interface-contract.md, docs/fixtures/ và tests giao thức được phân vùng; không sửa source plans. Phụ thuộc IF-001 và giải quyết C02–C07. Bàn giao byte spec + vectors + test host. Nghiệm thu MTU nhỏ, thiếu/lặp/sai, lệch giờ/reboot, ACK mất, hủy sai ID không gây tác dụng. Kiểm TDD/2 phía; 2 lần không tiến triển dừng. Chỉ tích hợp khi proposal được chốt và review.

## ESP-001
Mục tiêu: nhận dạng bo thật, chốt cấu hình Arduino CLI=IDE. Nguồn ESP §3,5,13 + chỉ thị Arduino. Người Hermes/Codex. Đọc ảnh/mã bo, sơ đồ nguồn/linh kiện chủ dự án cung cấp, contract. Sửa esp/board/, esp/README.md, profile build; không GPIO tùy đoán. Phụ thuộc thông tin bo. Bàn giao FQBN+options/core/library lock + CLI command, cấu hình IDE tương ứng. Nghiệm thu compile đúng bo (chưa flash). Kiểm nckh-arduino board details/compile; 2 lần dừng. Flash cần phép riêng.

## REV-001
Mục tiêu: phản biện độc lập code cảnh báo. Nguồn Android §4.4,4.5,7,10 + AND-001. Nhóm reviewer không triển khai, model xác minh trước gọi. Đọc chỉ source/test/diff AND-001. Không sửa source; output docs/evidence/review. Phụ thuộc AND-001. Bàn giao lỗi theo severity+file:line, race deadline/cancel/SOS, false SENT, mất mẫu/quality. Nghiệm thu không còn lỗi chặn trước demo, Hermes chạy lại test. Retry2; không tự cho passed từ lời agent.

## DEMO-001
Mục tiêu: demo Android dữ liệu→xử lý→UI→test sink. Người Hermes. Nguồn AND-001, Android §10. Đọc README/test result/review. Sửa chỉ evidence và report, không source trừ task sửa lỗi riêng. Phụ thuộc AND-001/REV-001. Bàn giao APK từ wrapper + test XML + emulator evidence nếu có. Nghiệm thu xác minh 10giây không bấm dẫn cảnh báo thử; SAFE và SOS đúng; không người thật. Kiểm build một lần, một emulator, bounded timeout; 2 lần lỗi dừng đổi cách. Không gộp emulator thành điện thoại thật.

## Backlog: phiếu tối thiểu trước tinh chỉnh
Mọi mục dưới: model chưa chốt; chỉ chọn từ model được gọi thành công, retry tối đa2 lần không tiến triển; TDD cho logic, review riêng phần rủi ro. Trước chạy phải liệt kê tệp cụ thể trong phân vùng, không tạo tác vụ mơ hồ.

|ID / mục tiêu|Nguồn & đầu vào|Nhóm / phạm vi sửa|Phụ thuộc|Bàn giao và nghiệm thu / kiểm chứng|
|---|---|---|---|---|
|AND-002 cảm biến đầy đủ/chạy nền|Android §4.5,8,9; AND-001|Codex; phonesensors,service,data,manifest và tests liên quan|M1|Optional null/units, bounded buffer, quyền/foreground/offline, lifecycle; unit + emulator + điện thoại khóa màn hình|
|UX-002 bốn mục/trạng thái|Android §3; UX-001, UI contract|AGY; ui và UI tests riêng|M1|Bốn mục, accessibility, quyền/thiếu/mất/lỗi; Compose UI tests + TalkBack thủ công|
|ESP-002 đo và cảnh báo cục bộ|ESP §5–7,11; board profile/contract|Codex; esp firmware sensors/detection/interaction/storage|ESP-001,IF-002|Arduino build; IMU lỗi không chặn SOS; timeout, ACK không hủy, buffer/pin; host tests + bo thật có phép|
|LINK-001 BLE hai phía|Android §6,8; ESP §8; vectors|Codex/reviewer; protocol/device Android và protocol/transport ESP theo lượt|IF-002,ESP-002,AND-001|Decoder rejects bad, replay/dedupe/reconnect; test cả phía và link thật; không gọi host test là BLE thật|
|AND-003 fusion ba chế độ|Android §4.5.4; nguồn đã chuẩn|Codex; fusion/ tests|LINK-001,AND-002|Clock/quality/dropout/mâu thuẫn đúng; test PHONE_ONLY/HYBRID/ESP32_ONLY_INPUT|
|DET-001 luật đa giai đoạn|Android §5; ESP §6,7; data có nhãn|Codex/reviewer; detection/features tests từng phía|AND-003|Ngã mạnh/thấp/instability, false-positive controls, triggerReasons; split calibration/evaluation và report|
|ALERT-002 vị trí/liên hệ/nhật ký|Android §7–9; UI + người nhận thử được phép|Codex/AGY; alert/location/data/ui chia tệp|AND-002,UX-002|No-response/offline/no-GPS, retry bounded, không SENT giả; unit + thử kênh được phép, nhật ký không PII|
|VAL-001 đối chứng/pin|Android §10; ESP §10,12; firmware/app build đã khóa|Hermes/người dùng; docs và dữ liệu được phép|Lõi pass|Protocol nghiên cứu an toàn, placements, số đo pin/báo giả/độ trễ, tách dataset; không khẳng định lâm sàng|
