# Astra — SOS runtime fix (production code + test)

Ngày: 2026-09-19 · WORKDIR: `/home/pdd/nckh27pa/android` · Không commit, không push.

Nguồn: `.ai/luna-sos-runtime-investigation.md` (điều tra, không sửa) → `.ai/commandcode-sos-investigation.md`
(xác nhận lại bằng code hiện tại) → ticket `.ai/T9-commandcode-v4pro-sos-continuation.md` (thực thi).

---

## ORIGINAL ASTRA WORK

Astra (phiên trước, thay đổi CHƯA COMMIT còn nguyên trong working tree) đã thay đường chạy cũ bằng pipeline
fail-soft. Phần này KHÔNG bị viết lại, chỉ được review và hoàn thiện:

- `emergency/EmergencyCore.kt`: thêm `LocationSource.{FUSED,GPS,NETWORK,CACHED,UNKNOWN}`, `LocationLookup`,
  `LocationRepository`, `SOS_LOCATION_TIMEOUT_MS=8_000`, `CallStatus`/`CallDispatchState`/`EmergencyCallGateway`,
  bước báo cáo `SosStep.SIM_CALL`, `EmergencyRecord.smsSentContacts`/`supplementSentContacts`,
  `dispatchSms`/`dispatchSimCall`/`updateLocation`, formatter fallback không bịa tọa độ,
  `SmsPartAggregation` không còn `require()` trong BroadcastReceiver, `allSucceeded` coi SKIPPED là thành công.
- `location/LocationRepository.kt` (mới): `LocationProviderAvailability.enabled` + `BestAvailableLocationRepository`
  với ngân sách bounded: quyền → provider → cache `lastKnown` → FUSED → GPS → NETWORK.
- `location/AndroidPlatformLocationSource.kt` (mới): `FusedLocationProviderClient.getCurrentLocation`
  (HIGH_ACCURACY cho FUSED, BALANCED cho NETWORK) + `LocationManager` cho GPS/network + `getLastKnownLocation`
  mọi provider; không throw ra ngoài.
- `location/AndroidEmergencyLocationController.kt`: `onVerifyingStarted` chạy lookup trên coroutine riêng có
  `generation` chống race, publish cache sớm rồi publish kết quả cuối, `Log` một dòng nguồn/accuracy/age/cause;
  `remediation` (“Ra nơi thoáng hơn…”) chỉ là gợi ý, KHÔNG phải điều kiện return.
- `emergency/AndroidSimCallGateway.kt` (mới): `Intent.ACTION_CALL` + `tel:`, kiểm tra `CALL_PHONE` và
  `FEATURE_TELEPHONY_CALLING`, lấy kết quả `startActivity` THẬT (latch ≤2 s khi gọi từ thread khác).
- `emergency/AndroidSmsManagerGateway.kt`: chọn subscription (SIM yêu cầu → SIM mặc định, không chặn cứng đa SIM),
  multipart + sent/delivered intent, state/log theo contact.
- `emergency/EmergencyPersistence.kt`: `SharedPrefsEventIdentityStore` đổi session mỗi process (không tái dùng
  record đã dispatch), `SharedPrefsEmergencyStore` bền qua tạo lại Activity.
- `DemoApplication.kt:77-89`: wiring `AndroidSimCallGateway`, `logStatus`, `onFix → updateLocation`,
  `identity` dùng chung với `SyncCoordinator`.
- Test mới/sửa: `SosCallStepTest.kt` (mới), `BestAvailableLocationRepositoryTest.kt` (mới),
  `SosDispatchReportTest.kt`, `EmergencyLogicTest.kt`, `LocationIntentAndProviderTest.kt`, `HomeScreenPureUiTest.kt`,
  `CapabilityAccessTest.kt`; xoá `BoundedLocationResolutionTest.kt` (thay bằng repository test).

## COMMANDCODE CONTINUATION MODEL

- Model được chọn: **`deepseek/deepseek-v4-pro`** (CommandCode CLI, `--effort high`) — model mạnh nhất còn
  quota phù hợp Android/Kotlin trong gói; xác nhận available bằng `commandcode --list-models`.
- Lý do: Codex/Luna hết quota; Astra hết quota. V4 Pro là model reasoning mạnh nhất còn dùng được trong
  CommandCode và là mặc định `~/.commandcode/config.json` (`reasoningEffort: high`).
- Chạy thật: `commandcode -p "$(cat .ai/T9-...md)" -m deepseek/deepseek-v4-pro --effort high --tools-all --yolo
  --trust --skip-onboarding --max-turns 150` trong `/home/pdd/nckh27pa/android`.
- Ghi chú vận hành: chế độ headless `-p` của CommandCode **giữ lại tool** (mọi `read_file`/`shell_command` bị
  trả “Permission denied by the user.”) và worker thoát ngay. Phải truyền `--tools-all --yolo`.
