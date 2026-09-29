# Thiết kế cuối cùng dự án Thiên Nhãn

> Hồ sơ thiết kế theo hiện trạng mã nguồn ngày 20/07/2026. Đây là tài liệu **as-built**: mô tả sản phẩm đang có trong kho mã, đồng thời chỉ rõ phần đã thiết kế nhưng chưa được chứng minh bằng kiểm thử phần cứng.

## 1. Mục tiêu sản phẩm

Thiên Nhãn là thiết bị trợ giúp người khiếm thị bằng giọng nói và hình ảnh. Raspberry Pi Zero 2 W đóng vai trò thiết bị đầu cuối đeo/mang theo; MacBook Apple Silicon là máy chủ xử lý trung tâm.

Sản phẩm thực hiện bốn việc chính:

1. Thu lời nói từ tai nghe Bluetooth.
2. Nhận dạng tiếng Việt và xác định yêu cầu có cần ảnh hay không.
3. Đọc chữ, mô tả cảnh hoặc trả lời câu hỏi.
4. Chuyển câu trả lời thành MP3 và phát lại cho người dùng.

Nguyên tắc kiến trúc cuối cùng:

- Pi chỉ làm việc gần phần cứng: Bluetooth, micro, camera, Wi-Fi, phát âm thanh và chuyển dữ liệu.
- Mac làm STT, OCR, AI, điều phối và TTS.
- Tác vụ xác định được bằng luật hoặc Apple Vision không gọi Gemini không cần thiết.
- Luồng chính chạy tuần tự cho một người dùng; chưa tối ưu cho nhiều Pi đồng thời.
- Dữ liệu trung gian ưu tiên nằm trong RAM; chỉ history ảnh/MP3 được ghi chậm xuống thẻ nhớ.

## 2. Sơ đồ tổng thể

```text
Người dùng
   │ giọng nói
   ▼
Tai nghe Bluetooth ── SCO/CVSD 8 kHz ──► Raspberry Pi Zero 2 W
                                                │
                                  WAV ── POST /stt
                                                │ Tailscale/HTTP
                                                ▼
                                      Mac ThienNhanServer
                            ┌──────────────┼──────────────┐
                            │              │              │
                       Groq Whisper   Apple Vision     Gemini
                            │              │              │
                            └──────────────┼──────────────┘
                                           │ Apple `say` + LAME
                                           ▼
                                  MP3 22,05 kHz/64 kbps
                                           │
                           POST /command ◄─┘
                                           │
                                           ▼
                              Pi phát qua Bluetooth A2DP

Camera ── rpicam-jpeg 1280×720 ──► RAM Pi ──► /command khi cần ảnh
```

## 3. Ranh giới trách nhiệm

| Công việc | Pi | Server Mac |
|---|---:|---:|
| Kết nối Wi-Fi/Bluetooth/camera | Có | Không |
| Thu WAV, chụp JPEG, phát MP3 | Có | Không |
| Phát âm báo có sẵn | Có | Không |
| Nhận dạng giọng nói | Không | Groq Whisper |
| Phân loại lệnh | Chỉ quản lý phiên | Luật trong `CommandRoutes` |
| OCR | Không | Apple Vision |
| Hiểu ảnh/trò chuyện/suy luận | Không | Gemini |
| Tổng hợp tiếng nói | Không | Apple `say` + LAME |
| Lưu history ảnh/âm thanh trả lời | Có, 7 ngày | Không |

---

# MẢNG SERVER

## 4. Nền tảng Server

- Máy: MacBook Apple Silicon.
- Hệ điều hành tối thiểu theo package: macOS 26; môi trường kiểm tra hiện tại là macOS 27.
- Ngôn ngữ và công cụ: Swift 6.4, Swift Package Manager.
- HTTP server: Hummingbird 2; bản đang khóa trong `Package.resolved` là 2.25.0.
- STT: Groq API, model mặc định `whisper-large-v3-turbo`.
- OCR: Apple Vision, chế độ `.accurate`, tự nhận dạng ngôn ngữ, ưu tiên `vi-VN` và `en-US`.
- AI: Gemini API với bốn model cấu hình theo nhóm fast, vision, reasoning và fallback.
- TTS: giọng Việt `Linh` của macOS, xuất AIFF PCM 16-bit/22,05 kHz rồi mã hóa MP3 mono 64 kbps bằng LAME.

