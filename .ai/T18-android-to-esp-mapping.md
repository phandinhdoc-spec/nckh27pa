# T18 — Mapping Android → ESP

---

### A. Nguồn sự thật (hằng số/ngưỡng đang dùng thật)

| Tên | Giá trị | Đơn vị gốc | Ý nghĩa | File:dòng trích dẫn |
|---|---|---|---|---|
| `FallDetectionConfig.DEFAULT` | `FallDetectionConfig(25f, 9.81f, 1f, 3000L, 1000L, 6, 250L)` | object | Cấu hình phát hiện ngã mặc định của Android | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `impactAccelerationMs2` | `25f` | $m/s^2$ | Ngưỡng gia tốc tổng nhận diện va đập (impact) | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `stillnessTargetAccelerationMs2` | `9.81f` | $m/s^2$ | Gia tốc trọng trường mục tiêu khi nằm yên bất động | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `stillnessToleranceMs2` | `1f` | $m/s^2$ | Dung sai cho phép quanh mốc gia tốc nằm yên ($|a - 9.81| \le 1.0$) | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `postImpactWindowMs` | `3000L` | ms | Cửa sổ thời gian tối đa sau va đập để hoàn tất pha nằm yên | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `postImpactStillnessDurationMs` | `1000L` | ms | Thời lượng tối thiểu liên tục nằm yên sau va đập | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `minimumStillnessSamples` | `6` | mẫu | Số lượng mẫu cảm biến tối thiểu đạt điều kiện nằm yên | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `maximumSampleGapMs` | `250L` | ms | Khoảng cách tối đa giữa 2 mẫu cảm biến; vượt quá sẽ hủy chuỗi đánh giá | [FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56) |
| `phonePressureEvidenceEnabled` | `false` | boolean | Cờ kích hoạt đối chứng áp suất khí quyển trên điện thoại | [FallDetectionProfiles.kt:23](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L23) |
| `phonePressureMinimumRisePa` | `12f` | Pa | Độ tăng áp suất tối thiểu trong cửa sổ lịch sử để xác nhận rơi | [FallDetectionProfiles.kt:24](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L24) |
| `MIN_IMPACT_ACCELERATION_MS2` | `1f` | $m/s^2$ | Giới hạn dưới cho phép của cấu hình `impactAccelerationMs2` | [FallDetectionProfiles.kt:40](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L40) |
| `MAX_IMPACT_ACCELERATION_MS2` | `100f` | $m/s^2$ | Giới hạn trên cho phép của cấu hình `impactAccelerationMs2` | [FallDetectionProfiles.kt:41](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L41) |
| `MIN_STILLNESS_TARGET_ACCELERATION_MS2` | `0f` | $m/s^2$ | Giới hạn dưới cho phép của `stillnessTargetAccelerationMs2` | [FallDetectionProfiles.kt:42](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L42) |
| `MAX_STILLNESS_TARGET_ACCELERATION_MS2` | `20f` | $m/s^2$ | Giới hạn trên cho phép của `stillnessTargetAccelerationMs2` | [FallDetectionProfiles.kt:43](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L43) |
| `MIN_STILLNESS_TOLERANCE_MS2` | `0.1f` | $m/s^2$ | Giới hạn dưới cho phép của `stillnessToleranceMs2` | [FallDetectionProfiles.kt:44](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L44) |
| `MAX_STILLNESS_TOLERANCE_MS2` | `10f` | $m/s^2$ | Giới hạn trên cho phép của `stillnessToleranceMs2` | [FallDetectionProfiles.kt:45](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L45) |
| `MIN_POST_IMPACT_WINDOW_MS` | `500L` | ms | Giới hạn dưới cho phép của `postImpactWindowMs` | [FallDetectionProfiles.kt:46](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L46) |
| `MAX_POST_IMPACT_WINDOW_MS` | `10000L` | ms | Giới hạn trên cho phép của `postImpactWindowMs` | [FallDetectionProfiles.kt:47](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L47) |
| `MIN_POST_IMPACT_STILLNESS_DURATION_MS` | `100L` | ms | Giới hạn dưới cho phép của `postImpactStillnessDurationMs` | [FallDetectionProfiles.kt:48](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L48) |
| `MAX_POST_IMPACT_STILLNESS_DURATION_MS` | `10000L` | ms | Giới hạn trên cho phép của `postImpactStillnessDurationMs` | [FallDetectionProfiles.kt:49](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L49) |
| `MINIMUM_STILLNESS_SAMPLES_MIN` | `2` | mẫu | Giới hạn dưới cho phép của `minimumStillnessSamples` | [FallDetectionProfiles.kt:50](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L50) |
| `MINIMUM_STILLNESS_SAMPLES_MAX` | `100` | mẫu | Giới hạn trên cho phép của `minimumStillnessSamples` | [FallDetectionProfiles.kt:51](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L51) |
| `MIN_MAXIMUM_SAMPLE_GAP_MS` | `10L` | ms | Giới hạn dưới cho phép của `maximumSampleGapMs` | [FallDetectionProfiles.kt:52](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L52) |
| `MAX_MAXIMUM_SAMPLE_GAP_MS` | `2000L` | ms | Giới hạn trên cho phép của `maximumSampleGapMs` | [FallDetectionProfiles.kt:53](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L53) |
| `phonePressureMinimumRisePa` range | `1f..200f` | Pa | Dải hợp lệ trong `isValid()` cho độ tăng áp suất điện thoại | [FallDetectionProfiles.kt:37](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L37) |
| `DEFAULT_PHONE_PRESSURE_MINIMUM_RISE_PA` | `12f` | Pa | Hằng số mặc định độ tăng áp suất xác nhận rơi | [FallDetectionProfiles.kt:54](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L54) |
| `PRESSURE_WINDOW_NS` | `5000000000L` (5 s) | ns | Độ rộng cửa sổ thời gian lưu vết áp suất để tính delta | [DemoLogic.kt:89](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L89) |
| `MAX_PRESSURE_SAMPLES` | `64` | mẫu | Số mẫu áp suất tối đa lưu trong hàng đợi trượt | [DemoLogic.kt:90](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L90) |
| Ngưỡng fresh của packet | `500000000L` (500 ms) | ns | Mẫu cảm biến quá 500 ms coi như mất tính tươi mới (stale) | [DemoLogic.kt:65](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L65) |
| Profile thử nghiệm mặc định khi không có repo | `DEFAULT_DETECTION_PROFILE` | object | Profile tên `"Bảng mặc định thử nghiệm"`, `id="experimental-default"`, `profileNumber=0`, dùng `FallDetectionConfig.DEFAULT` | [DemoLogic.kt:266-275](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L266-L275) |
| Profile thử nghiệm mặc định trong repo | `"Bảng 1"` | object | Snapshot v2 tạo ban đầu: `displayName="Bảng 1"`, `profileNumber=1`, `isActive=true`, dùng `FallDetectionConfig.DEFAULT` | [FallDetectionProfiles.kt:257-260](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L257-L260) |

