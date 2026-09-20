# T10 — Điều tra sự cố FALL-01: Không phát hiện ngã trên bề mặt mềm (nệm/giường) & Thiết kế bộ ghi dữ liệu thực nghiệm

> **Mã nhiệm vụ**: T10 (FALL-01 Investigation)  
> **Phạm vi**: Chỉ điều tra & thiết kế logging (Investigation-only). Không thay đổi logic nhận diện, không chỉnh ngưỡng tham số, không can thiệp SOS/SMS/CALL.  
> **Căn cứ**: Phân tích mã nguồn tĩnh trên tập tin thực tế tại repository `nckh27pa`.

---

## 1. Current detector logic (Logic nhận diện hiện tại)

Quy trình xử lý tín hiệu cảm biến và nhận diện té ngã phía điện thoại được tổ chức thành một pipeline đồng bộ khép kín:

```
[Phần cứng cảm biến Android]
      │
      ▼ (SensorManager.SENSOR_DELAY_GAME trên Main Looper)
PhoneSensorCollector.kt:25-63
      │  onSensorChanged() -> chuyển đổi Gyro (deg/s), Orientation (deg), Pressure (Pa)
      ▼
PhoneInputPipeline / PhoneNormalizer (DemoLogic.kt:23-89, 319-327)
      │  Đồng bộ khung thời gian 500ms; chỉ khi có ACCEL mới phát PhoneSensorPacket
      ▼
DemoSession.accept() (DemoLogic.kt:289-293)
      │  Chỉ tiếp nhận khi AlertCore đang ở State.MONITORING
      ▼
DemoDetector.accept() (DemoLogic.kt:129-201)
      │  State machine nhận diện: NORMAL -> IMPACT_DETECTED -> POST_IMPACT_STILLNESS -> FALL_CONFIRMED
      ▼ (nếu trả về true: FALL_CONFIRMED)
DemoSession -> AlertCore (DemoLogic.kt:291 & Core.kt:73-92)
      │  Gọi liên tiếp: core.suspected() -> core.evidenceConfirmed()
      ▼
AlertCore: State.VERIFYING (Đếm ngược 10 giây trước khi kích hoạt SOS)
```

### 1.1. Đăng ký & lấy mẫu cảm biến (`PhoneSensorCollector.kt`)
- **Khởi tạo & Đăng ký** (`PhoneSensorCollector.kt:25-35`): Đăng ký 5 loại cảm biến:
  - `Sensor.TYPE_ACCELEROMETER` (`SensorKind.ACCEL`)
  - `Sensor.TYPE_LINEAR_ACCELERATION` (`SensorKind.LINEAR`)
  - `Sensor.TYPE_GYROSCOPE` (`SensorKind.GYRO`)
  - `Sensor.TYPE_ROTATION_VECTOR` (`SensorKind.ORIENTATION`)
  - `Sensor.TYPE_PRESSURE` (`SensorKind.PRESSURE`)
- **Tần số lấy mẫu** (`PhoneSensorCollector.kt:31`): `SensorManager.SENSOR_DELAY_GAME` (~20 ms / ~50 Hz trên lý thuyết Android, thực tế phụ thuộc phần cứng OEM), callback chuyển về `Handler(Looper.getMainLooper())`.
- **Chuyển đổi đơn vị & định hướng** (`PhoneSensorCollector.kt:46-60`):
  - Khi nhận `ORIENTATION` (Rotation Vector): gọi `SensorManager.getRotationMatrixFromVector` và `SensorManager.getOrientation` (`:52-53`). Giá trị được đổi sang độ (`Math.toDegrees`): `pitch` (`orientation[1]`), `roll` (`orientation[2]`), `azimuth/yaw` (`orientation[0]`) (`:54-58`).
  - Gyroscope được nhân hệ số `180 / PI` để sang độ/giây (`deg/s`) tại `DemoLogic.kt:42`.
  - Áp suất khí quyển được nhân `100` để sang Pascal (`Pa`) tại `DemoLogic.kt:43`.
- **Nhịp phát gói tin** (`DemoLogic.kt:325`): Chỉ khi có sự kiện từ `SensorKind.ACCEL` thì `PhoneInputPipeline` mới gọi `onPacket(latest(nowNs, wallMs))`. Các cảm biến khác chỉ lưu đệm trong `PhoneNormalizer` (`DemoLogic.kt:52`).

### 1.2. Máy trạng thái nhận diện trên điện thoại (`DemoDetector` trong `DemoLogic.kt`)
Máy trạng thái gồm 4 pha: `NORMAL`, `IMPACT_DETECTED`, `POST_IMPACT_STILLNESS`, `FALL_CONFIRMED` (`DemoLogic.kt:92`).
Mỗi gói tin `PhoneSensorPacket` đi qua các bước tuần tự trong `DemoDetector.accept()` (`DemoLogic.kt:129-201`):

