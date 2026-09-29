# Thiết kế API

## GET /health
Kiểm tra server.

## POST /plan
Pi gửi JSON `{"text":"..."}` cùng Bearer Token sau khi Groq STT trên Pi nhận dạng được câu nói. Server phân loại yêu cầu và trả:

```json
{"command":"phía trước tôi là gì?","mode":"observe","needsImage":true}
```

Pi dùng `needsImage` để quyết định có chụp camera hay không. Lệnh xử lý cục bộ như `kết nối Bluetooth` không gửi lên endpoint này.

## POST /vision/ocr
Nhận ảnh JPEG, PNG, HEIC hoặc WebP dạng body thô cùng Bearer Token.

Trả text, kích thước ảnh, confidence và bounding box pixel với gốc tọa độ ở góc trên bên trái.

## POST /tts
Nhận JSON `{"text":"Nội dung tiếng Việt"}` cùng Bearer Token, tối đa 2.000 ký tự.

Trả trực tiếp MP3 mono 22,05 kHz, 64 kbps với `Content-Type: audio/mpeg`.

## POST /command
Endpoint chính Pi sử dụng để xử lý câu hỏi sau khi `/plan` xác định mode và nhu cầu ảnh.

Nhận `application/json` cùng Bearer Token. `image` là chuỗi base64 mà `JSONDecoder` giải mã trực tiếp thành dữ liệu nhị phân.

```json
{
  "text": "phía trước tôi là gì?",
  "mode": "observe",
  "image": "<base64 tùy chọn>",
  "imageContentType": "image/jpeg"
}
```

`mode`: `local`, `auto`, `chat`, `ocr` hoặc `observe`. `ocr` và `observe` yêu cầu ảnh. Text tối đa 2.000 ký tự, ảnh tối đa 10 MB, JSON tối đa 15 MB. Pi có thể đính kèm `sensor`; hiện Server chưa dùng dữ liệu cảm biến để suy luận.

Server xử lý bằng luật/Gemini, dùng Vision khi cần ảnh, rồi TTS; trả MP3 với `Content-Type: audio/mpeg` và text phản hồi trong header `X-ThienNhan-Text-Base64`.

## Luồng Pi
Mic Pi thu liên tục; VAD cục bộ chia đoạn lời nói, mã hóa thành MP3 và gửi lên Groq STT trực tiếp từ Pi để phát hiện wake phrase “Thiên Nhãn” và nhận lệnh. Pi gọi `/plan`, chụp ảnh nếu cần, rồi gửi text/ảnh tới `/command`. Mac Server không đăng ký `/stt` và không cần Groq API key để chạy luồng này.

Lỗi trả JSON ổn định dạng `{"error":{"code":"...","message":"..."}}`.
