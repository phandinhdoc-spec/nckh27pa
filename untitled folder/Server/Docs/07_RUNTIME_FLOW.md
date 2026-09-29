# Luồng vận hành

## Khởi động và chờ wake phrase
Pi phát qua MAX98357A lời mời nói “Thiên Nhãn để bắt đầu”. Mic USB chạy stream liên tục, VAD cục bộ chia đoạn lời nói; Pi mã hóa đoạn có tiếng nói thành MP3 và gửi thẳng lên Groq STT. Transcript không có wake phrase được bỏ qua. Bluetooth không tự kết nối.

## Hỏi vật trước mặt
Sau wake phrase, Pi nhận dạng câu hỏi bằng Groq STT, gọi `/plan` để Mac xác định `needsImage`, chụp ảnh nếu cần, rồi gọi `/command` với text/mode/ảnh. Mac chạy Gemini Vision, TTS và trả MP3; Pi phát bằng MAX.

## OCR
Pi gọi `/plan`; nếu cần đọc chữ, Pi chụp ảnh và gửi text/ảnh đến `/command`. Mac dùng Apple Vision OCR, gọi Gemini khi người dùng yêu cầu diễn giải/tóm tắt, sau đó TTS trả MP3.

## Lệnh kết nối Bluetooth
Chỉ lệnh giọng nói rõ ràng như “hãy kết nối Bluetooth” mới chạy kết nối có giới hạn. Pi thông báo thành công/thất bại qua MAX; không tự đổi đầu ra khỏi MAX.

## Mất dịch vụ
Lỗi Groq được báo qua MAX và vòng nghe tiếp tục. Lỗi Mac `/plan` hoặc `/command` được báo qua MAX; Pi tiếp tục nghe wake phrase.
