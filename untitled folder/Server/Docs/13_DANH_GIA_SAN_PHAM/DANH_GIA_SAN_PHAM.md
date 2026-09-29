# Đánh giá sản phẩm Thiên Nhãn

> Phạm vi: Server, Pi và kết nối Pi–Server; tập trung vào hiệu suất và mức tận dụng phần cứng. Nội dung bảo mật chủ động không nằm trong báo cáo này.

## 1. Kết luận quản lý

Sản phẩm đã đạt mức **prototype tích hợp hoàn chỉnh ở cấp mã nguồn**: Server build được, Pi client có đủ luồng thu âm–STT–camera–command–phát MP3 và có script bring-up phần cứng.

Sản phẩm **chưa đủ bằng chứng để công nhận là bản phần cứng hoàn thiện** vì kho mã không lưu kết quả kiểm thử end-to-end trên Pi thật, không có phân vị độ trễ Groq/Gemini, số liệu CPU/RAM/nhiệt độ Pi hoặc bài test chạy dài.

Đánh giá tổng hợp:

| Mảng | Kết luận | Điểm mạnh chính | Giới hạn chính |
|---|---|---|---|
| Server | Tốt cho một thiết bị | async HTTP, Apple Vision cục bộ, routing tránh AI thừa | phụ thuộc độ trễ API ngoài; STT bị tuần tự hóa qua actor |
| Pi | Khá tốt cho Pi Zero 2 W | runtime nhẹ, audio/camera trong RAM, luồng tuần tự | MAX98357A chưa được app dùng; package/runtime còn dư; chưa có số đo Pi thật |
| Kết nối | Phù hợp prototype một người dùng | dữ liệu audio/ảnh/output đều đã nén hợp lý | hai lượt HTTP, ảnh base64 tăng 33%, không tái sử dụng kết nối rõ ràng |
| Toàn hệ thống | Có kiến trúc đúng hướng | dồn xử lý nặng sang Mac | tổng độ trễ là chuỗi STT + AI/Vision + TTS, chưa benchmark end-to-end |

## 2. Phương pháp và giới hạn kiểm tra

Đã thực hiện ngày 20/07/2026:

- `swift build`: thành công bằng Swift 6.4 trên macOS 27.
- `python3 -m py_compile Pi/*.py Pi16/*.py`: thành công trên máy kiểm tra.
- `bash -n` cho các script setup/chẩn đoán: thành công.
- Khởi động bản Server debug trên loopback và gọi `/health` 50 lần tuần tự.

Baseline `/health`:

| Chỉ số | Kết quả |
|---|---:|
| Trung bình | 0,576 ms |
| Nhỏ nhất | 0,473 ms |
| Lớn nhất | 1,085 ms |
| RSS Server debug khi nhàn rỗi | khoảng 17,6 MiB |
| Kích thước binary debug | khoảng 24 MiB |

Các số trên chỉ đo Hummingbird/JSON trên chính Mac, không đo Tailscale, Pi, Groq, Gemini, Apple Vision, camera, Bluetooth hoặc TTS. Không được dùng chúng như độ trễ end-to-end của sản phẩm.

---

# ĐÁNH GIÁ MẢNG SERVER

## 3. Hiệu suất Server

### Điểm tốt

1. **HTTP framework gọn và bất đồng bộ.** Hummingbird/NIO phù hợp server I/O; health baseline cho thấy overhead nội bộ rất nhỏ so với các dịch vụ AI bên ngoài.
2. **Không đưa ảnh qua pipeline nặng khi không cần.** `/stt` trả `needsImage`; Pi chỉ chụp và tải JPEG cho OCR/observe.
3. **Apple Vision xử lý OCR cục bộ.** OCR thông thường không cần gửi ảnh sang Gemini, giảm một lần gọi API ngoài.
4. **Rule routing rẻ.** Phân loại local/OCR/observe/chat chỉ dùng normalize chuỗi, tập từ và `contains`.
5. **TTS chạy ngoài event loop.** Công việc đồng bộ được đưa vào `Task.detached`, tránh giữ handler bất đồng bộ trong lúc `say`/LAME chạy.
6. **Giới hạn request sớm.** Audio, ảnh, JSON và text đều có trần, ngăn một request chiếm bộ nhớ không giới hạn.
7. **File tạm được xóa bằng `defer`.** Audio STT, AIFF và MP3 TTS không tích lũy sau lượt xử lý bình thường.

