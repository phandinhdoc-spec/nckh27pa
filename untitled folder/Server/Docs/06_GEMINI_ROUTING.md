# Định tuyến mô hình Gemini

## Nhóm tác vụ
- fast: phân loại, trích xuất, chuẩn hóa text.
- vision: mô tả và hỏi đáp ảnh.
- reasoning: suy luận nhiều bước.
- fallback: dùng khi model chính lỗi.

## Quy tắc
- Apple Vision xử lý trước nếu phù hợp.
- Không gọi Gemini cho lệnh hệ thống đơn giản.
- Pi không chọn model.
- Tên model lưu trong .env.
- Router chọn model theo tác vụ.
