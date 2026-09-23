# T13 — Khu vực "DỮ LIỆU CẢM BIẾN" trong thẻ Settings (báo cáo)

STATUS: DONE (đã xác minh trên máy thật cho mọi bước chạy tự động được; bước xoay/lắc bằng tay còn CHỜ người dùng — §6.4).
OWNER (implementation): AGY CLI `gemini-3.8-flash-high` — 1 worker duy nhất (Level 1: simple UI code generation), 1 lượt, ~6 phút.
  Codex CLI (Level 2 theo AGENTS.md) vẫn hết quota tới 2026-09-22 → chọn worker thay thế hợp lệ, không tự "take over" code.
OWNER (verification/report): Hermes (project manager) — tự chạy Gradle, tự cài APK, tự đọc UI dump/logcat.
FILES: xem §2. GOAL: đưa các giá trị cảm biến điện thoại đang chảy vào FALL-01 lên UI Settings, không đổi hành vi.
NEXT_ACTION: người dùng chạy §6.4 (30 giây) để đóng 3 tiêu chí còn thiếu; sau đó mới bàn threshold (xem §7 — phát hiện quan trọng).

Mọi số liệu dưới đây do Hermes đọc trực tiếp từ artifact/thiết bị, không lấy từ báo cáo của worker.

---

## 1. Implementation cũ tìm thấy trong git history

Lệnh đã dùng (đúng như ticket): `rg`, `git log --oneline -- android/`, `git log -S/-G`, `git show <commit>:<path>`.

- `git log -S "Accelerometer" -- android/` → **không có commit nào**.
- `git log -S "Gyroscope" -- android/` → `b40ecf3`.
- `git log -G "ax|ay|az" -- android/` → cả 6 commit (`2e7bc99 b40ecf3 44ef59d 184d70a 2b0eacc 8761541`).
- `git log -S "cảm biến" / "gia tốc" / "m/s"` → `b40ecf3`, `2e7bc99`.

**Phiên bản UI từng hiển thị sensor values inline trong tab Settings:**

| Phiên bản | Nội dung | Truy vết |
| :-- | :-- | :-- |
| `2e7bc99` (Initial commit) | `MainActivity.kt` dòng 331–338: `"Cảm biến đã đăng ký: …"` + `"Gia tốc m/s²: {x}, {y}, {z}"` + `"Gyro °/s: …"` — panel chi tiết đặt ngay trong màn Settings, đọc từ `controller.packet` | `git show 2e7bc99:android/.../MainActivity.kt` |
| `b40ecf3` → `8761541` | panel bị rút dần; đến HEAD chỉ còn 1 dòng `"Cảm biến đã đăng ký: …"` (`MainActivity.kt:412`), phần hiển thị giá trị từng trục được chuyển vào màn Hiệu chỉnh | `git show <c>:.../MainActivity.kt`, `git grep` |
| HEAD (8761541) | `FallDetectionCalibrationScreen.kt` §`"DỮ LIỆU CẢM BIẾN & TRẠNG THÁI PHÁT HIỆN"` (dòng 648–752): `Gia tốc X / Y / Z` (2 chữ số thập phân), `|a|`, ngưỡng va chạm, giai đoạn, snapshot ~4 Hz | file hiện tại |

**File cũ đã tham khảo để reuse (không thiết kế lại pipeline):**

- `MainActivity.kt@2e7bc99` — ý tưởng hiển thị trực tiếp giá trị trục trong thẻ Settings.
- `FallDetectionCalibrationScreen.kt@HEAD` — **mẫu trình bày thực sự được reuse**: `takeSensorUiSnapshot(controller.packet, controller.observation)` (dòng 337–364), vòng throttle `while (isActive) { …; delay(250L) }` (dòng 404–412), `detectionPhaseVietnamese()` (dòng 300–305), cách format `String.format(Locale.US, "%.2f", …)` (dòng 700–709).
- Không dùng lại: `DemoReplay` (đường replay) và bất kỳ listener cảm biến riêng nào — xem §4.

## 2. File đã sửa/tạo