1. **Đo độ lớn gia tốc toàn phần** (`DemoLogic.kt:143`):
   $$|a| = \sqrt{a_x^2 + a_y^2 + a_z^2} \quad (\text{đơn vị: } \text{m/s}^2)$$
2. **Kiểm tra gián đoạn lấy mẫu** (`DemoLogic.kt:145-151`):
   So sánh khoảng cách thời gian giữa 2 mẫu liên tiếp: $\Delta t = t - t_{prev}$.
   Nếu $\Delta t \le 0$ hoặc $\Delta t > \text{config.maximumSampleGapMs} \times 10^6\text{ ns}$ (mặc định: **250 ms**):
   Gọi `clearEvidence()` (xóa sạch vết va chạm và mẫu tĩnh) và quay về `NORMAL`.
3. **Pha va chạm (Impact Phase)** (`DemoLogic.kt:153-162`):
   So sánh: $|a| \ge \text{config.impactAccelerationMs2}$ (mặc định: **25.0 m/s²**, tương đương ~2.55 g).
   - Nếu thỏa mãn: Ghi nhận thời điểm va chạm $t_{impact} = t$, reset `quiet = null`, `count = 0`, chuyển sang `IMPACT_DETECTED`, trả về `false`.
   - Lưu ý: Bất kỳ mẫu nào $\ge 25.0\text{ m/s}^2$ đều đè lại mốc $t_{impact}$ mới.
4. **Kiểm tra thời hạn cửa sổ hậu va chạm** (`DemoLogic.kt:168-174`):
   Nếu chưa có va chạm (`impact == null`), giữ `NORMAL`.
   Nếu $t - t_{impact} > \text{config.postImpactWindowMs} \times 10^6\text{ ns}$ (mặc định: **3000 ms**):
   Hết thời gian chờ trạng thái tĩnh hậu va chạm $\to$ Hủy va chạm (`impact = null`, `quiet = null`, `count = 0`), quay về `NORMAL`, trả về `false`.
5. **Pha kiểm tra độ tĩnh hậu va chạm (Post-impact Stillness)** (`DemoLogic.kt:175-181`):
   So sánh sai số với trọng lực chuẩn:
   $$\Big| |a| - \text{config.stillnessTargetAccelerationMs2} \Big| \le \text{config.stillnessToleranceMs2}$$
   Với cấu hình mặc định:
   $$\Big| |a| - 9.81 \Big| \le 1.0\text{ m/s}^2 \iff |a| \in [8.81, 10.81]\text{ m/s}^2$$
   - **Nếu KHÔNG thỏa mãn** (bị lệch ra ngoài khoảng $[8.81, 10.81]$):
     `quiet = null`, `count = 0`, giữ pha `IMPACT_DETECTED`, trả về `false`.
     *(Nghĩa là: chỉ cần 1 mẫu duy nhất trồi ra ngoài dải này, toàn bộ tiến trình tích lũy độ tĩnh bị reset về 0).*
6. **Xác nhận ngã (Confirmation)** (`DemoLogic.kt:182-201`):
   - Nếu thỏa mãn dải tĩnh: gán $t_{quiet} = t$ (nếu bắt đầu tĩnh), tăng `count++`.
   - Thời gian tĩnh tích lũy: $\Delta t_{quiet} = t - t_{quiet}$.
   - Điều kiện xác nhận:
     $$\text{count} \ge \text{config.minimumStillnessSamples (6 mẫu)} \quad \text{VÀ} \quad \Delta t_{quiet} \ge \text{config.postImpactStillnessDurationMs (1000 ms)}$$
   - Khi cả 2 điều kiện đạt: chuyển sang `FALL_CONFIRMED`, gọi `clearEvidence()`, trả về `true`.
   - *Khí áp kế*: Thuộc tính `pressureCorroborated` (`:195-197`) chỉ được đính kèm vào `FallDetectionObservation` để hiển thị/quan sát, **không** tham gia quyết định logic ngã (không chặn biến `confirmed`).

### 1.3. Bảng ngưỡng cấu hình (`FallDetectionProfiles.kt` & `espconfig/Models.kt`)
Toàn bộ thông số dùng trong thuật toán được đóng gói trong `FallDetectionConfig` (`FallDetectionProfiles.kt:15-58`):

