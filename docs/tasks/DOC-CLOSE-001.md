# DOC-CLOSE-001
STATUS: DONE
OWNER: AGY / gemini-3.8-flash-low — một worker, không reviewer; model đã gọi được, chưa có giá/quota xác thực để khẳng định rẻ nhất.
FILES: input docs/evidence/DOC-CLOSE-001-input.txt; output docs/evidence/DOC-CLOSE-001.txt; Hermes tích hợp các mục stale trong docs/project-status.md, docs/test-report.md, docs/task-board.md. Không source code.
GOAL: Viết báo cáo đóng mốc ngắn bằng tiếng Việt, phân biệt build/emulator/host/hardware; không tạo task triển khai mới hoặc bịa blockers.
RESULT: Worker trả3đoạn đóng mốc, CLIexit0; Hermes đối chiếu số liệu với final-verification.json đã kiểm chứng và cập nhật các mục stale. Không có reviewer/model thứ hai; không thay source.
NEXT_ACTION: Nhiệm vụ tiếp theo phải chọn một worker theo AGENTS.md trước khi triển khai; không tự tiếp quản code khi worker chạm quota. Không chạy lại nhiệm vụ này.
Nguồn: policy quota-first mới và docs/evidence/final-verification.json. Phụ thuộc: triển khai/test đã kết thúc. Nghiệm thu: đúng dữ kiện, không nói toàn dự án xong hoặc tất cả việc còn lại bị chặn. Worker chỉ nhận inline input, không tool/read/write/network. Giới hạn một lượt; lỗi thì giữ task chưa xong, không tự gọi model mạnh.
