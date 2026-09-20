# Current Task — SOS location / CALL / SMS end-to-end repair

## Goal
Sửa luồng SOS Android để (a) lấy được vị trí dùng được mà KHÔNG cần satellite fix, và (b) SMS + CALL
luôn được thực thi độc lập, không phụ thuộc kết quả vị trí. Không viết lại app, không phá contract backend.

## Status
DONE (chờ chủ dự án chốt commit). T6 (Codex) + T7 (AGY Gemini) + review L3 (AGY Claude) đã xong; Hermes đã
tự chạy lại toàn bộ test/build và chạy acceptance thật trên emulator.

## 9. KẾT QUẢ ĐÃ KIỂM CHỨNG (không tin báo cáo worker, đọc lại bằng chứng)
- `./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`: **170 test PASS, 0 fail/error/skip**
  (đọc từ `app/build/test-results/testDebugUnitTest/*.xml`); APK debug build được (12.566.848 byte).
- Emulator (API 36, `emulator-5554`), runner mới `android/scripts/sos-location-acceptance.py`: **2/2 PASS**.
  - Ngã THẬT bằng `adb emu sensor set acceleration` (không replay) → đếm ngược → fix lấy được TRONG lúc đếm ngược:
    `FallSafe/Location: source=FUSED accuracy=5.0 cause=none`; thẻ UI: "Đã xác định / ± 5 m • vừa xong".
  - SMS THẬT đã gửi: đọc lại `content://sms/sent` thấy `address=0901234567` + nội dung chứa
    `https://maps.google.com/?q=10.8231,106.62969833333334` (đúng tọa độ bơm vào) + "Độ chính xác: 5 m".
  - `ACTION_CALL` THẬT: `FallSafe/CALL: status=STARTED`, `SIM_CALL:SUCCESS`; màn hình gọi điện emulator hiện
    "Calling… 0901234567".
  - Nhánh fallback cache đã chứng minh: `source=CACHED accuracy=5.0 age=117559 cause=TIMEOUT` (cache cũ ~32 h)
    và **SOS vẫn được gửi** ⇒ hết hạn vị trí KHÔNG chặn SMS/CALL.
- Review L3: 0 BLOCKER, 4 MAJOR + 3 MINOR; đã sửa 5 (F-01 timedOut tất định, F-02 ngân sách từng provider,
  F-04 lastKnown không throw, F-09 startActivity phải ở main thread, F-07 allSucceeded tính cả SKIPPED),
  từ chối 3 có lý do ghi trong `.ai/T6-fixes-round2.txt`.
- Bằng chứng: `docs/evidence/sos-location/`; ghi vào `docs/test-report.md` và `docs/permission-flow.md` §5/§7.

## 10. GIỚI HẠN CHƯA KIỂM CHỨNG
- Chưa có thiết bị thật/SIM/GPS: biên nhận giao SMS của nhà mạng, cuộc gọi qua mạng di động thật, thời gian
  bắt fix ngoài trời, hộp thoại quyền theo hãng máy, FGS khi khoá màn hình.
- Bản DEMO tái sử dụng id sự kiện sau khi mở lại app (bộ đếm cục bộ reset về 1) → `dispatch` bỏ qua và UI hiện
  báo cáo cũ; runner đã phải xoá prefs sự kiện. Không phải lỗi luồng SOS nhưng cần xử lý khi làm bền vững hoá.
- ESP32/GNSS vẫn chỉ là hook `acceptEsp32Gnss` (nguồn bổ sung), chưa có transport BLE.

## 1. CURRENT BEHAVIOR (đo bằng đọc code có mục tiêu, không suy đoán)

### Location
- Implementation: `android.location.LocationManager` thuần. `FusedLocationProviderClient` KHÔNG được dùng —
  `com.google.android.gms:play-services-location` không có trong `app/build.gradle.kts`.
  Chuỗi `"fused"` (`AndroidEmergencyLocationController.kt:173`) là provider nền tảng `LocationManager`, không phải Play Services.