| Tham số cấu hình | Ý nghĩa & So sánh | Đơn vị | Mặc định (Phone) | Mặc định (ESP32) | Miền hợp lệ |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `impactAccelerationMs2` | Ngưỡng đỉnh gia tốc va chạm: $\|a\| \ge \text{ngưỡng}$ | m/s² | **25.0** (~2.55 g) | **25.000** | 1.0 .. 100.0 |
| `stillnessTargetAccelerationMs2` | Trọng lực mục tiêu khi đứng yên | m/s² | **9.81** (1.0 g) | **9.810** | 0.0 .. 20.0 |
| `stillnessToleranceMs2` | Dung sai độ tĩnh: $\| \|a\| - 9.81 \| \le \text{dung sai}$ | m/s² | **1.0** (~0.1 g) | **1.000** | 0.1 .. 10.0 |
| `postImpactWindowMs` | Cửa sổ tối đa tìm kiếm trạng thái tĩnh sau va chạm | ms | **3000** (3 giây) | **3000** | 500 .. 10,000 |
| `postImpactStillnessDurationMs` | Thời lượng tĩnh liên tục bắt buộc | ms | **1000** (1 giây) | **1000** | 100 .. 10,000 |
| `minimumStillnessSamples` | Số mẫu tĩnh liên tục tối thiểu | mẫu | **6** | **6** | 2 .. 100 |
| `maximumSampleGapMs` | Khoảng cách mẫu tối đa trước khi hủy vết va chạm | ms | **250** (4 Hz) | **250** | 10 .. 2,000 |

### 1.4. Máy trạng thái khẩn cấp chia sẻ (`AlertCore` trong `core/src/Core.kt`)
- `AlertCore` quản lý vòng đời cảnh báo: `MONITORING` $\to$ `SUSPECTED` $\to$ `VERIFYING` $\to$ `ALERTING` $\to$ `AWAITING_HELP` (`Core.kt:9`).
- Cơ chế kích hoạt từ `DemoSession`:
  Tại `DemoLogic.kt:290-292`:
  ```kotlin
  if (core.snapshot().state == State.MONITORING && detector.accept(packet)) {
      core.suspected(); core.evidenceConfirmed()
  }
  ```
  `core.suspected()` và `core.evidenceConfirmed()` được gọi nối tiếp tức thì. `AlertCore` chuyển thẳng từ `MONITORING` sang `VERIFYING`, mở đồng hồ đếm ngược 10 giây (`timeoutMs = 10_000`, `Core.kt:34`). Không có bước trung gian hay yêu cầu xác nhận lần hai nào cản trở bước chuyển này.

---

## 2. Suspected reason a soft-surface fall is missed (Nguyên nhân nghi ngờ bỏ sót ngã trên nệm)

Danh sách giả thuyết được sắp xếp theo mức độ khả dĩ giảm dần (khả dĩ nhất đặt lên đầu), kèm vị trí mã nguồn và tiêu chí xác thực/bác bỏ:

### Giả thuyết 1 (Xác suất cao nhất): Đỉnh va chạm trên bề mặt nệm bị tiêu hao năng lượng, không vượt qua ngưỡng 25.0 m/s²
- **Vị trí mã nguồn**: `DemoLogic.kt:153` (`val overImpact = magnitude >= config.impactAccelerationMs2`) và `FallDetectionProfiles.kt:56` (`impactAccelerationMs2 = 25f`).
- **Cơ chế vật lý**: Nệm lò xo/nệm mút có độ lún đàn hồi lớn ($s \approx 5\text{--}15\text{ cm}$), kéo dài thời gian giảm tốc $\Delta t$ khi tiếp xúc. Theo định luật II Newton ($F \Delta t = m \Delta v$), xung lực va chạm bị dàn trải làm đỉnh gia tốc cực đại $a_{peak}$ giảm mạnh. Khi rơi từ độ cao cầm tay bình thường (0.5m – 1.0m) xuống nệm, gia tốc cực đại thường chỉ đạt khoảng $14\text{--}22\text{ m/s}^2$ (dưới 2.55 g). Do thuật toán kiểm tra nhị phân tuyệt đối $|a| \ge 25.0$, nếu đỉnh chỉ đạt $24.8\text{ m/s}^2$, biến `overImpact` là `false`, `impact` không được ghi nhận (`:155`), máy trạng thái vĩnh viễn ở `NORMAL`.
- **Quan sát để xác thực**: File log thực nghiệm cho thấy sự kiện rơi xuống nệm có $|a|_{max}$ nằm trong khoảng $15\text{--}23\text{ m/s}^2$, trường `phase` giữ nguyên `NORMAL`, `impactOverThreshold` luôn là `false`.
- **Quan sát để bác bỏ**: File log ghi nhận $|a|_{max} \ge 25.0\text{ m/s}^2$ và `phase` đã chuyển thành công sang `IMPACT_DETECTED`.

