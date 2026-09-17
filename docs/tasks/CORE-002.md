# CORE-002 — Đóng khoảng trống kiểm thử trước tích hợp
Trạng thái: READY, chưa chạy, không tự coi review PASS là hoàn tất CORE-001.
Mục tiêu: bổ sung regression biên thời gian và successful reentrant sink; xử lý minh bạch thiếu TDD trước chuyển lõi vào ứng dụng.
Nguồn: CORE-001; REV-CORE-001.json; Android §4.4,7,9.
Người/nhóm/model: Codex gpt-6-astra đã gọi được; reviewer phiên riêng. Hermes quyết định khi khởi chạy.
Đầu vào: android/core/src, tests, README, log RED/GREEN, review.
Phạm vi sửa: android/core/tests, evidence, README; source chỉ nếu test tìm bug và có RED tái hiện. Không đổi API/wire hoặc kế hoạch.
Phụ thuộc: REV-CORE-001 hoàn thành.
Bàn giao: permanent regression tests successful reentrant sink, Long.MAX_VALUE clock/timeout; bằng chứng chạy thật. Với hành vi đã tồn tại, gọi đúng là regression bổ sung, không gán RED/GREEN lịch sử giả. Theo chỉ thị tiếp theo của chủ dự án: ghi rõ kiểm thử bổ sung sau triển khai; không gọi là test-first cho mã đã viết. Giữ thiếu sót lịch sử trong báo cáo, không tái viết mã đang chạy chỉ để tạo bằng chứng RED/GREEN hồi tố.
Nghiệm thu: suite + demo exit0, không phát cảnh báo thật; review không lỗi chặn; không đánh dấu CORE-001 đạt đầy đủ nếu thiếu TDD chưa giải quyết.
Kiểm chứng: ./android/core/run-tests.sh và demo; mỗi hướng tối đa2 lần không tiến triển. Không Gradle/emulator/network, không xóa source người dùng hoặc nới sandbox.
