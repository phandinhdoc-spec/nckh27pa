# Nhật ký phát triển dự án Thiên Nhãn

> Bản rà soát hội thoại đầy đủ ngày 20/07/2026. Múi giờ sử dụng trong nhật ký: Asia/Ho_Chi_Minh (UTC+7).

## 1. Phạm vi và nguồn phục dựng

Nhật ký này được viết lại sau khi đọc toàn bộ các task Codex còn truy cập được có liên quan trực tiếp đến sản phẩm:

| Task | ID | Thời gian | Số lượt |
|---|---|---|---:|
| Tiền sử phần cứng AI Green Eye/Pi2W | `019e7a8e-514a-75f3-bafa-95d61265c8ed` | 31/05 và 15/07/2026 | 3 |
| Rà soát và rút gọn Mac Server | `019f5fb4-dbd8-7551-95d8-f1b4195541e3` | 14/07/2026 | 9 |
| Xây dựng, bring-up và sửa lỗi Server–Pi | `019f60eb-eb16-7cc3-a680-1926571c8413` | 14–16/07/2026 | 118 |
| Tạo và rà soát bộ hồ sơ hiện tại | `019f7d33-e003-7f72-bf42-fbfc58913004` | 20/07/2026 | 2 |

Tổng cộng đã rà soát **132 lượt**, trong đó task Server–Pi có 109 lượt hoàn thành và 9 lượt bị ngắt. Lượt bị ngắt được giữ trong lịch sử nhưng không được coi là thay đổi đã hoàn thành nếu không có lượt tiếp theo xác nhận.

Một task không liên quan dù có cùng thư mục làm việc — cài Kitty cho macOS — đã bị loại khỏi nhật ký. Các task AI Green Eye chỉ được dùng ở phần chúng giải thích lịch sử phần cứng Pi hoặc nơi script DietPi được tạo theo yêu cầu chuyển giao từ task Thiên Nhãn.

Kho `/Users/phananh/Server` không có `.git`, vì vậy hội thoại là nguồn lịch sử mạnh hơn dấu thời gian inode. Dấu thời gian file chỉ dùng để đối chiếu trạng thái cuối.

### Quy ước độ chắc chắn

- **[Hội thoại xác nhận]**: người dùng yêu cầu, cung cấp log hoặc xác nhận kết quả trong chat.
- **[Mã nguồn xác nhận]**: trạng thái còn tồn tại trong source ngày 20/07/2026.
- **[Suy luận]**: giải thích hợp lý nhưng chat không ghi nguyên nhân trực tiếp.
- **[Đã thử rồi bỏ]**: từng được thực hiện/thử nghiệm nhưng không còn là lựa chọn cuối.
- **[Chưa hoàn thành]**: mới thảo luận, chưa có artifact cuối hoặc chưa được kiểm thử.

Nhật ký không sao chép nguyên văn token, mật khẩu, ảnh cấu hình hoặc hàng nghìn dòng terminal. Các log được tóm tắt theo triệu chứng, kết luận và quyết định.

## 2. Tóm tắt hành trình thật

```text
Pi Zero 2 W từng tự chạy model nhận diện rác
  → gặp giới hạn LiteRT/XNNPACK và độ phức tạp phần cứng
  → dựng Mac Swift Server nhưng over-engineer nhiều lớp chờ tương lai
  → audit và xóa gần 1.500 dòng scaffold
  → làm lại từng lát cắt: STT, Gemini, Vision, TTS, Command
  → chốt Pi là Python client mỏng, media đi qua RAM
  → bring-up thật: Bluetooth, camera, MAX98357A, DietPi, Tailscale
  → bỏ image DietPi 32-bit vì camera stack segfault
  → cài DietPi ARM64 và xử lý firmware start_cd/ISP camera
  → mSBC 16 kHz treo, quay về CVSD 8 kHz
  → đổi Apple Speech phía Server sang Groq Whisper Turbo
  → thêm lọc hallucination, phiên hội thoại, hướng dẫn âm thanh và phím Space
  → lập hồ sơ thiết kế/đánh giá và rà lại toàn bộ chat
```

---

# PHẦN I — TIỀN SỬ PHẦN CỨNG

## 3. Ngày 31/05/2026 — Pi Zero 2 W từng gánh AI cục bộ

**[Hội thoại xác nhận]** Trong task AI Green Eye, Pi Zero 2 W chạy một hệ thống nhận diện/phân loại rác thời gian thực:

- model TFLite/LiteRT khoảng 3,5 MB;
- 25 lớp;
- camera và nút GPIO25;
- systemd service tự restart;
- XNNPACK delegate trên CPU.

Model không load được tại node 124. Nhánh fallback cũ dùng `experimental_delegates=[]` nhưng vẫn không tắt XNNPACK mặc định. Code được sửa sang `BUILTIN_WITHOUT_DEFAULT_DELEGATES`; model được xác nhận có input `uint8 [1,224,224,3]` và output `uint8 [1,25]`.

Đây là **tiền sử sử dụng cùng dòng phần cứng**, không đủ bằng chứng để nói AI Green Eye và Thiên Nhãn là cùng một sản phẩm. Tuy nhiên nó cho thấy người làm đã thực sự trải qua việc chạy inference ngay trên Pi Zero 2 W trước khi kiến trúc Thiên Nhãn chốt nguyên tắc đưa AI nặng sang Mac.

**[Suy luận]** Kinh nghiệm với LiteRT/XNNPACK, camera, GPIO và service là một động lực thực tế cho lựa chọn “Pi mỏng, Mac xử lý” về sau.

---