- `LocationProviderSelector.choose` (`location/LocationResolution.kt:11-16`): `hasFine && gpsEnabled -> GPS`.
  Vì công tắc Vị trí của máy làm `gpsEnabled=true`, nên trên điện thoại thật gần như LUÔN chọn `GPS_PROVIDER`;
  `NETWORK_PROVIDER` và `fused` gần như không bao giờ được dùng.
- `GPS_PROVIDER`: có, là lựa chọn ưu tiên. `NETWORK_PROVIDER`: có code nhưng nằm sau GPS.
- `getCurrentLocation(...)`: CÓ (`:76`, API ≥ 30; API 26–29 dùng `requestSingleUpdate`).
- `lastLocation`/`getLastKnownLocation`: CÓ (`:107-117`) nhưng chỉ được gọi SAU KHI thất bại (timeout),
  không được thử trước. Không có fast-path cache.
- Khi GPS chưa fix: KHÔNG thử provider khác trong cùng lượt. Một lượt duy nhất → `CURRENT_FIX_TIMEOUT_MS=8_000`
  → `attempt.fail(TIMEOUT)` → cache (có thể null) → thông báo lỗi. Đây chính là triệu chứng "Ra chỗ thoáng".
- Chuỗi "thoáng" sinh ra ở: `AndroidEmergencyLocationController.kt:69` ("Di chuyển ra nơi thoáng; cảnh báo vẫn tiếp tục."),
  `:133` ("Bật GPS và thử ở nơi thoáng."), `:135` ("Chờ GPS cập nhật ở nơi thoáng."),
  và giá trị mặc định `EmergencyCore.kt:44` (`LocationState.remediation = "Di chuyển ra nơi thoáng và thử lại."`).
  UI in nó bằng màu cảnh báo ở `HomeScreen.kt:744-747`.
- SOS lấy vị trí qua port `EmergencyLocationController` (không phải repository): `SyncCoordinator` gọi
  `emergencyLocation?.onVerifyingStarted()` khi state ∈ {VERIFYING, ALERTING, AWAITING_HELP}, sau đó
  `emergency?.timeout(remoteId, contacts, name, emergencyLocation?.locationState?.fix)`; fix mới chảy qua
  `onFix` → `emergencyCoordinator.updateLocation(...)`. Không có `getBestAvailableLocation()`.

### SMS
- Manifest `SEND_SMS`: có. Runtime permission: có (PermissionCenter + `MainActivity.launchCapabilityRequest`).
- Gửi thật: CÓ — `emergency/AndroidSmsManagerGateway.kt` (`SmsManager.sendMultipartTextMessage`, chọn theo
  subscription, sent/delivered `PendingIntent`, `SmsPartAggregation`).
- Được gọi từ luồng SOS: CÓ — `EmergencyCoordinator.dispatchSms` (`EmergencyCore.kt:235-254`).
- Xử lý lỗi: CÓ (SecurityException/RuntimeException → FAILED + lý do thật; đa SIM → yêu cầu chọn SIM).
- Log: KHÔNG có (`android.util.Log` = 0 chỗ trong `app/src/main`); chỉ có state callback → backend `TRANSPORT_STATUS`.

### CALL
- Manifest `CALL_PHONE`: có. `ACTION_DIAL`: KHÔNG dùng ở đâu.
- Gọi thật: CÓ nhưng chỉ THỦ CÔNG — `emergency/ManualSimCallFallback.kt:18-19` (`Intent.ACTION_CALL` +
  kiểm tra `CALL_PHONE`), gọi từ `DemoController.callContactViaSim` → `HomeScreen.kt:1002` (hộp thoại chọn người thân).
- Trong luồng SOS: KHÔNG. Bước `VOICE_CALL` chỉ gọi `backend.startVoice(eventId)` (adapter thoại máy chủ,
  chưa cấu hình → luôn "Không khả dụng"). Quyết định D03/D07 + `docs/next-gate.md §4` đã hoãn auto-dial SIM.

## 2. ROOT CAUSE
1. **Chọn provider một-lượt, GPS-first** (`LocationResolution.kt:12`) → phụ thuộc GPS/GNSS; indoor/urban canyon
   hết 8 s mà không có fix → không thử Wi-Fi/cell/fused.
