# ESP32 Fall Detection Test Rig (`esp32-test`)

> **CẢNH BÁO QUAN TRỌNG:**
> **ĐÂY LÀ BỘ TEST THỬ NGHIỆM (TEST_RIG) — ĐÂY KHÔNG PHẢI PHẦN CỨNG PRODUCTION!**
> Cấu hình phần cứng trong thư mục này dùng để kiểm tra sớm thuật toán phát hiện té ngã, thu thập dữ liệu serial và tinh chỉnh ngưỡng; không đại diện cho thiết bị sản phẩm đích cuối cùng.

---

## 1. Cấu hình phần cứng Test Rig

| Thành phần | Linh kiện sử dụng | Ghi chú kỹ thuật |
|---|---|---|
| **Vi điều khiển** | **ESP32-WROOM-32** (NodeMCU-32S / ESP32 Dev Module) | Khác với sản phẩm đích (sản phẩm đích dùng **ESP32-S3 Super Mini**) |
| **Cảm biến IMU** | **MPU9250** (GY-9250) | Tạm thời cho bộ test. Sản phẩm đích dùng **MPU6050** |
| **Cảm biến khí áp** | **GY-63 / MS5611-01BA03** | Khí áp kế thực tế đang dùng trên cả bộ test và node sản phẩm |
| **Từ kế AK8963** | **KHÔNG SỬ DỤNG** | Khối từ kế trên MPU9250 bị vô hiệu hóa vì nhiễu từ trường lớn trong nhà và không cần thiết cho phát hiện ngã |
| **Kết nối** | Cáp Micro-USB nối máy tính | Cấp nguồn và truyền dữ liệu Serial Monitor (không bật BLE/Wi-Fi) |

---

## 2. Sơ đồ nối dây I2C (Wiring)

Cả hai cảm biến MPU9250 và GY-63 đều hoạt động ở điện áp **3.3V** và dùng chung một bus I2C duy nhất của ESP32-WROOM-32.

| Chân ESP32-WROOM-32 | Chân MPU9250 | Chân GY-63 (MS5611) | Chức năng | Ghi chú an toàn |
|:---:|:---:|:---:|---|---|
| **3V3** | VCC | VCC | Nguồn dương 3.3V | Tuyệt đối không cấp 5V vào chân tín hiệu I2C |
| **GND** | GND | GND | Nối đất chung | Nối chung đất toàn hệ thống |
| **GPIO 21** | SDA | SDA | I2C Data | Bus I2C mặc định của WROOM-32 |
| **GPIO 22** | SCL | SCL | I2C Clock | Tần số 400 kHz |
| *GND* | AD0 | - | Địa chỉ MPU9250 | AD0 nối GND $\rightarrow$ Địa chỉ `0x68` (nếu nối 3V3 là `0x69`) |
| *GND* | - | CSB | Địa chỉ MS5611 | CSB nối GND $\rightarrow$ Địa chỉ `0x77` (nếu nối 3V3 là `0x76`) |

> **CẢNH BÁO AN TOÀN `// VERIFY_TEST_WIRING`:**
> 1. **Chưa xác minh phần cứng thật:** Sơ đồ chân `SDA = GPIO 21`, `SCL = GPIO 22` là cấu hình mặc định chuẩn cho bo ESP32-WROOM-32. Nếu bo mạch thực tế của bạn hàn chân khác, bắt buộc phải sửa hai hằng số `PIN_TEST_I2C_SDA` và `PIN_TEST_I2C_SCL` ở đầu file `esp32-test.ino`.
> 2. **Chân cấm trên WROOM-32:** Tuyệt đối không dùng các chân **GPIO 6 đến 11** (nối trực tiếp vào chip SPI Flash nội bộ $\rightarrow$ chip sẽ bị panic/crash và reboot liên tục). Tránh dùng các chân Strapping bootloader (**GPIO 0, 2, 12, 15**).
> 3. **Không copy chân từ ESP32-S3:** Chân I2C của ESP32-S3 Super Mini (`GPIO 6, 7, 2, 3`) tuyệt đối không áp dụng cho ESP32-WROOM-32.