# PHẦN II — MAC SERVER TỪ PHÌNH TO ĐẾN CÁC LÁT CẮT CHẠY THẬT

## 4. Ngày 14/07/2026, 15:19 — audit codebase phình to

Người dùng yêu cầu đọc toàn bộ `~/Server/Docs` và đánh giá vì code ngày càng phình dù vẫn build được.

**[Hội thoại xác nhận]** Trạng thái trước refactor:

- 33 file Swift, khoảng 2.637 dòng;
- cấu trúc chia theo tầng kỹ thuật: `Models`, `Services`, `Protocols`, `Policies`, `Routers`, `Engines`;
- nhiều file rỗng và binary build nằm ở root;
- `CommandService`, planner, intent, model policy, Vision metadata và TTS placeholder chưa có caller;
- test bundle không tạo được tín hiệu hữu ích trong môi trường lúc đó.

Kết luận quản lý: build xanh không đồng nghĩa sản phẩm gọn hoặc đã chạy end-to-end. Cảm giác “phình” của người dùng là đúng.

## 5. 15:38–15:48 — đổi cấu trúc theo tính năng

Người dùng đồng ý làm gọn theo hướng feature-first.

**Quyết định:**

- bỏ các thư mục chung chung và abstraction chưa có nhu cầu thật;
- đặt route/model gần tính năng;
- chỉ giữ `main`, `Config`, `Server`, `STT/`, `Gemini/`;
- chỉ tạo `Command/`, `Vision/`, `TTS/` khi có lát cắt chạy thật.

**[Hội thoại xác nhận]** Sau refactor:

- Swift giảm từ 2.637 xuống khoảng 1.127 dòng;
- từ 33 còn 8 file runtime;
- `swift build` thành công;
- `/health` trả 200;
- `/stt` thiếu token trả 401.

Đây là thay đổi kiến trúc quan trọng nhất: xóa code chờ tương lai, sau đó thêm lại từng khả năng theo chiều dọc.

## 6. 15:49–15:56 — Vision OCR

Người dùng chọn làm Vision trước.

**[Hội thoại xác nhận]** Một file `Vision/VisionRoutes.swift` được thêm, dùng Apple Vision API async mới:

- nhận ảnh thô;
- OCR tiếng Việt và tiếng Anh;
- trả text, confidence và bounding box pixel;
- không tạo protocol/service/file tạm.

Build và smoke test OCR thành công; token thiếu trả 401, MIME sai trả 415.

## 7. 15:56–16:13 — TTS MP3

Người dùng yêu cầu đọc `dictation_fixed.swift` vì file này từng chạy thành công.

**[Hội thoại xác nhận]** File đó chứa Apple Speech/AVFoundation cho STT, không có TTS MP3. Quá trình thử công cụ cho kết quả:

- `say` với giọng Việt `Linh` tạo AIFF thành công;
- `afconvert` lỗi `fmt?`, không xuất MP3;
- `lame` đã có trên Mac và mã hóa MP3 64 kbps thành công.

**Quyết định cuối:** `say` → AIFF tạm → LAME → MP3. Endpoint `/tts` được giữ trong một file, dọn file bằng `defer`, trả `audio/mpeg` trực tiếp.

Smoke test xác nhận MP3 mono 22,05 kHz/64 kbps, chuỗi rỗng trả 400, thiếu token trả 401 và không còn file tạm.

## 8. 16:13–16:25 — `/command` lần đầu

Người dùng yêu cầu nối `/command`.

Ban đầu phương án là multipart, nhưng Hummingbird core không có decoder sẵn. Để tránh tự viết parser hoặc thêm dependency, request được chọn là JSON base64.

Phiên bản đầu của `/command` nhận audio bắt buộc, ảnh/mode tùy chọn rồi chạy:

```text
STT → phân loại bằng luật → Vision/Gemini → TTS → MP3
```

Các ca system, OCR, chat và hỏi ảnh đều được smoke test end-to-end. Sau đó người dùng xác nhận Mac Server MVP đủ để chuyển sang Pi.

## 9. 16:35 — nhận ra MVP không gian chưa hoàn thành

Người dùng nhắc hai thuật toán cốt lõi còn thiếu:

1. hướng dẫn ngón tay tới các nút của thiết bị;
2. suy ra khoảng cách tương đối từ pixel kết hợp VL53L1X.

Kết luận được sửa lại:

- MVP giao tiếp Mac đã xong;
- MVP hỗ trợ không gian chưa xong;
- có thể sang Pi để làm camera/cảm biến;
- chưa thể gọi toàn bộ sản phẩm hoàn thiện.

Hướng dự kiến là state machine trái/phải/lên/xuống, vùng chết chống rung và dữ liệu chiều sâu từ cảm biến. **[Chưa hoàn thành]** Không có triển khai VL53L1X trong source hiện tại.

---

# PHẦN III — CHỐT HỢP ĐỒNG SERVER–PI VÀ VIẾT PI CLIENT

## 10. Ngày 14/07/2026, 20:58 — đọc lại toàn bộ Docs và source

Task chính bắt đầu bằng yêu cầu đọc toàn bộ `Docs`, sau đó rà toàn bộ Swift và `.env`.

Đánh giá lúc này cho thấy Mac đã có các endpoint, nhưng Pi client, cảm biến và automated test còn thiếu. Người dùng xác nhận VL53L1X thuộc Pi và tạm bỏ khỏi dữ liệu.

## 11. 21:09–21:13 — đổi hợp đồng hai bước

Người dùng mô tả luồng mong muốn:

1. Pi thu WAV.
2. Pi gửi `/stt`.
3. Mac trả text và cho biết có cần ảnh.
4. Pi chụp ảnh nếu cần.
5. Pi gửi text/ảnh tới `/command`.
6. Mac xử lý và trả MP3.
7. Pi phát âm thanh.

**Quyết định:** `/stt` trả thêm `command`, `mode`, `needsImage`; `/command` không STT lại mà nhận text/ảnh. Đây là lần hợp đồng API được sửa từ request audio-all-in-one sang hai request phù hợp camera có điều kiện.

Người dùng yêu cầu làm ngay sáu phần Server, hoãn test tổng thể đến khi Pi xong. Build và các smoke test Server vẫn đạt.

Một vòng review tiếp theo phát hiện ba điểm:

- Pi có thể ghi đè `mode` của Mac;
- model routing/fallback từng trỏ cùng model;
- cấu hình lắng nghe chưa khớp hoàn toàn mô tả mạng.

Nhật ký ghi nhận đây là cảnh báo review; trạng thái source cuối phải được đọc từ code hiện tại thay vì mặc định mọi đề xuất lúc đó đều đã sửa.

## 12. 21:37 — khóa phạm vi Pi

Người dùng quy định rõ:

- Pi dùng Python;
- làm việc trong `/Users/phananh/Server/Pi`;
- từ đây mặc định chỉ sửa Pi;
- chỉ quay lại Swift khi có yêu cầu rõ.

Đây là ranh giới làm việc được giữ cho đến khi người dùng chủ động nói “quay lại với Server”.

## 13. 21:39–21:57 — thiết kế Pi tối giản và lập trình

Kế hoạch ban đầu chỉ có:

```text
app.py
config.py
devices.py
mac_client.py
.env.example
README.md
```

Người dùng bổ sung yêu cầu dữ liệu:

- WAV chỉ ở RAM, không ghi thẻ;
- JPEG gửi trước rồi mới xếp history;
- MP3 phát trong RAM trước rồi mới xếp history;
- `history/image` và `history/sound` giữ 7 ngày;
- chỉ ghi khi CPU rảnh hoặc RAM sắp đầy.

Vì vậy `history.py`, `.gitignore` và hai thư mục history được thêm. Python standard library được chọn thay `requests`, `python-dotenv` hoặc framework.

**[Hội thoại xác nhận]** Client đầu tiên đã kết nối Server, nhận MP3; history được kiểm tra mtime/xóa 7 ngày; toàn bộ Python compile được.

---

# PHẦN IV — BRING-UP PHẦN CỨNG VÀ TRẢI NGHIỆM KHÔNG MÀN HÌNH

## 14. Ngày 15/07 — khởi động ban đầu bị đánh giá là “rất dở”

Người dùng phản hồi app chạy ngay, không cho biết Wi-Fi, camera hay Bluetooth đang ở trạng thái nào, gặp lỗi thì chết.

**Thay đổi đầu tiên:** startup tuần tự Wi-Fi → camera → Bluetooth, mỗi bước in terminal, lỗi thì đợi 5 giây và thử lại.

Tai nghe được cố định:

```text
E0:1A:03:08:A9:7C
A2DP để phát
SCO để thu
```

Khi tai nghe từng kết nối thiết bị khác, `br-connection-page-timeout` xuất hiện. Code được bổ sung scan rồi retry; không tự `remove` bond ngay.

## 15. Âm báo giọng Linh và MAX98357A

Người dùng cung cấp sơ đồ MAX98357A:

```text
GPIO18/BCLK, GPIO19/LRC, GPIO21/DIN
```

Ba file trạng thái Wi-Fi, camera và Bluetooth được tạo bằng `say` giọng Linh. Thiết kế lúc này là:

- Wi-Fi/camera báo qua MAX98357A;
- sau khi Bluetooth kết nối, mọi audio chuyển sang A2DP.

Người dùng còn hỏi về nút cơ học GND + pin 18/22. Khuyến nghị dùng pin vật lý 22/GPIO25 làm push-to-talk vì pin vật lý 12/GPIO18 đã dành cho I2S. Người dùng quyết định để chức năng nút lại sau. **[Chưa hoàn thành]** Source Thiên Nhãn hiện không dùng GPIO25.

## 16. Bỏ thu cố định 5 giây

Người dùng hỏi rõ `RECORD_SECONDS=5` và phản đối cách thu cố định.

**Yêu cầu mới:**

- ba tiếng bíp nhanh trước khi thu;
- nếu không nói gì trong 2 giây thì kết thúc;
- không gửi đoạn hoàn toàn im lặng;
- vẫn có giới hạn cứng 30 giây.

Thu âm được chuyển sang raw PCM theo chunk, tính RMS bằng thư viện chuẩn và đóng WAV trong RAM.

## 17. Lối tắt cấu hình `.env.example`

Do copy/paste Terminal bất tiện, người dùng yêu cầu chèn thông số từ ảnh vào `.env.example`. Thao tác này từng được thực hiện trong chat.

Sau đó Pi báo thiếu `MAC_SERVER_URL` vì app chỉ đọc `.env`. Một hướng fallback sang `.env.example` được chuẩn bị nhưng lượt xử lý bị ngắt.

**[Mã nguồn xác nhận]** Trạng thái cuối ngày 20/07 là `Pi/.env` tồn tại còn `Pi/.env.example` không tồn tại; README cũ vẫn hướng dẫn copy file mẫu. Vì vậy không được coi giải pháp fallback trong lượt bị ngắt là hoàn thành.

## 18. Treo sau Wi-Fi thực ra là phát MP3