### Nút thắt

1. **Dịch vụ ngoài chi phối thời gian.** Chat phải chờ Gemini; mọi câu có giọng phải chờ Groq; sau đó còn chờ `say` và LAME. Hummingbird nhanh không làm giảm phần lớn tổng latency.
2. **`SpeechEngine` là actor dùng chung.** Mọi request STT đi qua một actor, nên nhiều Pi đồng thời có thể bị tuần tự hóa. Với một thiết bị đây là lựa chọn đơn giản và chấp nhận được.
3. **Tạo `URLSession` mới mỗi request Groq/Gemini.** Mã không giữ session dùng chung, làm mất cơ hội tái sử dụng kết nối và thêm chi phí setup.
4. **TTS tạo hai process cho mỗi câu.** `say` rồi `lame` là đường ổn định nhưng có chi phí process startup và hai file tạm.
5. **Fallback Gemini thử cả lỗi không nhất thiết nên thử lại.** Engine thử model fallback cho mọi lỗi từ model chính; khi lỗi là mạng chung, lần hai có thể chỉ kéo dài thời gian chờ.
6. **Không có cache.** Đây chưa phải lỗi với câu hỏi/camera thay đổi liên tục; chỉ các câu trả lời cố định đang được xử lý local.

### Đánh giá khả năng tải

- **Một Pi, một người dùng:** phù hợp. Luồng tuần tự giúp tránh cạnh tranh audio/camera và đơn giản hóa trạng thái.
- **Nhiều Pi đồng thời:** chưa được thiết kế/đo. Actor STT, process TTS và quota API sẽ là các điểm phải kiểm tra trước.
- **Chạy dài:** chưa có soak test, chưa có thống kê số file tạm/rò bộ nhớ sau hàng trăm lượt.

## 4. Tối ưu phần cứng Mac

### Đã tối ưu hợp lý

- Dùng Apple Vision thay vì chạy OCR Python/model riêng.
- Dùng giọng hệ thống của macOS thay vì chạy model TTS trên Pi.
- Mac giữ API key/model routing và toàn bộ xử lý nặng; Pi không phải dành RAM/CPU cho các engine này.
- Dữ liệu ảnh được decode đúng lúc cần OCR/AI, không có pipeline video liên tục.

### Chưa tối ưu hoặc chưa cần tối ưu

- Chưa tái sử dụng `URLSession`.
- Chưa đo xem Apple Vision, `say`, LAME hay network là bottleneck thực tế.
- Chưa build/benchmark bản `release`; số đo hiện tại là debug.
- Chưa có queue/back-pressure rõ ràng cho nhiều request TTS/Gemini đồng thời.

Kết luận: với một Pi, phần cứng Mac còn dư nhiều so với HTTP orchestration. Tối ưu thêm concurrency/caching lúc này là sớm nếu chưa có benchmark end-to-end.

---

# ĐÁNH GIÁ MẢNG PI

## 5. Hiệu suất Pi

### Chi phí audio có thể tính chính xác

CVSD được đóng thành PCM S16_LE mono 8 kHz trước khi gửi:

```text
8.000 sample/giây × 2 byte = 16.000 byte/giây
30 giây tối đa ≈ 480.000 byte PCM + 44 byte WAV header
```

Các vùng dữ liệu nhỏ:

- 0,5 giây calibration: khoảng 8 KiB PCM.
- 0,3 giây pre-roll: khoảng 4,8 KiB PCM.
- một chunk 100 ms: khoảng 1,6 KiB.

Đây là tải rất nhẹ với Pi Zero 2 W. Tính RMS bằng Python trên 8.000 sample/giây cũng nhỏ hơn nhiều so với tải camera hoặc encode/decode media.

### Điểm tốt

1. **Runtime Python dùng thư viện chuẩn.** Không cần NumPy/Pillow trong luồng ứng dụng.
2. **Không ghi WAV xuống thẻ nhớ.** PCM và WAV chỉ nằm trong RAM.
3. **Camera dùng `/dev/shm`.** JPEG tạm không gây ghi/xóa liên tục trên thẻ SD.
4. **Ảnh đã nén ở nguồn.** `rpicam-jpeg` tạo JPEG 1280×720 thay vì gửi frame raw.
5. **MP3 được stream qua stdin của `mpg123`.** Không cần file phát tạm.
6. **Voice gate giảm request rỗng.** Không có giọng nói thì WAV không được gửi.
7. **Luồng tuần tự.** Một Pi không đồng thời thu, chụp, gửi và phát; RAM peak dễ kiểm soát hơn.
8. **History ghi nền.** Ảnh/MP3 được xếp hàng và chỉ ghi khi CPU nhàn hoặc RAM xuống thấp.
9. **Codec cuối ưu tiên ổn định.** CVSD 8 kHz giảm một nửa bandwidth/PCM so với thử nghiệm 16 kHz.

