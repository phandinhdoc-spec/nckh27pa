# Kiến trúc hệ thống hiện tại

```text
Người dùng
  ↓ nói “Thiên Nhãn”
Raspberry Pi Zero 2 W
  ├─ Mic USB luôn mở; VAD cục bộ chia đoạn lời nói
  ├─ Mã hóa đoạn nói thành MP3, gửi Groq STT từ Pi
  ├─ Xác định wake phrase và lệnh kết nối Bluetooth tại Pi
  ├─ Camera + GY25/GY63 khi cần
  ├─ Loa MAX98357A cho mọi thông báo và phản hồi
  └─ Gửi text → Mac /plan → Pi chụp ảnh khi cần → Mac /command
       ↓ Tailscale + Bearer Token
MacBook Server
  ├─ Hummingbird HTTP
  ├─ /plan phân loại nhu cầu camera
  ├─ Gemini xử lý văn bản/ảnh
  ├─ Apple Vision OCR
  └─ TTS + MP3
       ↓
Pi phát qua MAX98357A
```

Pi nghe mic liên tục, chỉ gửi đoạn có tiếng nói (VAD) lên Groq; Mac Server không còn đăng ký `/stt` và không cần Groq API key cho luồng Pi. Server vẫn chạy AI, OCR, TTS. Bluetooth không tự kết nối khi khởi động; chỉ lệnh giọng nói rõ ràng mới bắt đầu kết nối.
