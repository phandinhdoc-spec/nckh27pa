# FALL-01 — Instrumentation real-device (T12)

Trạng thái: **triển khai xong, build/test xanh, đã DỪNG trước bước tuning threshold.**
Chưa có log thực nghiệm FALL01-BED nào trong repo (sẽ do chủ dự án chạy trên điện thoại thật).

## 1. HEAD ghi nhận trước khi sửa

```
$ git rev-parse --short HEAD
8761541
$ git status --short
(không có thay đổi)
```

## 2. File đã sửa / tạo

| File | Loại | Nội dung |
| :-- | :-- | :-- |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/Fall01Trace.kt` | MỚI (204 dòng) | Toàn bộ logger FALL01, tag duy nhất `FALL01` |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt` | SỬA (chỉ thêm) | Hook tại state machine + stream mẫu + `gyroTimestampNs` |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt` | SỬA (+2 dòng) | Mốc START/STOP của phiên ghi |

Không file nào khác bị thay đổi. `FallDetectionProfiles.kt`, `espconfig/Models.kt`, `core/src/Core.kt`,
`MonitoringService.kt`, UI, Gradle đều nguyên trạng.

## 3. Pipeline FALL-01 (đã kiểm chứng bằng mã nguồn)

```
PhoneSensorCollector.onSensorChanged()          PhoneSensorCollector.kt:43-63
  → PhoneInputPipeline.update()                 DemoLogic.kt:350-354   (1 packet / mỗi event ACCEL)
  → PhoneNormalizer.packet()                    DemoLogic.kt:63-86     (frame 500 ms, đổi đơn vị)
  → DemoController.acceptPhone()                DemoApplication.kt:434-440
  → DemoInputAdapter.acceptPhone()              DemoLogic.kt:362-373   (chỉ đường PHONE_ONLY)
  → DemoSession.accept()                        DemoLogic.kt:315-321
  → DemoDetector.accept()                       DemoLogic.kt:132-222   (chỉ chạy khi AlertCore == MONITORING)
  → AlertCore.suspected()/evidenceConfirmed()   core/src/Core.kt:73-92
