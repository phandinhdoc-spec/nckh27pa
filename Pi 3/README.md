# Thiên Nhãn Pi

Client Python cho Raspberry Pi Zero 2 W chạy DietPi Trixie ARM64 (Python 3.10+). Pi giữ **một luồng mic USB mở liên tục**, tự cắt đoạn có giọng nói bằng VAD cục bộ, mã hoá MP3 và gọi **Groq Whisper** để nhận dạng; transcript được gửi sang Mac `/command`. Pi cũng đọc cảm biến GY25 + GY63, chụp ảnh khi cần và phát MP3 trả về **chỉ qua loa MAX98357A**.

## Kiến trúc âm thanh

- **Mic = mic USB rời**, mở một lần bằng `arecord` (`S16_LE`, mono, `MIC_SAMPLE_RATE`) và giữ mở suốt thời gian chạy. VAD tách từng đoạn giọng nói; GPIO25 chuyển chế độ sau khi khởi động.
- **VAD cục bộ:** đo nền 0,5 s, ngưỡng = `max(SILENCE_RMS, 2 × RMS nền)`, cần vượt ngưỡng liên tục 0,3 s để bắt đầu một utterance, tự kết thúc sau `SILENCE_SECONDS` im lặng (tối đa `MAX_RECORD_SECONDS`).
- **STT:** đoạn PCM được mã hoá MP3 bằng `lame` rồi gửi tới Groq Whisper (`GROQ_STT_MODEL`, mặc định `whisper-large-v3-turbo`). Ngôn ngữ mặc định `vi`.
- **Phát âm thanh:** mọi prompt và câu trả lời luôn phát qua loa **MAX98357A** (`ONBOARD_PLAYBACK_DEVICE`).
- **Bluetooth:** KHÔNG được chạm tới khi khởi động hoặc khi rảnh. Chỉ khi người dùng nói yêu cầu rõ ràng (ví dụ “Thiên Nhãn kết nối tai nghe”) app mới thử kết nối giới hạn (tối đa 2 lần × 3 s) và báo kết quả qua loa MAX.

## Từ khoá đánh thức

- Từ khoá: **“Thiên Nhãn”** — khớp không phân biệt hoa/thường và không phân biệt dấu (NFD đã được chuẩn hoá).
- Chấp nhận hai dạng:
  - Cùng đoạn: “Thiên Nhãn mấy giờ rồi” → gửi ngay `mấy giờ rồi`.
  - Đoạn kế tiếp: “Thiên Nhãn” (chờ) rồi “mấy giờ rồi” ở lượt nói sau.
- Đoạn nói không có từ khoá (và không nằm trong trạng thái chờ) bị bỏ qua.

## Cấu hình

```sh
cp .env.example .env
micro .env
```

Điền `MAC_SERVER_URL`, `SERVER_TOKEN` và `GROQ_API_KEY`. Tailscale trên hệ thống triển khai hiện đã hoạt động; setup chỉ giữ bước kiểm tra service.

Các biến quan trọng:

```dotenv
ONBOARD_PLAYBACK_DEVICE=default
BLUETOOTH_DEVICE_MAC=04:60:61:EF:78:22
MIC_DEVICE=default
MIC_SAMPLE_RATE=16000
SILENCE_SECONDS=2
SILENCE_RMS=500
MAX_RECORD_SECONDS=30
GROQ_API_KEY=...
GROQ_STT_MODEL=whisper-large-v3-turbo
GROQ_STT_URL=https://api.groq.com/openai/v1/audio/transcriptions
GROQ_LANGUAGE=vi
GROQ_TIMEOUT_SECONDS=30
COMMAND_TIMEOUT_SECONDS=60
HTTP_RETRIES=2
```

`GROQ_API_KEY` có thể đặt trong `.env` hoặc trong môi trường của service (`EnvironmentFile=/opt/thiennhan/.env`). `BLUETOOTH_CAPTURE_DEVICE` (SCO) đã bị bỏ — mic luôn là mic USB.

## Đấu MAX98357A

- Pin 2 (5V) → VIN
- Pin 6 (GND) → GND
- Pin 12 (GPIO18) → BCLK
- Pin 35 (GPIO19) → LRC / WS
- Pin 40 (GPIO21) → DIN
- SPK+ → loa +
- SPK- → loa -

## Đấu cảm biến

