# Hướng dẫn học sinh — Sketch ESP32 phát hiện té ngã

Tài liệu này dành cho học sinh lớp 9 mới học lập trình và mới làm quen với Arduino.
Đọc theo thứ tự từ trên xuống, mỗi phần giải thích một khái niệm bằng ví dụ đời sống.

---

## 1. Phần cứng

Bộ thử nghiệm (test rig) hiện tại gồm ba thành phần chính:

| Linh kiện | Chức năng | Ghi chú |
|-----------|-----------|---------|
| ESP32-WROOM-32 | Vi điều khiển (bộ não) | Bo "ESP32 Dev Module", KHÔNG phải ESP32-S3 |
| MPU9250 (GY-9250) | Đo gia tốc + vận tốc góc (IMU) | Giao tiếp I2C, địa chỉ 0x68 hoặc 0x69 |
| GY-63 (MS5611-01BA03) | Đo áp suất khí quyển | Giao tiếp I2C, địa chỉ 0x77 hoặc 0x76 |

Dây nối I2C mặc định: **SDA = GPIO 21**, **SCL = GPIO 22**, tốc độ **400 kHz**.

> ⚠️ **BẮT BUỘC PHẢI NHỚ:**
> **MPU9250 chỉ được dùng trên bộ test hiện tại. Sản phẩm cuối cùng sử dụng MPU6050.**

> ⚠️ Đừng dùng chân GPIO 6..11 của ESP32-WROOM-32 để nối I2C, vì các chân đó nối
> với chip Flash bên trong, sẽ gây treo bo hoặc khởi động lại liên tục.

---

## 2. Mục tiêu dự án

- Xây dựng một thiết bị đeo phát hiện khi người dùng bị **té ngã** rồi phát cảnh báo.
- Ở giai đoạn này, chúng ta chỉ thử nghiệm **thuật toán** trên bộ test, chưa phải sản phẩm cuối.
- Học cách **đọc dữ liệu cảm biến**, **kết hợp nhiều cảm biến** để nhận định chính xác,
  và hiểu cách một chương trình nhúng "suy nghĩ" qua **máy trạng thái**.

---

## 3. Vai trò từng cảm biến

| Cảm biến | Đo gì | Giúp phát hiện ngã bằng cách nào |
|----------|-------|----------------------------------|
| Gia tốc kế (trong MPU9250) | Gia tốc theo 3 trục | Nhận ra cú **va đập mạnh** khi chạm đất và lúc **rơi tự do** |
| Con quay hồi chuyển (trong MPU9250) | Vận tốc góc theo 3 trục | Nhận ra người **xoay người, lật ngửa** khi ngã |
| Khí áp kế GY-63 | Áp suất khí quyển | Nhận ra **đổi độ cao** (ngã từ đứng cao xuống thấp) |

---

## 4. Gia tốc là gì?

**Gia tốc** cho biết vật đang chuyển động "mạnh lên hay chậm đi" nhanh thế nào.

Cảm biến gia tốc trong điện thoại (và MPU9250) đo luôn cả **trọng lực Trái Đất**, nên:

- Đặt điện thoại **yên trên bàn** → cảm biến vẫn đọc khoảng **1g** (≈ 9,81 m/s²),
  vì trọng lực đang kéo nó xuống.
- **Thả rơi** điện thoại → số đo tổng **tụt mạnh xuống gần 0** (vì nó "rơi cùng" trọng lực).
- Điện thoại **đập xuống đất** → số đo **vọt lên rất cao** (hơn 2,5g).
- **Đi lại bình thường** → số đo **dao động nhẹ** quanh 1g.

Trong code, `tongGiaToc = sqrt(ax² + ay² + az²)`. Đây là **độ dài vectơ gia tốc**:
nó cho ta biết gia tốc **lớn đến mức nào** mà không cần quan tâm thiết bị đang nằm
nghiêng hay úp ngược.

---

## 5. Vận tốc góc là gì?

**Vận tốc góc** cho biết vật đang **xoay nhanh thế nào**, đơn vị là **độ/giây (dps)**.

Ví dụ đời sống:

- Điện thoại **đứng yên** → vận tốc góc ≈ **0 dps**.
- **Xoay điện thoại** nửa vòng trong 1 giây → vài trăm dps.
- **Nghiêng người, ngã sang bên, lật thiết bị** → vận tốc góc tăng lên rõ rệt.