### Điểm yếu và lãng phí

1. **Mọi lời nói có giọng đều gọi `/stt`.** Khi phiên chưa mở, Pi vẫn cần Groq để biết câu có bắt đầu bằng “Thiên Nhãn” hay không. Điều này tốn network, thời gian và lượt STT cho tiếng nói xung quanh.
2. **Queue history có thể giữ RAM lâu.** Khi CPU chưa đạt mức idle và RAM chưa thấp, ảnh/MP3 tiếp tục nằm trong deque. Không có trần số item/byte.
3. **`History.close()` không flush bắt buộc.** Dừng app ngay sau một lượt có thể bỏ history còn trong RAM. Việc này không ảnh hưởng câu trả lời vừa phát nhưng làm số liệu history thiếu.
4. **Setup cài `python3-numpy` và `python3-pil` nhưng runtime không import.** Tăng thời gian cài và dung lượng thẻ nhớ mà không giúp sản phẩm hiện tại.
5. **MAX98357A chưa được ứng dụng dùng.** Cấu hình `onboard_playback_device` tồn tại nhưng mọi `_announce` đều dùng Bluetooth. Phần cứng/overlay I2S hiện chưa tạo giá trị trong app.
6. **Khởi động bị chặn bởi Bluetooth.** App kiểm tra Bluetooth trước, nên chưa thể dùng MAX98357A để báo lỗi Bluetooth cho người dùng.
7. **Mỗi ảnh mở một process camera.** `rpicam-jpeg` có timeout 500 ms; với câu hỏi thị giác, riêng bước khởi động/chụp đã thêm ít nhất khoảng nửa giây theo cấu hình.
8. **Hai cây source `Pi`/`Pi16` và hai script setup.** Không tốn CPU runtime nhưng tăng nguy cơ triển khai nhầm bản, làm đánh giá phần cứng sai.
9. **`Pi.zip` đã cũ.** Gói được tạo trước những sửa đổi cuối về audio/Space; triển khai từ zip không phản ánh source mới nhất.

## 6. Tối ưu phần cứng Pi

### CPU

- VAD hiện tại là RMS đơn giản theo chunk, phù hợp Pi Zero 2 W hơn một model local nặng.
- Không chạy OCR/STT/Gemini/TTS trên Pi là quyết định tối ưu lớn nhất.
- Worker history lấy mẫu `/proc/stat` 0,25 giây mỗi chu kỳ; chi phí thấp nhưng vẫn cần đo trên Pi thật.

### RAM

- Audio peak dưới 0,5 MiB ở giới hạn 30 giây.
- JPEG 1280×720 và MP3 thường nhỏ hơn giới hạn API nhiều, nhưng kích thước thực tế chưa được log.
- Khi gửi ảnh, RAM đồng thời có JPEG bytes, chuỗi base64 và JSON bytes; base64 làm phần ảnh tăng khoảng 4/3 trước các bản sao tạm.
- History queue có thể giữ thêm bản ảnh/MP3 sau khi phát.

### Thẻ nhớ

- `/dev/shm` cho WAV/JPEG tạm là tốt.
- History chỉ ghi chậm và xóa sau 7 ngày, giảm ghi đúng lúc app đang bận.
- Chưa có giới hạn dung lượng tuyệt đối cho history; giữ 7 ngày có thể vẫn lớn nếu tần suất dùng cao.

### Camera

- 1280×720 cân bằng hợp lý giữa chi tiết và bandwidth cho prototype.
- Chưa có tự điều chỉnh exposure, chất lượng JPEG hoặc kích thước theo loại tác vụ.
- Camera ISP service và sửa `gpu_mem` cho thấy setup đã tính đến lỗi boot thực tế.

### Bluetooth/audio

