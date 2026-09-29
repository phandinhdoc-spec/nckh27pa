# Thiên Nhãn Server

Mac server cho Raspberry Pi Zero 2 W, dùng Hummingbird, Groq Whisper, Apple Vision và Gemini.

```text
Sources/ThienNhanServer/
├── main.swift
├── Config.swift
├── Server.swift
├── STT/
├── Gemini/
├── Vision/
├── TTS/
└── Command/
```

Chạy server:

```sh
cp .env.example .env
swift run
```

MP3 cần bộ mã hóa `lame` (`brew install lame`).

API hiện có: `GET /health`, `POST /plan`, `POST /gemini`, `POST /vision/ocr`, `POST /tts`, `POST /command`.

Luồng Pi: Pi nghe mic liên tục, VAD cắt đoạn giọng nói và gửi MP3 trực tiếp lên Groq để nhận dạng wake phrase/lệnh. Sau khi có text, Pi gọi `/plan` để biết có cần camera; nếu cần thì chụp ảnh rồi gửi text/ảnh đến `/command`. Server xử lý AI/TTS và trả MP3. Groq STT không còn chạy trong Server.