App dừng sau dòng “Wi-Fi đã kết nối”. Phân tích dòng chạy cho thấy chương trình chưa tới camera; nó đang chờ `mpg123` trên ALSA `default`.

Thông báo hệ thống được giới hạn timeout và lỗi loa không còn chặn chuỗi startup. Đây là bài học chẩn đoán: vị trí dòng log quan trọng hơn phỏng đoán camera.

## 19. Camera: từ stdout tới `/dev/shm` và `rpicam-jpeg`

Các bước mò camera diễn ra liên tiếp:

1. Cách lấy JPEG qua stdout không ổn trên máy thật.
2. Chuyển sang file tạm trong `/dev/shm`, đọc/kiểm tra rồi xóa.
3. Source từng gọi `rpicam-still`; người dùng cung cấp code Pi OS Lite chạy được bằng `rpicam-jpeg`.
4. Code được chốt chỉ dùng `rpicam-jpeg`, 1280×720, timeout 500 ms.
5. Một `test_camera.py` riêng được tạo để thử 500 ms, 2 giây và `--immediate`.
6. Script test đầu tiên gặp lỗi f-string chứa byte escape; lỗi được sửa để tương thích Python 3.9–3.13.

Sau đó cả `rpicam-jpeg --version` cũng segmentation fault. Điều này xác nhận lỗi dưới Python.

## 20. Bỏ image DietPi 32-bit và cài mới ARM64

Log máy cho thấy:

```text
armv7l
armhf
DietPi 32-bit
```

Camera stack/kernel/package không đồng bộ và `rpicam-jpeg` segfault ngay ở `--version`. Người dùng chấp nhận cài mới thay vì vá tiếp 32-bit.

**Quyết định:** flash DietPi ARMv8/ARM64; sau cài phải có:

```text
uname -m → aarch64
dpkg --print-architecture → arm64
```

Người dùng hỏi có phải cài lại Tailscale/Bluetooth/MAX98357A không; câu trả lời là có, nhưng chỉ mang code, cấu hình cần thiết và history, không sao chép toàn bộ hệ thống cũ.

---

# PHẦN V — SCRIPT SETUP VÀ DIETPI ARM64

## 21. 15/07, 23:29 — tạo script setup một lần chạy

Theo yêu cầu chuyển giao sang task Pi2W, `setup_pi2w.sh` được tạo với mục tiêu:

- chạy `sudo bash setup_pi2w.sh`;
- fail sớm nếu không phải `aarch64/arm64`;
- apt package tối thiểu;
- Tailscale không nhúng auth key;
- Bluetooth pair/trust/connect có giới hạn;
- xác nhận overlay MAX98357A trước khi sửa boot config;
- kiểm tra `rpicam-jpeg` và JPEG trong `/dev/shm`;
- in báo cáo cuối;
- không tự reboot.

Không có README trong task V5 nên chỉ script được tạo. Sau đó file được chuyển về `/Users/phananh/Server/setup_pi2w.sh` theo yêu cầu để dễ tìm.

## 22. Đối chiếu DietPi cũ

Bốn file `cmdline.txt`, `config.txt`, `dietpi-wifi.txt`, `dietpi.txt` cũ được đọc làm tham khảo.

Chỉ hai thiết lập phần cứng được giữ:

```ini
camera_auto_detect=1
dtparam=audio=off
```

Wi-Fi, mật khẩu, token, hostname và cấu hình mạng cũ không được chép sang image mới.

## 23. First boot và tìm IP

Người dùng trải qua first boot DietPi lâu, phải dò IP nhiều lần. Mạng Mac thay đổi từ dải `192.168.x.x` sang `172.30.36.x`, khiến các lần quét ban đầu sai subnet.

Sau khi SSH được, DietPi tự update/upgrade. Trước khi chạy setup, các kiểm tra được chốt:

- ARM64 đúng;
- không còn apt/dpkg chạy;
- mạng và dung lượng ổn;
- reboot trước nếu DietPi yêu cầu.

Tailscale được đưa lên với hostname cố định `hieunga`.

## 24. BlueALSA: cần cả loa và micro

Rà theo DietPi/Debian cho thấy chỉ `bluez` là chưa đủ. Script bổ sung:

- `bluez-alsa-utils`;
- `libasound2-plugin-bluez`;
- A2DP-source để phát;
- HFP-AG/HSP-AG để thu SCO.

Bluetooth pair ban đầu nhiều lần lỗi `page-timeout` vì trạng thái tai nghe/bond cũ. Người dùng cuối cùng kết nối được sau khi dừng scan và đưa tai nghe đúng trạng thái.

Kết quả BlueALSA lúc đầu:

```text
A2DP 48 kHz playback
HFP mSBC 16 kHz capture/playback
CVSD cũng khả dụng
```

Một `headset-reconnect.service` được đề xuất. Người dùng phản đối việc chạy lại toàn bộ setup chỉ để thêm service; vì vậy service được đưa thành khối cài độc lập. Bài học: thay đổi vận hành nhỏ không nên bắt chạy lại mọi bước apt/camera.

---

# PHẦN VI — CAMERA TRÊN DIETPI ARM64

## 25. `rpicam-apps` báo “other platform” dù đúng Pi

Trên image ARM64 mới, Pi đúng model và quyền nhóm, IMX708/unicam xuất hiện nhưng `rpicam-apps` báo chỉ hỗ trợ Raspberry Pi.

Người dùng từng đề nghị viết lại toàn bộ Pi vì nghĩ code cũ không hợp hệ điều hành. Việc viết lại được dừng lại: lỗi xảy ra trong binary/platform detection, trước code Python.

