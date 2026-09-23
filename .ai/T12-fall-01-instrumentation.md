# T12 — FALL-01 real-device instrumentation (report)

STATUS: DONE — instrumentation đã vào source, build/test xanh, chưa tuning.
OWNER (implementation): AGY CLI `gemini-3.8-flash-high` (Level 1, một worker duy nhất, 2 lượt trên cùng file).
OWNER (verification/report): Hermes (project manager).
FILES: `android/app/src/main/java/vn/nckh27pa/fallsafe/Fall01Trace.kt` (mới),
`…/DemoLogic.kt`, `…/PhoneSensorCollector.kt`, `docs/evidence/fall-01/*`, `.ai/T12-*`.
GOAL: quan sát được pipeline FALL hiện tại trên máy thật qua đúng một tag log, không đổi hành vi.
RESULT: xem §7–§8. NEXT_ACTION: chủ dự án chạy FALL01-BED-01/02/03 (lệnh ở §9), sau đó mới phân tích và
bàn tới threshold.

Ghi nhận phối hợp worker (theo AGENTS.md): Codex CLI — worker Level 2 được chỉ định cho việc này — đã
**hết quota** (`ERROR: You've hit your usage limit … try again at Sep 22nd, 2026 2:28 PM`), nên Hermes đã
chọn worker thay thế hợp lệ là AGY Gemini (Level 1, dùng cho simple code generation/routine fix), một
worker duy nhất, không có hai agent cùng sửa một file. Không tự động "take over" code.

---

## 1. HEAD ban đầu

```
$ git rev-parse --short HEAD
8761541
$ git status --short
(clean)
```
(`git branch --show-current` = `main`)

## 2. Files đã sửa / tạo

| File | Thay đổi |
| :-- | :-- |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/Fall01Trace.kt` | **MỚI**, 204 dòng — toàn bộ logger, tag `FALL01` |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt` | chỉ thêm (hook state machine, stream mẫu, `gyroTimestampNs`) — `44 insertions(+), 8 deletions(-)` sau khi gộp dòng |
| `android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt` | +2 dòng: mốc START/STOP của phiên ghi |
| `docs/evidence/fall-01/instrumentation-summary.md` | bằng chứng instrumentation (mô tả + format + kiểm chứng) |
| `docs/evidence/fall-01/fall01-bed-runbook.md` | lệnh chạy thí nghiệm thật |
| `.ai/T12-fall-01-ticket.md`, `.ai/T12-fall-01-instrumentation.md` | phiếu việc + báo cáo này |

Không sửa: `FallDetectionProfiles.kt`, `espconfig/Models.kt`, `core/src/Core.kt`, `MonitoringService.kt`,
UI, các file Gradle.

## 3. Điểm chèn instrumentation

| Dòng log | Vị trí |
| :-- | :-- |
| `FALL01_SESSION` (START/STOP) | `PhoneSensorCollector.kt:35` (sau khi đăng ký cảm biến), `:42` (trong `stop()`) |
| `FALL01_EVENT` = `PIPELINE_STALE_RESET` | `DemoLogic.kt:371` — nhánh `packet == null` của `DemoInputAdapter.acceptPhone` |
| `FALL01_PROFILE` | `Fall01Trace.kt:99`, phát tự động từ `Fall01Trace.sample()` khi profile id/config đổi |
| `FALL01_SAMPLE` | `DemoLogic.kt:368` — mỗi packet của pipeline điện thoại (1 packet / event ACCEL), sau khi detector xử lý |
| `FALL01_STATE` | `DemoLogic.kt:139, 147, 159, 174, 180, 188, 197, 218` — mỗi khi `DetectionPhase` đổi |
| `FALL01_DECISION` | `DemoLogic.kt:140, 148, 161, 189, 198, 219` — mỗi phán quyết kết thúc một episode |
| `FALL01_ALERT_STATE` | `DemoLogic.kt:311` (helper `alertStep` ở `:308-312`) — mỗi lần `core.State` đổi |

Luồng đã instrumentation (đúng pipeline hiện có, không rút ngắn bước nào):

```
PhoneSensorCollector.onSensorChanged  (PhoneSensorCollector.kt:43-63)
 → PhoneInputPipeline.update          (DemoLogic.kt:350-354)
 → DemoController.acceptPhone         (DemoApplication.kt:434-440)
 → DemoInputAdapter.acceptPhone       (DemoLogic.kt:362-373)   ← FALL01_SAMPLE
 → DemoSession.accept                 (DemoLogic.kt:315-321)   ← FALL01_ALERT_STATE
 → DemoDetector.accept                (DemoLogic.kt:132-222)   ← FALL01_STATE / FALL01_DECISION
 → AlertCore.suspected()/evidenceConfirmed()  (core/src/Core.kt:73-92)
```

## 4. Format `FALL01_SAMPLE`

