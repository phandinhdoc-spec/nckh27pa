# Lộ trình phát triển

## Đã hoàn thành
- Swift Package và Groq SpeechEngine.
- Hummingbird, /health, /stt, Bearer Token, AppConfig.
- Gemini task routing, GeminiEngine text/image và endpoint debug `/gemini`.
- Apple Vision OCR, bounding box pixel và endpoint `/vision/ocr`.
- Apple Speech TTS, LAME MP3 và endpoint `/tts`.
- `/stt` trả text, mode và trạng thái cần ảnh; `/command` nhận text/ảnh, xử lý và trả MP3.

## Tiếp theo
1. Pi client: thu âm, chụp ảnh, gửi request, nhận/phát MP3.
2. Kiểm thử end-to-end sau khi Pi client hoàn thiện.