## 26. Tìm root cause `start_cd`

Chẩn đoán cho thấy:

```text
gpu_mem_256=16
gpu_mem_512=16
gpu_mem_1024=16
firmware variant start_cd
bcm2835_mmal_vchiq: Failed to open VCHI service connection
```

Các giá trị GPU memory 16 MB khiến firmware rút gọn `start_cd` được dùng và ISP không hoạt động. Chúng được đổi lên 64 MB, sau đó reboot.

**Kết quả:** firmware chuyển sang `variant start`, `bcm2835-isp` nạp được và IMX708 được `rpicam-hello` liệt kê.

## 27. Auto-detect, overlay và module ISP

Có một giai đoạn sau reboot camera báo “no cameras available”; `camera_auto_detect=1` chưa tạo device tree như mong đợi. `dtoverlay=imx708` từng được đề xuất thử thủ công.

Sau đó IMX708/unicam xuất hiện nhưng ISP vẫn thiếu. `sudo modprobe bcm2835-isp` làm camera hoạt động ngay.

Ban đầu module được đặt trong `/etc/modules-load.d`, nhưng sau reboot lại lỗi vì module nạp quá sớm khi firmware/VCHI chưa sẵn sàng.

**Quyết định cuối:** tạo `camera-isp.service` oneshot, nạp muộn và thử lại tối đa 10 lần, cách nhau 2 giây. Đây là giải pháp còn tồn tại trong source/setup cuối.

---

# PHẦN VII — MICRO, TIẾNG ỒN VÀ QUYẾT ĐỊNH 8 KHZ

## 28. Thu âm bị quá 30 giây

Người dùng báo môi trường ồn khiến thu đủ 30 giây hoặc không phát hiện giọng. Một vấn đề khác là `read()` có thể block khi BlueALSA không trả frame, nên deadline Python không được kiểm tra.

Các cải tiến được thảo luận/thực hiện:

- đếm lùi thời gian trên terminal;
- lấy chunk bằng `select` để deadline thực hoạt động;
- đo noise floor 0,5 giây;
- yêu cầu 0,3 giây liên tục vượt ngưỡng mới coi là giọng;
- giữ pre-roll;
- dừng sau 2 giây dưới ngưỡng;
- không gửi đoạn không có giọng.

## 29. mSBC 16 kHz bị treo

Tai nghe quảng cáo mSBC 16 kHz và BlueALSA thương lượng thành công, nhưng `arecord` đứng ở 00% và không nhận frame SCO. `timeout` phải gửi tín hiệu ngắt; WAV chỉ có rất ít dữ liệu.

Chuyển sang CVSD:

```text
S16_LE
mono
8 kHz
```

thì thu ổn định hơn.

`test_micro.py` được viết để:

- ép CVSD trước khi thu;
- thu 10 giây;
- kiểm tra WAV;
- phát lại;
- có timeout cứng.

## 30. Volume và chất lượng

BlueALSA báo:

- micro 9/15, sau đó người dùng tăng thử 12/15;
- SCO playback 15/15;
- A2DP playback 127/127.

Lỗi `Device or resource busy` xuất hiện khi app/test/arecord khác còn giữ PCM. Kết luận: không chạy `app.py` và `test_micro.py` đồng thời.

CVSD nghe không rõ do giới hạn thoại 8 kHz; phát lại qua SCO còn làm chất lượng tệ hơn. A2DP hoặc nghe file trên Mac được dùng để phân biệt lỗi thu và lỗi phát.

## 31. Thử 16, 20 và 32 kHz

Theo yêu cầu người dùng, `test_micro_rates.py` thử 16/20/32 kHz. Chat xác nhận 20/32 chỉ là nội suy từ nguồn mSBC 16 kHz, không tạo thêm chi tiết.

Sau lỗi cú pháp PCM `plug:bluealsa`, test được sửa. mSBC vẫn thương lượng nhưng mọi sample rate đều treo.

Người dùng yêu cầu bỏ 20/32 và cố 16 kHz, sau đó dừng thay đổi nhánh chính và yêu cầu tạo `Pi16` riêng.

**[Đã thử rồi bỏ]** `Pi16` dùng mSBC 16 kHz, BlueALSA real-time priority và script `enable_msbc_16khz.sh`. Nó vẫn gặp mất frame SCO và cả vấn đề reconnect tai nghe.

## 32. Quay về CVSD 8 kHz

Người dùng ra lệnh quay lại 8 kHz và dự định xóa `Pi16`.

Chỉ restart service hoặc xóa thư mục không đủ vì script mSBC đã sửa systemd override. `restore_cvsd_8khz.sh` được tạo để:

- tắt mSBC/LC3-SWB;
- restart BlueALSA;
- giữ CVSD;
- không chủ động ngắt tai nghe.

**[Mã nguồn xác nhận]** Lựa chọn cuối là `Pi/`, CVSD mono 8 kHz. `Pi16/` vẫn còn trong workspace như artifact thử nghiệm dù người dùng từng nói sẽ xóa.

---

# PHẦN VIII — TRẢI NGHIỆM HỘI THOẠI VÀ SERVER GROQ

## 33. Hướng dẫn sử dụng và từ khóa

Người dùng yêu cầu app phải phát hướng dẫn khi khởi động hoặc khi nghe “Giúp tôi”. File `usage-guide.mp3` được tạo; timeout phát được tăng vì file dài hơn thông báo trạng thái.

Ban đầu mọi câu phải bắt đầu bằng “Thiên Nhãn”. Người dùng phản đối vì không tự nhiên.