---

## 3. Cài đặt môi trường & Thư viện

- **Thư viện bên thứ ba:** **KHÔNG CẦN CÀI BẤT KỲ THƯ VIỆN NGOÀI NÀO!** Firmware được lập trình mức thanh ghi trực tiếp (register-level) thông qua thư viện `Wire.h` tích hợp sẵn trong ESP32 Arduino Core.
- **Board chọn trong Arduino IDE:**
  - Board: **`ESP32 Dev Module`**
  - FQBN: **`esp32:esp32:esp32`**
  - Upload Speed: `921600` hoặc `115200`
  - Flash Frequency: `80MHz`
  - Core Debug Level: `None`
- **Tốc độ Baud Serial:** **`115200`**

---

## 4. Biên dịch bằng `arduino-cli`

Sử dụng công cụ `arduino-cli` tích hợp sẵn trên máy host macOS:

```bash
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"

# Biên dịch kiểm tra cú pháp (0 lỗi):
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32 --build-path /tmp/t18-build esp32-test

# Nạp vào board (thay /dev/cu.usbserial-xxx bằng cổng nối tiếp thực tế):
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml upload -p /dev/cu.usbserial-0001 \
  --fqbn esp32:esp32:esp32 --build-path /tmp/t18-build esp32-test
```

---

## 5. Chế độ Serial & Lệnh điều khiển

Mở Serial Monitor ở tốc độ baud **115200**.

### 5.1. Chế độ HUMAN MODE (Mặc định)
Tần số in 1 lần/giây (mỗi 1000 ms, tương đương ~4 dòng/giây thỏa yêu cầu 2–5 dòng/giây) kèm các dòng sự kiện tức thời:

```text
[OK] MPU9250 | Acc=1.02g | Gyro=12°/s | Still=YES
[OK] GY63   | P=1008.42 hPa | ΔH=+0.08 m
[STATE] NORMAL
[RISK] Fall score: 12%
```

Khi có biến động chuyển động, firmware in dòng sự kiện chuyên biệt:
- `[EVENT] FREE FALL detected (Acc=0.32g | 3.14 m/s^2, dur=85 ms)`
- `[EVENT] IMPACT 27.50 m/s^2 (2.80g)`
- `[EVENT] IMPACT 28.10 m/s^2 (2.86g) [re-latched]`
- `[EVENT] POST-IMPACT low motion started`
- `[ALERT] FALL CONFIRMED (impact=27.50 m/s^2 | 2.80g, stillness=1050 ms, deltaP=+14.2 Pa, deltaH=-0.72 m, gyro=YES, dH=YES)`
- `[EVENT] POST-IMPACT window expired (3000 ms), false alarm`

### 5.2. Chế độ CSV RAW MODE (100 Hz)
Xuất luồng dữ liệu 14 cột ở đúng tốc độ 100 Hz phục vụ vẽ đồ thị hoặc lưu file CSV:
```text
timestamp_ms,ax_ms2,ay_ms2,az_ms2,acc_mag,gx_dps,gy_dps,gz_dps,gyro_mag,p_pa,temp_c,alt_m,state,event
```
*Lưu ý:* Cột `event` mang các tag sự kiện viết hoa: `FREE_FALL`, `IMPACT`, `POST_IMPACT_START`, `FALL_CONFIRMED`, `WINDOW_EXPIRED`, `SAMPLE_GAP_RESET`, `ALERT_HOLD_END` (chỉ dòng đầu tiên của sự kiện mang tag, các dòng còn lại là `NONE`). Khi ở chế độ CSV Raw, firmware không in bất kỳ dòng chữ văn bản nào xen lẫn để tránh làm lỗi parser của các công cụ phân tích (Python/Pandas).

### 5.3. Các phím lệnh tương tác Serial
Gõ trực tiếp vào ô gửi lệnh của Serial Monitor (không cần Enter):
- **`r`** : Bật / tắt chuyển đổi giữa HUMAN MODE và CSV RAW MODE.
- **`t`** : In bảng giá trị cấu hình ngưỡng `FallProfile` hiện tại.
- **`c`** : Kích hoạt lại chu trình hiệu chuẩn cảm biến lúc đứng yên (3 giây).
- **`h`** : In menu trợ giúp các lệnh Serial.