## 5. Cấu trúc Server

```text
Sources/ThienNhanServer/
├── main.swift                         điểm vào chương trình
├── Config.swift                       đọc và kiểm tra .env
├── Server.swift                       tạo router, engine và health check
├── STT/
│   ├── STTRoutes.swift                API /stt, file âm thanh tạm
│   └── SpeechEngine.swift             Groq Whisper và lọc không có tiếng nói
├── Gemini/
│   ├── GeminiEngine.swift             gọi model chính/fallback
│   └── GeminiRoutes.swift             endpoint chẩn đoán /gemini
├── Vision/
│   └── VisionRoutes.swift             OCR và bounding box pixel
├── TTS/
│   └── TTSRoutes.swift                Apple `say`, LAME và MP3 response
└── Command/
    └── CommandRoutes.swift            phân loại và điều phối end-to-end
```

Không có tầng service/protocol/planner trung gian. Route dùng trực tiếp các engine hiện có.

## 6. Cấu hình Server

| Biến | Vai trò | Mặc định |
|---|---|---|
| `SERVER_HOST` | địa chỉ lắng nghe | `0.0.0.0` |
| `SERVER_PORT` | cổng HTTP | `8765` |
| `SERVER_TOKEN` | token dùng chung với Pi | bắt buộc |
| `GROQ_API_KEY` | khóa Groq | bắt buộc |
| `GROQ_STT_MODEL` | model STT | `whisper-large-v3-turbo` |
| `GEMINI_API_KEY` | khóa Gemini | bắt buộc |
| `GEMINI_MODEL_FAST` | trò chuyện đơn giản/phân loại | bắt buộc |
| `GEMINI_MODEL_VISION` | ảnh, OCR diễn giải | bắt buộc |
| `GEMINI_MODEL_REASONING` | suy luận phức tạp | bắt buộc |
| `GEMINI_MODEL_FALLBACK` | model thử lại | bắt buộc |
| `REQUEST_TIMEOUT_SECONDS` | timeout Groq/Gemini | `30` giây |

Giá trị trong biến môi trường của tiến trình ghi đè giá trị trong `.env`.

## 7. API Server

### `GET /health`

Trả trạng thái server và phiên bản `0.1.0`. Đây là endpoint kiểm tra nhanh, không đi qua STT/AI/TTS.

### `POST /stt`

- Body thô: WAV, M4A, AIFF hoặc CAF.
- Giới hạn: 20 MiB.
- Server ghi file tạm theo UUID, gọi Groq rồi xóa file.
- Groq dùng tiếng Việt, nhiệt độ 0 và `verbose_json`.
- Kết quả bị loại nếu không có lời nói, xác suất no-speech cao hoặc trùng một số câu hallucination thường gặp.
- Server bỏ wake word ở đầu câu, phân loại lệnh và trả:

```json
{
  "success": true,
  "text": "Thiên Nhãn, phía trước tôi là gì?",
  "command": "phía trước tôi là gì?",
  "mode": "observe",
  "needsImage": true,
  "locale": "vi",
  "audioDuration": 4.2,
  "processingTime": 0.8,
  "timestamp": "..."
}
```

### `POST /command`

Endpoint sản phẩm chính sau `/stt`.

- Nhận JSON tối đa 15 MiB.
- Text tối đa 2.000 ký tự.
- Ảnh tùy chọn được gửi dạng base64 cùng MIME type, tối đa 10 MiB trước base64.
- Nếu Pi không gửi `mode`, Server tự phân loại lại từ `text`.
- Trả trực tiếp MP3 với `Content-Type: audio/mpeg`.