- CVSD 8 kHz phù hợp thoại và giảm tải, nhưng là giới hạn chất lượng đầu vào rõ ràng cho STT.
- A2DP cho playback giữ chất lượng nghe tốt hơn SCO.
- Chuyển profile SCO/A2DP là đặc tính vật lý cần kiểm thử nhiều lượt; chưa có log soak test.

Kết luận: kiến trúc đã tận dụng Pi Zero 2 W đúng vai trò. Hai việc nên làm ngay không cần thiết kế lại là bỏ package Python không dùng và chọn một script/source Pi duy nhất để triển khai. MAX98357A chỉ nên giữ nếu sản phẩm thực sự dùng nó làm kênh báo trạng thái.

---

# ĐÁNH GIÁ KẾT NỐI PI–SERVER

## 7. Hiệu suất kết nối

### Điểm tốt

- WAV 8 kHz, JPEG và MP3 đều là định dạng phù hợp bandwidth; không truyền raw image/video.
- Chỉ chụp/gửi ảnh theo `needsImage`.
- `/command` trả MP3 nhị phân, không base64 hóa response.
- Timeout và retry có giới hạn; app không treo vô hạn khi Mac mất kết nối.
- Tailscale cho phép dùng kết nối IP trực tiếp giữa Pi và Mac mà không cần duy trì hotspot riêng.

### Độ trễ end-to-end

Một câu chat có đường tới hạn:

```text
thu âm + 2 giây xác nhận im lặng
→ upload WAV
→ Groq STT
→ upload JSON /command
→ Gemini
→ Apple say
→ LAME
→ download MP3
→ playback
```

Một câu quan sát còn thêm:

```text
chụp camera khoảng 0,5 giây theo timeout cấu hình
→ base64 JPEG
→ upload ảnh
→ Gemini Vision
```

Vì các bước chạy tuần tự, tổng latency gần bằng tổng từng chặng; tối ưu HTTP server vài phần mười mili-giây không thay đổi trải nghiệm nếu Groq/Gemini/TTS mất nhiều giây.

### Điểm yếu

1. **Hai HTTP round trip bắt buộc.** `/stt` trước, `/command` sau. Đây là chủ ý để Pi biết khi nào chụp ảnh, nhưng tăng latency cho câu không cần ảnh.
2. **Ảnh base64 tăng khoảng 33% kích thước.** JSON còn tạo thêm bản sao ở Pi và Server.
3. **Python `urlopen` không thể hiện connection pool lâu dài.** Mỗi call có thể phải tạo lại kết nối TCP.
4. **Server cũng tạo `URLSession` mới cho Groq/Gemini.** Kết nối upstream chưa được tái sử dụng rõ ràng.
5. **Retry chỉ cho lỗi kết nối/timeout.** HTTP 5xx không được thử lại; đây là lựa chọn an toàn cho luồng đơn nhưng giảm khả năng tự hồi phục.
6. **Không kiểm tra `/health` lúc startup.** Pi chỉ phát hiện Mac không sẵn sàng sau khi đã thu và gửi lượt STT đầu tiên.
7. **`mode` từ `/stt` không được Pi gửi lại.** Pi chỉ dùng `needsImage`; `/command` phân loại lại text. Việc này chưa gây tải đáng kể nhưng tạo hai lần quyết định thay vì dùng kế hoạch đã có.

## 8. Ước lượng lưu lượng

| Dữ liệu | Công thức/kích thước |
|---|---:|
| WAV 5 giây | khoảng 80 KiB |
| WAV tối đa 30 giây | khoảng 469 KiB |
| MP3 TTS | 64 kbps ≈ 8 KiB/giây phát |
| JPEG 1280×720 | phụ thuộc cảnh/chất lượng; chưa có log thực tế |
| JPEG trong JSON | kích thước JPEG × khoảng 1,33, chưa tính JSON |

Âm thanh không phải bottleneck bandwidth. Ảnh và độ trễ API ngoài mới là phần cần đo trên mạng triển khai.

## 9. Xếp hạng vấn đề cần xử lý

### Sai lệch hồ sơ ảnh hưởng trực tiếp tới triển khai

- `Pi/README.md` hướng dẫn `cp .env.example .env`, nhưng `Pi/.env.example` không tồn tại trong kho hiện tại.
- Cùng README nhắc Raspberry Pi OS Lite trong phần camera, trong khi thiết kế và setup chính dùng DietPi.
- `Docs/10_ROADMAP.md` vẫn ghi Pi client là bước tiếp theo dù client đã tồn tại.
- Trình tự startup trong README không trùng code hiện tại: code kiểm tra Bluetooth → Wi-Fi → camera.
- Chưa có unit/integration test tự động cho rule phân loại, HTTP client hoặc state phiên; các `test_*.py` hiện là script chẩn đoán phần cứng.