Khi một người bị ngã, thân người thường **xoay** (đổ về trước/sau/bên), nên con quay
hồi chuyển giúp ta có thêm một bằng chứng "có chuyện gì đó đang xảy ra".

---

## 6. Áp suất là gì?

**Áp suất khí quyển** là "sức nặng" của lớp không khí phía trên đè xuống mặt đất.

- Càng **lên cao** (leo núi, đi thang máy lên) → không khí loãng hơn → áp suất **giảm**.
- Càng **xuống thấp** → áp suất **tăng**.

Cảm biến GY-63 đo áp suất và từ đó ước lượng **độ cao tương đối** so với lúc khởi động.
Lưu ý quan trọng: **áp suất một mình không đủ để xác nhận té ngã**, vì thời tiết và gió
cũng làm áp suất thay đổi. Vì vậy nó chỉ đóng vai trò "bằng chứng phụ".

---

## 7. Vì sao cần kết hợp nhiều cảm biến?

Mỗi cảm biến đều có **điểm mù** — tình huống dễ báo nhầm:

- **Gia tốc kế**: nhảy mạnh, vỗ tay cũng tạo va đập, dễ tưởng là ngã.
- **Con quay hồi chuyển**: chỉ biết "đang xoay", không biết người có nằm xuống thật không.
- **Khí áp kế**: thời tiết, gió cũng làm áp suất đổi, không dùng một mình được.

Kết hợp nhiều cảm biến giúp **giảm báo sai**: một cú ngã thật thường có đủ nhiều dấu hiệu
(va đập mạnh + xoay người + đổi độ cao). Càng nhiều bằng chứng trùng khớp thì càng chắc chắn.

---

## 8. Luồng chương trình

Chương trình Arduino gồm hai hàm chính:

- **`setup()`** — chạy đúng **một lần** khi cấp nguồn: mở Serial, khởi tạo I2C, khởi tạo
  MPU9250 và GY-63, in bảng ngưỡng, rồi hiệu chuẩn.
- **`loop()`** — chạy **liên tục** sau đó. Mỗi vòng lặp làm 4 việc theo thứ tự:

```
(1) ĐỌC CẢM BIẾN → (2) XỬ LÝ PHÁT HIỆN TÉ NGÃ → (3) IN DỮ LIỆU → (4) LẶP LẠI
```

- Đọc cảm biến gia tốc/vận tốc góc mỗi **10 ms** (100 Hz).
- Đọc cảm biến khí áp mỗi **40 ms** (25 Hz), theo kiểu không chặn (không làm kẹt vòng lặp).
- In dữ liệu cho con người đọc mỗi **1 giây**.

---

## 9. State machine (máy trạng thái)

Máy trạng thái là cách chương trình "nhớ" mình đang ở bước nào. Mỗi lúc nó chỉ ở
**đúng một trạng thái**.

### Sơ đồ mũi tên

```
TRANG_THAI_DANG_HIEU_CHUAN
        │ (hiệu chuẩn xong)
        ▼
TRANG_THAI_BINH_THUONG ────── gia tốc < 4,90 m/s² trong ≥ 80 ms ──────► TRANG_THAI_NGHI_ROI
        │                                                                      │
        │ gia tốc ≥ 25,0 m/s²                                                 │ gia tốc ≥ 25,0 m/s²
        ▼                                                                      ▼
TRANG_THAI_PHAT_HIEN_VA_CHAM ◄──────────────────────────────────────────────────┘
        │ (chuyển ngay lập tức)
        ▼
TRANG_THAI_THEO_DOI_SAU_VA_CHAM ── nằm yên ≥ 6 mẫu và ≥ 1000 ms ──► TRANG_THAI_XAC_NHAN_TE_NGA
        │ (quá 3000 ms chưa nằm yên)                                              │ (sau 3000 ms)
        ▼                                                                          ▼
TRANG_THAI_BINH_THUONG ◄────────────────────────────────────────────────────────────┘
```

### Bảng trạng thái