| File | Loại | Thay đổi |
| :-- | :-- | :-- |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/SettingsSensorDiagnostics.kt` | **MỚI** (83 dòng) | Hàm thuần `settingsSensorDiagnostics(packet, observation, sensorsAvailable)`, `SensorDataState {LIVE, LOST, UNSUPPORTED}`, `formatDiagnosticNumber`, `sensorDataStateVietnamese`. Không import Android/sensor → test được trên JVM |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` | sửa (+84 / −2) | Thêm `SensorDiagnosticsSection` + `DiagnosticAxisRow`; gọi ở cuối `SettingsScreen`; hằng `NO_SENSOR_SUMMARY` thay 2 literal (hành vi y hệt) |
| `android/app/src/test/java/vn/nckh27pa/fallsafe/SettingsSensorDiagnosticsTest.kt` | **MỚI** (108 dòng) | 6 test: trạng thái LIVE/LOST/UNSUPPORTED, quy đổi deg/s→rad/s, |a|, |ω|, format, nhãn tiếng Việt, phase |

Không sửa: `FallDetectionProfiles.kt`, `espconfig/Models.kt`, `core/src/Core.kt`, `MonitoringService.kt`, `PhoneSensorCollector.kt`(T13), `DemoLogic.kt`(T13), `FallDetectionTrace*.kt`, `FallDetectionCalibrationScreen.kt`.

## 3. UI hiển thị gì

Trong thẻ **Cài đặt** (tab thứ 4), ngay sau dòng "Cảm biến đã đăng ký", một card mới:

```
DỮ LIỆU CẢM BIẾN
Gia tốc kế   X / Y / Z / Độ lớn   (2 chữ số thập phân, m/s²)
Con quay hồi chuyển  X / Y / Z / Độ lớn   (3 chữ số thập phân, rad/s)
Nguồn cảm biến: PHONE
Trạng thái dữ liệu: Đang nhận dữ liệu (LIVE) | Mất dữ liệu (LOST) | Không hỗ trợ (UNSUPPORTED)
FALL state: <DetectionPhase.name> · <nhãn tiếng Việt>
Hệ thống: <core.State.name>
```

Bám đúng yêu cầu §3 của ticket:

- nằm trong Settings card/page hiện có ✔
- realtime ~4 Hz ✔ (throttle 250 ms, giống màn Hiệu chỉnh)
- **không nhảy layout**: mỗi hàng là `Row` 2 con cố định (label `weight(0.35)`, value `weight(0.65)` + `TextAlign.End`), số đổi trong ô có kích thước cố định — chứng minh định lượng ở §6.3
- format 2 chữ số (gia tốc) / 3 chữ số (gyro rad/s), label rõ từng trục, không đồ thị ✔

## 4. UI nhận dữ liệu từ pipeline hiện tại (không có listener mới)

```
PhoneSensorCollector.onSensorChanged            (SENSOR_DELAY_GAME, không đổi)
  └─ PhoneInputPipeline.update()  → PhoneNormalizer.update()        (một instance duy nhất)
       ├─ mỗi sự kiện ACCEL: onPacket(latest())  → DemoController.acceptPhone → DemoInputAdapter.acceptPhone
       │                                            ├─ display(packet)      → controller.packet
       │                                            ├─ DemoSession.accept → DemoDetector.accept   ← FALL-01
       │                                            └─ Fall01Trace.sample(...)                   ← FALL01_SAMPLE
       └─ polling 250 ms (MainActivity.poll khi foreground, MonitoringService.poll khi nền):
            controller.displayPhone(collector.latest()) → controller.packet
```

`SensorDiagnosticsSection` **chỉ đọc** `controller.packet` và `controller.observation` (cả hai là `mutableStateOf` trong `DemoApplication.kt`) — đúng hai giá trị mà màn Hiệu chỉnh đang dùng. Không có `SensorManager.registerListener` mới, không có collector thứ hai, không đổi quy tắc freshness 500 ms trong `PhoneNormalizer.packet`.