Các sai lệch này không làm chậm runtime, nhưng có thể khiến cài sai hệ, dùng sai artifact hoặc đánh giá nhầm kết quả phần cứng.

### P0 — cần trước khi gọi là release phần cứng

1. Chạy test end-to-end thật trên Pi với đủ chat, OCR, observe, mất mạng và reconnect Bluetooth.
2. Tạo lại artifact triển khai từ `Pi/` hiện tại; không dùng `Pi.zip` cũ.
3. Chốt một script setup chính thức. Hiện script gốc đầy đủ hơn `Pi/setup_pi2w.sh`.
4. Bổ sung `Pi/.env.example` và đồng bộ README/roadmap theo DietPi cùng thứ tự startup thật.

### P1 — cải thiện rõ với thay đổi nhỏ

1. Ghi thời gian từng chặng ngay trong `app.py`: record, `/stt`, capture, `/command`, playback; không cần dependency mới.
2. Ghi kích thước WAV/JPEG/MP3 và peak RAM trong bài test chạy dài.
3. Bỏ `python3-numpy`/`python3-pil` khỏi setup nếu xác nhận không có caller.
4. Hoặc dùng MAX98357A để báo trạng thái trước Bluetooth, hoặc bỏ cấu hình/biến không dùng để tránh phần cứng chết.
5. Đặt giới hạn byte/item cho history queue và flush khi đóng ứng dụng nếu history phải đầy đủ.

### P2 — chỉ làm khi số đo chứng minh cần

1. Giữ `URLSession` dùng chung trên Server.
2. Dùng kết nối HTTP lâu dài ở Pi.
3. Thay base64 bằng multipart/body ảnh thô.
4. Cho phép nhiều STT đồng thời thay vì actor dùng chung.
5. Gộp `/stt` và `/command` cho câu không ảnh.

Những mục P2 làm kiến trúc phức tạp hơn; chưa nên thực hiện trước khi có p50/p95 end-to-end.

## 10. Bộ kiểm tra nghiệm thu đề xuất

Thực hiện trên đúng Pi Zero 2 W, tai nghe, camera và Mac triển khai:

| Kịch bản | Số lượt tối thiểu | Cần ghi |
|---|---:|---|
| Chat không ảnh | 30 | latency từng chặng, tỉ lệ STT đúng |
| OCR đủ sáng/thiếu sáng/nghiêng | 10 mỗi loại | thời gian camera/OCR, nội dung đọc được |
| Observe | 30 | kích thước JPEG, upload, Gemini, tổng latency |
| Không có giọng/tiếng nền | 30 | số WAV bị chặn tại Pi, số request STT phát sinh |
| Mở/đóng phiên 60 giây | 20 phiên | lỗi state/wake word |
| Ngắt Mac/Tailscale | 10 | thời gian báo lỗi, số retry, khả năng phục hồi |
| Ngắt/tắt tai nghe | 10 | reconnect, chuyển SCO/A2DP |
| Chạy liên tục | ít nhất 2 giờ | RAM, CPU, nhiệt độ, queue history, dung lượng thẻ |

Chỉ nên ký nghiệm thu khi:

- không crash/treo/thiếu RAM;
- không thermal throttle trong bài chạy dài;
- Bluetooth và camera tự phục hồi theo kịch bản đã định;
- p95 độ trễ từng loại lệnh được ghi và chấp nhận bởi người dùng thử;
- artifact triển khai tái tạo được từ source hiện tại.

## 11. Phán quyết kiểm tra sản phẩm

- **Server:** đạt kiểm tra build và baseline HTTP; chưa đạt kiểm tra dịch vụ ngoài/run dài.
- **Pi:** đạt kiểm tra cú pháp và có đủ code bring-up; chưa đạt kiểm tra trên phần cứng trong vòng đánh giá này.
- **Kết nối:** thiết kế hợp lý và có retry/giới hạn; chưa có benchmark Tailscale end-to-end.
- **Toàn sản phẩm:** sẵn sàng bước vào nghiệm thu phần cứng có đo đạc, chưa nên tuyên bố hoàn thiện thực địa chỉ dựa vào source.