2. **Không có fused provider** (thiếu dependency) → không có nguồn kết hợp GPS + Wi-Fi + cell + sensor.
3. **Cache chỉ dùng khi đã thất bại**, không dùng làm fast path; không có warm-up trước sự kiện.
4. **Thông báo "ra chỗ thoáng" là remediation mặc định** của `LocationState` và của nhánh lỗi → UI hiển thị
   như lỗi chặn, đúng như chủ dự án báo.
5. **CALL không nằm trong luồng SOS** — chỉ SMS + adapter thoại máy chủ; SIM call là thao tác tay.

## 3. FILES INVOLVED
Sửa: `android/app/build.gradle.kts`, `.../emergency/EmergencyCore.kt`,
`.../location/AndroidEmergencyLocationController.kt`, `.../location/LocationResolution.kt`,
`.../DemoApplication.kt`, `.../HomeScreen.kt`, `.../api/SyncCoordinator.kt` (chỉ nếu cần wiring).
Thêm: `.../location/LocationRepository.kt`, `.../location/AndroidPlatformLocationSource.kt`,
`.../emergency/AndroidSimCallGateway.kt`, test `.../location/BestAvailableLocationRepositoryTest.kt`,
`.../emergency/SosCallStepTest.kt`.
Ghi chú: `EmergencyCore.kt:26` từng hardcode link Maps; nay là 1 helper duy nhất `LocationFix.mapsUrl`
(`https://maps.google.com/?q={lat},{lon}`) + `LocationFix.geoUri`.

## 4. CONTRACT PROPOSED — ĐÃ CHỐT (Hermes, đã áp dụng vào EmergencyCore.kt)
```kotlin
const val SOS_LOCATION_TIMEOUT_MS = 8_000L
enum class LocationSource { PHONE, ESP32_GNSS, FUSED, GPS, NETWORK, CACHED, UNKNOWN }
data class LocationLookup(val fix: LocationFix?, val cause: LocationFailureCause?, val fromCache: Boolean, val elapsedMs: Long)
interface LocationRepository { suspend fun getBestAvailableLocation(timeoutMs: Long = SOS_LOCATION_TIMEOUT_MS): LocationLookup }
enum class CallStatus { STARTED, PERMISSION_MISSING, UNAVAILABLE, FAILED }
data class CallDispatchState(val status: CallStatus, val detail: String? = null)
fun interface EmergencyCallGateway { fun call(phone: String): CallDispatchState }
enum class SosStep { LOCATION, SMS, VOICE_CALL, SIM_CALL, MAP_LINK }
```
Port nền tảng (JVM-testable): `PlatformLocationSource { permission(); enabledProviders(); suspend lastKnown();
suspend current(kind, timeoutMs) }` + `BestAvailableLocationRepository` (thuần Kotlin), implementation
Android `AndroidPlatformLocationSource` (fused trước, LocationManager dự phòng).
Thứ tự lấy vị trí: permission → cached-fresh dùng ngay → current theo FUSED/GPS/NETWORK → cache cũ (TIMEOUT)
→ thất bại rõ ràng. Không ngưỡng accuracy; không bao giờ throw; toàn bộ ≤ 8 s.
Backend contract: KHÔNG đổi. `ActionRequest.location` đã là nullable
(`api/ApiService.kt:67`), nên khi không có vị trí backend vẫn nhận event (đã kiểm tra, không phá schema).

## 5. TASK SPLIT
- **T6 — Codex (Level 2)**: `build.gradle.kts` + `location/**` + `emergency/**` + `DemoApplication.kt` +
  test location/emergency. Ticket: `.ai/T6-codex-location-sos.md`. Đang chạy.
- **T7 — AGY Gemini `gemini-3.8-flash-medium` (Level 1)**: chỉ `HomeScreen.kt` + `HomeScreenPureUiTest.kt`.
  Ticket: `.ai/T7-agy-location-ui.md`. Chạy sau T6 (một lượt Gradle tại một thời điểm, không trùng file).