Độ trung thực của phép so sánh (nói rõ, không tô hồng): UI đọc **cùng một normalizer** sinh ra packet cho detector, nhưng qua nhánh polling 250 ms, nên mẫu đang hiển thị có thể lệch tối đa 1 chu kỳ poll so với mẫu detector vừa "ăn". Bằng chứng định lượng ở §6.5 cho thấy 2203/2203 mẫu trong log đều `observed=true`, tức mọi mẫu được hiển thị đều nằm trong luồng detector thực sự tiêu thụ.

## 5. Kết quả Gradle (Hermes tự chạy)

```
$ cd android && ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon --max-workers=2
BUILD SUCCESSFUL in 18s    (45 actionable tasks: 45 executed)
```

- Đọc từ `app/build/test-results/testDebugUnitTest/TEST-*.xml`: **31 suite / 212 test / 0 failure / 0 error / 0 skip**.
- Baseline trước task: 30 suite / 206 test → T13 **thêm** 1 suite + 6 test, không xoá/không làm yếu test nào.
- 6 test mới đều xanh: `SettingsSensorDiagnosticsTest` (6/0/0).
- APK: `app/build/outputs/apk/debug/app-debug.apk`, 12.599.616 B,
  sha256 `86c1612a65c038006541446850fb47a9fc962db5d914ffe20616fa705bb2a9b4`.
- JDK 17 (Temurin 17.0.20.1) — đúng cảnh báo "không dùng Java 25 cho Gradle" trong `task_on_progress.md`.

## 6. Acceptance trên máy thật

Thiết bị: `2201117SG` (Xiaomi Redmi Note 11), USB `adb`, màn hình 1080×2400 @440dpi, `ro.kernel.qemu` ≠ 1 (không phải emulator).

```
$ adb install -r app/build/outputs/apk/debug/app-debug.apk
Performing Streamed Install
Success
```

### 6.1 Bước 1 — "điện thoại để yên" (ĐẠT)

`adb logcat -d -v threadtime FALL01:V '*:S'` → **2203 mẫu** `FALL01_SAMPLE` (≈44 s ở 50 Hz), thống kê theo từng trục:

| trục | mean | stdev | min | max |
| :-- | --: | --: | --: | --: |
| ax | 0.301 | 0.0068 | 0.243 | 0.356 |
| ay | 0.388 | 0.0067 | 0.326 | 0.482 |
| az | 10.034 | 0.0170 | 9.967 | 10.087 |
| \|a\| | 10.046 | 0.0170 | 9.980 | 10.099 |
| gx (°/s) | 0.002 | 0.051 | −0.481 | 0.315 |
| gy (°/s) | −0.003 | 0.315 | −3.395 | 2.584 |
| gz (°/s) | 0.001 | 0.038 | −0.449 | 0.228 |
| \|ω\| (°/s) | 0.182 | 0.264 | 0.026 | 3.431 |

Diễn giải: vector tổng phản ánh trọng lực đúng theo hướng cầm máy (Z ≈ 10.03, X/Y ≈ 0); gyro trung bình ≈ 0 (|ω| trung bình 0.182 °/s = **0.0032 rad/s**, đúng như UI hiển thị `0.000–0.002 rad/s`).

Ghi nhận cho task tuning sau (không phải lỗi T13): **gia tốc kế của máy này đọc ≈ 10.05 m/s² khi đứng yên**, không phải 9.81 (+2.4 %). Vì vậy tiêu chí "`|a|` gần 9.8" chỉ đúng trong khoảng ~2.5 % trên thiết bị cụ thể này.

Nguồn số liệu: `docs/evidence/t13-settings-sensor/logcat-fall01-raw.txt`.

### 6.2 Bước 4 — UI hiển thị (ĐẠT)

`adb shell uiautomator dump` (XML trong `docs/evidence/t13-settings-sensor/`), các node thực tế trên màn hình:

```
DỮ LIỆU CẢM BIẾN
Gia tốc kế            X 0.30 m/s² | Y 0.39 m/s² | Z 10.06 m/s² | Độ lớn 10.07 m/s²
Con quay hồi chuyển   X 0.000 rad/s | Y 0.001 rad/s | Z -0.001 rad/s | Độ lớn 0.001 rad/s
Nguồn cảm biến: PHONE
Trạng thái dữ liệu: Đang nhận dữ liệu (LIVE)
FALL state: IMPACT_DETECTED · Phát hiện va chạm
Hệ thống: MONITORING
```

