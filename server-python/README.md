# Thiên Nhãn Server (Python)

Bản Python giữ API của `Server/` (Swift/macOS) và bổ sung giao thức hội thoại Part 1: `GET /health`, `POST /conversation`, `POST /conversation/capture`, `POST /conversation/cancel`, cùng các API tương thích `/plan`, `/gemini`, `/vision/ocr`, `/tts`, `/command`. Pi tự thu âm và gửi MP3 trực tiếp Groq STT; server không nhận audio STT.

## Chuẩn bị

- Python 3.11 trở lên.
- Tesseract OCR cùng dữ liệu ngôn ngữ `vie` và `eng` (thêm `tesseract` vào PATH hoặc cấu hình `TESSERACT_CMD`).
- FFmpeg trên PATH (hoặc cấu hình `FFMPEG_CMD`).
- Kết nối Internet để gọi Gemini và Edge TTS.

```powershell
cd server-python
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
Copy-Item .env.example .env
# Sửa .env: SERVER_TOKEN, GEMINI_API_KEY và tên model phù hợp với tài khoản.
python main.py
```

Trên Linux/macOS, kích hoạt môi trường bằng `source .venv/bin/activate` và sao chép cấu hình bằng `cp .env.example .env`.

## Hội thoại Part 1

- `POST /conversation` nhận `{sessionId, turnId, text}`. Mọi POST yêu cầu `Authorization: Bearer <SERVER_TOKEN>`.
- Nếu không cần ảnh, trả MP3 `audio/mpeg` cùng `X-ThienNhan-Session-Id`, `X-ThienNhan-Turn-Id` và text base64.
- Nếu cần ảnh, chỉ trả JSON `action: CAPTUREX` cùng session/turn ID, command và mode. `NOCAPTUREX` không bao giờ được gửi tới Pi.
- Gemini phải trả JSON với `decision` đúng `CAPTUREX` hoặc `NOCAPTUREX`. Quyết định sai định dạng hoặc mâu thuẫn với yêu cầu nhìn ảnh bị từ chối; không cấp quyền chụp. Với câu hỏi đọc chữ trong hội thoại, server gửi ảnh cho Gemini cùng câu hỏi.
- `POST /conversation/capture` nhận session/turn ID, JPEG base64 và `imageContentType`; chỉ lượt đang chờ capture mới được dùng, một lần.
- `POST /conversation/cancel` vô hiệu hoá lượt pending. Kết quả model đang chạy có thể tiếp tục ở nhà cung cấp, nhưng server/Pi loại bỏ kết quả cũ và không phát lại.
- Cancel đến trước request lượt nói cũng được ghi nhớ trong một khoảng giới hạn; request đến muộn bị từ chối. Header `Server-Timing` ghi thời gian xử lý decision, answer và TTS, không chứa nội dung người dùng.
- Quyết định camera không hợp lệ hoặc Gemini lỗi trả `camera_decision_failed`; không cấp quyền capture.

## Tương thích API

Tất cả POST cần `Authorization: Bearer <SERVER_TOKEN>`. `/plan` nhận `{"text":"..."}` và trả `command`, `mode`, `needsImage`. `/command` nhận `text`, `mode` tùy chọn, `image` base64 và `imageContentType` khi cần ảnh; trả MP3 `audio/mpeg` với văn bản trong `X-ThienNhan-Text-Base64`. `/tts` nhận `{"text":"..."}` và trả MP3. `/vision/ocr` nhận body ảnh thô; tọa độ bounding box tính theo pixel từ góc trên trái. Lỗi trả `{"error":{"code":"...","message":"..."}}`.

Giới hạn: văn bản 2.000 ký tự, ảnh 10 MiB, JSON `/command` và `/plan` 15 MiB, JSON `/gemini` và `/tts` 16 KiB. `/health` không cần token.

Apple Vision và giọng `say -v Linh` chỉ có trên macOS. Bản Python dùng Tesseract (OCR tiếng Việt và tiếng Anh) và Edge TTS giọng Việt, sau đó FFmpeg chuyển về MP3 mono 22,05 kHz, 64 kbps. Kết quả chữ đọc và chất giọng sẽ khác bản Swift dù hình dạng API tương thích. Plugin `pillow-heif` giải mã `image/heic` và `image/heif`.

Chạy kiểm thử: `python -m pytest -q`.