- **Review (Level 3)**: AGY `claude-sonnet-4-6` review diff T6/T7 sau khi cả hai land (1 reviewer).
- **Hermes**: contract, review, tích hợp, Gradle + emulator acceptance, tài liệu. Sẽ tự chạy build cuối.
- Không worker nào chạm vào file của worker khác; `HomeScreen.kt` chỉ thuộc T7.

## 6. TEST PLAN
Unit (JVM, `testDebugUnitTest`):
1. fine + cached FRESH → dùng cache ngay, không gọi current. 2. cached STALE + có fix mới → dùng fix mới.
3. GPS không có fix nhưng FUSED/NETWORK có → PASS (GPS unavailable ≠ location unavailable).
4. accuracy 150 m → vẫn trả về, không chặn SOS. 5. không có vị trí nào → cause rõ ràng, không throw.
6. permission DENIED → PERMISSION_DENIED, không gọi provider. 7. mọi provider tắt → PROVIDER_DISABLED.
8. cache cũ + current timeout → trả cache cũ với cause=TIMEOUT.
9. SEND_SMS granted → gateway được gọi; 10. SEND_SMS denied → step PERMISSION_MISSING, CALL/backend vẫn chạy.
11. CALL_PHONE granted + backend không STARTED → `ACTION_CALL` được tạo cho người thân ưu tiên.
12. CALL_PHONE denied → PERMISSION_MISSING, không crash, SMS vẫn gửi.
13. Critical: GPS unavailable + Wi-Fi/cell available = SOS chạy (SMS có link đúng + CALL).
14. Critical: không có location = SMS cảnh báo + CALL vẫn chạy.
15. Link Maps = `https://maps.google.com/?q=lat,lon` đúng tọa độ.
Emulator (`adb emu geo fix <lon> <lat>`): UI cập nhật vị trí, link đúng lat/lon, payload SOS đúng, không còn
lỗi chặn "ra chỗ thoáng". SMS/call thật trên emulator không khả dụng → chứng minh bằng invocation + test,
ghi rõ là không phải PASS thiết bị thật.

## 7. PROBLEMS / BLOCKERS
- Chưa có thiết bị thật/SIM/GPS: biên nhận SMS thật, cuộc gọi SIM thật, thời gian fix ngoài trời chưa kiểm chứng.
- Emulator không gửi được SMS/không gọi điện thật → evidence chỉ ở mức invocation/unit + UI.
- `play-services-location` là dependency mới (cần mạng Gradle lần đầu; Google Maven đã kiểm tra truy cập được,
  bản 21.4.0 có thật). Máy không có Google Play Services sẽ dùng nhánh LocationManager dự phòng.

## 8. NEXT ACTION
1. Chủ dự án chốt: có commit/push không (hiện KHÔNG commit gì).
2. Kiểm thử thực địa trên điện thoại thật có SIM + GPS theo `docs/permission-flow.md` §6 (đặc biệt bước 5 và 7).
3. Việc nên làm tiếp (chưa trong phạm vi lần này): bền vững hoá id sự kiện (bản DEMO reset bộ đếm → dispatch bị
   bỏ qua), `allSucceeded` coi SKIPPED là thành công đã sửa, và xem lại `EmergencyRecord.dispatchReport` khi
   dispatch lần hai.
4. Chỉ commit/push khi chủ dự án yêu cầu.

## 9. Lịch sử phân công (phiên trước, đã đóng)
- T2 Codex: logic quyền/location/SMS/call/maps (DONE). T3 AGY Gemini: Permission Center + UI (DONE).

## 11. Lịch sử phân công (phiên này)
- T6 — Codex (Level 2, một lượt Gradle): `LocationRepository`/`AndroidPlatformLocationSource`/controller/coordinator/
  `AndroidSimCallGateway`/`build.gradle.kts`/test. Sau đó nhận 1 vòng sửa từ review (`.ai/T6-codex-location-sos.md`,
  `.ai/T6-fixes-round2.txt`) — DONE, 170 test PASS (Hermes chạy lại xác nhận).