→ `Nguồn: PHONE` ✔, `Dữ liệu: LIVE` ✔. Giá trị trên UI **trùng** mẫu log tại cùng thời điểm
(`log FALL01_SAMPLE … ax=0.301,ay=0.388,az=10.039,amag=10.051` ↔ UI `0.30 / 0.39 / 10.06 / 10.07`) — tức màn hình đang hiển thị đúng dữ liệu đang chảy vào FALL-01.

Ảnh chụp: `docs/evidence/t13-settings-sensor/screen-settings-sensor-diagnostics.png` (mở bằng Preview/Quick Look).

### 6.3 "realtime + không nhảy layout" (ĐẠT — đo được)

Hai lần dump UI cách nhau **8 giây** (`ui-live-t0.xml`, `ui-live-t1.xml`):

| | t0 | t1 |
| :-- | --: | --: |
| ax | 1.44 | 0.00 |
| ay | 0.41 | 0.24 |
| az | 9.98 | 10.04 |
| \|a\| | 10.09 | 10.04 |
| gx | 0.001 | 0.000 |
| \|ω\| | 0.002 | 0.001 |
| Trạng thái | LIVE | LIVE |

Giá trị **đổi** giữa hai lần dump ⇒ realtime ✔. Đồng thời **64/64 node có toạ độ bounds giống hệt nhau** ở cả hai lần dump (`IDENTICAL LAYOUT: True`) ⇒ trang không nhảy layout ✔.

### 6.4 Bước 2–3 — xoay máy / lắc nhẹ (**CHƯA XÁC MINH**)

Không thể tự thực hiện: cần tay người tác động vật lý lên máy. Đã hỏi người dùng qua prompt (`clarify`) nhưng hết thời gian chờ, không có phản hồi; sau đó màn hình máy tự khoá lại và `adb shell wm dismiss-keyguard` không mở được nữa (khoá bằng credential — Hermes không nhập mã khoá). **Không ghi nhận là đã kiểm chứng.**

Bằng chứng gián tiếp đã có: kênh gyro **đang sống**, không bị đóng băng (`gx` biến thiên −0.481…0.315 °/s, `|ω|` 0.026…3.431 °/s qua 2203 mẫu; UI cũng nhảy 0.000→0.002 rad/s giữa hai lần dump).

Lệnh chạy 30 giây để đóng 3 tiêu chí (người dùng thực hiện, app đang mở ở tab Cài đặt / cuộn tới card DỮ LIỆU CẢM BIẾN):

```bash
adb shell wm dismiss-keyguard; adb shell am start -n vn.nckh27pa.fallsafe/.MainActivity
# 1) giữ yên ~10 s, 2) xoay/nghiêng máy qua lại ~10 s, 3) lắc nhẹ ~5 s
adb logcat -d -v threadtime FALL01:V '*:S' > docs/evidence/t13-settings-sensor/fall01-motion.log
grep -c FALL01_SAMPLE docs/evidence/t13-settings-sensor/fall01-motion.log
```

Kỳ vọng: khi xoay, `gx/gy/gz` (và |ω|) tăng rõ so với mức 0.003–0.18 °/s lúc đứng yên; khi lắc, `ax/ay/az` và |a| vượt xa dải 9.98–10.10 m/s² lúc đứng yên.

### 6.5 Bước 5 — logcat vẫn có `FALL01_SAMPLE` (ĐẠT)

Cùng buffer ở §6.1: **2203 dòng `FALL01_SAMPLE`**, tất cả `observed=true` (0 dòng `observed=false`), `alert=MONITORING`, `phase=IMPACT_DETECTED`, `profileNumber=1`, `dtMs=20` (50 Hz không đổi). Instrumentation của T12 không bị ảnh hưởng.

## 7. Phát hiện ngoài phạm vi T13 (chỉ báo cáo, KHÔNG sửa)