```
FALL01_SAMPLE,t=<ms>,tns=<ns>,dtMs=<ms>,wallMs=<ms>,ax=<f>,ay=<f>,az=<f>,amag=<f>,lx=<f>,ly=<f>,lz=<f>,lmag=<f>,gx=<f>,gy=<f>,gz=<f>,gmag=<f>,gtns=<ns|null>,pitch=<f>,roll=<f>,yaw=<f>,pressurePa=<f>,alert=<State>,observed=<bool>,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,pressureCorroborated=<bool|null>
```

Ví dụ định dạng (giá trị minh hoạ, không phải số đo thật):

```
FALL01_SAMPLE,t=1524,tns=41293001678000,dtMs=20,wallMs=1789000001524,ax=0.310,ay=1.020,az=9.720,amag=9.777,lx=0.010,ly=-0.020,lz=0.030,lmag=0.037,gx=0.010,gy=-0.030,gz=0.120,gmag=0.124,gtns=41293001661000,pitch=-1.200,roll=0.400,yaw=178.900,pressurePa=101325.0,alert=MONITORING,observed=true,phase=NORMAL,impactOver=false,withinStill=false,stillProgress=0.00,stillSamples=0,profileNumber=1,pressureCorroborated=null
```

- `amag = sqrt(ax²+ay²+az²)` (m/s², TYPE_ACCELEROMETER có trọng lực) — đúng đại lượng detector so với ngưỡng.
- `gmag = sqrt(gx²+gy²+gz²)`, đơn vị deg/s (đã scale 180/π trong `PhoneNormalizer`).
- `lmag` = linear acceleration (chỉ quan sát; detector không dùng).
- `tns` = timestamp cảm biến ACCEL; `gtns` = timestamp riêng của mẫu gyro gần nhất (giữ nguyên, không nội suy).
- `t` = ms kể từ mốc bắt đầu ghi; `dtMs` = khoảng cách tới mẫu log trước (dùng cho giả thuyết mất mẫu > 250 ms).
- `alert` = `core.State`; `observed=true` ⇔ mẫu này thực sự được `DemoDetector` tiêu thụ (AlertCore đang `MONITORING`).
- Dòng `FALL01_SAMPLE` được ghi cho **mọi** packet, kể cả sau khi xác nhận ngã (khi đó `observed=false`),
  nên vẫn giữ được cửa sổ ~5 giây sau va chạm.

## 5. Format `FALL01_STATE`

```
FALL01_STATE,t=<ms>,tns=<ns>,from=<DetectionPhase>,to=<DetectionPhase>,reason=<REASON>,amag=<f>,gmag=<f>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,impact=<f>,stillTarget=<f>,stillTol=<f>,postWindowMs=<n>,stillDurationMs=<n>,minSamples=<n>,maxGapMs=<n>
```

Ví dụ định dạng:

```
FALL01_STATE,t=1843,tns=41293002098000,from=NORMAL,to=IMPACT_DETECTED,reason=IMPACT_THRESHOLD_REACHED,amag=24.800,gmag=5.600,stillProgress=0.00,stillSamples=0,profileNumber=1,impact=25.000,stillTarget=9.810,stillTol=1.000,postWindowMs=3000,stillDurationMs=1000,minSamples=6,maxGapMs=250
```

Tên state lấy đúng enum có sẵn `DetectionPhase` (`NORMAL`, `IMPACT_DETECTED`, `POST_IMPACT_STILLNESS`,
`FALL_CONFIRMED`); không thêm state mới. `reason` lấy đúng nhánh điều kiện sẵn có trong `DemoDetector.accept()`.

## 6. Format `FALL01_DECISION`

```
FALL01_DECISION,t=<ms>,tns=<ns>,detected=<bool>,reason=<REASON>,amag=<f>,gmag=<f>,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,profileId=<uuid>,impact=<f>,stillTarget=<f>,stillTol=<f>,postWindowMs=<n>,stillDurationMs=<n>,minSamples=<n>,maxGapMs=<n>
```

Ví dụ định dạng:

```
FALL01_DECISION,t=2076,tns=41293002331000,detected=true,reason=STILLNESS_CONFIRMED,amag=9.750,gmag=0.120,phase=FALL_CONFIRMED,impactOver=false,withinStill=true,stillProgress=1.00,stillSamples=6,profileNumber=1,profileId=3f...c1,impact=25.000,stillTarget=9.810,stillTol=1.000,postWindowMs=3000,stillDurationMs=1000,minSamples=6,maxGapMs=250
```

`detected` chính là giá trị `confirmed` mà `DemoDetector.accept()` đã trả về (không tính lại). Lượt ghi
không hề vượt ngưỡng va chạm vẫn có phán quyết tường minh ở dòng chốt phiên:

```
FALL01_SESSION,event=STOP,tns=…,wallMs=…,elapsedMs=…,samples=<n>,confirmations=<n>,confirmed=<true|false>
```

Ngoài ra `FALL01_ALERT_STATE,from=<State>,to=<State>,reason=<FALL_CONFIRMED_BY_DETECTOR|FALL_EVIDENCE_CONFIRMED|USER_SAFE|USER_NEED_HELP|EVENT_COMPLETE|VERIFICATION_TICK>` ghi lại chuyển trạng thái của
state machine cảnh báo, và `FALL01_PROFILE` in toàn bộ 7 tham số profile đang chạy mỗi khi profile đổi.