**Quyết định mới:**

- nói “Thiên Nhãn” một lần để mở phiên;
- trong 60 giây nói tự nhiên, mỗi lượt hợp lệ gia hạn phiên;
- “Hết”, “Thôi”, “Dừng lại”, “Kết thúc” đóng phiên;
- phiên được giữ hoàn toàn trên Pi, Server xử lý từng câu độc lập.

Yêu cầu “nghe chữ Hết ngay khi đang thu để dừng micro” được phân tích là không khả thi với STT không streaming: Pi chỉ biết text sau khi gửi hết WAV. **[Chưa hoàn thành]** Từ kết thúc chỉ đóng phiên sau một lượt STT.

## 34. Startup đổi sang Bluetooth trước

Người dùng chỉ ra nếu Bluetooth chưa sẵn sàng thì gần như không dùng được sản phẩm, nên yêu cầu đưa nó lên bước đầu và phát mọi trạng thái qua tai nghe.

Thứ tự cuối trong code:

```text
Bluetooth → Wi-Fi → camera
```

Điều này làm thiết kế MAX98357A ban đầu bị thu hẹp: overlay và biến cấu hình vẫn tồn tại, nhưng app hiện phát `_announce` qua Bluetooth. Đây là thay đổi hướng có thật, không nên tiếp tục mô tả MAX98357A như kênh startup đang hoạt động nếu chưa sửa code.

## 35. Chuyển STT Server từ Apple sang Groq

Người dùng chủ động nói “quay lại với Server” và yêu cầu không dùng công cụ macOS cho STT nữa, chuyển sang Groq Whisper Turbo.

**Thay đổi:**

- xóa Apple Speech/DictationTranscriber/AudioConverter khỏi target STT;
- giữ nguyên API `/stt` để Pi không đổi;
- Mac gửi WAV trực tiếp tới Groq multipart;
- model mặc định `whisper-large-v3-turbo`;
- ngôn ngữ `vi`;
- Pi không giữ Groq API key.

Build thành công. Một lần chạy Server mới lỗi NIO vì process Server cũ còn giữ cổng 8765; đây không phải lỗi build hay Groq.

## 36. Lọc hallucination Groq

Log Pi cho thấy tiếng ồn đôi khi bị nhận thành:

```text
“Cảm ơn các bạn đã theo dõi và hẹn gặp lại.”
```

Server được đổi sang `verbose_json`, dùng `no_speech_prob`, log probability và danh sách hallucination phổ biến để loại transcript giả. Prompt tiếng Việt và tên “Thiên Nhãn” được bổ sung.

Luồng cuối được người dùng xác nhận giữ nguyên:

```text
Pi WAV → Mac /stt → Groq → text trên Mac → /command → Gemini/TTS → MP3 → Pi
```

## 37. Phím Space để dừng thu

Người dùng muốn bản test trên laptop/SSH để tránh thu lung tung.

Đầu tiên `app_keyboard.py` được tạo riêng:

- Enter bắt đầu;
- Space dừng;
- bỏ wake word để test thẳng `/command`;
- `q` thoát.

Sau đó người dùng yêu cầu giữ luồng chính và tích hợp Space vào `app.py`. Code cuối hỗ trợ:

- có TTY: Space dừng ngay và đóng WAV đã thu;
- chạy service không TTY: vẫn dùng silence/deadline.

## 38. Ý tưởng đóng gói Server

Người dùng hỏi có thể build phần mềm độc lập sau khi test không.

Khả năng được xác nhận:

- `swift build -c release` tạo executable;
- tương lai có thể làm `ThienNhanServer.app`/menu bar;
- cần xử lý dependency LAME và trạng thái/log.

**[Chưa hoàn thành]** Kho hiện tại chưa có `.app`, menu bar hoặc launch-at-login.

---

# PHẦN IX — HỒ SƠ NGÀY 20/07/2026

## 39. Yêu cầu quản lý, thiết kế, sản xuất và kiểm tra

Người dùng yêu cầu ba thư mục, ưu tiên một Markdown mỗi yêu cầu:

1. thiết kế cuối cùng, chia Server/Pi;
2. nhật ký từ ý tưởng sơ khai và các lần đổi công cụ/hướng đi;
3. đánh giá Server/Pi/kết nối về hiệu suất và tối ưu phần cứng.

Ba hồ sơ được tạo. Swift build, Python compile, shell syntax và `/health` baseline được kiểm tra.

## 40. Rà lại toàn bộ chat

Người dùng nhận thấy nhật ký dựng chủ yếu từ file chưa phản ánh hết quá trình hỏi–đáp. Task hiện tại được đọc lại bằng công cụ quản lý Codex; sau đó hai task lịch sử Server và task phần cứng V5 liên quan được đọc hết theo cursor.

Kết quả của lần rà này là bản nhật ký hiện tại: bổ sung giai đoạn codebase phình to, contract `/command` thay đổi, cài lại DietPi, chuỗi lỗi camera/ISP, toàn bộ thử nghiệm micro, chuyển Apple STT sang Groq, phiên hội thoại và các lượt bị ngắt.

---

# PHẦN X — SỔ QUYẾT ĐỊNH CUỐI

## 41. Các lần đổi công cụ và hướng đi