### Giả thuyết 2 (Xác suất cao): Dao động nhún đàn hồi của nệm làm đứt quãng chuỗi mẫu tĩnh trong dải hẹp $[8.81, 10.81]\text{ m/s}^2$
- **Vị trí mã nguồn**: `DemoLogic.kt:168, 175-181`.
- **Cơ chế vật lý**: Giả sử cú va chạm ban đầu đủ mạnh để vượt ngưỡng 25 m/s² và vào pha `IMPACT_DETECTED`. Bề mặt nệm không dừng đứng yên ngay như sàn gạch mà dao động tắt dần (damped oscillation) trong 1.0 – 2.0 giây. Ngưỡng tĩnh hiện tại đòi hỏi $|a| \in [8.81, 10.81]\text{ m/s}^2$. Chỉ cần một mẫu bị nẩy nhẹ khiến $|a| = 10.85\text{ m/s}^2$ hoặc chùng xuống $8.70\text{ m/s}^2$, các dòng `177-178` ngay lập tức thực thi:
  ```kotlin
  quiet = null
  count = 0
  ```
  Số mẫu tĩnh tích lũy bị xóa sạch về 0. Đồng thời, thời hạn tối đa sau va chạm là `postImpactWindowMs = 3000 ms` (`:168`). Nếu các dao động nhún của nệm kéo dài quá 2000 ms, thời gian tĩnh còn lại không đủ 1000 ms (`postImpactStillnessDurationMs`), đồng hồ 3000 ms hết hạn (`:168`), va chạm bị hủy bỏ (`impact = null`), đưa hệ thống về `NORMAL`.
- **Quan sát để xác thực**: File log cho thấy `phase = IMPACT_DETECTED`, nhưng `stillnessSampleCount` liên tục bị reset về 0 do $|a|$ vượt biên dung sai $\pm 1.0\text{ m/s}^2$; sau 3000 ms kể từ va chạm, `phase` tự động rớt về `NORMAL` mà không bao giờ tới `FALL_CONFIRMED`.
- **Quan sát để bác bỏ**: File log cho thấy sau va chạm, $|a|$ rơi vào dải $[8.81, 10.81]\text{ m/s}^2$ ngay lập tức và giữ ổn định liên tục $> 1000\text{ ms}$ mà vẫn không xác nhận ngã.

### Giả thuyết 3: Chu kỳ lấy mẫu cảm biến (`SENSOR_DELAY_GAME`) trên Main Looper bị trễ/bỏ lỡ đỉnh va chạm hẹp
- **Vị trí mã nguồn**: `PhoneSensorCollector.kt:31` (`manager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME, Handler(Looper.getMainLooper()))`).
- **Cơ chế vật lý**: `SENSOR_DELAY_GAME` yêu cầu chu kỳ lý thuyết ~20 ms (50 Hz). Tuy nhiên, trên Android, callback được điều phối qua `Looper.getMainLooper()`. Nếu Main UI thread bị bận (recomposition Compose, garbage collection, chuyển màn hình), các sự kiện cảm biến bị dồn hoặc trễ. Thêm vào đó, nếu xung va chạm nảy chỉ diễn ra trong 15–20 ms, việc lấy mẫu ở tần số 50 Hz (hoặc thấp hơn nếu phần cứng OEM throttling) có thể chụp trúng sườn dốc (ví dụ $16\text{ m/s}^2$ và $18\text{ m/s}^2$) mà trượt mất điểm đỉnh $> 25\text{ m/s}^2$.
- **Quan sát để xác thực**: File log thực nghiệm cho thấy bước nhảy thời gian giữa các mẫu quanh điểm va chạm dao động bất thường $> 30\text{--}50\text{ ms}$, đường cong gia tốc có dạng bậc thang cụt đầu.
- **Quan sát để bác bỏ**: File log cho thấy mật độ mẫu dày đặc, đều đặn $\le 20\text{ ms}$, bắt trọn vẹn đường cong parabol mượt mà của gia tốc.

### Giả thuyết 4: Gián đoạn dòng dữ liệu vượt quá `maximumSampleGapMs` (250 ms) xóa sạch dấu vết va chạm
- **Vị trí mã nguồn**: `DemoLogic.kt:145-151`.
- **Cơ chế mã nguồn**:
  ```kotlin
  val maximumGapNs = config.maximumSampleGapMs * 1_000_000L
  if (previous != null && (t <= previous || t - previous > maximumGapNs)) {
      clearEvidence()
      last = t
      currentObservation = observationFor(profile, magnitude, DetectionPhase.NORMAL)
      return false
  }
  ```
  Nếu giữa cú va chạm và giai đoạn nằm yên có một khoảng trống mẫu cảm biến $> 250\text{ ms}$ (do Android Sensor HAL tạm ngưng khi điện thoại rơi tự do, lật mặt hoặc nghẽn Main thread), hàm `clearEvidence()` tự động xóa `impact = null; quiet = null; count = 0;`.
- **Quan sát để xác thực**: File log ghi nhận $t - t_{prev} > 250\text{ ms}$ ngay trong khoảng $0\text{--}3000\text{ ms}$ sau va chạm, lập tức kéo theo việc hủy trạng thái `IMPACT_DETECTED`.
- **Quan sát để bác bỏ**: Mọi khoảng cách mẫu trong toàn bộ quá trình thử nghiệm đều $< 250\text{ ms}$.