## 7. Gradle verification

```
$ cd android && ./gradlew --offline --no-daemon --max-workers=2 --rerun-tasks testDebugUnitTest assembleDebug
> Task :app:testDebugUnitTest
> Task :app:assembleDebug
BUILD SUCCESSFUL in 21s
45 actionable tasks: 45 executed
```

Đếm từ `android/app/build/test-results/testDebugUnitTest/*.xml` (chạy lại toàn bộ, không dùng cache):

```
test suites = 30
tests       = 206
failures    = 0
errors      = 0
skipped     = 0
```

APK: `android/app/build/outputs/apk/debug/app-debug.apk` (12.599.616 bytes).
`git diff --check`: sạch.

## 8. Xác nhận không đổi ngưỡng / logic

```
git diff --name-only -- android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt \
                        android/app/src/main/java/vn/nckh27pa/fallsafe/espconfig/Models.kt
→ (rỗng)
```

Toàn bộ dòng bị xoá trong diff (8 dòng) đều là hệ quả của việc gộp dòng khi thêm instrumentation:

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

Mọi câu lệnh so sánh/trả về trong `DemoDetector.accept()` giữ nguyên: `magnitude >= config.impactAccelerationMs2`,
`t - hit > config.postImpactWindowMs * 1_000_000L`, `abs(magnitude - config.stillnessTargetAccelerationMs2) <=
config.stillnessToleranceMs2`, `count >= config.minimumStillnessSamples && elapsedNs >= requiredNs`,
`t <= previous || t - previous > maximumGapNs`. Các tham số chỉ xuất hiện thêm trong `Fall01Trace.kt` với
mục đích **in giá trị**, không gán lại giá trị nào. `FallDetectionConfig.DEFAULT` không đổi.

```
THRESHOLD_CHANGED       = NO
DETECTION_LOGIC_CHANGED = NO
```

## 9. Lệnh chính xác để chạy thí nghiệm trên điện thoại thật

Chuẩn bị:

```bash
cd /Users/phananh/TEMP/nckh27pa/nckh27pa
ADB=~/Library/Android/sdk/platform-tools/adb     # hoặc dùng `adb` nếu đã có trong PATH
OUT=docs/evidence/fall-01
$ADB devices
(cd android && ./gradlew --offline --no-daemon --max-workers=2 assembleDebug)
$ADB install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Một lượt ghi (thay `01` bằng `02`, `03` cho các lượt sau):

```bash
$ADB logcat -c
$ADB logcat -v threadtime FALL01:V '*:S' | tee $OUT/fall01-bed-01.log
```

Khi lệnh trên đang chạy, thực hiện đúng thứ tự:

1. Trong app FallSafe: bật Giám sát (nguồn `PHONE_ONLY • cảm biến thật`) và bật giám sát nền → phải thấy
   `FALL01_SESSION,event=START` và `FALL01_PROFILE`.
2. Đặt máy đứng yên trên giường khoảng 3 giây.
3. Cầm máy ở độ cao thực tế như khi người dùng cầm máy (~0,7–1,0 m).
4. Thả máy rơi tự nhiên xuống GIƯỜNG mềm.
5. Không can thiệp vào chuyển động sau khi máy chạm giường; để nguyên khoảng 5 giây.
6. Trong app: dừng Giám sát (dòng `FALL01_SESSION,event=STOP` chốt `confirmed=`), rồi `Ctrl-C` terminal logcat.

Lặp lại cho `fall01-bed-02.log` và `fall01-bed-03.log`. Lưu ý: nếu detector xác nhận ngã, app sẽ đếm ngược
10 giây rồi gửi SOS thật tới liên hệ đã cấu hình — bấm **An toàn** trong lúc đếm ngược nếu không muốn gửi.

Kiểm tra nhanh sau mỗi lượt:

```bash
grep -c 'FALL01_SAMPLE' $OUT/fall01-bed-01.log
grep -E 'FALL01_(SESSION|PROFILE|DECISION|STATE|ALERT_STATE)' $OUT/fall01-bed-01.log
awk -F',' '/FALL01_SAMPLE/ {for(i=1;i<=NF;i++) if($i ~ /^amag=/){split($i,a,"="); if(a[2]+0>m) m=a[2]+0}} END {print "peak amag =", m, "m/s^2"}' $OUT/fall01-bed-01.log
```

## STOP GATE

Dừng tại đây. Không chỉnh threshold, không yêu cầu agent tuning, không tự kết luận ngưỡng mới, không thay
profile, không commit thay đổi tuning. Bước tiếp theo chỉ bắt đầu khi có log FALL01-BED thật:
`acceleration magnitude vs time`, `gyro magnitude vs time`, đỉnh impact, khoảng trước va chạm, độ tĩnh sau
va chạm, các chuyển pha, và các chuyển pha sai/bỏ sót.