---

### B. Pipeline Android thực tế (DemoDetector)

#### 1. Trình tự logic trong `DemoDetector.accept(p: PhoneSensorPacket)` ([DemoLogic.kt:132-222](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L132-L222))
1. **Kiểm tra thay đổi cấu hình Profile:** ([DemoLogic.kt:134-141](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L134-L141))
   - Nếu `profile.id` hoặc `profile.config` thay đổi so với `observedProfile`: xóa sạch bằng chứng đang theo dõi (`clearEvidence()`), đưa observation về `NORMAL`, ghi log trace `PROFILE_CHANGED` và trả về `false`.
2. **Kiểm tra tính hợp lệ dữ liệu đầu vào:** ([DemoLogic.kt:144-150](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L144-L150))
   - Kiểm tra `timestampNs < 0` hoặc bất kỳ trục nào trong `(accelXMs2, accelYMs2, accelZMs2)` là NaN/Infinity.
   - Nếu không hợp lệ: gọi `reset()`, ghi log trace `INVALID_SAMPLE_INPUT` và trả về `false`.
3. **Tính độ lớn gia tốc (Magnitude):** ([DemoLogic.kt:151](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L151))
   - Công thức: `magnitude = sqrt(accelX^2 + accelY^2 + accelZ^2)`.
4. **Bảo vệ khoảng cách mẫu (Gap guard):** ([DemoLogic.kt:152-165](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L152-L165))
   - Kiểm tra: nếu `previous != null` và `(t <= previous || t - previous > config.maximumSampleGapMs * 1_000_000L)`:
     - Toàn bộ bằng chứng bị xóa sạch (`clearEvidence()`), đặt `last = t`.
     - Observation quay về `NORMAL`. Nếu trước đó đang có va đập dở dang (`impact != null`), ghi log trace `SAMPLE_GAP_ABORTED_IMPACT_EPISODE`. Trả về `false`.
   - Cập nhật `last = t`.
5. **Kiểm tra vượt ngưỡng va đập (Impact threshold):** ([DemoLogic.kt:166-176](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L166-L176))
   - `overImpact = magnitude >= config.impactAccelerationMs2` ($25.0 m/s^2$).
   - Nếu đạt: lưu mốc thời gian va đập `impact = t`, xóa mốc tĩnh `quiet = null`, đặt đếm `count = 0`.
   - Chuyển observation sang `DetectionPhase.IMPACT_DETECTED` với `impactOverThreshold = true`. Trả về `false`.
6. **Kiểm tra hết hạn cửa sổ sau va đập (Post-impact window guard):** ([DemoLogic.kt:177-191](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L177-L191))
   - Nếu `impact == null`: observation chuyển về `NORMAL`, trả về `false`.
   - Nếu `t - impact > config.postImpactWindowMs * 1_000_000L` ($3000 ms$): cửa sổ hết hạn. Xóa `impact = null, quiet = null, count = 0`, chuyển về `NORMAL`, ghi log trace `POST_IMPACT_WINDOW_EXPIRED`, trả về `false`.
7. **Kiểm tra điều kiện bất động (Stillness check):** ([DemoLogic.kt:192-200](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L192-L200))
   - Công thức kiểm tra mẫu tĩnh: `abs(magnitude - config.stillnessTargetAccelerationMs2) <= config.stillnessToleranceMs2` ($|magnitude - 9.81| \le 1.0$).
   - Nếu vi phạm (mẫu bị rung lắc, chưa tĩnh): làm gián đoạn chuỗi tĩnh, xóa `quiet = null, count = 0`, observation giữ ở `IMPACT_DETECTED`, ghi log trace `STILLNESS_INTERRUPTED`, trả về `false`.