### `POST /vision/ocr`

Endpoint chẩn đoán OCR độc lập. Nhận body ảnh thô JPEG, PNG, HEIC/HEIF hoặc WebP tối đa 10 MiB; trả text, confidence, kích thước ảnh và bounding box pixel có gốc ở góc trên trái.

### `POST /tts`

Endpoint chẩn đoán TTS độc lập. Nhận JSON text tối đa 2.000 ký tự và trả MP3.

### `POST /gemini`

Endpoint chẩn đoán model routing. Nhận prompt và `GeminiTask`; không nằm trong luồng chính của Pi.

Mọi lỗi xử lý dùng cấu trúc ổn định:

```json
{"error":{"code":"...","message":"..."}}
```

## 8. Luật điều phối Server

| Chế độ | Điều kiện điển hình | Xử lý |
|---|---|---|
| `local` | “kiểm tra hệ thống”, “trạng thái hệ thống”, “máy chủ” | câu trả lời cố định, không Gemini |
| `ocr` | đọc/chữ/OCR/văn bản/biển báo/nhãn sản phẩm | Apple Vision; Gemini chỉ khi cần giải thích/tóm tắt |
| `observe` | nhìn/phía trước/trước mắt/vật gì/màu gì/mô tả/xung quanh | Gemini Vision với ảnh |
| `chat` | các câu còn lại | Gemini fast |
| `auto` | giá trị yêu cầu tự chọn | được thay bằng kết quả phân loại |

Sau khi có câu trả lời, Server cắt tối đa 2.000 ký tự, tạo MP3 và trả về Pi.

## 9. Luồng xử lý Server

### Câu hỏi thông thường

```text
/stt → Groq → mode=chat → /command → Gemini fast → TTS → MP3
```

### Quan sát cảnh

```text
/stt → Groq → mode=observe, needsImage=true
     → Pi chụp ảnh
     → /command + JPEG → Gemini vision → TTS → MP3
```

### Đọc văn bản

```text
/stt → Groq → mode=ocr, needsImage=true
     → Pi chụp ảnh
     → /command + JPEG → Apple Vision OCR
     → [Gemini nếu cần diễn giải] → TTS → MP3
```

---

# MẢNG PI

## 10. Phần cứng Pi

- Raspberry Pi Zero 2 W, ARM64.
- Camera dùng `rpicam-jpeg`.
- Tai nghe Bluetooth cố định, dùng:
  - A2DP để phát âm thanh;
  - HFP/HSP SCO, codec CVSD mono 8 kHz để thu micro.
- Mạch khuếch đại I2S MAX98357A:
  - GPIO18 → BCLK;
  - GPIO19 → LRC/WS;
  - GPIO21 → DIN;
  - 5V và GND theo sơ đồ phần cứng.

Lưu ý hiện trạng: script setup cấu hình MAX98357A, nhưng ứng dụng chính đang phát mọi thông báo qua thiết bị Bluetooth; `ONBOARD_PLAYBACK_DEVICE` đã có trong cấu hình nhưng chưa được caller sử dụng.

## 11. Nền tảng Pi

- DietPi Trixie ARM64, chạy headless.
- Python 3 từ hệ điều hành; snapshot bytecode cho thấy nhánh Pi đã chạy bằng Python 3.13, nhưng script setup không khóa patch version.
- Không có dependency Python trong runtime chính; mã dùng thư viện chuẩn.
- Công cụ hệ thống: `arecord`, `mpg123`, `rpicam-jpeg`, `bluetoothctl`, `bluealsa-cli`, Tailscale và systemd.
- Script chuẩn bị phần cứng đầy đủ nhất hiện nằm ở `/setup_pi2w.sh` tại gốc kho mã.

## 12. Nhánh Pi chính thức