- Hermes (PM) KHÔNG tin báo cáo worker: tự đọc diff, tự chạy lại toàn bộ test + build và tự parse XML.

## CONTINUED WORK

Hai defect còn lại đã được sửa (đúng phạm vi ticket, không refactor):

- **Defect A — SMS bổ sung bị gửi dù SMS đầu đã có tọa độ.** `EmergencyRecord` thêm
  `var dispatchedWithFix: Boolean = false` (mặc định ⇒ Gson vẫn đọc được record đã lưu của bản cũ);
  `dispatch()` đặt `record.dispatchedWithFix = record.fix != null`; `updateLocation()` chỉ gửi supplement khi
  `record.dispatched && !record.dispatchedWithFix`. Vẫn bảo đảm: tối đa 1 supplement/contact/sự kiện, bỏ qua khi
  đã cancel, bỏ qua khi thiếu quyền MESSAGING.
- **Defect B — số điện thoại trống vẫn bị đẩy xuống gateway SMS.** `dispatchSms()` giờ bỏ qua contact có
  `phone.isBlank()` ngay tại coordinator, đếm riêng (`skipped`) và nêu trong detail/SMS step; nêu rõ số đã bỏ qua.
  KHÔNG lọc theo regex VN ⇒ số quốc tế/short code vẫn được gửi. Vòng lặp supplement cũng thêm guard
  `contact.phone.isNotBlank()`. Không hard-code số nào.
- Kết luận review: không còn nhánh nào để location chặn SMS/CALL; không có duplicate call/SMS; idempotency một
  sự kiện = một chuỗi cầu cứu vẫn do `if(record.dispatched) return` giữ.
- **Mâu thuẫn D09 KHÔNG được tự ý sửa** (xem UNRESOLVED).

## FILES CHANGED (chỉ 2 file, đúng phạm vi cho phép)

```text
M android/app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt
M android/app/src/test/java/vn/nckh27pa/fallsafe/emergency/SosCallStepTest.kt
```

Không file nào khác bị chạm: kiểm chứng bằng `find /home/pdd/nckh27pa -newermt "2026-09-19 13:00"`
(ngoài build/) chỉ trả về đúng 2 file trên + báo cáo điều tra của Hermes. `docs/`,
`.ai/luna-sos-runtime-investigation.md`, `HomeScreen.kt`, `MainActivity.kt`, `AndroidManifest.xml`,
`app/build.gradle.kts` KHÔNG bị sửa trong bước này.

## PIPELINE SOS MỚI (trạng thái sau sửa)

```text
SOS (giữ nút 3 s, hoặc hết countdown)
 ├── [async, không chặn] location: cache → FUSED → GPS → NETWORK, trần 8 s
 ├── [gửi ngay] SMS khẩn cấp tới mọi contact receiveSos (có/không có tọa độ đều gửi)
 └── [gọi ngay] cuộc gọi SIM thật (ACTION_CALL, contact ưu tiên receiveSos + số hợp lệ)
      backend voice = một nhánh riêng, lỗi backend không ảnh hưởng 3 nhánh trên

Có fix trước dispatch : SMS kèm tọa độ + https://maps.google.com/?q=<lat>,<lon>
Location FAIL/TIMEOUT/UNAVAILABLE/PERMISSION_DENIED/PROVIDER_DISABLED:
      LOCATION=UNAVAILABLE nhưng SMS=SUCCESS và SIM_CALL vẫn được thực hiện (không return, không abort)
Fix đến SAU, và SMS đầu CHƯA có vị trí:
      đúng MỘT SMS bổ sung “Đã xác định được vị trí: … <maps-link>”, MAP_LINK chuyển SUCCESS
Fix đến sau nhưng SMS đầu ĐÃ có vị trí: không gửi thêm (sau Defect A)
Thiếu 1 quyền: chỉ bước tương ứng PERMISSION_MISSING, các bước còn lại vẫn chạy
```

## TEST RESULT

```text
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon
(JDK 17 bắt buộc: Java 25 làm Gradle fail trước khi cấu hình project)

suites=27  tests=183  failures=0  errors=0  skips=0
(đọc trực tiếp từ app/build/test-results/testDebugUnitTest/TEST-*.xml, Hermes tự chạy lại)
```

Case theo yêu cầu ticket:

```text
1  location success → SMS + CALL         gpsUnavailableNetworkFixProducesMapsSmsAndSimCall
2  location timeout → SMS + CALL         locationTimeoutOrUnavailable... + repository timeout test
3  location unavailable → SMS + CALL     locationTimeoutOrUnavailable... (PROVIDER_DISABLED)
4  location permission denied → SMS+CALL deniedLocationPermissionStillQueuesSmsAndAttemptsSimCall
5  SMS failure → CALL vẫn chạy           smsFailureStillDialsAndCallExceptionIsReported
6  CALL failure → SMS vẫn chạy           everyGatewayFailureMapsDetailWithoutRetry
7  backend failure → SMS + CALL local    backendNotStartedAttemptsHandsetCall
8  late location → đúng 1 supplement     supplementIsStillExactlyOneWhenFirstSmsHadNoFixAndSeveralFixesArrive
9  callback lặp → không spam SMS         lateLocationSendsOnlyOneSupplementAcrossCoordinatorRecreation (repeat(3))
10 SOS lặp/recomposition → không nhân đôi callsFirstEligibleContactExactlyOnceEvenOnRepeatedDispatch
   (mới) SMS đầu đã có fix → KHÔNG supplement        noSupplementWhenFirstSmsAlreadyCarriedAFix
   (mới) số trống không chặn contact khác            blankPhoneContactIsSkippedWithoutStoppingOtherContacts
```

## BUILD RESULT

```text
BUILD SUCCESSFUL in 1m 1s · 45 actionable tasks: 45 executed (--rerun-tasks, --no-daemon)
APK: android/app/build/outputs/apk/debug/app-debug.apk  12.583.232 B
sha256 b9f0148f5969fd7e7fb7fbbb1b095d0138255636f08ccd41591b6857c677c585
```

## PHÂN LOẠI BẰNG CHỨNG

**VERIFIED BY UNIT TEST** (183 test, đọc từ XML): độc lập SMS/CALL với location và backend; fallback SMS không
bịa tọa độ; SIM call chọn/normalize số đúng và gọi đúng một lần; mọi trạng thái lỗi gateway; exactly-one
supplement khi location đến muộn; không supplement khi SMS đầu đã có fix; số trống không chặn contact khác.

**VERIFIED BY EMULATOR** (từ lượt trước, artifact khác — KHÔNG chạy lại ở bước này): `docs/evidence/sos-location/`
(`source=FUSED accuracy=5.0`, SMS thật trong `content://sms/sent` với `https://maps.google.com/?q=10.8231,106.6296983`,
`SIM_CALL:SUCCESS` + `FallSafe/CALL: status=STARTED`) và `docs/evidence/sos-runtime/hermes-degraded-location-off*`
(`LOCATION=UNAVAILABLE cause=PROVIDER_DISABLED`, `SMS=SUCCESS`, `SIM_CALL=SUCCESS`, `MAP_LINK=SKIPPED`).
Bước này chỉ chạy unit test + build, KHÔNG chạy emulator ⇒ không tuyên bố lại evidence emulator cho APK mới.

**REQUIRES REAL DEVICE TEST**: mọi thứ dưới đây CHƯA được chứng minh và KHÔNG được coi là PASS:
- SMS thực sự rời máy và tới tay người nhận (biên nhận nhà mạng) trên máy có SIM thật;
- cuộc gọi SIM thật qua mạng di động (không chỉ `startActivity` thành công);
- đa SIM thật (chọn đúng subscription mặc định);
- GNSS ngoài trời và hành vi trong nhà (FUSED/NETWORK fix, accuracy thực);
- FGS/khoá màn hình khi SOS đang chạy;
- máy không có Google Play Services (nhánh `LocationManager` dự phòng);
- nhánh quyền bị từ chối trên máy thật (Settings → quyền).

## UNRESOLVED

Không còn mục nào. Mâu thuẫn D09 đã được **chủ dự án chốt phương án (a) ngày 2026-09-19**: cập nhật tài liệu
theo hành vi always-call-handset hiện tại; KHÔNG sửa production code, KHÔNG flip test.

Đã cập nhật tài liệu (chỉ tài liệu):

```text
docs/decisions.md        D09 — bỏ điều kiện "chỉ khi adapter thoại máy chủ chưa STARTED"; ghi rõ backend là
                         nhánh best-effort PHỤ TRỢ, SMS/CALL handset/location là ba nhánh ĐỘC LẬP; STARTED
                         không suppress handset call; backend lỗi/vắng mặt không ảnh hưởng cuộc gọi SIM.
docs/next-gate.md        §4 — câu trả lời chốt lại theo nguyên tắc trên.
docs/permission-flow.md  §4 — hai gạch đầu dòng về nhánh gọi tự động đã bỏ "khi máy chủ chưa tiếp nhận".
.ai/architecture.md      §"Nhánh SOS độc lập" — SIM_CALL ghi rõ là nhánh độc lập, STARTED không suppress.
.ai/task_on_progress.md  DO_NOT_REPEAT — đổi "Không gỡ guard D09" thành "Không khôi phục guard D09 cũ".
```

Giữ nguyên (không sửa): `.ai/luna-sos-runtime-investigation.md` (tài liệu của Luna), các ticket/log lịch sử
(`.ai/T6-*.md`, `.ai/T8-*.md`, `.ai/T8-run*.log`) — chúng ghi lại trạng thái tại thời điểm đó, không phải
quy tắc đang hiệu lực.

Không còn lỗi nào khác đang mở.
