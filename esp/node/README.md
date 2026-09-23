# esp_node — node cảm biến ESP32-S3 (MPU6050 + MS5611) cho NCKH27PA / FALL-01

Firmware Arduino ghi dữ liệu **thô** ra serial dưới dạng CSV để thu dataset cho FALL-01
(pha instrumentation). Đây **chỉ là thu thập dữ liệu**: không có ngưỡng ngã, không phân loại,
không lọc/làm mượt/cắt biên, không bù trôi gyro, không BLE/HTTP/flash/JSON/RTOS.

```
esp/node/
├── README.md
├── run-tests.sh                    # test trên máy host (g++, không cần board)
├── firmware/esp_node/              # sketch Arduino
│   ├── esp_node.ino                # setup/loop: lịch 100 Hz + 25 Hz, báo cáo khởi động, in CSV
│   ├── node_config.h               # chân, tốc độ bus, rate, DLPF/OSR, địa chỉ, hằng số khởi động
│   ├── i2c_bus.h                   # interface I2cBus (thuần C++17, không Arduino)
│   ├── arduino_i2c_bus.h/.cpp      # adapter TwoWire (2 controller I2C độc lập) — file duy nhất có Arduino
│   ├── mpu6050.h/.cpp              # driver MPU6050 mức thanh ghi (thuần)
│   ├── ms5611.h/.cpp               # driver MS5611 máy trạng thái KHÔNG chặn (thuần)
│   ├── node_math.h/.cpp            # magnitude + cao độ tương đối theo khí áp + baseline (thuần)
│   └── csv_row.h/.cpp              # format CSV (thuần)
└── tests/                          # test host: fake_i2c.h + 4 file test
```

Chỉ `arduino_i2c_bus.cpp` và `esp_node.ino` được phép `#include <Arduino.h>/<Wire.h>`; các module
còn lại là C++17 thuần nên chạy test được trên máy tính (`esp/node/run-tests.sh`).

## 1. Phần cứng và đấu dây (cố định, không đổi)

| Bus | Controller | SDA | SCL | Thiết bị | Địa chỉ |
|-----|-----------|-----|-----|----------|---------|
| 0   | `TwoWire busImu(0)`  | GPIO7 | GPIO6 | MPU6050 (GY-521) | `0x68`, fallback `0x69` |
| 1   | `TwoWire busBaro(1)` | GPIO3 | GPIO2 | MS5611 (GY-63)  | `0x77`, fallback `0x76` |

* Hai **controller I2C độc lập**, không dùng chung bus; tốc độ 400 kHz cả hai (`node_config.h`).
* Nguồn 3V3, GND chung. **Không cắm cảm biến vào chân USB/strapping khi đang flash.**
* Board: ESP32-S3 Super Mini. Lưu ý: core `esp32` 3.3.12 **không có board entry riêng cho Super
  Mini**, nên dùng profile `esp32:esp32:esp32s3` (gần nhất) — xem mục 3.

## 2. Build (arduino-cli)

Yêu cầu: arduino-cli + core `esp32` 3.3.12 (không dùng thư viện cảm biến bên thứ ba).

```bash
cd /Users/phananh/TEMP/nckh27pa/nckh27pa
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"

# (a) USB-CDC native — khuyến nghị cho Super Mini (USB Mode = Hardware CDC, CDC on boot)
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" \
  esp/node/firmware/esp_node

# (b) USB-UART (Serial0) — kiểm tra sketch biên dịch được cả khi CDCOnBoot=default
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 esp/node/firmware/esp_node

# tìm cổng nối tiếp
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml board list
```

## 3. Flash (macOS) — FQBN gần nhất cho Super Mini

```bash
PORT=/dev/cu.usbmodem1101        # thay bằng cổng thật in ra từ `board list`

# (a) USB-CDC native (khuyến nghị: 921600 qua USB CDC, không cần chip USB-UART)
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml upload -p "$PORT" \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" \
  esp/node/firmware/esp_node

# (b) nếu board chỉ cắm qua mạch USB-UART rời
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml upload -p "$PORT" \
  --fqbn esp32:esp32:esp32s3 esp/node/firmware/esp_node
```

Nếu auto-reset không vào được bootloader: giữ **BOOT**, bấm **RESET**, nhả **BOOT**, rồi upload;
có thể thêm `--upload-field before=default_reset` hoặc `--board-options` tương ứng.

