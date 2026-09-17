# Hermes kiểm tra đầu ra UX-001
Nguồn: UX-001-agy-inline.txt. AGY trả được nội dung với gemini-3.8-flash-low sau khi cung cấp đoạn kế hoạch inline; lần đọc tệp trước bị permission auto-denied. Không bật bypass hoặc sửa quyền toàn cục.
Chấp nhận dùng làm checklist dự thảo, KHÔNG nghiệm thu UI (chưa có ứng dụng). Các điểm phải sửa khi dùng:
- “an toàn tuyệt đối” là khẳng định không có căn cứ; loại bỏ. Không dùng “đang bảo vệ bạn” khi thuật toán chưa kiểm chứng. Hiển thị “Đang thu dữ liệu điện thoại — bản thử nghiệm”.
- Accelerometer/gyro thông thường không có quyền runtime chung tên “quyền cảm biến”; phân biệt ACTIVITY_RECOGNITION/bước, location, notifications. Không đồng nhất full-screen intent với quyền vẽ đè. Màn hình khóa chỉ khi hệ điều hành và quyền cho phép (Android:115).
- Điện thoại nằm yên không chứng minh nằm trên bàn; dùng “có thể”, confidence/UNKNOWN. Không tạo suy luận chắc chắn từ một trạng thái.
- AWAITING_HELP chỉ sau sink báo gửi thành công. Sink ACK chỉ là nhận thử nghiệm, không chứng minh người thân nhận hay đã hỗ trợ.
- Test sink trong M1 ở bộ nhớ, không cần Mock Server/mạng. Không thêm backend. UI báo thử lại chỉ khi thực sự có retry; lỗi không báo SENT.
- Đề xuất nút quay số thật không đưa vào bản thử. Phát triển này không tự khởi dialer/SMS/emergency call.
- Checklist thiếu rõ trường hợp accelerometer không có/lỗi: phải báo không có dữ liệu giám sát, SOS thử vẫn hoạt động; không hiện an toàn giả.
Kết luận: UX-001 hoàn thành đầu ra tư vấn có hiệu chỉnh trên; chưa đủ là review code độc lập. REV-001 vẫn bắt buộc với lõi cảnh báo.
