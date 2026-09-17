# REV-CORE-001 — Phản biện lõi cảnh báo
Mục tiêu: tìm lỗi logic/an toàn độc lập triển khai CORE-001, không tự sửa.
Nguồn: Android §4.4,7,9; docs/tasks/CORE-001.md.
Người: reviewer Hermes qua delegate_task (tool có sẵn, ngữ cảnh độc lập, kế thừa model phiên); không gọi vai trò này là Codex/AGY. Không dùng lại hội thoại người triển khai. UX review không phải code review.
Đầu vào: android/core/src, tests, run-tests.sh, README, log kiểm chứng Hermes; chỉ vùng này và phiếu. Phạm vi sửa: không; Hermes lưu kết quả reviewer tại docs/evidence/REV-CORE-001.json.
Phụ thuộc: CORE-001 kết thúc, Hermes chạy lại suite.
Bàn giao: verdict PASS/FAIL, findings file:line + repro; phân biệt blocker và việc chưa nằm trong core. Kiểm no-response deadline, SAFE boundary, SOS/reentrant sink, duplicate, failure, completion, monotonic/overflow, bounded storage, ownership.
Nghiệm thu: không còn lỗi chặn trong phạm vi; reviewer không coi deadline chỉ tick khi được gọi là tự chạy nền. Việc tích hợp Android còn chặn là giới hạn, không che đi.
Cách kiểm: đọc diff/source, có thể chạy test read-only nếu output tạm không sửa tracked/source, không network/Gradle/emulator. Giới hạn2 lượt sửa-review, sau đó dừng và báo. Hermes quyết định tích hợp; không commit/push.