- `Pi/`: nhánh sản phẩm hiện tại, CVSD 8 kHz; đây là nguồn phải triển khai.
- `Pi16/`: nhánh thử nghiệm mSBC 16 kHz; không phải nhánh triển khai cuối.
- `Pi.zip`: gói chụp lúc 13:59 ngày 16/07/2026, cũ hơn các sửa đổi cuối trong `Pi/` lúc 16:28 cùng ngày; không được xem là bản mới nhất.

## 13. Cấu trúc Pi

```text
Pi/
├── app.py                  vòng đời sản phẩm và phiên hội thoại
├── app_keyboard.py         công cụ thử thủ công, bỏ qua wake-word/session
├── config.py               đọc/kiểm tra .env
├── devices.py              Wi-Fi, Bluetooth, thu/phát và camera
├── mac_client.py           HTTP client, retry và kiểm tra response
├── history.py              ghi history nền và dọn dữ liệu 7 ngày
├── test_camera.py          chẩn đoán camera độc lập
├── test_micro.py           chẩn đoán CVSD 8 kHz
├── test_micro_rates.py     công cụ thử mSBC/các sample rate
├── restore_cvsd_8khz.sh    đưa BlueALSA về cấu hình cuối
├── setup_pi2w.sh           setup tối giản cũ hơn script ở gốc
└── sounds/                 MP3 thông báo có sẵn
```

## 14. Cấu hình Pi

| Biến | Vai trò | Mặc định |
|---|---|---|
| `MAC_SERVER_URL` | URL Mac qua Tailscale | bắt buộc |
| `SERVER_TOKEN` | token dùng chung | bắt buộc |
| `BLUETOOTH_DEVICE_MAC` | MAC tai nghe | bắt buộc |
| `BLUETOOTH_PLAYBACK_DEVICE` | ALSA A2DP | bắt buộc |
| `BLUETOOTH_CAPTURE_DEVICE` | ALSA SCO | bắt buộc |
| `ONBOARD_PLAYBACK_DEVICE` | ALSA MAX98357A | `default`, hiện chưa dùng |
| `SILENCE_SECONDS` | thời gian chờ/kết thúc im lặng | `2` giây |
| `SILENCE_RMS` | ngưỡng RMS tối thiểu | `500` |
| `MAX_RECORD_SECONDS` | giới hạn một lượt thu | `30` giây |
| `HTTP_TIMEOUT_SECONDS` | timeout mỗi request | `30` giây |
| `HTTP_RETRIES` | số lần thử lại sau lần đầu | `2` |
| `CPU_IDLE_PERCENT` | điều kiện ghi history | `80%` idle |
| `RAM_AVAILABLE_PERCENT` | ngưỡng RAM thấp để xả queue | `15%` |

## 15. Trình tự khởi động Pi

Mã hiện tại kiểm tra theo thứ tự:

1. Pair/trust/kết nối tai nghe Bluetooth và ép micro về CVSD.
2. Kiểm tra Wi-Fi bằng `/sys/class/net` và lấy SSID nếu có `iwgetid`.
3. Chụp thử JPEG bằng camera.
4. Phát âm “hệ thống sẵn sàng” và hướng dẫn sử dụng.
5. Vào vòng lặp nghe liên tục.

Mỗi bước thử lại sau 5 giây cho đến khi thành công hoặc người vận hành dừng chương trình.

## 16. Thu âm và phát hiện giọng nói

- `arecord` xuất raw PCM S16_LE, mono, 8 kHz theo từng chunk 100 ms.
- 0,5 giây đầu được dùng để đo noise floor và vẫn nằm trong luồng xử lý.
- Ngưỡng giọng nói là giá trị lớn hơn giữa `SILENCE_RMS` và hai lần noise RMS thấp nhất trong giai đoạn chuẩn.
- Cần 0,3 giây liên tiếp vượt ngưỡng để xác nhận có giọng nói.
- Giữ pre-roll 0,3 giây để tránh mất đầu câu.
- Nếu không nghe thấy giọng trong `SILENCE_SECONDS`, lượt thu bị bỏ và không gửi Mac.
- Sau khi có giọng, `SILENCE_SECONDS` im lặng kết thúc lượt thu.
- `MAX_RECORD_SECONDS` là chặn cứng.
- Khi chạy trong terminal/SSH, phím Space dừng ngay lượt thu.
- PCM được đóng gói WAV trong RAM; không ghi WAV xuống thẻ nhớ.