### Giả thuyết 5: Dữ liệu con quay hồi chuyển và góc nghiêng không được sử dụng trong quyết định
- **Vị trí mã nguồn**: `DemoLogic.kt:14, 15, 68, 69, 138-201`.
- **Hiện trạng thực tế**: `PhoneSensorPacket` có thu thập `gyroXDps, gyroYDps, gyroZDps` và `pitchDeg, rollDeg, yawDeg`, nhưng trong hàm `DemoDetector.accept()`, **hoàn toàn không có bất kỳ dòng lệnh nào đọc hay so sánh các giá trị này**. Thuật toán dựa 100% vào độ lớn gia tốc 3 trục. Khi rơi xuống nệm, gia tốc tuyến tính có thể thấp, nhưng sự xoay chuyển tư thế (từ thẳng đứng sang nằm ngang) và vận tốc góc khi rơi là bằng chứng rất rõ rệt nhưng đã bị bỏ qua hoàn toàn.
- **Quan sát để xác thực**: Khẳng định bằng phân tích mã nguồn: `DemoDetector` không dùng Gyro/Orientation.
- **Quan sát để bác bỏ**: Không thể bác bỏ (đây là sự thật cấu trúc mã nguồn).

### Giả thuyết 6: Thu thập cảm biến bị dừng hoặc CPU ngủ khi màn hình tắt / ứng dụng xuống nền
- **Vị trí mã nguồn**: `MainActivity.kt:150-156`, `MonitoringService.kt:79-87`, `SensorOwnership.kt:5-10`.
- **Cơ chế**: Khi màn hình tắt hoặc app ra nền mà `MonitoringService` chưa được bật, `collector.stop()` được gọi ngay lập tức (`MainActivity.kt:153`). Ngay cả khi `MonitoringService` đang chạy, `PARTIAL_WAKE_LOCK` chỉ được giữ khi trạng thái là `State.VERIFYING` (`MonitoringService.kt:79-87`). Trong trạng thái `MONITORING`, không có WakeLock; nếu màn hình tắt trong lúc thả rơi, CPU Android có thể sleep làm ngưng luồng cảm biến.
- **Quan sát để xác thực**: File log bị ngắt quãng hoàn toàn khi tắt màn hình; không có sự kiện nào được ghi lại trong khoảnh khắc thả rơi.
- **Quan sát để bác bỏ**: Thử nghiệm được thực hiện khi màn hình sáng, app đang mở ở tiền sảnh, dữ liệu cảm biến vẫn đổ đều đặn.

---

### CÁC GIẢ THUYẾT BỊ BÁC BỎ TRỰC TIẾP BỞI MÃ NGUỒN (KHÔNG KHẢ THI):
1. **Pha rơi tự do (free-fall / minimum-acceleration) quá ngắn hoặc không đạt ngưỡng**:
   - *Bác bỏ*: Thuật toán `DemoDetector` (`DemoLogic.kt:108-255`) **hoàn toàn không có pha rơi tự do**. Nó chuyển thẳng từ `NORMAL` sang `IMPACT_DETECTED` chỉ dựa vào `magnitude >= config.impactAccelerationMs2`. Giả thuyết này không tồn tại trong mã nguồn hiện tại.
2. **Độ tĩnh hậu va chạm bị phát hiện quá sớm ngay trong lúc đang rơi (airborne phase)**:
   - *Bác bỏ*: Trong lúc rơi tự do, $|a| \approx 0\text{ m/s}^2$. Điều kiện tĩnh yêu cầu $|a| \in [8.81, 10.81]\text{ m/s}^2$ (`DemoLogic.kt:175`). Ngoài ra, kiểm tra tĩnh chỉ diễn ra sau khi `impact != null` (`:163`). Do đó không thể có hiện tượng thỏa mãn độ tĩnh khi đang bay trên không.
3. **Trạng thái nghi ngờ (Suspect state) cần một xác nhận thứ hai mà rơi trên nệm không tạo ra được**:
   - *Bác bỏ*: Tại `DemoLogic.kt:291`, ngay khi detector trả về `true`, hệ thống gọi liên tiếp `core.suspected(); core.evidenceConfirmed()`, đưa thẳng vào đếm ngược `VERIFYING`. Không có điều kiện xác nhận thứ hai nào được yêu cầu trong mã.

---

## 3. Logging / instrumentation proposal (Thiết kế bộ ghi dữ liệu thực nghiệm trên thiết bị thật)

Để có cơ sở dữ liệu thực nghiệm định lượng chính xác các giả thuyết trên mà không phải đoán mò hay hạ ngưỡng bừa bãi, thiết kế bộ ghi dữ liệu độc lập (không sửa đổi logic nhận diện) như sau:

### 3.1. Điểm móc phát dữ liệu (Hook Points)
- **Điểm móc chính**: `DemoSession.accept(packet: PhoneSensorPacket)` tại `DemoLogic.kt:289-293` (hoặc trong `DemoController.acceptPhone` tại `DemoApplication.kt:424-430`).
  - *Lý do*: Tại đây, chúng ta có đồng thời:
    1. Gói cảm biến đầy đủ `packet` (`timestampNs`, `wallClockTimestampMs`, gia tốc, con quay, định hướng, áp suất).
    2. Kết quả quan sát của detector: `detector.observation()` (`phase`, `impactOverThreshold`, `withinStillness`, `stillnessProgress`, `stillnessSampleCount`).
    3. Cấu hình profile đang kích hoạt: `profile.displayName`, `config.*`.
    4. Trạng thái lõi: `core.snapshot().state`.