## 4. Ghi log (sau khi flash)

```bash
# cách 1: đọc thẳng cổng serial, ghi ra file CSV (Ctrl-C để dừng)
python3 - <<'PY'
import serial, time                     # pip install pyserial
PORT = "/dev/cu.usbmodem1101"
with serial.Serial(PORT, 921600, timeout=1) as s, open("raw.csv", "wb") as f:
    t0 = time.time()
    while time.time() - t0 < 60:        # ghi 60 s
        f.write(s.read(4096))
PY

# cách 2: chỉ lấy các dòng CSV (bỏ dòng báo cáo '#') rồi lưu file .csv
python3 - <<'PY'
import serial
PORT = "/dev/cu.usbmodem1101"
with serial.Serial(PORT, 921600, timeout=1) as s, open("esp_node_data.csv", "w") as f:
    while True:
        line = s.readline().decode("ascii", "replace").rstrip("\r\n")
        if line.startswith("#"):
            print(line)                 # báo cáo khởi động: hiện ra terminal, không ghi file
        elif line:
            f.write(line + "\n")
PY
```

Baud **921600** (`node_config.h: kSerialBaud`). Firmware **không bao giờ chặn vô hạn trên Serial**:
chờ host tối đa 3000 ms rồi chạy tiếp; với build USB-CDC, `Serial.setTxTimeoutMs(0)` chỉ được gọi
bên trong `#if ARDUINO_USB_CDC_ON_BOOT` (build USB-UART dùng `Serial0`, không có phương thức đó).

## 5. Định dạng đầu ra

**Trước khi logger chạy**: các dòng thông tin bắt đầu bằng `#` (tên/phiên bản firmware, cặp chân và
địa chỉ của hai bus, byte WHO_AM_I, DLPF/SMPLRT_DIV/FS của MPU6050, C1..C6 + CRC-4 của MS5611,
áp suất baseline + số mẫu, và trạng thái init của từng cảm biến — nêu rõ cảm biến nào lỗi và vì sao),
kèm `# WARNING ...` nếu baseline không hợp lệ.

**Sau đó đúng một dòng header** (in một lần), rồi **chỉ có dòng CSV**: không văn bản, không trang trí,
không dòng trống.

```
timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,temperature_c,relative_altitude_m,flags
```

| Cột | Đơn vị | Định dạng |
|-----|--------|-----------|
| `timestamp_ms` | ms (từ `micros()/1000`, một epoch duy nhất từ lúc boot) | số nguyên |
| `ax,ay,az,a_mag` | m/s² (thang ±2 g) | 3 chữ số thập phân |
| `gx,gy,gz,g_mag` | °/s (thang ±250 dps) | 2 chữ số thập phân |
| `pressure_pa` | Pa (đã bù theo datasheet, mode 0) | 1 chữ số thập phân |
| `temperature_c` | °C | 2 chữ số thập phân |
| `relative_altitude_m` | m, **cao độ tương đối theo khí áp** (không phải cao độ thật) | 3 chữ số thập phân |
| `flags` | bitmask thập phân | số nguyên |

Bit `flags`:

| Bit | Tên | Ý nghĩa |
|-----|-----|---------|
| `0x01` | `MPU_OK` | đọc burst 14 byte thành công cho dòng này |
| `0x02` | `MPU_ERR` | đọc MPU6050 lỗi → các cột accel/gyro để **trống** |
| `0x04` | `P_FRESH` | dòng này mang mẫu MS5611 **mới** (các cột áp suất/nhiệt độ được điền) |
| `0x08` | `P_ERR` | MS5611 lỗi đọc (hoặc mẫu không hữu hạn) |
| `0x10` | `ALT_VALID` | baseline hợp lệ **và** dòng này có `relative_altitude_m` |
| `0x20` | `P_OUT_OF_RANGE` | đọc được nhưng áp suất ngoài dải 30000..110000 Pa → không ghi giá trị |
| `0x40` | `PROM_CRC_MISMATCH` | CRC-4 của PROM không khớp — **chỉ để thông tin, KHÔNG chặn dữ liệu áp suất** |

**Giá trị không hợp lệ luôn là ô TRỐNG (`,,`)**, không bao giờ là `nan`/`inf`/số 0 thay thế.
Ô `timestamp_ms` và `flags` luôn có giá trị; các ô khác có thể trống.