| Chủ đề | Từ | Sang | Lý do được chat xác nhận |
|---|---|---|---|
| Kiến trúc Swift | 33 file theo tầng, nhiều scaffold | feature-first, thêm lát cắt khi chạy thật | code phình dù build xanh |
| Vision | placeholder/metadata | Apple Vision OCR thật | cần text + bounding box, xử lý cục bộ trên Mac |
| TTS MP3 | `afconvert` | `say` giọng Linh + LAME | `afconvert` thực tế lỗi `fmt?`, LAME chạy được |
| `/command` | audio + mọi thứ một request | `/stt` trước, text/ảnh `/command` sau | Pi chỉ chụp ảnh khi Mac báo cần |
| Pi runtime | framework/dependency chưa cần | Python standard library + công cụ hệ thống | yêu cầu gọn, đúng, đủ |
| Media Pi | file tạm trên thẻ | WAV/JPEG/MP3 ưu tiên RAM | giảm ghi thẻ và đúng thứ tự truyền/phát trước history |
| Startup | chạy ngay, die khi lỗi | các bước có trạng thái/retry | người dùng không biết app đang kẹt ở đâu |
| Startup order | Wi-Fi → camera → Bluetooth | Bluetooth → Wi-Fi → camera | không có tai nghe thì sản phẩm gần như không dùng được |
| Camera command | `rpicam-still`/stdout | `rpicam-jpeg` + `/dev/shm` | code mẫu chạy thật trên Pi và stdout không ổn |
| Pi OS | DietPi 32-bit armhf | DietPi ARM64 | `rpicam-jpeg --version` segfault, không đổi kiến trúc bằng upgrade |
| Camera firmware | `gpu_mem=16`, `start_cd` | `gpu_mem=64`, firmware `start` | ISP/VCHI không hoạt động với firmware rút gọn |
| ISP boot | `modules-load.d` | systemd oneshot nạp muộn/retry | module nạp quá sớm và không tự thử lại |
| Bluetooth backend | chỉ BlueZ | BlueALSA A2DP + HFP/HSP | cần cả loa và micro |
| Thu âm | cố định 5 giây | 3 bíp + VAD RMS + 2 giây im lặng + deadline | tự nhiên hơn và không gửi im lặng |
| Codec micro | CVSD 8 kHz → thử mSBC 16 kHz | quay lại CVSD 8 kHz | mSBC thương lượng được nhưng SCO không trả frame |
| STT Server | Apple Speech | Groq Whisper Large V3 Turbo | người dùng ưu tiên Groq và giữ Pi gửi WAV qua Mac |
| Wake word | lặp “Thiên Nhãn” mỗi câu | mở phiên 60 giây bằng một lần gọi | cách cũ không tự nhiên |
| Dừng thu test | chỉ VAD | thêm Space khi có TTY | tránh thu lung tung khi test SSH/laptop |
| Cấu hình hệ thống | chạy lại setup cho thay đổi nhỏ | script/service riêng khi phù hợp | tránh cài lại toàn bộ chỉ để thêm reconnect/rollback |

## 42. Các ngõ cụt và bài học

1. **Build xanh nhưng code vẫn sai hướng.** Audit đã xóa gần 1.500 dòng scaffold chưa có caller.
2. **Không viết lại ứng dụng khi lỗi ở binary/kernel.** Hai lần người dùng nghi cần viết lại Pi, nhưng camera lỗi ở rpicam stack/ISP.
3. **Đọc vị trí log trước khi kết luận.** Treo sau Wi-Fi là `mpg123`, không phải camera.
4. **Thông số được quảng cáo không đồng nghĩa đường dữ liệu hoạt động.** mSBC được chọn nhưng không có frame SCO.
5. **Tăng sample rate không khôi phục chi tiết.** 20/32 kHz chỉ nội suy nguồn 16 kHz.
6. **Restart service không hoàn tác config.** Cần script CVSD ghi lại systemd override.
7. **Module load thành công chưa chắc device đã sẵn sàng.** ISP còn phụ thuộc firmware/VCHI và thời điểm boot.
8. **Lượt chat bị ngắt không phải trạng thái hoàn thành.** Fallback `.env.example`, thay đổi mSBC trực tiếp và một số patch camera chỉ được coi là thử nghiệm cho tới khi có lượt sau xác nhận.
9. **Artifact đóng gói có thể cũ hơn source.** `Pi.zip` được tạo trước các thay đổi cuối ngày 16/07.
10. **Tài liệu phải theo code thật.** README cũ còn Raspberry Pi OS, `.env.example` và startup order đã lỗi thời.

---

# PHẦN XI — TRẠNG THÁI CUỐI VÀ PHẦN CÒN THIẾU

## 43. Trạng thái được source ngày 20/07 xác nhận

- Server Swift 6.4/Hummingbird build thành công.
- STT dùng Groq Whisper Large V3 Turbo.
- Vision dùng Apple Vision.
- Chat/ảnh dùng Gemini theo routing.
- TTS dùng `say` giọng Linh + LAME.
- Pi chính là `Pi/`, Python, CVSD mono 8 kHz.
- Camera dùng `rpicam-jpeg`, JPEG 1280×720 trong `/dev/shm`.
- Startup: Bluetooth → Wi-Fi → camera.
- Phiên hội thoại: wake word một lần, timeout 60 giây.
- Space dừng thu khi chạy terminal.
- History ảnh/MP3 giữ 7 ngày, WAV không lưu.
- Setup gốc có BlueALSA, Tailscale, camera ISP và MAX98357A.

## 44. Những việc đã nói nhưng chưa hoàn thành

- thuật toán hướng ngón tay tới nút thiết bị;
- hợp nhất pixel camera với VL53L1X;
- ngữ cảnh hội thoại nhiều lượt thật trên Server;
- STT streaming để từ “Hết” ngắt micro ngay;
- dùng nút cơ học GPIO25;
- sử dụng MAX98357A thật trong app startup hiện tại;
- automated test đầy đủ;
- benchmark end-to-end và soak test phần cứng;
- đóng gói `ThienNhanServer.app`;
- release archive mới thay `Pi.zip` cũ;
- một `.env.example` Pi sạch, đồng bộ README.