8. **Đếm mẫu và tích lũy thời lượng bất động:** ([DemoLogic.kt:201-206](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L201-L206))
   - Nếu `quiet == null`: đánh dấu bắt đầu tĩnh `quiet = t`.
   - Tăng số mẫu: `count++`.
   - Tính thời gian tĩnh: `elapsedNs = t - quiet!`.
   - Tính tiến độ: `progress = (elapsedNs.toDouble() / requiredNs).toFloat().coerceIn(0f, 1f)` với `requiredNs = config.postImpactStillnessDurationMs * 1_000_000L` ($1000 ms$).
9. **Xác nhận ngã (Confirm condition) & Đối chứng áp suất (Pressure corroboration):** ([DemoLogic.kt:206-222](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L206-L222))
   - Điều kiện xác nhận: `confirmed = (count >= config.minimumStillnessSamples) && (elapsedNs >= requiredNs)`. (Tối thiểu 6 mẫu và tối thiểu 1000 ms tĩnh).
   - Trạng thái phase: Nếu `confirmed == true` chuyển `DetectionPhase.FALL_CONFIRMED`; nếu chưa đủ chuyển `DetectionPhase.POST_IMPACT_STILLNESS`.
   - Đối chứng áp suất: Nếu `confirmed && config.phonePressureEvidenceEnabled && pressureCapability()`:
     - `pressureCorroborated = (p.pressureWindowDeltaPa ?: -Infinity) >= config.phonePressureMinimumRisePa` ($12.0 Pa$).
     - Ngược lại `pressureCorroborated = null`.
   - Khi `confirmed == true`: gọi `clearEvidence()`, trả về `true`.

