# Kế hoạch kiểm thử

## Pi audio/wake
- Stream mic liên tục, VAD không gửi đoạn im lặng.
- VAD segment kết thúc theo im lặng/giới hạn, không cắt bỏ phần đầu/cuối câu.
- MP3 được tạo đúng định dạng và gửi tới Groq bằng Bearer API key, model cấu hình; lỗi/timeout được xử lý.
- Phân biệt wake phrase “Thiên Nhãn” không phân biệt hoa thường/dấu; bỏ transcript không wake, chấp nhận wake + lệnh cùng đoạn.
- Không có GPIO/button wait hay kết nối Bluetooth tự động khi khởi động.
- Lệnh kết nối Bluetooth chỉ connect khi được nhận dạng; thông báo thành công/thất bại luôn phát qua MAX.

## HTTP
- `/health`, `/plan`, `/vision/ocr`, `/tts`, `/command`: có/không token, dữ liệu rỗng/lớn, Content-Type sai.
- `/plan` trả mode/needsImage đúng với yêu cầu; `/command` nhận text/ảnh.

## Pi–Mac–Groq
- Pi gửi speech MP3 lên Groq, nhận text, gọi `/plan`, chụp ảnh khi cần, gọi `/command`, phát MP3 qua MAX.
- Tailscale, timeout/retry; lỗi mạng Groq/Mac không làm dừng vòng nghe.

## Vision, Gemini, TTS
- OCR/observe có ảnh, chat không ảnh, văn bản tiếng Việt, timeout/quota/fallback; xác nhận MP3 phát được.

Chạy kiểm thử logic trên Mac không thay thế kiểm tra mic, loa, camera, adapter Bluetooth trên Pi Zero 2 W thật.