UI mới cho thấy ngay một vấn đề thật của dữ liệu, không phải của UI:

- `FALL state: IMPACT_DETECTED` **thường trực** và log ghi `impactOver=true` trong khi `|a| ≈ 10.05`.
- Profile đang dùng (`profileNumber=1`, `isActive=true`, `profileRevision=2`, đọc từ `shared_prefs/fallsafe_detection_profiles_v1_user-01.xml`) có **`impactAccelerationMs2 = 4.8 m/s²`** — thấp hơn cả gia tốc trọng lực máy đọc lúc đứng yên (≈10.05).
- Hệ quả: mọi mẫu đều "vượt ngưỡng va chạm"; cờ va chạm mất khả năng phân biệt, trạng thái detector không bao giờ trở về NORMAL.

Đây chính là loại vấn đề mà khu vực chẩn đoán này sinh ra để lộ ra. **T13 không đổi threshold** — việc chọn giá trị mới thuộc task tuning sau, khi có log xoay/lắc (§6.4) và log té thật.

## 8. Xác nhận bắt buộc

```
THRESHOLD_CHANGED      = NO
DETECTION_LOGIC_CHANGED = NO
SAMPLING_CHANGED       = NO
```

Bằng chứng (chạy trên cây làm việc sau khi worker dừng):

- `git diff --name-only` → chỉ 3 file: `MainActivity.kt` (T13), `DemoLogic.kt`, `PhoneSensorCollector.kt`.
  Hai file sau đã ở trạng thái modified **từ trước khi T13 bắt đầu** (ghi nhận ở `git status --short` đầu phiên) và khớp đúng mô tả thay đổi của T12 (+44/−8 instrumentation reflow; +2 dòng session trace) — T13 không chạm vào.
- `FallDetectionProfiles.kt`, `espconfig/Models.kt`, `core/src/Core.kt` **không nằm trong** `git diff --name-only` ⇒ ngưỡng/profile không đổi.
- Toàn bộ dòng bị xoá trên cả cây: 10 dòng (MainActivity 2 = thay literal bằng hằng `NO_SENSOR_SUMMARY`, hành vi y hệt; DemoLogic 8 = reflow instrumentation của T12). Không dòng nào chứa giá trị ngưỡng, không dòng nào chứa tốc độ lấy mẫu.
- `SensorManager.SENSOR_DELAY_GAME`, cửa sổ freshness `500_000_000` ns, `maximumSampleGapMs`, `postImpactWindowMs`, `postImpactStillnessDurationMs`, `minimumStillnessSamples`, `stillnessTargetAccelerationMs2`, `stillnessToleranceMs2`, `impactAccelerationMs2`: không xuất hiện ở bất kỳ dòng thêm/xoá nào của T13.
- Không sửa `FallDetectionCalibrationScreen.kt` (màn hiệu chỉnh ngưỡng) nên đường chỉnh threshold vẫn nguyên trạng.

Chưa chạy threshold tuning — đúng yêu cầu.

## 9. Bằng chứng kèm theo

`docs/evidence/t13-settings-sensor/`

| File | Nội dung |
| :-- | :-- |
| `logcat-fall01-raw.txt` | 2203 mẫu `FALL01_SAMPLE` (44 s đứng yên) — dùng cho §6.1 |
| `logcat-fall01-samples-sample.txt` | 3 dòng đầu trích ra cho dễ đọc |
| `ui-section-full.xml` | UI dump đầy đủ card DỮ LIỆU CẢM BIẾN (§6.2) |
| `ui-settings-top.xml` | UI dump phần đầu màn Cài đặt |
| `ui-live-t0.xml`, `ui-live-t1.xml` | hai dump cách 8 s cho phép đo realtime + layout (§6.3) |
| `screen-settings-sensor-diagnostics.png` | ảnh chụp màn hình |
| `.ai/T13-settings-sensor-diagnostics-ticket.md` | phiếu việc đã phát cho worker |

Thiết bị đã được trả về trạng thái ban đầu: `svc power stayon false` (đã bật tạm trong lúc đo rồi tắt lại).