### 3.2. Định dạng & cấu trúc cột CSV
Bộ ghi sẽ xuất ra tệp CSV phẳng, dòng đầu tiên là tiêu đề (Header):

```csv
wall_ms,elapsed_ns,ax,ay,az,mag_a,gx,gy,gz,mag_g,min_a_500ms,max_a_500ms,pitch,roll,yaw,phase,impact_over,within_still,still_samples,still_progress,core_state,profile_name,cfg_impact,cfg_still_target,cfg_still_tol,cfg_post_win,cfg_post_dur,cfg_min_samples,cfg_max_gap
```

**Chi tiết trường dữ liệu**:
- `wall_ms`: `packet.wallClockTimestampMs` (thời gian thực epoch ms).
- `elapsed_ns`: `packet.timestampNs` (thời gian cảm biến từ boot, độ phân giải nanosecond).
- `ax, ay, az`: `packet.accelXMs2, accelYMs2, accelZMs2` (m/s²).
- `mag_a`: $\sqrt{a_x^2 + a_y^2 + a_z^2}$ (m/s²). *(Mã hiện tại đã tính ở `DemoLogic.kt:143`)*.
- `gx, gy, gz`: `packet.gyroXDps, gyroYDps, gyroZDps` (độ/giây).
- `mag_g`: $\sqrt{g_x^2 + g_y^2 + g_z^2}$ (độ/giây). *(Tính mới)*.
- `min_a_500ms`: Giá trị $|a|_{min}$ nhỏ nhất trong cửa sổ trượt 500 ms gần nhất. *(Tính mới — dùng đánh giá dấu hiệu rơi tự do)*.
- `max_a_500ms`: Giá trị $|a|_{max}$ lớn nhất trong cửa sổ trượt 500 ms gần nhất. *(Tính mới — dùng bắt đỉnh va chạm)*.
- `pitch, roll, yaw`: `packet.pitchDeg, rollDeg, yawDeg` (độ). *(Đã được tính sẵn từ `PhoneSensorCollector.kt:54-58`)*.
- `phase`: Tên enum pha hiện tại (`NORMAL`, `IMPACT_DETECTED`, `POST_IMPACT_STILLNESS`, `FALL_CONFIRMED`).
- `impact_over`: `true`/`false` (`observation.impactOverThreshold`).
- `within_still`: `true`/`false` (`observation.withinStillness`).
- `still_samples`: Số mẫu tĩnh hiện tại (`observation.stillnessSampleCount`).
- `still_progress`: Tiến độ thời gian tĩnh $0.0 \to 1.0$ (`observation.stillnessProgress`).
- `core_state`: Trạng thái lõi (`MONITORING`, `SUSPECTED`, `VERIFYING`, v.v.).
- `profile_name`: Tên bảng cấu hình đang chạy (ví dụ `Bảng 1`).
- `cfg_impact` ... `cfg_max_gap`: Toàn bộ 7 giá trị số của `FallDetectionConfig` đang áp dụng.

### 3.3. Vị trí lưu trữ & chính sách xoay vòng tệp (File Path & Rotation)
- **Đường dẫn tệp trên thiết bị**:  
  `context.getExternalFilesDir(null)/fall_logs/fall_trial_<yyyyMMdd_HHmmss>.csv`  
  *(Tương đương: `/sdcard/Android/data/vn.nckh27pa.fallsafe/files/fall_logs/...`)*.
  - *Lợi thế*: Không cần quyền `MANAGE_EXTERNAL_STORAGE`, hoàn toàn truy cập được thông qua `adb pull` trên bản debug.
- **Chính sách xoay vòng (Rotation)**:
  - Mỗi lần bấm Bắt đầu/Dừng ghi sẽ tạo 1 tệp mới gắn nhãn thời gian.
  - Giới hạn kích thước tối đa 1 tệp: 10 MB (tương đương ~20 phút ghi liên tục).
  - Tối đa lưu giữ 10 tệp gần nhất trong thư mục; tự động xóa tệp cũ nhất khi vượt quá giới hạn.
  - Sử dụng bộ đệm ghi `BufferedWriter` (kích thước đệm 8 KB), tự động `flush` định kỳ mỗi 1 giây hoặc khi có chuyển pha nhận diện (`phase != NORMAL`), đảm bảo không nghẽn Main thread và không mất dữ liệu khi rơi.