| Trạng thái | Điều kiện vào | Điều kiện ra (sang trạng thái khác) |
|------------|---------------|--------------------------------------|
| `TRANG_THAI_DANG_HIEU_CHUAN` | Khởi động / nhấn phím `c` | Hiệu chuẩn xong → `TRANG_THAI_BINH_THUONG` |
| `TRANG_THAI_BINH_THUONG` | Hiệu chuẩn xong | Gia tốc < 4,90 m/s² trong ≥ 80 ms → `TRANG_THAI_NGHI_ROI`; gia tốc ≥ 25 m/s² → `TRANG_THAI_PHAT_HIEN_VA_CHAM` |
| `TRANG_THAI_NGHI_ROI` | Gia tốc tụt thấp (nghi rơi) | Gia tốc ≥ 25 m/s² → `TRANG_THAI_PHAT_HIEN_VA_CHAM`; quá 600 ms không va chạm → `TRANG_THAI_BINH_THUONG` |
| `TRANG_THAI_PHAT_HIEN_VA_CHAM` | Đo được cú va đập mạnh | Chuyển ngay → `TRANG_THAI_THEO_DOI_SAU_VA_CHAM` |
| `TRANG_THAI_THEO_DOI_SAU_VA_CHAM` | Sau khi phát hiện va chạm | Nằm yên ≥ 6 mẫu và ≥ 1000 ms → `TRANG_THAI_XAC_NHAN_TE_NGA`; quá 3000 ms chưa yên → `TRANG_THAI_BINH_THUONG` |
| `TRANG_THAI_XAC_NHAN_TE_NGA` | Đủ bằng chứng → đã té ngã | Sau 3000 ms → `TRANG_THAI_BINH_THUONG` |

---

## 10. Các biến quan trọng

| Tên cũ | Tên mới | Ý nghĩa | Đơn vị |
|--------|---------|---------|--------|
| `currentState` | `trangThaiHienTai` | Trạng thái hiện tại của máy trạng thái | (enum) |
| `rawCsvMode` | `cheDoCsvRaw` | Đang ở chế độ in CSV thô hay không | true/false |
| `accelMagnitude` | `tongGiaToc` | Độ lớn vectơ gia tốc | m/s² |
| `gyroMagnitude` | `tongVanTocGoc` | Độ lớn vectơ vận tốc góc | dps |
| `pressurePa` | `apSuatPa` | Áp suất khí quyển | Pa |
| `altitudeDeltaM` | `chenhLechDoCaoM` | Chênh lệch độ cao so với lúc khởi động | m |
| `stationary` | `dangDungYen` | Thiết bị đang đứng yên | true/false |
| `lastSampleTimeMs` | `thoiDiemMauTruocMs` | Thời điểm đọc mẫu trước đó | ms |
| `freeFallStartTimeMs` | `thoiDiemBatDauRoiMs` | Thời điểm bắt đầu rơi tự do | ms |
| `impactTimeMs` | `thoiDiemVaChamMs` | Thời điểm va chạm | ms |
| `impactPeakMs2` | `dinhVaChamMs2` | Đỉnh gia tốc va chạm lớn nhất | m/s² |
| `quietStartTimeMs` | `thoiDiemBatDauYenMs` | Thời điểm bắt đầu nằm yên | ms |
| `stillnessSampleCount` | `soMauDungYen` | Số mẫu đứng yên liên tiếp | (số mẫu) |
| `confirmedAlertTimeMs` | `thoiDiemXacNhanBaoDongMs` | Thời điểm xác nhận báo động | ms |
| `gyroTurnEvidence` | `coBangChungXoayNgua` | Có bằng chứng xoay người | true/false |
| `altitudeDropEvidence` | `coBangChungGiamDoCao` | Có bằng chứng giảm độ cao | true/false |
| `pendingEventTag` | `nhanSuKienCho` | Nhãn sự kiện đang chờ in | (chuỗi) |
| `mpuAvailable` | `mpuSanSang` | MPU9250 đã sẵn sàng | true/false |
| `baroAvailable` | `khiApSanSang` | Khí áp kế đã sẵn sàng | true/false |
| `accelSaturated` | `giaTocBiBaoHoa` | Gia tốc bị bão hòa (chạm giới hạn đo) | true/false |
| `gyroSaturated` | `vanTocGocBiBaoHoa` | Vận tốc góc bị bão hòa | true/false |
| `gyroBiasX/Y/Z` | `saiSoVanTocGocX/Y/Z` | Sai số lệch của con quay hồi chuyển | dps |
| `baselinePressurePa` | `apSuatGocPa` | Áp suất gốc lúc hiệu chuẩn | Pa |
| `baselineValid` | `apSuatGocHopLe` | Áp suất gốc có hợp lệ | true/false |
| `msC` | `heSoHieuChuanMs` | Hệ số hiệu chuẩn đọc từ chip MS5611 | (mảng 8 số) |
| `profile` | `cauHinhNgua` | Cấu hình ngưỡng nhận diện ngã | (struct) |
| `currentSample` | `mauCamBien` | Mẫu cảm biến đo được hiện tại | (struct) |
| `imuSampleCount` | `soMauImu` | Số mẫu IMU đã đọc | (số mẫu) |
| `baroTimerUs` | `henGioKhiApUs` | Hẹn giờ cho chu kỳ đọc khí áp | µs |
| `lastBaroCycleUs` | `chuKyKhiApTruocUs` | Thời điểm chu kỳ khí áp trước | µs |
| `rawD1` | `giaTriThoD1` | Giá trị thô D1 (áp suất) | (số thô) |
| `rawD2` | `giaTriThoD2` | Giá trị thô D2 (nhiệt độ) | (số thô) |