## 45. Giới hạn của nhật ký

Các task Codex có thể đọc ngày 20/07 đã được rà hết. Tuy nhiên vẫn có giới hạn:

- không có Git commit để biết diff chính xác sau từng lượt;
- một cuộc trao đổi VL53L1X trước đó được người dùng nhắc tới nhưng không tìm thấy trong danh sách task hiện có;
- một số ảnh/log đính kèm cũ chỉ còn tóm tắt trong turn, không còn nội dung trực quan đầy đủ;
- một số lời đáp trong chat mô tả ý định hoặc trạng thái tạm thời đã bị thay đổi ở lượt sau.

Vì vậy “trạng thái cuối” luôn lấy từ source hiện tại; “hành trình” lấy từ chat.

---

# PHỤ LỤC — CHỈ MỤC BAO PHỦ TOÀN BỘ LƯỢT CHAT

## A. Task audit Server — 9/9 lượt

| Lượt | Nội dung |
|---:|---|
| 1 | Yêu cầu audit code phình; đọc Docs/source, xác định scaffold chết. |
| 2 | Đánh giá cấu trúc thư mục; đề xuất feature-first. |
| 3 | Người dùng đồng ý refactor; giảm 33 xuống 8 file runtime. |
| 4 | Hỏi bước tiếp theo; chọn Vision OCR. |
| 5 | Làm Vision OCR end-to-end. |
| 6 | Làm TTS MP3, đối chiếu `dictation_fixed.swift`, chọn LAME. |
| 7 | Làm `/command` end-to-end. |
| 8 | Review toàn bộ, xác nhận đủ chuyển sang Pi. |
| 9 | Bổ sung hai thuật toán không gian/VL53L1X còn thiếu. |

## B. Task Server–Pi — 118/118 lượt

| Lượt | Nội dung được bao phủ |
|---:|---|
| 1–9 | Đọc Docs/Swift; chốt contract; bỏ VL53L1X tạm thời; khóa scope Pi; lập trình Pi và history RAM. |
| 10–19 | Bluetooth ban đầu; startup status; cố định tai nghe; page-timeout; tạo âm báo Linh và đường MAX98357A/A2DP. |
| 20–29 | Nút GPIO25; bỏ thu cố định; 3 bíp/VAD; cấu hình `.env`; treo playback; đổi camera sang `rpicam-jpeg`; tạo test camera. |
| 30–37 | Sửa test camera; xác định rpicam segfault trên armhf; quyết định cài mới DietPi ARM64 và chuẩn bị khôi phục dịch vụ. |
| 38–52 | Tạo setup; đối chiếu config cũ; first boot; tìm IP; kiểm tra ARM64; rà BlueALSA; chuyển script về Server. |
| 53–62 | Chép/chạy script; Tailscale hostname; pair Bluetooth; xử lý timeout; xác nhận A2DP + HFP/mSBC. |
| 63–70 | Reconnect service; package camera; lỗi “other platform”; chẩn đoán model/driver/ISP; tìm `start_cd`. |
| 71–77 | Sửa GPU memory; xử lý tiếng ồn/deadline; kiểm tra micro; xác định mSBC block; cập nhật setup CVSD/camera. |
| 78–90 | Hướng dẫn sử dụng; chuỗi lỗi camera sau reboot; nạp ISP; test micro; sửa codec; thêm phiên hội thoại 60 giây. |
| 91–101 | Volume/noise; PCM busy; chất lượng CVSD; thử 16/20/32 kHz; chuẩn codec; bật lại HFP/mSBC. |
| 102–109 | Bluetooth reconnect tranh chấp; test mSBC tiếp; tạo Pi16 riêng; giải thích mất frame; quay về CVSD và tạo rollback script. |
| 110–113 | Chuyển STT Apple sang Groq; xử lý process chiếm cổng; thảo luận app macOS độc lập; sửa camera boot service. |
| 114–118 | Tạo app test Space; lọc hallucination Groq; xác nhận Pi không cần Groq key; tích hợp Space vào app chính; sửa lệnh arecord đúng MAC. |

Chín lượt bị ngắt nằm trong các nhóm trên gồm: yêu cầu startup trùng lặp, yêu cầu âm báo trùng, xử lý `.env` chưa chốt, một bước chẩn đoán camera, tách reconnect service, countdown thu, thay đổi trực tiếp 16 kHz và lệnh quay về 8 kHz. Mỗi trường hợp đều được đánh giá bằng lượt hoàn thành kế tiếp hoặc trạng thái source cuối.

## C. Task phần cứng V5 — 3/3 lượt

| Lượt | Nội dung |
|---:|---|
| 1 | Lỗi model TFLite/XNNPACK trên Pi Zero 2 W; sửa resolver không delegate. |
| 2 | Task Thiên Nhãn chuyển giao yêu cầu tạo script DietPi ARM64 tối giản. |
| 3 | Đọc bốn file DietPi cũ, chỉ giữ `camera_auto_detect=1` và `dtparam=audio=off`. |

## D. Task hồ sơ — 2/2 lượt

| Lượt | Nội dung |
|---:|---|
| 1 | Tạo ba thư mục/tài liệu: thiết kế, nhật ký, đánh giá. |
| 2 | Yêu cầu rà toàn bộ chat; đọc các task lịch sử và viết lại nhật ký này. |