### 3.4. Lệnh trích xuất bằng một dòng `adb`
Để kéo toàn bộ dữ liệu log từ điện thoại về máy tính phân tích:
```bash
adb pull /sdcard/Android/data/vn.nckh27pa.fallsafe/files/fall_logs/ ./fall_logs/
```
Hoặc xem trực tiếp tệp mới nhất:
```bash
adb shell "cat \$(ls -t /sdcard/Android/data/vn.nckh27pa.fallsafe/files/fall_logs/*.csv | head -n 1)" > latest_run.csv
```

### 3.5. Cách người dùng khởi động / dừng ghi âm
- Thêm một nút chuyển đổi (Switch) đơn giản trong màn hình **Cài đặt $\to$ Phát hiện té ngã $\to$ Hiệu chỉnh thử nghiệm** (`FallDetectionCalibrationScreen.kt`):
  `"Ghi nhật ký cảm biến (CSV)" [BẬT / TẮT]`.
- Khi BẬT: Khởi tạo file CSV, ghi dòng header và bắt đầu stream dữ liệu.
- Khi TẮT: Flush dữ liệu, đóng file và hiển thị đường dẫn tệp đã lưu trên UI để người dùng biết.

### 3.6. Chi phí tài nguyên (Storage & CPU Overhead)
- Tần số lấy mẫu: ~50 Hz (50 dòng/giây).
- Kích thước trung bình 1 dòng CSV: ~160 bytes.
- Tốc độ sinh dữ liệu: $50 \times 160\text{ bytes} \approx 8.000\text{ bytes/giây} \approx 8\text{ KB/s}$.
- Dung lượng mỗi phút: $8\text{ KB/s} \times 60 \approx 480\text{ KB/phút}$ (~**0.48 MB / phút**).
- Một đợt thử nghiệm 6 kịch bản (mỗi kịch bản kéo dài ~30 giây) tốn chưa đầy **1.5 MB** bộ nhớ trong, hoàn toàn không gây áp lực tài nguyên.

### 3.7. Danh mục các trường: Có sẵn vs Cần tính mới
- **Đã có sẵn trong mã nguồn**:
  - `wallClockTimestampMs`, `timestampNs` (`PhoneSensorPacket`, `DemoLogic.kt:10`)
  - `ax, ay, az` (`PhoneSensorPacket`, `DemoLogic.kt:11`)
  - `|a|` (`DemoDetector.kt:143`)
  - `gx, gy, gz` (`PhoneSensorPacket`, `DemoLogic.kt:14`)
  - `pitch, roll, yaw` (`PhoneSensorPacket`, `DemoLogic.kt:15` & `PhoneSensorCollector.kt:54-58`)
  - `phase`, `impactOverThreshold`, `withinStillness`, `stillnessProgress`, `stillnessSampleCount` (`FallDetectionObservation`, `DemoLogic.kt:94-105`)
  - Cấu hình & tên bảng (`FallDetectionProfile`, `FallDetectionConfig`, `DemoLogic.kt:97-99`)
- **Cần tính toán bổ sung trong logger**:
  - `mag_g`: $\sqrt{g_x^2 + g_y^2 + g_z^2}$ (độ lớn vector vận tốc góc).
  - `min_a_500ms` & `max_a_500ms`: Cửa sổ trượt hàng đợi 500 ms tính $|a|_{min}$ và $|a|_{max}$.

---

## 4. Files that would need modification (Danh sách tập tin cần sửa đổi)

Để tích hợp hệ thống đo kiểm trên, danh sách tập tin can thiệp tối thiểu gồm:

1. **`android/app/src/main/java/vn/nckh27pa/fallsafe/logging/FallSensorLogger.kt`** (TẠO MỚI):
   - Đảm nhận toàn bộ việc duy trì cửa sổ trượt `min |a|`, `max |a|`, tính `mag_g`, định dạng dòng CSV, ghi file nền và quản lý xoay vòng file.
2. **`android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt`**:
   - Khởi tạo instance của `FallSensorLogger`, gắn hook vào hàm `acceptPhone` (`:424-430`) để chuyển tiếp gói tin và trạng thái quan sát sang logger.
3. **`android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionCalibrationScreen.kt`**:
   - Bổ sung UI nút bật/tắt ghi dữ liệu thử nghiệm trong màn hình hiệu chỉnh để chủ dự án dễ dàng thao tác trên tay.

### TUYỆT ĐỐI KHÔNG CHẠM VÀO TRONG BƯỚC NÀY:
- **Không sửa bất kỳ ngưỡng nào** trong `FallDetectionProfiles.kt` hoặc `espconfig/Models.kt`.
- **Không thay đổi thuật toán nhận diện** trong `DemoLogic.kt` (`DemoDetector.accept`).
- **Không can thiệp luồng cảnh báo khẩn cấp**: `EmergencyCore.kt`, `AndroidSmsManagerGateway.kt`, `AndroidSimCallGateway.kt`.
- **Không chỉnh sửa giao diện chính của người dùng**: `HomeScreen.kt` và các chuỗi hiển thị trợ năng.

---

## 5. Recommended next experiment (Kịch bản thực nghiệm đề xuất trên thiết bị thật)