- T7 — AGY `gemini-3.8-flash-medium` (Level 1): chỉ `HomeScreen.kt` + `HomeScreenPureUiTest.kt`
  (`.ai/T7-agy-location-ui.md`). Worker thoát khi còn 1 tiến trình Gradle nền bị kill theo → Hermes tự build lại: PASS.
- Review L3 — AGY `claude-sonnet-4-6`, `--mode plan` (`.ai/review-prompt-T6.txt`): 0 BLOCKER / 4 MAJOR / 3 MINOR.
- Hermes: khảo sát có mục tiêu, chốt contract trong `EmergencyCore.kt`, chia T6/T7, review diff, tự chạy
  test/build đọc XML thật, viết runner acceptance emulator, chạy acceptance, cập nhật tài liệu.
- Kết quả đã xác minh: 147 unit test PASS, APK debug PASS, 11/11 check emulator (permission flow).

## T8 — Điều tra & sửa lỗi SOS runtime (Terra Medium / Codex) — ĐÃ SỬA, CHỜ THIẾT BỊ THẬT
PROVIDER: Codex `gpt-5.6-terra`, reasoning medium (kiểm quota trước khi giao; chưa cần CommandCode).
3 lượt: T8 (điều tra + sửa) → T8-fix1 (khôi phục quyết định D09) → T8-fix2 (làm suite xanh).
Ticket: `.ai/T8-terra-medium-sos-runtime.md`, `.ai/T8-fix1-d09-guard.md`, `.ai/T8-fix2-green-suite.md`.
ROOT_CAUSE (đã xác nhận bằng code + logcat + test đỏ trước khi sửa):
- H1 `AndroidPlatformLocationSource.enabledProviders()` coi Play Services là provider đang bật ⇒ khi công tắc Vị trí
  của máy TẮT, `PROVIDER_DISABLED` không bao giờ được trả về (chỉ TIMEOUT 8 s + copy sai). Nay tách hàm thuần
  `LocationProviderAvailability.enabled` (FUSED chỉ khi hệ thống thật sự bật vị trí).
- H2 nhánh đa SIM cũ CHẶN HẲN SMS khẩn cấp ("Có nhiều SIM; cần chọn SIM gửi rõ ràng"), trong khi `READ_PHONE_STATE`
  chưa bao giờ được xin lúc chạy. Nay `SmsSubscriptionChoice.resolve(requested, default)` → lấy SIM yêu cầu, không
  thì SIM mặc định, và ghi `subscriptionId=…` vào state + log.
- H3 `AndroidSimCallGateway` báo `STARTED` chỉ vì `Handler.post` thành công. Nay chờ kết quả `startActivity` thật
  (latch ≤ 2 s) → `FAILED` nếu không mở được.
- H4 guard D09 bị bỏ trong lượt T8 ⇒ SIM call chạy cả khi adapter thoại máy chủ đã `STARTED` (trái quyết định chủ
  dự án). ĐÃ KHÔI PHỤC ở T8-fix1 (`SKIPPED`, đúng thông điệp D09).
  **[LỊCH SỬ — ĐÃ BỊ THAY THẾ 2026-09-19]** Chủ dự án cập nhật D09: cuộc gọi SIM handset là nhánh ĐỘC LẬP, adapter
  máy chủ chỉ là phụ trợ best-effort ⇒ guard `SKIPPED` KHÔNG còn hiệu lực và `STARTED` KHÔNG suppress handset call.
  Dòng trên chỉ ghi lại trạng thái tại thời điểm T8.
- H5 `SmsPartAggregation` dùng `require(index in 0 until total)` bên trong BroadcastReceiver ⇒ có thể crash khi
  callback muộn; nay trả về kết quả cũ thay vì throw.
- H7 log phân mảnh; nay có một dòng `FallSafe/SOS` cho mỗi bước (eventId, step, status, elapsedMs, source,
  accuracyM, capability) — không log số điện thoại.