---

## 11. Các hàm quan trọng

| Tên hàm (mới) | Tên cũ | Nhiệm vụ |
|---------------|--------|----------|
| `hieuChuanLucKhoiDong` | `performBootCalibration` | Hiệu chuẩn cảm biến lúc khởi động |
| `ghiI2cMotByte` | `i2cWriteByte` | Ghi 1 byte dữ liệu xuống bus I2C |
| `docI2cNhieuByte` | `i2cReadBytes` | Đọc nhiều byte từ bus I2C |
| `khoiTaoMpu9250` | `initMPU9250` | Khởi tạo cảm biến MPU9250 |
| `docMpu9250` | `readMPU9250` | Đọc gia tốc + vận tốc góc từ MPU9250 |
| `tinhCrc4Ms5611` | `computeMs5611Crc4` | Tính mã kiểm lỗi CRC-4 của MS5611 |
| `khoiTaoMs5611` | `initMS5611` | Khởi tạo cảm biến khí áp MS5611 |
| `docMs5611KhongChan` | `pollMS5611` | Đọc khí áp kiểu không chặn |
| `tinhDoTangApSuat` | `calculatePressureRise` | Tính độ tăng áp suất trong một cửa sổ thời gian |
| `tinhDiemNguyCoTeNga` | `calculateFallRiskScore` | Tính điểm nguy cơ té ngã (0–100%) |
| `xuLyPhatHienTeNga` | `processFallDetection` | Thuật toán chính phát hiện té ngã |
| `inTieuDeCsv` | `printCsvHeader` | In dòng tiêu đề CSV |
| `inDongCsv` | `outputCsvRow` | In một dòng dữ liệu CSV |
| `inThongTinDeDoc` | `outputHumanLog` | In dữ liệu dạng con người dễ đọc |
| `inBangNguong` | `printProfileTable` | In bảng các ngưỡng cấu hình |
| `xuLyLenhSerial` | `handleSerialCommands` | Xử lý phím người dùng gõ từ Serial |
| `tenTrangThai` | `stateToString` | Đổi trạng thái (enum) thành chuỗi chữ |

---

## 12. Threshold (các ngưỡng)

Tất cả ngưỡng dưới đây đều là **giá trị thử nghiệm**, chưa phải giá trị cuối cùng,
cần **đo thực tế** để chỉnh lại cho phù hợp.

| Tên | Giá trị | Đơn vị | Ý nghĩa | Ghi chú |
|-----|---------|--------|---------|---------|
| `impactAccelerationMs2` | 25,0 | m/s² (~2,55g) | Ngưỡng gia tốc va đập | Thử nghiệm |
| `stillnessTargetAccelerationMs2` | 9,81 | m/s² | Gia tốc khi nằm yên (1g) | Hằng số vật lý, không đổi |
| `stillnessToleranceMs2` | 1,0 | m/s² | Dung sai quanh 9,81 | Thử nghiệm |
| `postImpactWindowMs` | 3000 | ms | Thời gian tối đa chờ sau va đập | Thử nghiệm |
| `postImpactStillnessDurationMs` | 1000 | ms | Thời gian nằm yên tối thiểu | Thử nghiệm |
| `minimumStillnessSamples` | 6 | (số mẫu) | Số mẫu đứng yên tối thiểu | Thử nghiệm |
| `maximumSampleGapMs` | 250 | ms | Hủy chuỗi nếu mất mẫu quá lâu | Thử nghiệm |
| `freeFallThresholdMs2` | 4,90 | m/s² (<0,5g) | Ngưỡng nhận biết rơi tự do | Thử nghiệm |
| `freeFallMinDurationMs` | 80 | ms | Thời gian rơi tự do tối thiểu | Thử nghiệm |
| `gyroTurnThresholdDps` | 120,0 | dps | Ngưỡng vận tốc góc xoay thân | Thử nghiệm |
| `pressureEvidenceMinRisePa` | 12,0 | Pa | Ngưỡng tăng áp suất đối chứng | Thử nghiệm |
| `pressureWindowMs` | 5000 | ms | Cửa sổ tính áp suất tăng | Thử nghiệm |
| `altitudeDropMinM` | -0,40 | m | Ngưỡng giảm độ cao đối chứng | Thử nghiệm |
| `sampleWatchdogMs` | 100 | ms | Cảnh báo nếu trễ đọc cảm biến | Thử nghiệm |

