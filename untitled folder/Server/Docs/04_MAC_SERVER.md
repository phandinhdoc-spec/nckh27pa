# Nhiệm vụ của Mac Server

## Công nghệ
- Swift 6.4, macOS 27, Hummingbird 2.
- Groq Whisper, Apple Vision, AVFoundation, URLSession.
- Gemini API.

## Module
- STT: route, file tạm, Groq Whisper Large V3 Turbo, phân loại bằng luật và báo Pi có cần ảnh hay không.
- Gemini: route, chọn model và gọi Gemini API.
- Vision: Apple Vision OCR, text và bounding box pixel.
- TTS: giọng Việt Apple `Linh`, AIFF PCM và LAME MP3 64 kbps.
- Command: nhận text/ảnh, dùng Vision/Gemini khi cần và trả MP3 từ TTS.
- Server: cấu hình Hummingbird, health check và Bearer Token.

Command dùng chung trực tiếp các engine hiện có, không có service, protocol hoặc planner trung gian.