- H8 readiness chỉ xét quyền, bỏ qua phần cứng; nay `CapabilityPlatform.isSupported` (telephony calling/messaging).
- H6 BỊ BÁC BỎ: cache đã qua `validated()`/`freshness()` trước khi publish nên không phát fix rác.
BOTTLENECK ĐÃ GẶP: T8-fix1 để lại suite ĐỎ (1 test) rồi dừng ⇒ Hermes giao T8-fix2 sửa đúng kỳ vọng test và chạy lại.
FILES_INSPECTED: `location/AndroidPlatformLocationSource.kt`, `location/LocationRepository.kt`,
`emergency/EmergencyCore.kt`, `emergency/AndroidSmsManagerGateway.kt`, `emergency/AndroidSimCallGateway.kt`,
`permissions/CapabilityAccess.kt`, `permissions/AndroidCapabilityPlatform.kt`, `MainActivity.kt`, manifest, test SOS.
FILES_CHANGED: `location/LocationRepository.kt`, `location/AndroidPlatformLocationSource.kt`,
`emergency/EmergencyCore.kt`, `emergency/AndroidSmsManagerGateway.kt`, `emergency/AndroidSimCallGateway.kt`,
`permissions/CapabilityAccess.kt`, `permissions/AndroidCapabilityPlatform.kt`,
`app/src/test/.../location/BestAvailableLocationRepositoryTest.kt`, `.../emergency/EmergencyLogicTest.kt`,
`.../emergency/SosCallStepTest.kt`, `.../emergency/SosDispatchReportTest.kt`, `.../permissions/CapabilityAccessTest.kt`,
`android/scripts/sos-location-acceptance.py`. Không worker nào chạm UI/`HomeScreen.kt`.
TESTS_RUN (Hermes tự chạy lại, không tin báo cáo worker): `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon` — chạy 2 lần (sau T8 và sau T8-fix2).
TEST_RESULTS: **27 suite / 175 test / 0 failure / 0 error / 0 skip** đọc từ
`app/build/test-results/testDebugUnitTest/TEST-*.xml`; APK `app/build/outputs/apk/debug/app-debug.apk` 12.566.848 B,
sha256 `2c16eca3f14a7b216042e573bf17a3b8275bfd05f4997909b8f5a7a5cd1daa06`, đã cài lên emulator.
EMULATOR (Hermes chạy trên chính artifact cuối):
- `scripts/sos-location-acceptance.py`: `critical-sos-flow` PASS 34.19 s, `location-card` PASS 42.76 s (2/2).
  Trong đó: `source=FUSED accuracy=5.0`; SMS thật trong `content://sms/sent` với `https://maps.google.com/?q=10.8231,106.6296983`;
  `SIM_CALL:SUCCESS` + `FallSafe/CALL: status=STARTED`; 5 bước báo cáo đều có mặt.
- Degraded location-off (do Hermes viết thêm, `/tmp/t8-degraded-location-off.py`): công tắc Vị trí TẮT + ngã thật →
  `LOCATION=UNAVAILABLE (cause=PROVIDER_DISABLED)`, `SMS=SUCCESS`, `SIM_CALL=SUCCESS`, `MAP_LINK=SKIPPED`
  ⇒ SMS/CALL không còn phụ thuộc vị trí (đóng lỗ hổng bằng chứng mà T8 còn để mở).
EVIDENCE: `docs/evidence/sos-location/` (results.json, logcat-dispatch.txt, sent-sms-provider.txt, sos-state.txt,
logcat-location-during-countdown.txt) và `docs/evidence/sos-runtime/hermes-degraded-location-off*`.
CURRENT_FAILURE / BLOCKER: KHÔNG có thiết bị thật ⇒ **không được kết luận DEVICE VERIFIED**. Chưa kiểm chứng:
biên nhận giao SMS của nhà mạng, cuộc gọi SIM thật qua mạng di động, đa SIM thật, GNSS ngoài trời, FGS khi khoá màn hình.
NEXT_ACTION: (1) chủ dự án chốt có commit/push không (hiện KHÔNG commit gì); (2) chạy checklist
`docs/permission-flow.md` §6 trên điện thoại thật có SIM + GPS, ưu tiên bước SMS/CALL và nhánh location-off;
(3) ghi kết quả thật vào `docs/test-report.md`.
DO_NOT_REPEAT: Không dùng Java 25 cho Gradle (fail trước khi cấu hình project) — dùng JDK 17. Không khôi phục
"guard D09" cũ (SKIPPED khi máy chủ `STARTED`): D09 đã được chủ dự án cập nhật 2026-09-19 — cuộc gọi SIM trên
handset là nhánh ĐỘC LẬP, backend chỉ là phụ trợ best-effort.
Không để worker dừng khi suite còn đỏ. Giữ nguyên thay đổi chưa commit của người dùng.