---

## 13. Calibration (hiệu chuẩn lúc khởi động)

**Hiệu chuẩn** là lúc thiết bị tự "học" giá trị nền khi chưa có chuyển động:

- Nó đo liên tục trong **3 giây** rồi tính **sai số lệch của con quay hồi chuyển**
  (lúc đứng yên, con quay hồi chuyển lý tưởng phải đọc 0, nhưng thực tế luôn lệch một chút).
- Nó cũng đo **áp suất gốc** để sau này tính "chênh lệch độ cao" so với điểm xuất phát.

**Vì sao phải giữ yên máy khi cấp nguồn?** Vì nếu thiết bị rung lắc trong lúc hiệu chuẩn,
nó sẽ ghi nhận "sai số" sai, dẫn đến đọc dữ liệu không chính xác suốt phiên chạy.
Vì vậy khi cấp nguồn, hãy **đặt thiết bị đứng yên trên mặt phẳng trong 3 giây**.

---

## 14. Arduino IDE (cài đặt, thêm board ESP32)

1. Tải và cài **Arduino IDE** từ trang chính thức `https://www.arduino.cc/en/software`.
2. Mở Arduino IDE → **File → Preferences**.
3. Trong ô "Additional boards manager URLs", dán link:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   (nếu đã có link khác thì thêm dấu phẩy rồi dán tiếp).
4. Mở **Tools → Board → Boards Manager…**, tìm `esp32`, bấm **Install** cho gói
   "esp32 by Espressif Systems".
5. Chờ cài xong là có thể chọn board ESP32.

---

## 15. Chọn board (ESP32 Dev Module)

Trong Arduino IDE, mở menu **Tools** và chọn:

- **Board:** `ESP32 Dev Module` (tên chính xác của bo ESP32-WROOM-32 trong Arduino IDE).
- **Port:** cổng COM/USB mà máy tính nhận bo (xem mục 20 nếu không thấy cổng).

---

## 16. Compile (biên dịch)

Có thể bấm nút **Verify** (dấu ✓) trong Arduino IDE, hoặc dùng dòng lệnh `arduino-cli`
như dưới đây (đường dẫn áp dụng cho máy Mac cài Arduino IDE):

```bash
cd /Users/phananh/TEMP/nckh27pa/nckh27pa
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32 --build-path /tmp/hs-test-build esp32-test-hocsinh
```

Nếu biên dịch thành công, bạn sẽ thấy dòng dạng:

```
Sketch uses 310864 bytes (23%) of program storage space. Maximum is 1310720 bytes.
```

---

## 17. Upload (nạp lên bo)

1. Nối bo ESP32 với máy tính bằng cáp USB.
2. Trong Arduino IDE chọn đúng **Board** (ESP32 Dev Module) và **Port**.
3. Bấm nút **Upload** (mũi tên →).
4. Nếu bo không tự vào chế độ nạp, hãy **giữ nút BOOT** trên bo khi thấy dòng
   "Connecting..." rồi thả ra.

---

## 18. Serial Monitor (baud rate)

- Mở **Tools → Serial Monitor** (hoặc bấm phím tắt).
- **Baud rate phải chọn 115200** (đúng với `Serial.begin(115200)` trong code).

Nếu chọn sai baud rate, dữ liệu in ra sẽ thành các ký tự lạ (`!@#`...).

---

## 19. Cách đọc kết quả

Khi chương trình chạy ở chế độ con người đọc, mỗi giây nó in một khối như sau:

```
=== DU LIEU CAM BIEN ===
Gia toc tong : 1.03 g
Van toc goc  : 12.4 do/s
Ap suat      : 1008.32 hPa
Chenh cao    : +0.04 m
Trang thai   : TRANG_THAI_BINH_THUONG
```