## 17. Phiên hội thoại Pi

```text
Chưa mở phiên
  ├─ lời nói hợp lệ → gửi /stt để nhận text
  ├─ không có “Thiên Nhãn” → không gọi /command
  └─ có “Thiên Nhãn” → mở phiên 60 giây

Đang mở phiên
  ├─ câu hợp lệ → /stt → [chụp ảnh nếu cần] → /command → phát MP3
  ├─ mỗi câu hợp lệ gia hạn thêm 60 giây
  ├─ “Hết/Thôi/Dừng lại/Kết thúc” → đóng phiên
  └─ hết 60 giây không có lượt hợp lệ → tự đóng
```

“Giúp tôi”, “Hãy giúp tôi” và biến thể có wake word phát file hướng dẫn có sẵn, không gọi `/command`.

Điểm cần hiểu đúng: wake word được kiểm tra **sau** khi Mac đã nhận dạng STT. Ngoài phiên, đoạn có giọng vẫn được gửi tới `/stt`; chỉ bước `/command`/Gemini bị chặn.

## 18. Camera và dữ liệu

- Camera chụp JPEG 1280×720, timeout khởi động 500 ms.
- File tạm nằm trong `/dev/shm`, được đọc vào RAM rồi xóa ngay.
- Ảnh chỉ được chụp khi phản hồi `/stt` có `needsImage=true`.
- WAV được giải phóng sau STT.
- JPEG và MP3 được giữ trong RAM, sau đó xếp hàng cho worker history.
- Worker ghi khi CPU đủ rảnh hoặc RAM xuống thấp; file được ghi atomically.
- History ảnh và âm thanh được giữ 7 ngày theo thời điểm truyền/phát.

## 19. Kết nối Pi–Server

Luồng vận hành sử dụng HTTP qua địa chỉ Tailscale của Mac:

```text
Pi record WAV
  → POST /stt
  ← text + mode + needsImage
  → [capture JPEG nếu cần]
  → POST /command (text + JPEG base64 tùy chọn)
  ← MP3
  → phát A2DP
```

Pi giới hạn response `/stt` ở 64 KiB và `/command` ở 10 MiB. Lỗi mạng/timeout được thử lại tối đa theo `HTTP_RETRIES`, nghỉ 1 giây rồi 2 giây với cấu hình mặc định. Lỗi HTTP được trả ngay cho vòng lặp chính; ứng dụng đợi 2 giây rồi nhận lượt mới.

## 20. Trạng thái hoàn thiện theo mã nguồn

| Hạng mục | Trạng thái |
|---|---|
| Swift package và các route | Build thành công ngày 20/07/2026 |
| Python Pi và shell script | Qua kiểm tra cú pháp ngày 20/07/2026 |
| Luồng STT → Command → TTS | Đã hiện thực trong mã |
| Camera/Bluetooth/Tailscale setup | Đã có script và công cụ chẩn đoán |
| Nhánh âm thanh cuối | `Pi/`, CVSD 8 kHz |
| Test end-to-end trên Pi thật trong lần kiểm tra này | Chưa thực hiện |
| Số liệu độ trễ Groq/Gemini/camera/Bluetooth thực tế | Chưa có trong kho mã |
| Bản đóng gói mới nhất | Chưa có; `Pi.zip` cũ hơn source hiện tại |

Thiết kế được coi là chốt ở cấp mã nguồn. Việc công nhận bản sản phẩm phần cứng hoàn thiện cần một vòng kiểm thử end-to-end trên đúng Pi, camera, tai nghe và mạng triển khai.