```

Pha của detector (`DetectionPhase`, DemoLogic.kt:92) — dùng đúng tên có sẵn, không phát minh thêm:
`NORMAL` → `IMPACT_DETECTED` → `POST_IMPACT_STILLNESS` → `FALL_CONFIRMED`.

Trạng thái AlertCore (`core.State`, core/src/Core.kt:9): `MONITORING`, `SUSPECTED`, `VERIFYING`,
`ALERTING`, `AWAITING_HELP`.

## 4. Điểm chèn instrumentation

| Dòng log | Vị trí phát | Thời điểm |
| :-- | :-- | :-- |
| `FALL01_SESSION` (START/STOP) | `PhoneSensorCollector.kt:35` / `:42` | `start()` sau khi đăng ký cảm biến; `stop()` sau khi huỷ đăng ký |
| `FALL01_EVENT` | `DemoLogic.kt:371` | packet null → `resetDetection()` (nguồn cảm biến cũ > 500 ms) |
| `FALL01_PROFILE` | `Fall01Trace.kt:99` (gọi từ `DemoLogic.kt:368`) | tự động khi profile id hoặc config thay đổi (cũng là dòng đầu của mỗi phiên) |
| `FALL01_SAMPLE` | `DemoLogic.kt:368` | mỗi packet của pipeline điện thoại, sau khi detector đã xử lý |
| `FALL01_STATE` | `DemoLogic.kt:139,147,159,174,180,188,197,218` | mỗi lần `DetectionPhase` đổi giá trị |
| `FALL01_DECISION` | `DemoLogic.kt:140,148,161,189,198,219` | mỗi khi detector chốt phán quyết (kể cả `detected=false`) |
| `FALL01_ALERT_STATE` | `DemoLogic.kt:311` (helper `alertStep`) | mỗi lần `core.State` đổi (ngã đã xác nhận, người dùng An toàn/Cần giúp, hết đếm ngược, hoàn tất) |

`DemoDetector.accept()` giữ nguyên mọi điều kiện, phép so sánh, thứ tự và giá trị trả về; chỉ thêm biến
`previousPhase` (chỉ đọc) và `hadActiveImpact` (chỉ đọc) cùng các lời gọi log.

## 5. Định dạng log (một tag duy nhất: FALL01, mức I)

Mọi dòng là key=value phân cách bằng dấu phẩy, bắt đầu bằng token `FALL01_`. Giá trị float `%.3f`
(accel/gyro/mag), `%.1f` (áp suất Pa), `%.2f` (tiến độ tĩnh); giá trị thiếu in literal `null`.

```
FALL01_SESSION,event=START,tns=<ns>,wallMs=<ms>,sensors=ACCEL+LINEAR+GYRO+ORIENTATION+PRESSURE
FALL01_SESSION,event=STOP,tns=<ns>,wallMs=<ms>,elapsedMs=<ms>,samples=<n>,confirmations=<n>,confirmed=<true|false>
FALL01_EVENT,event=PIPELINE_STALE_RESET,t=<ms>
FALL01_PROFILE,t=<ms>,profileId=<uuid>,profileNumber=<n>,displayName="<tên>",announcements=<n>,impact=..,stillTarget=..,stillTol=..,postWindowMs=..,stillDurationMs=..,minSamples=..,maxGapMs=..,pressureEvidence=..,minPressureRisePa=..
FALL01_SAMPLE,t=<ms>,tns=<ns>,dtMs=<ms>,wallMs=<ms>,ax=..,ay=..,az=..,amag=..,lx=..,ly=..,lz=..,lmag=..,gx=..,gy=..,gz=..,gmag=..,gtns=<ns|null>,pitch=..,roll=..,yaw=..,pressurePa=..,alert=<State>,observed=<bool>,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=..,stillSamples=<n>,profileNumber=<n>,pressureCorroborated=<bool|null>
FALL01_STATE,t=<ms>,tns=<ns>,from=<DetectionPhase>,to=<DetectionPhase>,reason=<REASON>,amag=..,gmag=..,stillProgress=..,stillSamples=<n>,profileNumber=<n>,impact=..,stillTarget=..,stillTol=..,postWindowMs=..,stillDurationMs=..,minSamples=..,maxGapMs=..
FALL01_DECISION,t=<ms>,tns=<ns>,detected=<bool>,reason=<REASON>,amag=..,gmag=..,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=..,stillSamples=<n>,profileNumber=<n>,profileId=<uuid>,impact=..,stillTarget=..,stillTol=..,postWindowMs=..,stillDurationMs=..,minSamples=..,maxGapMs=..
FALL01_ALERT_STATE,t=<ms|null>,from=<State>,to=<State>,reason=<REASON>,profileNumber=<n|null>,amag=<f|null>
```

### Ví dụ minh hoạ định dạng (giá trị là ví dụ, KHÔNG phải số đo)

```
FALL01_SESSION,event=START,tns=41293000123456,wallMs=1789000000000,sensors=ACCEL+LINEAR+GYRO+ORIENTATION+PRESSURE
FALL01_PROFILE,t=0,profileId=3f...c1,profileNumber=1,displayName="Bảng 1",announcements=1,impact=25.000,stillTarget=9.810,stillTol=1.000,postWindowMs=3000,stillDurationMs=1000,minSamples=6,maxGapMs=250,pressureEvidence=false,minPressureRisePa=12.0
FALL01_SAMPLE,t=1524,tns=41293001678000,dtMs=20,wallMs=1789000001524,ax=0.310,ay=1.020,az=9.720,amag=9.777,lx=0.010,ly=-0.020,lz=0.030,lmag=0.037,gx=0.010,gy=-0.030,gz=0.120,gmag=0.124,gtns=41293001661000,pitch=-1.200,roll=0.400,yaw=178.900,pressurePa=101325.0,alert=MONITORING,observed=true,phase=NORMAL,impactOver=false,withinStill=false,stillProgress=0.00,stillSamples=0,profileNumber=1,pressureCorroborated=null
FALL01_STATE,t=1843,tns=41293002098000,from=NORMAL,to=IMPACT_DETECTED,reason=IMPACT_THRESHOLD_REACHED,amag=24.800,gmag=5.600,stillProgress=0.00,stillSamples=0,profileNumber=1,impact=25.000,stillTarget=9.810,stillTol=1.000,postWindowMs=3000,stillDurationMs=1000,minSamples=6,maxGapMs=250
FALL01_DECISION,t=2076,tns=41293002331000,detected=true,reason=STILLNESS_CONFIRMED,amag=9.750,gmag=0.120,phase=FALL_CONFIRMED,impactOver=false,withinStill=true,stillProgress=1.00,stillSamples=6,profileNumber=1,profileId=3f...c1,impact=25.000,stillTarget=9.810,stillTol=1.000,postWindowMs=3000,stillDurationMs=1000,minSamples=6,maxGapMs=250
FALL01_ALERT_STATE,t=2076,from=MONITORING,to=SUSPECTED,reason=FALL_CONFIRMED_BY_DETECTOR,profileNumber=1,amag=9.750
FALL01_SESSION,event=STOP,tns=41293009555000,wallMs=1789000009555,elapsedMs=9530,samples=476,confirmations=1,confirmed=true
```

### Ý nghĩa các trường then chốt

- `tns`: `SensorEvent.timestamp` của mẫu ACCEL (đơn vị ns, cùng gốc `elapsedRealtimeNanos`).
- `t`: mili-giây kể từ mốc bắt đầu phiên ghi (`PhoneSensorCollector.start()`), hoặc từ mẫu đầu tiên
  nếu chưa có mốc phiên.
- `dtMs`: khoảng cách tới mẫu được log trước đó — dùng trực tiếp cho giả thuyết H3/H4 (mất mẫu, gap > 250 ms).
- `gtns`: timestamp **riêng** của mẫu gyro gần nhất (giữ nguyên, không nội suy/resample) → phân tích đồng bộ accel–gyro về sau.
- `amag`, `gmag`, `lmag`: chuẩn Euclid của vector accel / gyro / linear-accel. Detector chỉ dùng `amag`
  từ `accelXMs2..ZMs2` (TYPE_ACCELEROMETER gồm trọng lực); `gmag`/`lmag` chỉ để quan sát.
- `alert`: `core.State` hiện tại; `observed=true` nghĩa là mẫu này **thực sự được detector tiêu thụ**
  (AlertCore đang `MONITORING`). Khi `observed=false`, các trường `phase/impactOver/...` là ảnh chụp cuối cùng.
- `reason` của `FALL01_STATE`/`FALL01_DECISION` lấy đúng nhánh điều kiện sẵn có trong `DemoDetector.accept()`:
  `PROFILE_CHANGED`, `INVALID_SAMPLE_INPUT`, `SAMPLE_GAP_RESET`, `SAMPLE_GAP_ABORTED_IMPACT_EPISODE`,
  `IMPACT_THRESHOLD_REACHED`, `NO_ACTIVE_IMPACT`, `POST_IMPACT_WINDOW_EXPIRED`, `STILLNESS_INTERRUPTED`,
  `STILLNESS_IN_PROGRESS`, `STILLNESS_CONFIRMED`; của `FALL01_ALERT_STATE`:
  `FALL_CONFIRMED_BY_DETECTOR`, `FALL_EVIDENCE_CONFIRMED`, `USER_SAFE`, `USER_NEED_HELP`, `EVENT_COMPLETE`,
  `VERIFICATION_TICK`.

### Thứ tự dòng trong log

Với **cùng một mẫu cảm biến**: các dòng `FALL01_STATE`/`FALL01_DECISION` được ghi trong lúc detector xử lý,
trước dòng `FALL01_SAMPLE` của chính mẫu đó (cùng giá trị `t`). `FALL01_SAMPLE` được ghi cho **mọi** packet
mà pipeline điện thoại tạo ra — kể cả sau khi detector đã xác nhận ngã và ngừng tiêu thụ mẫu — nên cửa sổ
5 giây sau va chạm vẫn được ghi đầy đủ.

## 6. Sampling / dữ liệu

- Không đổi tần số lấy mẫu, `SensorManager.SENSOR_DELAY_GAME`, hay cửa sổ tươi 500 ms của `PhoneNormalizer`.
- Không nội suy/resample; hai timestamp accel/gyro giữ nguyên như phần cứng phát ra.
- Dữ liệu mô phỏng (`DemoReplay`, `acceptReplay`) **không** được hook → file log chỉ chứa dữ liệu cảm biến thật.
- Không log dữ liệu riêng tư (không số điện thoại, không vị trí, không nội dung tin nhắn).

## 7. Kiểm chứng (thực thi thật)

```
$ cd android && ./gradlew --offline --no-daemon --max-workers=2 --rerun-tasks testDebugUnitTest assembleDebug
BUILD SUCCESSFUL in 21s
45 actionable tasks: 45 executed
```
Kết quả test lấy từ `android/app/build/test-results/testDebugUnitTest/*.xml` (chạy lại toàn bộ):

```
test suites = 30
tests       = 206
failures    = 0
errors      = 0
skipped     = 0
```
APK: `android/app/build/outputs/apk/debug/app-debug.apk` (12,599,616 bytes).

Kiểm tra diff:

```
$ git diff --check          -> sạch (không whitespace error)
$ git diff --stat
 android/.../DemoLogic.kt          | 50 ++++++++++++++++++----
 android/.../PhoneSensorCollector.kt |  2 +
 2 files changed, 44 insertions(+), 8 deletions(-)
 (+ 1 file mới: Fall01Trace.kt, 204 dòng)
```
Toàn bộ 8 dòng bị xoá đều thuộc phần instrumentation (gộp field `gyroTimestampNs`, tách dòng constructor
packet, bọc `core.*` trong helper `alertStep`, tách nhánh `if/else` của adapter):

```
-    val sensorQuality: Int = 0
-            o?.get(0), o?.get(1), o?.get(2), pressure, altitudeDelta, pressureWindowDelta)
-            core.suspected(); core.evidenceConfirmed()
-    fun tick() = core.tick()
-    fun safe() { core.safe(); detector.reset() }
-    fun needHelp() { core.needHelp(); detector.reset() }
-    fun complete() { core.complete(); detector.reset() }
-        if (packet != null) session.accept(packet) else session.resetDetection()
```

Xác nhận bằng `rg`/`git diff`:

```
$ git diff --name-only -- android/.../FallDetectionProfiles.kt android/.../espconfig/Models.kt
(không có kết quả → hai file ngưỡng/profile không bị chạm)
$ rg -n 'impactAccelerationMs2|stillnessTargetAccelerationMs2|stillnessToleranceMs2|postImpactWindowMs|
        postImpactStillnessDurationMs|minimumStillnessSamples|maximumSampleGapMs' <diff>
(các dòng này chỉ xuất hiện trong Fall01Trace.kt để IN giá trị, không có dòng nào bị sửa)
```

```
THRESHOLD_CHANGED       = NO
PROFILE_VALUE_CHANGED   = NO
DETECTION_LOGIC_CHANGED = NO
```

## 8. STOP GATE

Bước tuning threshold **chưa** bắt đầu và không được bắt đầu trong task này. Chỉ sau khi có
`fall01-bed-01.log` … `fall01-bed-03.log` thật từ thiết bị mới phân tích: `acceleration magnitude vs time`,
`gyro magnitude vs time`, đỉnh impact, khoảng trước va chạm, độ tĩnh sau va chạm, các chuyển pha, và các
chuyển pha sai/bỏ sót.