- GY25 (UART): `GY25 TX → Pi RX (GPIO15)`, `GY25 RX → Pi TX (GPIO14)`, chung GND.
- GY63 / MS5611 (I2C bus 1): SCL GPIO3, SDA GPIO2, địa chỉ `0x76`.
- GY53 / VL53L1X (I2C bus 1): SCL GPIO3, SDA GPIO2, địa chỉ 

GPIO25 dùng một bộ phân loại chung (kéo lên, nút nối GND): hai lần nhấn chuyển
giữa hội thoại và chế độ nhìn; một lần nhấn trong chế độ nhìn ngắt tác vụ đang
phát/đợi phản hồi; từ ba lần nhấn trở lên chỉ được ghi nhận, chưa điều hướng.
Các lần nhấn cách nhau tối đa 1,5 giây (bao gồm biên), chống dội 60 ms;
chỉ phân loại sau 1,51 giây yên lặng. Cần `python3-gpiozero` trên Pi.

Chế độ nhìn: nói yêu cầu trực tiếp, không cần từ khóa; sau transcript không rỗng
Pi chụp **một** JPEG và gửi `/command` với `mode=observe`, rồi tiếp tục chờ.
Không mở camera liên tục, không lưu ảnh/âm thanh của chế độ này vào history.
Phản hồi trễ bị loại bằng token phiên/tác vụ; tiến trình `mpg123` của phần trả
lời đang phát có thể bị dừng. Đây mới là kiểm thử giả lập trên macOS, **chưa**
kiểm tra GPIO, camera, tiếng vọng mic, tư thế ảnh hoặc Gemini trên Pi. Các lời
nhắc âm thanh cũ của hội thoại và thao tác chụp/encode, HTTP đang chạy chưa bị
hủy ngay ở tầng OS; HTTP có thể hoàn tất ở Mac dù Pi bỏ kết quả. Máy chủ cần
xác nhận prompt cho tìm vật/đọc tài liệu, chính sách lưu dữ liệu và hỗ trợ hủy
từ xa trước khi dùng với trang riêng tư. Chưa có MP3 hướng dẫn chế độ nhìn.

## Cài đặt chính thức

Đặt tai nghe vào pairing mode, rồi chạy trong thư mục dự án:

```sh
sudo bash setup_pi2w.sh
```

Setup sẽ:

1. Cài `rpicam-apps`, ALSA, `lame`, `mpg123`, BlueZ, BlueALSA, Python, `python3-serial` và `python3-smbus2`.
2. Cấu hình BlueALSA chỉ `-p a2dp-source` (phát ra tai nghe; không bật SCO/HFP capture).
3. Pair/trust/connect tai nghe `04:60:61:EF:78:22`, tối đa 2 lần; nếu không được, bỏ qua kiểm tra A2DP và tiếp tục cài đặt.
4. Mở thử PCM WAV A2DP — không test capture Bluetooth.
5. Cấu hình camera, MAX98357A và **bật UART/I2C** (`enable_uart=1`, `dtparam=i2c_arm=on`) trong `/boot/firmware/config.txt`.
6. Thêm **WiFi dự phòng** (mặc định SSID `Pdmq`) vào wpa_supplicant *nếu* wpa_supplicant đang là trình quản lý mạng đang chạy; nếu không thì bỏ qua và in cảnh báo.
7. Cài ứng dụng vào `/opt/thiennhan` và enable `thiennhan.service`; cấp quyền nhóm `audio`/`video`/`bluetooth`/`dialout`/`i2c` cho user `thiennhan`.

Nếu setup sửa boot config, camera và MAX98357A chưa được kết luận trước reboot. Chạy:

```sh
sudo reboot
```

Sau reboot, chạy lại setup một lần để thực hiện camera JPEG test, MAX98357A MP3 test và xác nhận service active:

```sh
cd /đường/dẫn/tới/thư-mục-dự-án
sudo bash setup_pi2w.sh
```

## Quản lý service

```sh
sudo systemctl status thiennhan.service
sudo journalctl -u thiennhan.service -f
sudo systemctl restart thiennhan.service
```

Ứng dụng chạy bằng user hệ thống `thiennhan`, đọc cấu hình từ `/opt/thiennhan/.env` và tự restart khi lỗi.

## Kiểm tra độc lập

```sh
python3 test_camera.py
aplay -L | grep -i max
arecord -l
bluetoothctl info 04:60:61:EF:78:22
```

Kiểm tra mic USB + mã hoá MP3 bằng đúng định dạng runtime:

```sh
# Lấy tên device ALSA của mic USB (điền vào MIC_DEVICE trước):
arecord -l
# Thu 3 giây mono 16-bit 16kHz rồi mã hoá MP3:
arecord -D "$MIC_DEVICE" -f S16_LE -r 16000 -c 1 -d 3 -t raw - \
  | lame --silent -r -s 16000 -m m --preset voice - - > test-usb.mp3
```

## Hành vi ứng dụng

Startup chạy theo thứ tự:

1. Kiểm tra Wi-Fi.
2. Chụp thử JPEG trong RAM.

Thông báo Wi-Fi, camera và hệ thống sẵn sàng phát qua MAX98357A. Bluetooth không được đụng tới ở startup.

Runtime: mic USB mở liên tục và VAD cắt từng utterance. Mỗi utterance được mã hoá MP3 rồi gửi Groq Whisper; nếu transcript có “Thiên Nhãn” (cùng đoạn hoặc lượt kế tiếp) thì phần câu lệnh được gửi sang Mac. Trước khi gửi, Pi đọc nhanh cảm biến GY25 + GY63 và đính kèm vào payload `/command` — **chỉ transcript được gửi, không gửi audio**.

Camera: Pi dùng một danh sách từ khoá thị giác ngắn (`nhìn`, `xem`, `cầm`, `trước mặt`, `cái này`, `màn hình`, …) để quyết định chụp ảnh, vì `/stt` phân loại của Mac không còn nằm trên đường đi. Khi khớp, Pi chụp JPEG và gửi kèm `mode=observe`; ngược lại gửi `mode=auto`. Đây là điểm cần thay bằng phân loại thực khi Mac hỗ trợ.

Yêu cầu kết nối tai nghe: nếu câu lệnh khớp các cụm như “kết nối tai nghe”, “bật bluetooth”, app thử kết nối giới hạn và báo qua loa MAX (không ảnh hưởng đường phát — mọi âm thanh vẫn qua MAX).

## Cảm biến (GY25 + GY63)

Pi đọc cảm biến lightweight (KHÔNG chạy AI trên Pi) và đính kèm kết quả vào mỗi request `/command` để Mac sử dụng cho phát hiện ngã / độ cao.

- **GY25** = module MPU6050 ở chế độ UART góc. Đọc qua `/dev/serial0` baud `115200`, gửi yaw/pitch/roll (khung `0xAA … 0x55`, giá trị ×0,01 độ).
  - Cấu hình: `GY25_DEVICE` (để trống = tắt), `GY25_BAUD` (mặc định 115200).
- **GY63** = cảm biến áp suất **MS5611-01BA03**. Đọc qua I2C bus 1, địa chỉ `0x76`, trả `pressure_pa`, `temperature_c`, `altitude_m`.
  - Cấu hình: `MS5611_BUS=1`, `MS5611_ADDR=0x76`, `MS5611_ENABLED=true`.
  - Driver áp dụng bù nhiệt/áp suất bậc 1 và bậc 2 theo datasheet, kèm kiểm tra PROM CRC-4.

Thiếu thiết bị/lỗi bus → payload trả `imu: null` / `pressure: null`; vòng chính và request vẫn chạy bình thường.

## WiFi dự phòng

Setup thêm mạng dự phòng (mặc định **SSID `Pdmq`**, ghi đè được bằng biến môi trường `WIFI_FALLBACK_SSID`/`WIFI_FALLBACK_PSK`) vào `/etc/wpa_supplicant/wpa_supplicant.conf` *chỉ khi* wpa_supplicant đang là trình quản lý mạng đang chạy; file được giữ quyền `0600` và không ghi đè mạng sẵn có. Việc Pi có tự chuyển sang hotspot khi mạng chính mất hay không phụ thuộc trình quản lý mạng (DietPi có thể dùng NetworkManager/systemd-networkd); script **không đảm bảo** tự chuyển mạng.

## Dữ liệu

- PCM microphone chỉ tồn tại trong RAM và bị giải phóng sau khi mã hoá MP3.
- MP3 của đoạn nói được gửi tới Groq rồi giải phóng; không lưu lại trên Pi.
- JPEG được gửi Mac rồi xếp hàng lưu vào `history/image`.
- MP3 phản hồi được phát qua MAX98357A rồi xếp hàng lưu vào `history/sound`.
- History giữ tối đa 7 ngày theo thời điểm ảnh được truyền hoặc MP3 được phát.

## Kiểm thử

```sh
/opt/homebrew/bin/python3.11 -m unittest discover -s tests
```