---

## 6. Quy trình Hiệu chuẩn lúc Khởi động (Calibration)

Khi cấp nguồn hoặc bấm Reset:
1. Firmware kiểm tra giao tiếp với MPU9250 (WHO_AM_I = `0x71`). Nếu không tìm thấy, hệ thống dừng lại báo lỗi `[FATAL]`.
2. Firmware kiểm tra MS5611 và nạp hệ số bù nhiệt độ PROM.
3. In thông báo: `[CAL] BẮT ĐẦU HIỆU CHUẨN: Giữ thiết bị đứng yên trong 3 giây...`.
4. **Hành động của người thử:** Đặt bo mạch đứng yên trên mặt bàn phẳng hoặc giữ nguyên tư thế đứng trong 3 giây.
5. Firmware tự động tính toán độ lệch con quay (`gyroBiasX, Y, Z`) và lấy trung vị (median) áp suất làm mốc chuẩn $P_0$.
6. In `[READY] Monitoring started. State transitioned to NORMAL.` và bắt đầu đo đạc.

---

## 7. Quy trình Kiểm thử Nhanh (Quick Test)

1. **Kiểm tra tĩnh:** Để yên trên bàn $\rightarrow$ Xem Serial thấy `[STATE] NORMAL`, `Acc ≈ 1.00g`, `Gyro ≈ 0°/s`, `Risk score: 5%`.
2. **Kiểm tra va đập giả:** Lắc nhẹ hoặc đập bàn $\rightarrow$ Thấy `Acc` tăng nhưng không đạt $25 m/s^2$, trạng thái vẫn ở `NORMAL`.
3. **Mô phỏng ngã an toàn:** Cầm thiết bị rơi nhanh xuống gối mềm $\rightarrow$ Sau cú chạm, giữ yên bất động thiết bị trên gối 1.5 giây $\rightarrow$ Quan sát dòng `[ALERT] FALL CONFIRMED` xuất hiện.

---

## 8. Cách tùy chỉnh Ngưỡng (Threshold Tuning)

Toàn bộ các ngưỡng và cửa sổ thời gian được quy định tập trung trong struct `FallProfile` tại đầu file `esp32-test/esp32-test.ino` (khoảng dòng 90–106):

```cpp
struct FallProfile {
  float impactAccelerationMs2 = 25.0f;           // Tăng/giảm ngưỡng nhạy va đập (Android = 25.0 m/s^2)
  float stillnessTargetAccelerationMs2 = 9.81f;  // Mốc trọng trường khi nằm yên
  float stillnessToleranceMs2 = 1.0f;            // Dung sai quanh mốc tĩnh (±1.0 m/s^2)
  uint32_t postImpactWindowMs = 3000;            // Cửa sổ tối đa sau va chạm (3000 ms)
  uint32_t postImpactStillnessDurationMs = 1000; // Thời lượng tĩnh yêu cầu liên tục (1000 ms)
  uint16_t minimumStillnessSamples = 6;          // Số mẫu tĩnh tối thiểu (Android = 6)
  uint32_t maximumSampleGapMs = 250;             // Giới hạn đứt đoạn mẫu để reset
  float freeFallThresholdMs2 = 4.90f;            // Ngưỡng phát hiện rơi tự do (0.5g)
  uint32_t freeFallMinDurationMs = 80;           // Thời gian rơi tự do tối thiểu (80 ms)
  float gyroTurnThresholdDps = 120.0f;           // Ngưỡng vận tốc góc xoay thân ngã
  float pressureEvidenceMinRisePa = 12.0f;       // Mức tăng áp suất đối chứng (12 Pa)
  float altitudeDropMinM = -0.40f;               // Độ cao hạ thấp đối chứng (-0.40 m)
};
```

Sau khi điều chỉnh tham số, biên dịch lại và nạp vào ESP32. Sử dụng tài liệu `TEST_PLAN.md` để ghi nhận kết quả 13 ca kiểm thử thực nghiệm.