#### 2. Các đặc điểm cốt lõi của thuật toán Android:
- **Pha rơi tự do (Free-fall):** **KHÔNG CÓ**. Android hoàn toàn không kiểm tra giảm trọng lực / rơi tự do trước va đập.
- **Sử dụng Gyro trong quyết định:** **KHÔNG DÙNG**. Mặc dù `PhoneSensorPacket` có thu thập `gyroXDps, gyroYDps, gyroZDps` ([DemoLogic.kt:14](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L14)), `DemoDetector` không dùng bất kỳ trục con quay nào trong quyết định.
- **Sử dụng Độ cao (Altitude) trong quyết định:** **KHÔNG DÙNG**. Mặc dù `PhoneNormalizer` có tính `altitudeDeltaM` ([DemoLogic.kt:73-78](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L73-L78)), `DemoDetector` hoàn toàn không dùng `altitudeDeltaM`. Điểm đối chứng duy nhất là `pressureWindowDeltaPa` (chênh lệch áp suất Pa trong cửa sổ 5s), và chỉ dùng để gán cờ `pressureCorroborated` chứ không phủ quyết hay kích hoạt ngã ([DemoLogic.kt:214-216](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L214-L216)).
- **Cơ chế Debounce / Hysteresis / Chống báo trùng:**
  - Cục bộ `DemoDetector`: Khi phát hiện `confirmed == true`, bộ dò gọi ngay `clearEvidence()` ([DemoLogic.kt:220-221](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L220-L221)) xóa sạch mốc `impact, quiet, count`. Mẫu kế tiếp tự động trở về `NORMAL`.
  - Tầng quản lý phiên (`DemoSession.accept`): `if (core.snapshot().state == State.MONITORING && detector.accept(packet))` ([DemoLogic.kt:316](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L316)). Khi ngã được xác nhận, hệ thống gọi `core.suspected()` và `core.evidenceConfirmed()`, đưa trạng thái sang `State.VERIFYING` (đếm ngược 10.000 ms).
  - Tầng `AlertCore` ([Core.kt:73-92](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/core/src/Core.kt#L73-L92)): Chỉ chấp nhận `suspected()` khi `state == State.MONITORING`. Một khi đã vào `VERIFYING`, toàn bộ các mẫu hay sự kiện ngã lặp lại đều bị phớt lờ, không sinh event mới, không restart đếm ngược.

#### 3. Danh sách chính xác các `DetectionPhase` ([DemoLogic.kt:95](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L95)):
1. `DetectionPhase.NORMAL`
2. `DetectionPhase.IMPACT_DETECTED`
3. `DetectionPhase.POST_IMPACT_STILLNESS`
4. `DetectionPhase.FALL_CONFIRMED`

---

### C. Bảng ánh xạ Android → dữ liệu cần → ESP

| # | Android logic (file:dòng) | Dữ liệu/đại lượng cần | Đơn vị Android | Đơn vị ESP đề xuất | Cách tính trên ESP | Ghi chú |
|---|---|---|---|---|---|---|
| 1 | [DemoLogic.kt:144](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L144) | Gia tốc 3 trục $a_x, a_y, a_z$ | $m/s^2$ | $m/s^2$ | Đọc raw 16-bit từ MPU qua I2C, đổi thang đo: $raw \times \frac{FS\_G \times 9.80665}{32768.0}$ | Cần dải đo tối thiểu $\pm 8g$ hoặc $\pm 16g$ để tránh bão hòa lúc va đập. |
| 2 | [DemoLogic.kt:151](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L151) | Tổng gia tốc (Magnitude) | $m/s^2$ | $m/s^2$ | Dùng hàm `magnitude3(ax, ay, az)` có sẵn tại [node_math.cpp:10](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_math.cpp#L10) | Thuật toán chuẩn hóa tỷ lệ tránh tràn số float. |
| 3 | [DemoLogic.kt:14](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L14) | Tốc độ góc 3 trục $g_x, g_y, g_z$ | độ/s (dps) | độ/s (dps) | Đọc raw gyro 16-bit từ MPU, scale: $raw \times \frac{FS\_DPS}{32768.0}$ | Trừ offset bias hiệu chuẩn lúc đứng yên. |
| 4 | [PhoneSensorCollector.kt:56-60](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt#L56-L60) | Góc định hướng / Thay đổi tư thế | độ (pitch, roll, yaw) | độ (pitch, roll, tilt) | Khi tĩnh: $pitch = \arctan2(a_x, \sqrt{a_y^2+a_z^2})$, $roll = \arctan2(a_y, a_z)$; kết hợp tích phân gyro qua bộ lọc bù | Đo góc lệch so với phương thẳng đứng trước và sau va đập. |
| 5 | [DemoLogic.kt:72](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L72) | Áp suất khí quyển tức thời | Pascal (Pa) | Pascal (Pa) | Đọc D1/D2 từ MS5611, tính bù bậc 2 theo PROM ra Pa ([ms5611.h:50](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/ms5611.h#L50)) | Android nhân hPa x 100 ra Pa; MS5611 ra Pa trực tiếp. |
| 6 | [DemoLogic.kt:80](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L80) | Chênh lệch áp suất cửa sổ ($\Delta pressure$) | Pascal (Pa) | Pascal (Pa) | Hàng đợi vòng lưu các mẫu áp suất trong 3-5 giây; $\Delta P = P_{current} - P_{oldest}$ | Rơi xuống đất làm áp suất tăng ($\Delta P > 0$). |
| 7 | [DemoLogic.kt:75](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L75) | Độ cao tương đối ($\Delta altitude$) | mét (m) | mét (m) | Dùng hàm `relativeAltitudeMeters(p, p0, out)` tại [node_math.cpp:26](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_math.cpp#L26) so với baseline khởi động | Dương khi lên cao, âm khi hạ thấp xuống đất. |
| 8 | NOT_FOUND (Android không có) | Pha rơi tự do (Free-fall) | Không có | $m/s^2$ hoặc $g$ | $magnitude < THRESHOLD\_FREE\_FALL$ ($< 0.5g \approx 4.9 m/s^2$) duy trì liên tục trong $80 - 200 ms$ | Dấu hiệu phân biệt va chạm do rơi với va quẹt thông thường. |
| 9 | [DemoLogic.kt:166](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L166) | Pha va đập mạnh (Impact) | $m/s^2$ | $m/s^2$ | $magnitude \ge impactAccelerationMs2$ ($25.0 m/s^2$) | Đỉnh gia tốc tức thời trong chu kỳ 100 Hz. |
| 10 | NOT_FOUND (Android không có) | Đổi tư thế nằm ngang (Posture change) | Không có | độ (deg) | So sánh vector trọng lực trung bình trước va đập và sau khi nằm yên: $\Delta \theta = \arccos(\frac{\vec{g}_{pre} \cdot \vec{g}_{post}}{\|\vec{g}_{pre}\| \|\vec{g}_{post}\|})$ | Ngã thường đi kèm đổi tư thế từ đứng/ngồi sang nằm ($\Delta \theta > 45^\circ - 60^\circ$). |
| 11 | [DemoLogic.kt:192-206](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L192-L206) | Bất động sau va chạm (Stillness) | $m/s^2$, ms, số mẫu | $m/s^2$, ms, số mẫu | $|magnitude - 9.81| \le stillnessToleranceMs2$ liên tục trong $\ge postImpactStillnessDurationMs$ | Ở 100 Hz trên ESP, cần tích lũy số mẫu tĩnh liên tục (ví dụ $\ge 50$ mẫu thay vì 6 mẫu của Android). |
| 12 | [DemoLogic.kt:183](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L183) | Khoảng thời gian giữa các pha | ms | ms (hoặc micro giây) | Dùng đồng hồ đơn điệu `micros()` / `esp_timer_get_time()` để so sánh mốc $t - t_{impact} \le postImpactWindowMs$ | Tuyệt đối không dùng giờ RTC/NTP có thể nhảy giờ. |
| 13 | [FallDetectionProfiles.kt:15-25](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L15-L25) | Bộ tham số cấu hình (Thresholds) | Hỗn hợp | Hỗn hợp | Cấu trúc struct `Esp32Config` lưu trong NVS/RAM nhận từ Android qua HTTP/BLE | Áp dụng cấu hình ngay mà không cần khởi động lại. |
| 14 | [DemoLogic.kt:220-221](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L220-L221) | Cơ chế dập tắt / Debounce / Hysteresis | Không có | ms | Sau khi phát hiện ngã hoặc hủy cảnh báo, áp dụng thời gian câm (refractory period) $3 - 5 s$ trước khi nhận impact mới | Tránh kích hoạt lặp lại sự kiện trong cùng 1 lần ngã. |
| 15 | [DemoLogic.kt:154](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L154) | Chống báo giả do mất mẫu / sensor lỗi | ms | ms | Kiểm tra chu kỳ ngắt cảm biến; nếu mất mẫu $> 50 ms$ hoặc cảm biến bão hòa/lỗi thì reset chuỗi đánh giá | Không điền 0 giả mạo thành rơi tự do ([esp32-plan.md:169](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/esp32-plan.md#L169)). |
| 16 | [DemoLogic.kt:95](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L95) & [Core.kt:43](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/core/src/Core.kt#L43) | Máy trạng thái phát hiện (State machine) | Enum | Enum | Quản lý chuyển trạng thái: `MONITORING` $\rightarrow$ `SUSPECTED` $\rightarrow$ `VERIFYING` $\rightarrow$ `LOCAL_ALERTING` ([esp32-plan.md:204-212](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/esp32-plan.md#L204-L212)) | Bật còi bíp trong `VERIFYING`, còi liên tục trong `LOCAL_ALERTING`. |

---

### D. Khoảng trống cảm biến & thay thế

#### 1. Tín hiệu Android dùng mà TEST_RIG (MPU9250 + MS5611) không có trực tiếp:
- **`LINEAR_ACCELERATION` (Gia tốc tuyến tính đã triệt tiêu trọng lực $g$):**
  - Android sử dụng thuật toán tổng hợp cảm biến ngầm bên trong Android Sensor Framework (kết hợp gia tốc kế, con quay hồi chuyển, từ kế) để ước lượng vector trọng lực và loại trừ nó ra khỏi gia tốc tổng.
  - TEST_RIG chỉ có dữ liệu gia tốc thô 3 trục chứa cả thành phần trọng lực ($1g \approx 9.81 m/s^2$).
- **`ROTATION_VECTOR` / `ORIENTATION` (Góc Euler tuyệt đối pitch, roll, yaw):**
  - Android gọi `SensorManager.getRotationMatrixFromVector()` và `SensorManager.getOrientation()` ([PhoneSensorCollector.kt:54-60](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt#L54-L60)) để tính ra góc nghiêng tuyệt đối theo độ.
  - TEST_RIG chỉ có tốc độ góc thô ($^\circ/s$) từ con quay và gia tốc thô từ gia tốc kế.
- **`PRESSURE` tự động bù và quy đổi của SensorManager:**
  - Android nhận áp suất đơn vị hPa đã được phần cứng điện thoại cân chỉnh hoàn chỉnh.
  - MS5611 trên TEST_RIG xuất ra từ mã ADC 24-bit (D1 cho áp suất, D2 cho nhiệt độ), bắt buộc firmware phải thực hiện tính toán bù bậc 2 theo 6 hằng số hiệu chuẩn PROM ($C_1 \dots C_6$) bằng số thực `double` để thu được Pascal.
- **Tần số và tính tất định (Determinism):**
  - Android chạy ở chế độ `SENSOR_DELAY_GAME` (~50 Hz) trên Main Looper, phụ thuộc vào tiến trình hệ điều hành, có hiện tượng giật cục (jitter) và khoảng trống mẫu đột ngột do Garbage Collection (nên phải dùng `maximumSampleGapMs = 250 ms`).
  - ESP32 vận hành vòng lặp thời gian thực với timer ngắt phần cứng cố định: 100 Hz cho IMU (chu kỳ 10 ms) và 25 Hz cho MS5611 (chu kỳ 40 ms), hoàn toàn không bị trễ hệ điều hành.

#### 2. Phương án thay thế trên ESP32:
- **Suy ra vector trọng lực ($g$):** Khi người đeo ở trạng thái tĩnh (magnitude xấp xỉ $9.81 m/s^2$ và tốc độ góc nhỏ), hướng của vector gia tốc 3 trục chính là hướng của vector trọng lực Trái Đất.
- **Ước lượng góc nghiêng và thay đổi tư thế:**
  - Sử dụng bộ lọc bù (Complementary Filter) nhẹ: $\theta = \alpha \times (\theta + \omega \cdot \Delta t) + (1 - \alpha) \times \theta_{accel}$.
  - Do thiết bị gắn ở thắt lưng (cố định vào cơ thể tốt hơn điện thoại trong túi), chỉ cần xác định góc lệch của trục dọc cơ thể trước sự kiện và sau khi nằm yên để phát hiện tư thế ngã.
- **Chuẩn hóa trục:** Quy ước hệ trục thiết bị đeo thắt lưng theo quy tắc bàn tay phải ([esp32-plan.md:165](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/esp32-plan.md#L165)), gắn nhãn rõ trên vỏ hộp; không cố ghép trực tiếp từng trục với điện thoại vì hướng đặt khác nhau.

#### 3. Tác động tới thuật toán:
- **Ưu điểm:** Tần số 100 Hz chuẩn giúp bắt trọn vẹn đỉnh va đập tức thời ($peakAccelerationMs2$) mà không bị hiện tượng lướt đỉnh (undersampling) như ở tần số thấp của điện thoại.
- **Mất mát & Rủi ro:** Không có hệ điều hành tính sẵn tư thế toàn cầu; nếu tích phân con quay đơn thuần trong thời gian dài sẽ bị trôi (gyro drift).
- **Cách bù trừ:** Chỉ tính góc xoay và tích phân tốc độ góc trong cửa sổ ngắn (cửa sổ trượt 1 đến 3 giây xung quanh va đập); khi bất động, sử dụng trực tiếp gia tốc kế để định vị lại phương thẳng đứng.

#### 4. Vấn đề sử dụng từ kế AK8963 trên MPU9250:
- **Có nên dùng cho bài toán phát hiện ngã không?** **KHÔNG NÊN DÙNG**.
- **Lý do kỹ thuật:**
  1. *Nhiễu từ trường cực lớn:* Thiết bị đeo hoạt động trong môi trường trong nhà (gần khung sắt, sàn bê tông cốt thép, thiết bị gia dụng) và ngay trên bo mạch có còi buzzer nam châm, cuộn cảm nguồn xung làm méo từ trường nghiêm trọng (hard-iron / soft-iron distortion), khiến dữ liệu từ kế sai lệch nặng nếu không liên tục hiệu chuẩn 8 chữ số.
  2. *Không cần thiết cho phát hiện ngã:* Việc nhận diện ngã, va đập, rơi tự do và nằm yên chỉ phụ thuộc vào độ lớn gia tốc, tốc độ góc và phương của trọng lực Trái Đất (được cung cấp đầy đủ bởi 6 trục Accel + Gyro). Từ kế chỉ có vai trò xác định hướng bắc la bàn (Yaw tuyệt đối), không đóng góp vào việc phân biệt ngã và sinh hoạt thường ngày.
  3. *Tắc nghẽn bus I2C:* Đọc AK8963 qua chế độ I2C Master nội bộ của MPU9250 làm phức tạp hóa máy trạng thái giao tiếp I2C, tăng thời gian chiếm dụng bus và có nguy cơ làm rớt tần số 100 Hz của luồng IMU chính.
- **Nếu dùng trong tương lai:** Chỉ dùng khi cần xác định hướng di chuyển của người dùng sau khi đã ổn định an toàn, và chỉ đọc ở tần số rất thấp (1 - 5 Hz) tách biệt hoàn toàn khỏi luồng phát hiện ngã.

---

### E. Ranh giới hợp đồng

Firmware ESP32 sau này bắt buộc phải giữ đúng tên trường, kiểu dữ liệu và đơn vị theo hợp đồng hiện hành:

#### 1. Các trường trong `POST /api/v1/sensors/ingest` ([docs/esp32-api.md:108-126](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/docs/esp32-api.md#L108-L126)) và `Esp32SensorPacket` ([Esp32PacketDecoder.kt:57-64](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/protocol/Esp32PacketDecoder.kt#L57-L64)):
- `protocolVersion`: integer (`1`).
- `deviceId`: string (ví dụ `"FALLSAFE-01A2"`).
- `sequenceNumber`: integer tăng dần liên tục (32-bit/64-bit).
- `timestampMs`: integer epoch ms hoặc monotonic ms.
- `accelXMs2`, `accelYMs2`, `accelZMs2`: float ($m/s^2$, có trọng lực $9.81 m/s^2$).
- `gyroXDps`, `gyroYDps`, `gyroZDps`: float ($^\circ/s$ hay dps; nếu lỗi/không có phải gửi `null` cả 3, không được gửi 0 thay cho thiếu).
- `pressurePa`: float hoặc `null` (Pascal).
- `temperatureC`: float hoặc `null` ($^\circ C$).
- `altitudeDeltaM`: float hoặc `null` (mét, tính từ mốc baseline áp suất).
- `batteryPercent`: integer ($0 \dots 100$).
- `batteryVoltageMv`: integer mV hoặc `null`.
- `isCharging`: boolean (`true`/`false`).
- `sosButtonPressed`: boolean (`true`/`false`).
- `sensorQuality`: integer ($0 \dots 100$).

#### 2. Các trường trong `POST /api/v1/events` ([docs/esp32-api.md:155-172](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/docs/esp32-api.md#L155-L172)) và `Esp32EventPacket` ([esp32-plan.md:309-327](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/esp32-plan.md#L309-L327)):
- `eventId`: string (duy nhất, ví dụ `"evt-esp-18423"`).
- `eventType`: string thuộc danh sách đóng:
  - `"IMPACT_DETECTED"`
  - `"FREE_FALL_SUSPECTED"`
  - `"POSTURE_CHANGED"`
  - `"INSTABILITY_DETECTED"`
  - `"INACTIVITY_DETECTED"`
  - `"SOS_PRESSED"`
  - `"SOS_CANCELLED"`
  - `"LOW_BATTERY"`
  - `"SENSOR_ERROR"`
- `severity` (hoặc `eventSeverity`): string (`"INFO"`, `"WARNING"`, `"CRITICAL"`).
- `alertState`: string (`"MONITORING"`, `"SUSPECTED"`, `"VERIFYING"`, `"ALERTING"`, `"AWAITING_HELP"`).
- `peakAccelerationMs2`: float ($m/s^2$) hoặc `null`.
- `orientationChangeDeg`: float ($^\circ$) hoặc `null`.
- `altitudeDeltaM`: float (mét) hoặc `null`.
- `sosButtonPressed`: boolean.
- `confidencePercent` (hoặc `eventConfidence`): integer ($0 \dots 100$).

#### 3. Các trường trong `Esp32Config` Schema v1 ([docs/esp32-config-contract.md:63-78](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/docs/esp32-config-contract.md#L63-L78) & [Models.kt:11-25](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/espconfig/Models.kt#L11-L25)):
- `impactAccelerationMs2`: Double ($m/s^2$)
- `stillnessTargetAccelerationMs2`: Double ($m/s^2$)
- `stillnessToleranceMs2`: Double ($m/s^2$)
- `postImpactWindowMs`: Long (ms)
- `postImpactStillnessDurationMs`: Long (ms)
- `minimumStillnessSamples`: Int (mẫu)
- `maximumSampleGapMs`: Long (ms)
- `pressureEvidenceEnabled`: Boolean
- `pressureMinimumRisePa`: Double (Pa)
- `pressureWindowMs`: Long (ms)
- `pressureMinimumSamples`: Int (mẫu)
- `pressureFilterAlpha`: Double ($0.01 \dots 1.0$)
- `pressureStaleAfterMs`: Long (ms)

---

### F. Profile thử nghiệm khởi điểm cho TEST_RIG

> **CẢNH BÁO:** Toàn bộ các giá trị dưới đây là **NGƯỠNG THỬ NGHIỆM KHỞI ĐIỂM (EXPERIMENTAL)**, phục vụ kiểm thử kỹ thuật và thu thập dataset, **CHƯA ĐƯỢC KIỂM CHỨNG Y KHOA**.
> Quy đổi chuẩn trọng trường: $1g = 9.80665 m/s^2 \approx 9.81 m/s^2$.

| Tham số | Giá trị Android (file:dòng) | Giá trị đề xuất cho ESP | Đơn vị | Nguồn | Lý do kỹ thuật |
|---|---|---|---|---|---|
| free-fall | `NOT_FOUND` (Android không có) | `0.5` $g$ ($\approx 4.90 m/s^2$) trong $\ge 80 ms$ | $g$ ($m/s^2$) & ms | Suy luận kỹ thuật | Người rơi tự do làm gia tốc tổng giảm mạnh trước khi chạm sàn; ngưỡng $0.5g$ trong $\ge 80 ms$ giúp lọc nhiễu rung lắc nhẹ. |
| impact | `25.0f` ([FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56)) | `25.0` ($\approx 2.55 g$) | $m/s^2$ | Kế thừa từ Android | Giữ đồng nhất với logic Android. **Lưu ý phần cứng:** Cần chỉnh full-scale MPU9250 lên $\pm 8g$ hoặc $\pm 16g$, vì cấu hình mặc định $\pm 2g$ trong `node_config.h:42` sẽ bão hòa ở $19.62 m/s^2$ và không bao giờ chạm tới $25 m/s^2$. |
| gyro (tốc độ góc ngã) | `NOT_FOUND` (Android không dùng trong quyết định) | `120.0` | độ/s (dps) | Suy luận kỹ thuật | Cơ thể khi mất thăng bằng ngã xuống sẽ có chuyển động xoay thân; tốc độ góc tổng $> 120^\circ/s$ trong cửa sổ va đập giúp loại trừ va quẹt tĩnh. |
| $\Delta altitude$ (độ cao) | `NOT_FOUND` (Android không dùng trong quyết định) | `-0.40` | m | Suy luận kỹ thuật | Thắt lưng người lớn khi đứng/ngồi cao khoảng $0.8 - 1.0 m$; khi ngã xuống sàn độ cao giảm tối thiểu $0.4 - 0.6 m$. |
| postImpactWindowMs | `3000L` ([FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56)) | `3000` | ms | Kế thừa từ Android | Cửa sổ 3 giây là khoảng thời gian sinh lý đủ để một người sau cú va đập rơi vào trạng thái nằm bất động. |
| postImpactStillnessDurationMs | `1000L` ([FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56)) | `1000` | ms | Kế thừa từ Android | Yêu cầu nằm yên tối thiểu 1 giây liên tục để loại trừ trường hợp người lập tức đứng dậy đi tiếp. |
| minimumStillnessSamples | `6` ([FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56)) | `50` | mẫu | Suy luận kỹ thuật | Android lấy mẫu thưa/bị delay nên chỉ cần 6 mẫu; trên ESP32 với tốc độ 100 Hz, trong 1000 ms có 100 mẫu, do đó cần ít nhất 50 mẫu tĩnh liên tục để bảo đảm độ tin cậy. |
| maximumSampleGapMs | `250L` ([FallDetectionProfiles.kt:56](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L56)) | `50` | ms | Suy luận kỹ thuật | Chu kỳ lấy mẫu của MPU trên ESP là 10 ms (100 Hz); mất mẫu quá 50 ms tương đương mất 5 mẫu liên tiếp, chứng tỏ cảm biến bị treo hoặc bus I2C kẹt. |
| pressureMinimumRisePa | `12.0f` ([FallDetectionProfiles.kt:54](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt#L54)) | `8.0` | Pa | Kế thừa / Suy luận kỹ thuật | $12 Pa \approx 1 m$ độ cao ở mực nước biển. Ngã từ độ cao thắt lưng (~0.6 - 0.8 m) tương ứng mức tăng áp suất khoảng $7 - 9 Pa$. Đặt khởi điểm $8.0 Pa$ (trong dải hợp lệ $1 \dots 200 Pa$). |
| pressureWindowMs | `5000000000L` ns ([DemoLogic.kt:89](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt#L89)) | `3000` | ms | Kế thừa từ `Esp32Config` schema v1 | Khớp với `pressureWindowMs` chuẩn trong hợp đồng ESP32 ([esp32-config-contract.md:74](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/docs/esp32-config-contract.md#L74)). |
| pressureMinimumSamples | `NOT_FOUND` (Android chỉ có `MAX_PRESSURE_SAMPLES = 64`) | `5` | mẫu | Kế thừa từ `Esp32Config` schema v1 | Khớp với `pressureMinimumSamples` trong hợp đồng ESP32 ([esp32-config-contract.md:75](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/docs/esp32-config-contract.md#L75)). |

---

### G. Phần ESP tái dùng được

#### 1. Các module C++17 thuần tái dùng được nguyên vẹn (100% không cần sửa):
- **`node_math.h` và `node_math.cpp`** ([esp/node/firmware/esp_node/node_math.h](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_math.h)):
  - Hàm `magnitude3(x, y, z)`: Thuật toán chuẩn hóa vector float tránh tràn số, dùng trực tiếp cho gia tốc và tốc độ góc.
  - Hàm `relativeAltitudeMeters(pressure_pa, baseline_pa, out)`: Công thức barometric độ cao tương đối chuẩn quốc tế ($44330.77 \times (1 - (P/P_0)^{0.190263})$).
  - Lớp `BaroBaseline`: Máy thu thập và tính trung vị (median) áp suất khởi động đóng băng mốc độ cao 0m.
- **`ms5611.h` và `ms5611.cpp`** ([esp/node/firmware/esp_node/ms5611.h](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/ms5611.h)):
  - Driver MS5611 máy trạng thái non-blocking, đọc PROM, kiểm tra CRC-4, chuyển đổi OSR 4096 chu kỳ 25 Hz mà không làm nghẽn vòng lặp. Cả TARGET_PRODUCT và TEST_RIG đều dùng GY-63 (MS5611-01BA03).
- **`csv_row.h` và `csv_row.cpp`** ([esp/node/firmware/esp_node/csv_row.h](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/csv_row.h)):
  - Định dạng chuẩn các cột CSV phục vụ ghi log serial thu thập dataset và kiểm thử host.
- **`i2c_bus.h`** ([esp/node/firmware/esp_node/i2c_bus.h](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/i2c_bus.h)):
  - Interface trừu tượng cho tầng giao tiếp I2C phần cứng.

#### 2. Phần bắt buộc phải thay thế cho TEST_RIG (ESP32-WROOM-32 + MPU9250):
- **Thay thế Driver IMU (`mpu6050.h/.cpp` $\rightarrow$ `mpu9250.h/.cpp`):**
  - *Mã định danh WhoAmI:* MPU6050 thanh ghi `0x75` trả về `0x68` ([mpu6050.h:36](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_config.h#L36)), trong khi MPU9250 trả về `0x71` hoặc `0x73`. Driver cũ sẽ báo lỗi `kWhoAmIMismatch` và dừng hoạt động nếu không sửa.
  - *Dải đo gia tốc (AFS_SEL):* Driver cũ đặt `kAccelFsSel = 0` ($\pm 2g$) ([node_config.h:42](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_config.h#L42)). Với ngưỡng va đập $25 m/s^2$ ($\approx 2.55 g$), bắt buộc phải cấu hình lại thanh ghi `ACCEL_CONFIG` lên AFS_SEL = 2 ($\pm 8g$) hoặc AFS_SEL = 3 ($\pm 16g$), đồng thời cập nhật hệ số tỷ lệ chia raw tương ứng.
  - *Từ kế nội bộ AK8963:* Cần cấu hình thanh ghi `USER_CTRL` hoặc `BYPASS_EN` của MPU9250 để vô hiệu hóa/bỏ qua khối từ kế, tránh treo I2C aux bus.
- **Thay đổi cấu hình chân I2C (`node_config.h`):**
  - Trong `node_config.h` của ESP32-S3 Super Mini đang dùng: Bus 0 (SDA=GPIO7, SCL=GPIO6) và Bus 1 (SDA=GPIO3, SCL=GPIO2) ([node_config.h:24-28](file:///Users/phananh/TEMP/nckh27pa/nckh27pa/esp/node/firmware/esp_node/node_config.h#L24-L28)).
  - **LÝ DO NGUY HIỂM:** Trên ESP32-WROOM-32, các chân GPIO 6, 7, 8, 9, 10, 11 được nối trực tiếp vào chip SPI Flash tích hợp bên trong module. Nếu gán GPIO 6 và 7 làm I2C trên WROOM-32, chip sẽ lập tức gặp lỗi bộ nhớ (Crash / Panic / Reboot loop) ngay khi khởi động! Ngoài ra GPIO 2 là chân Strapping bootloader và GPIO 3 là RX0.
  - **Phương án cho TEST_RIG WROOM-32:** Phải chuyển sang các GPIO an toàn:
    - *Tùy chọn 1 (Chung 1 bus I2C):* Vì MPU9250 có địa chỉ I2C `0x68` và MS5611 có địa chỉ `0x77` (không hề xung đột địa chỉ), có thể dùng chung Bus 0 với cặp chân I2C mặc định của WROOM-32: `SDA = GPIO21`, `SCL = GPIO22`.
    - *Tùy chọn 2 (Hai bus I2C độc lập):* Bus 0 (MPU9250) dùng `SDA = GPIO21`, `SCL = GPIO22`; Bus 1 (MS5611) dùng `SDA = GPIO18` (hoặc 25), `SCL = GPIO19` (hoặc 26).