- **Gia toc tong** ≈ 1g khi đứng yên; vọt lên > 2,5g khi va đập; tụt gần 0 khi rơi.
- **Van toc goc** ≈ 0 khi đứng yên; tăng lên khi xoay người.
- **Ap suat** ≈ 1000 hPa ở mặt đất; thay đổi khi lên/xuống cao.
- **Chenh cao** là độ cao so với lúc khởi động.

Khi có sự kiện té ngã, nó in các dòng cảnh báo:

```
>>> PHAT HIEN DAU HIEU ROI TU DO
>>> PHAT HIEN VA CHAM
>>> DANG THEO DOI SAU VA CHAM
>>> CANH BAO: CO THE DA XAY RA TE NGA
```

Bạn cũng có thể gõ các phím sau trên Serial Monitor:

- `r` — bật/tắt chế độ CSV thô (dành cho máy tính xử lý dữ liệu).
- `t` — in bảng ngưỡng cấu hình.
- `c` — hiệu chuẩn lại.
- `h` — in hướng dẫn lệnh.

---

## 20. Lỗi thường gặp

1. **Không thấy cổng COM** — Cáp USB có thể chỉ để sạc, không truyền dữ liệu; thử cáp khác
   hoặc cài driver (CP210x/CH340) cho bo.
2. **I2C không nhận thiết bị** — Dây SDA/SCL nối sai chân hoặc lỏng; kiểm tra lại GPIO 21/22.
3. **Sai chân** — Nối nhầm vào GPIO 6..11 sẽ làm bo treo; chỉ dùng chân I2C an toàn.
4. **Chọn sai board** — Phải chọn "ESP32 Dev Module", không phải ESP32-S3.
5. **Nhiễu dữ liệu** — Dây nối dài hoặc nguồn yếu gây số đo nhảy loạn; dùng dây ngắn, nguồn ổn định.
6. **Baud rate sai** — Phải chọn 115200, nếu không sẽ hiện ký tự lạ.
7. **Bo bị treo / khởi động lại liên tục** — Nguồn không đủ dòng (dùng cổng USB ổn định)
   hoặc nối I2C vào chân Flash (GPIO 6..11).
8. **Cảnh báo sai (báo té ngã khi không ngã)** — Ngưỡng còn là giá trị thử nghiệm;
   cần đo thực tế để chỉnh lại, hoặc hiệu chuẩn lại cho đúng.

---

## 21. Câu hỏi tự kiểm tra

1. Gia tốc kế đo gì? Đơn vị là gì?
2. Khi điện thoại đặt yên trên bàn, tổng gia tốc đọc được khoảng bao nhiêu? Vì sao?
3. Khi thả rơi điện thoại, tổng gia tốc thay đổi thế nào?
4. Vận tốc góc có đơn vị gì? Khi đứng yên thì nó khoảng bao nhiêu?
5. Lên cao thì áp suất tăng hay giảm?
6. Vì sao áp suất một mình không đủ để xác nhận té ngã?
7. `struct` là gì? Nêu một ví dụ `struct` trong chương trình này.
8. `enum` là gì? Máy trạng thái có đặc điểm gì quan trọng?
9. Chương trình cần bao nhiêu bước chính trong `loop()`? Kể tên.
10. Vì sao phải giữ thiết bị đứng yên khi cấp nguồn?
11. Serial Monitor phải chọn baud rate bao nhiêu?
12. Một cú té ngã thật thường có những dấu hiệu nào (từ các cảm biến)?

### Đáp án ngắn

1. Gia tốc theo 3 trục; đơn vị m/s².
2. Khoảng 1g (9,81 m/s²), vì cảm biến đo cả trọng lực Trái Đất.
3. Tụt mạnh xuống gần 0 (rơi cùng trọng lực).
4. Đơn vị độ/giây (dps); khi đứng yên khoảng 0 dps.
5. Giảm.
6. Vì thời tiết và gió cũng làm áp suất thay đổi.
7. `struct` là "cái hộp" gom nhiều biến liên quan; ví dụ `SensorSnapshot` gom mọi dữ liệu một lần đọc.
8. `enum` là danh sách các trạng thái; máy trạng thái luôn ở đúng một trạng thái tại một thời điểm.
9. Ba bước chính: đọc cảm biến → xử lý phát hiện té ngã → in dữ liệu (rồi lặp lại).
10. Để việc hiệu chuẩn ghi đúng "giá trị nền" khi đứng yên, tránh sai lệch về sau.
11. 115200.
12. Va đập mạnh + xoay người + đổi độ cao (càng nhiều dấu hiệu càng chắc chắn).