## T11 — LOC-01: Location OFF→ON phải tự phục hồi không cần khởi động lại app
ROOT_CAUSE: Quyền vị trí (FINE/COARSE) và trạng thái "Location Services" là hai sự thật độc lập. App cũ chỉ đánh giá
lại đúng một chuyển tiếp PERMISSION_DENIED→đã cấp trong `refreshPermissionTruth()`; cache `PROVIDER_DISABLED`
(hoặc TIMEOUT/NO_FIX) không bao giờ được đánh giá lại nên khi người dùng bật Vị trí từ Quick Settings/Settings rồi
quay lại app, state vẫn là "Vị trí đang tắt"/"Chưa có GPS" cho tới khi khởi động lại app.
FILES_INSPECTED: `location/AndroidEmergencyLocationController.kt`, `location/AndroidPlatformLocationSource.kt`,
`location/LocationRepository.kt`, `emergency/EmergencyCore.kt`, `DemoApplication.kt`, `MainActivity.kt`,
`api/SyncCoordinator.kt`, `location/BestAvailableLocationRepositoryTest.kt`, `location/LocationIntentAndProviderTest.kt`.
FILES_CHANGED: thêm `location/LocationRecovery.kt` (pure: LocationSignal/LocationAvailability/LocationRecoveryDecision/
LocationRecoveryInput/LocationRecoveryPolicy), `location/LocationRecoveryEngine.kt` (pure re-check + lookup),
`location/SystemLocationAvailabilityObserver.kt` (ContentObserver best-effort); sửa
`location/AndroidEmergencyLocationController.kt` (refreshPermissionTruth = full re-check, onSystemLocationChanged,
close, markAttempt, source injectable), `DemoApplication.kt` (wiring observer + close). Thêm test
`location/LocationRecoveryPolicyTest.kt` (11) + `location/LocationRecoveryEngineTest.kt` (6).
TESTS_RUN: `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest --tests
'vn.nckh27pa.fallsafe.location.*' --rerun-tasks --no-daemon` (red trước, xanh sau); full
`testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`.
TEST_RESULTS: **30 suite / 206 test / 0 failure / 0 error / 0 skip** đọc từ `TEST-*.xml`; APK debug assembleDebug PASS.
Baseline thực tế trên repo hiện tại (đã có thêm test telephony từ commit trước) = 28 suite / 189 test; task mô tả
"27/175" là snapshot cũ. Không test cũ nào bị yếu/xoá. Test mới: 6 (engine: startupWithLocationOnAcquiresAndPublishesFix,
startupWithLocationOffIsProviderDisabledWithZeroProviderCalls, offToOnRecoversWithSingleAcquireAndLookup,
resumeAfterSettingsChangeRecovers, repeatedResumeHasNoOverlapAndRespectsCooldown, permissionDeniedThenGrantedRecovers)
+ 11 (policy).
CURRENT_FAILURE: KHÔNG — unit + build xanh. Hành vi emulator/thiết bị thật chưa được kiểm chứng bởi worker này (không
dùng adb/emulator theo quy tắc).
NEXT_ACTION: Coordinator chạy acceptance OFF→ON trên `emulator-5554`; chủ dự án chạy trên điện thoại thật (GPS thật,
app Cài đặt thật, toggle Quick Settings thật).
DO_NOT_REPEAT: Không đặt case `requestInFlight` làm lần đánh giá đầu tiên trong test (nó "ăn" trạng thái first-changed);
mô hình đúng thứ tự: khởi động (changed=true → Acquire) rồi mới resume khi in-flight. Không dùng Java 25 cho Gradle.