### 5.1. Trình tự thực hiện trên điện thoại thật (6 bước chuẩn hóa)
Sau khi nạp bản dựng có gắn bộ ghi CSV, người thử nghiệm thực hiện tuần tự 6 bài test độc lập. Trước mỗi bài test, bấm Bật ghi, thực hiện động tác, đợi 5 giây nằm yên, sau đó bấm Tắt ghi để có các file CSV tách biệt:

1. **Rơi tự do xuống nệm (Drop onto bed)**: Cầm điện thoại ở độ cao ngực (~1.0m) hoặc ngang hông (~0.7m), thả rơi tự nhiên xuống giường/nệm mút (thử cả 3 tư thế: tiếp xúc lưng, tiếp xúc úp mặt, tiếp xúc cạnh).
2. **Ngồi phịch mạnh xuống ghế/giường (Sit down fast)**: Bỏ điện thoại trong túi quần, ngồi thả người mạnh xuống nệm hoặc ghế sô-pha có đệm.
3. **Nằm nhanh xuống giường (Lie down)**: Cầm điện thoại hoặc để trong túi áo/túi quần, ngả người nằm nhanh xuống giường như khi mệt mỏi.
4. **Đặt mạnh điện thoại lên bàn cứng (Place firmly on table)**: Cầm điện thoại dằn dập dằn xuống mặt bàn gỗ/mặt kính với lực dứt khoát (mô phỏng va chạm cứng nhưng không phải ngã).
5. **Đi bộ nhanh và chạy bộ (Walk / Run)**: Bỏ điện thoại vào túi quần, đi bộ nhanh và chạy bước nhỏ trong phòng / cầu thang trong 30 giây.
6. **Thay đổi tư thế sinh hoạt bình thường (Normal posture change)**: Cúi người buộc dây giày, nhặt đồ dưới sàn, đứng lên ngồi xuống từ từ.

### 5.2. Các chỉ số đối sánh cần trích xuất từ dữ liệu CSV
Sau khi thu thập 6 file CSV, mở trên công cụ phân tích (Python / Jupyter Notebook / Excel) để so sánh các đại lượng then chốt:

| Chỉ số phân tích | Mục đích xác định | Câu hỏi cần trả lời |
| :--- | :--- | :--- |
| **Đỉnh $|a|_{max}$ va chạm** | So sánh giữa Rơi xuống nệm vs Đặt mạnh lên bàn vs Ngồi phịch | Ngưỡng 25 m/s² cao hơn hay thấp hơn đỉnh thực tế khi rơi xuống nệm? Có khoảng cách an toàn (margin) với thao tác ngồi mạnh không? |
| **Độ sâu rơi tự do $|a|_{min}$** | Khảo sát cửa sổ 100–400 ms trước va chạm | Rơi xuống nệm có xuất hiện giai đoạn mất trọng lực ($|a| < 3.0\text{ m/s}^2$) rõ rệt mà các hành vi ngồi/đặt bàn không có không? |
| **Thời gian dập tắt dao động nệm** | Đo thời gian từ đỉnh va chạm đến khi $|a| \in [8.81, 10.81]$ | Nệm rung lắc trong bao lâu? Cửa sổ 3000 ms (`postImpactWindowMs`) và dung sai $\pm 1.0\text{ m/s}^2$ có đủ để dung nạp dao động đàn hồi không? |
| **Vận tốc góc $|g|$ & Biến thiên góc ($\Delta pitch, \Delta roll$)** | Đo độ xoay và đổi hướng tư thế | Điện thoại có xoay lật góc lớn ($> 45^\circ$) khi rơi không? Tư thế nằm im trên nệm có khác biệt với tư thế đứng trong túi quần không? |

### 5.3. Các kết luận KHÔNG THỂ rút ra nếu thiếu dữ liệu thiết bị thật
1. **Không thể biết đỉnh gia tốc thực tế trên chiếc nệm cụ thể của chủ dự án là bao nhiêu**: Độ dày, độ đàn hồi của nệm mút hay nệm lò xo có thể tạo ra đỉnh dao động từ 12 m/s² đến 24 m/s². Mọi con số suy đoán lúc này đều là võ đoán.
2. **Không thể khẳng định liệu có cần hạ ngưỡng gia tốc hay chuyển sang thuật toán kết hợp rơi tự do + thay đổi góc xoay**: Nếu hạ ngưỡng va chạm xuống quá thấp để bắt được nệm, hệ thống có thể bị báo động giả hàng loạt khi ngồi mạnh. Chỉ có biểu đồ dữ liệu thực tế mới cho thấy vùng phân tách (decision boundary) tối ưu.
3. **Không thể biết tần số lấy mẫu thực tế của phần cứng điện thoại khi rơi**: Không thể biết liệu cảm biến của máy có bị lag, drop frame hay trôi mẫu trên 250 ms hay không nếu không có file log ghi nhận từ `elapsedRealtimeNanos`.