### Chính sách "độ tươi" của áp suất (`kRepeatLastPressure = false`)

Các cột `pressure_pa`, `temperature_c`, `relative_altitude_m` **chỉ được điền ở dòng có mẫu MS5611
mới hoàn tất** (25 Hz), các dòng khác để trống — giá trị cũ không bao giờ được lặp lại như một phép đo
mới. Vì vậy `P_FRESH` cũng chỉ bật ở dòng thực sự mang dữ liệu áp suất (dòng có mẫu mới nhưng ngoài dải
thì bật `P_OUT_OF_RANGE`, không bật `P_FRESH`). Muốn có chuỗi áp suất 25 Hz đều đặn thì forward-fill
trong pandas (`df["pressure_pa"].ffill()`) — timestamp vẫn là thời điểm lấy mẫu thật.

### Baseline áp suất (mốc cao độ, FIXED cho cả phiên)

* Thu mẫu MS5611 trong cửa sổ khởi động có giới hạn (2000 ms, tối đa 40 mẫu, cần ≥ 20 mẫu trong dải).
* Baseline = **median** của các mẫu đó, chốt lại sau khi cửa sổ đóng và **không bao giờ tự dịch**.
* Nếu baseline không hợp lệ: in `# WARNING ...` trước header, và **cả phiên** `relative_altitude_m`
  để TRỐNG với bit `ALT_VALID` luôn = 0 (không in NaN, không thay bằng 0.0, không lấy lại mốc giữa chừng).
* Công thức/biển dấu: `h = 44330.77 * (1 - (p/p0)^0.190263)`, `h > 0` = cao hơn mốc baseline.

## 6. Những lựa chọn có chủ đích (không phải thiếu sót)

* **Không lọc / không làm mượt / không cắt biên / không xoay trục**, **không bù trôi gyro** — dữ liệu
  thô là dữ liệu nghiên cứu; mọi xử lý phải làm ở phía phân tích để còn tái lập.
* **Không có ngưỡng ngã / phân loại / máy trạng thái** (pha instrumentation).
* MPU6050 DLPF = 1 (accel 184 Hz / gyro 188 Hz) là hằng số có tên `kDlpfCfg` để dễ chỉnh lại.
* Nếu một cảm biến hỏng lúc khởi động: firmware **thử lại tối đa 1 lần mỗi 5000 ms**, kết quả chỉ
  thể hiện qua cột `flags` — không in văn bản xen giữa các dòng CSV.
* Nếu MPU6050 lỗi đọc liên tục ~200 ms giữa phiên, việc dò lại được bật lại theo cùng nhịp 5 s
  (bus vẫn có thể tự hồi phục); các dòng trong lúc lỗi vẫn mang `MPU_ERR`.
* Bộ đếm "slot MPU bị bỏ" (do vòng lặp trễ) được theo dõi trong firmware nhưng **không in ra giữa
  phiên**: hợp đồng "sau header chỉ có CSV" được ưu tiên.
* Nếu một dòng CSV không đủ chỗ trong buffer 200 byte thì dòng đó **không được in** (thà mất dòng còn
  hơn in dòng bị cắt méo).

## 7. Test trên máy host

```bash
bash esp/node/run-tests.sh              # bắt buộc PASS
bash esp/node/run-tests.sh --sanitize   # thêm -fsanitize=address,undefined
```

Test dùng `tests/fake_i2c.h` (thanh ghi kịch bản) nên chạy được không cần phần cứng; chúng kiểm tra thứ
tự thanh ghi khi init, giải mã burst 14 byte, vector datasheet MS5611 (≈20.08 °C / ≈100.0 kPa), CRC-4,
máy trạng thái không chặn, biên dải áp suất, median baseline, và schema CSV.

## 8. Chưa được kiểm chứng (cần phần cứng thật)

Không có board trong phiên làm việc này, nên **chưa** xác minh: ACK thật của MPU6050/MS5611, byte
`WHO_AM_I` thật, tốc độ lấy mẫu thực tế (100 Hz / 25 Hz), thông lượng 921600 baud, nhiễu ADC, cũng như
độ ổn định nhiệt của baseline. Firmware biên dịch sạch cho cả hai FQBN; test host PASS.
