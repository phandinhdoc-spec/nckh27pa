# CommandCode — SOS runtime investigation

Ngày: 2026-09-19 · WORKDIR: `/home/pdd/nckh27pa/android` · Không sửa production code trong bước này.

WORKER ĐIỀU TRA: `deepseek/deepseek-v4-pro` (CommandCode CLI, headless `-p`, effort high) — model rẻ hơn
Astra/Claude trong cùng gói, đủ khả năng đọc và trace Kotlin/Android.
Nguồn đối chiếu trước đó: `.ai/luna-sos-runtime-investigation.md` (giữ nguyên, không sửa).

Lưu ý baseline: working tree có thay đổi CHƯA COMMIT (location repository/provider + AndroidSimCallGateway +
gateway/Core tests). Báo cáo này mô tả code đang có trong working tree đó và đối chiếu với `HEAD` để chỉ ra
đường chạy CŨ đã bị thay thế. Mọi số dòng dưới đây là của working tree hiện tại.

---

## 1. CURRENT PIPELINE (luồng runtime thực tế hiện nay)

```text
HomeScreen.kt:1640-1651        giữ nút SOS 3 giây → controller.sos()
DemoApplication.kt:449-450     sos() → help() → session.needHelp()
DemoApplication.kt:415-421     refresh() → snapshot mới → onSyncStateChanged()

SyncCoordinator.kt:62-85       callback trạng thái (chạy trên main)
  :68-69   VERIFYING|ALERTING|AWAITING_HELP + EventLocationRefreshGate → locationController.onVerifyingStarted()
  :71      VERIFYING → emergency.beginVerifying(remoteId)   (id sự kiện ổn định qua EventIdentityStore)
  :73      ALERTING|AWAITING_HELP → emergency.timeout(remoteId, contacts, displayName, locationState?.fix)

EmergencyCore.kt:249-292       EmergencyCoordinator.dispatch  (idempotent: :253 nếu record.dispatched → return)
  :255     contacts = chỉ receiveSos + ContactValidator.normalize + sort isPrimary/callPriority
  :260-262 Bước LOCATION  (fix != null → SUCCESS, ngược lại UNAVAILABLE) — KHÔNG return/abort
  :264     dispatchSms()            → SMS độc lập
  :266     dispatchSimCall()        → cuộc gọi SIM độc lập  (đặt TRƯỚC khi gọi backend)
  :267-278 backend voice (EmergencyBackendGateway → SyncCoordinator.requestVoice) — chỉ là 1 nhánh
  :284-288 Bước MAP_LINK

SMS    EmergencyCore.kt:316-338 dispatchSms → AndroidSmsManagerGateway.send :46-75
         :52 feature telephony messaging   :52 quyền SEND_SMS   :53-56 chọn subscription (SIM yêu cầu → mặc định)
         :61 divideMessage (multipart)     :70 sendMultipartTextMessage  → SmsPartAggregation :156-181
CALL   EmergencyCore.kt:294-313 dispatchSimCall → AndroidSimCallGateway.call :18-62
         :21 quyền CALL_PHONE   :23 feature FEATURE_TELEPHONY_CALLING   :25 số trống
         :28 Intent.ACTION_CALL + Uri "tel:"   :63-72 kết quả startActivity thật (không báo STARTED chỉ vì Handler.post)
LOCATION
       AndroidEmergencyLocationController.kt:28-44 onVerifyingStarted
         → BestAvailableLocationRepository.getBestAvailableLocation(8_000 ms) trên coroutine riêng (SupervisorJob)
       LocationRepository.kt:37-84  bounded budget: quyền → provider → cache lastKnown → FUSED → GPS → NETWORK
       AndroidPlatformLocationSource.kt:33-37 permission(); :44-59 enabledProviders(); :60-78 lastKnown();
         :79-98 current() = FusedLocationProviderClient.getCurrentLocation (FUSED/BALANCED) | LocationManager (GPS/network)
       late fix: AndroidEmergencyLocationController.kt:45-55 publish() → onFix
         → DemoApplication.kt:87 emergencyCoordinator.updateLocation(identity.id(eventId), fix)
         → EmergencyCore.kt:339-361 gửi TỐI ĐA 1 SMS bổ sung/contact (supplementSentContacts)
```

---

## 2. ROOT CAUSE — LOCATION

### L1 — GPS-only / one-shot (đã bị thay thế trong working tree)

```text
file:line      HEAD: app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidEmergencyLocationController.kt
               (đường cũ) — working tree hiện tại: :28-44
class/function AndroidEmergencyLocationController.onVerifyingStarted
problem        Đường cũ chọn MỘT provider qua LocationProviderSelector, ưu tiên GPS, gọi một lần
               LocationManager.getCurrentLocation rồi chờ hết timeout. Không có Fused Location, không ưu tiên cache.
why            Trong nhà/không thấy vệ tinh ⇒ GPS một phát gần như luôn thất bại ⇒ SOS mất vị trí, và
               copy UI sinh ra thông báo kiểu “Ra nơi thoáng hơn…” khiến người dùng tưởng cứu hộ bị dừng.
required fix   Dùng pipeline best-available: cache mới → FUSED → GPS → NETWORK, timeout bounded, không lấy
               accuracy làm điều kiện thành công.  → ĐÃ CÓ trong working tree.
```

### L2 — Play Services bị coi là provider đang bật (đã sửa)

```text
file:line      app/src/main/java/vn/nckh27pa/fallsafe/location/LocationRepository.kt:10-23
               app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidPlatformLocationSource.kt:44-59
class/function LocationProviderAvailability.enabled / AndroidPlatformLocationSource.enabledProviders
problem        Đường cũ kiểm tra Play Services availability là đủ để coi location dùng được, không kiểm tra
               công tắc Vị trí của hệ thống.
why            Khi công tắc Vị trí TẮT, app không trả PROVIDER_DISABLED mà chờ hết 8 s rồi báo TIMEOUT ⇒
               sai nguyên nhân và chậm 8 giây trong tình huống khẩn cấp.
required fix   FUSED chỉ khi system location thật sự bật VÀ (Play Services hoặc platform fused); NETWORK là fallback
               luôn được xét.  → ĐÃ CÓ (hàm thuần, test được).
```

### L3 — Điểm cần giữ (đã đúng, không cần sửa)

```text
file:line      AndroidEmergencyLocationController.kt:45-55 publish(); :56-67 explanation()/remediation()
problem        Không có (kiểm tra phòng ngừa).
why            :66 và :68 là HAI nơi duy nhất còn chuỗi “Ra nơi thoáng hơn…”, và cả hai chỉ nằm trong
               LocationState.remediation/explanation — không phải điều kiện return.
required fix   Không sửa. Chỉ cần bảo đảm không worker nào biến remediation thành điều kiện dừng dispatch.
```

### L4 — Chưa có: provider PASSIVE / requestLocationUpdates nền

```text
file:line      AndroidPlatformLocationSource.kt:79-98 current()
problem        Chỉ có current(kind) + lastKnown(); không có passive listener.
why            Không phải lỗi: cache lastKnown + NETWORK current đã bao phủ tình huống trong nhà; passive
               làm tăng chi phí pin và độ phức tạp vòng đời.
required fix   Không bắt buộc. Ghi nhận là lựa chọn kỹ thuật, không đưa vào phạm vi sửa.
```

---

## 3. ROOT CAUSE — SMS

### S1 — SMS từng bị buộc vào nhánh location/backend (đã cắt)

```text
file:line      app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt:249-292 (dispatch), :316-338 (dispatchSms)
class/function EmergencyCoordinator.dispatch / dispatchSms
problem        Đường cũ phát SMS gắn với kết quả location/đường gửi location; khi không có fix pipeline có
               thể kết thúc trước khi gọi gateway.
why            Quyền SEND_SMS PASS chỉ chứng minh quyền đã cấp, không chứng minh SmsManager được gọi. Location
               là dữ liệu bổ sung, KHÔNG phải điều kiện tiên quyết của kênh cầu cứu.
required fix   Luôn gọi dispatchSms sau khi tạo record, với fix=null khi chưa có vị trí và thông điệp fallback
               không bịa tọa độ.  → ĐÃ CÓ (:137 formatter fallback, :264 gọi vô điều kiện).
```

### S2 — Vẫn còn: số điện thoại TRỐNG được đưa thẳng xuống gateway

```text
file:line      EmergencyCore.kt:319-330 (vòng lặp gửi theo contact)
class/function EmergencyCoordinator.dispatchSms
problem        Contact có `phone` rỗng vẫn đi vào EmergencySmsGateway.send; chỉ tới tầng radio mới thất bại.
why            Sai số liệu báo cáo (1 contact rác làm bước SMS có thể thành PARTIAL/FAILED kèm lý do của tầng
               radio) và tốn một lần gọi hệ thống vô nghĩa.
required fix   Bỏ qua contact có số trống ngay tại coordinator, đếm riêng và nêu trong detail; KHÔNG lọc theo
               regex VN (số quốc tế/short code hợp lệ vẫn phải gửi). Không hard-code số.
               → Thuộc phạm vi T9 (Astra/CommandCode continuation).
```

### S3 — Vẫn còn: SMS bổ sung có thể bị gửi dù SMS đầu ĐÃ có tọa độ

```text
file:line      EmergencyCore.kt:339-361 (updateLocation)
class/function EmergencyCoordinator.updateLocation
problem        Chỉ cần record đã dispatch và có fix đến sau là gửi SMS bổ sung — kể cả khi SMS đầu đã chứa tọa độ.
why            Yêu cầu chỉ rõ: SMS bổ sung tồn tại cho trường hợp “SMS đầu chưa có vị trí, vị trí đến sau”.
               Nếu không gate, người thân nhận 2 tin cho cùng một sự kiện ⇒ nhiễu và có thể coi là spam.
required fix   Ghi nhớ tại thời điểm dispatch là đã có fix hay chưa (field mặc định để tương thích Gson với
               record đã lưu) và chỉ gửi supplement khi lần dispatch đó KHÔNG có fix. Vẫn giữ tối đa 1
               supplement/contact/sự kiện.  → Thuộc phạm vi T9.
```

### S4 — Chống lặp đã đúng (giữ nguyên)

```text
file:line      EmergencyCore.kt:205-207 (smsSentContacts/supplementSentContacts), :320, :345
class/function EmergencyRecord + dispatchSms + updateLocation
problem        Không có.
why            Hai set riêng biệt ⇒ tối đa 1 SMS đầu + 1 SMS bổ sung cho mỗi contact cho mỗi sự kiện, kể cả
               khi callback vị trí lặp nhiều lần.
required fix   Không sửa.
```

---

## 4. ROOT CAUSE — CALL

### C1 — SOS chỉ gọi backend, không gọi handset (đã thêm đường thật)

```text
file:line      HEAD: EmergencyCore.kt (dispatch chỉ có backend.startVoice) — working tree: :266, :294-313
               app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSimCallGateway.kt:18-62
class/function EmergencyCoordinator.dispatchSimCall / AndroidSimCallGateway.call
problem        Đường cũ chỉ gọi backend.startVoice(eventId). ManualSimCallFallback chỉ reachable từ nút gọi tay.
why            Backend nhận yêu cầu KHÔNG phải bằng chứng handset đã gọi; trên điện thoại thật không có SIM call.
required fix   Gateway cuộc gọi thật dùng ACTION_CALL + "tel:", chọn contact ưu tiên hợp lệ, gọi đúng một lần,
               map CallStatus → bước SIM_CALL.  → ĐÃ CÓ (`DemoApplication.kt:80` wiring, untracked file mới).
```

### C2 — STARTED giả do Handler.post (đã sửa)

```text
file:line      AndroidSimCallGateway.kt:29-50, :63-72
class/function AndroidSimCallGateway.call / launch
problem        Đường cũ báo STARTED chỉ vì Handler.post thành công, không biết startActivity có thật sự mở được.
why            Báo cáo “đã gọi” khi thực tế không có cuộc gọi ⇒ che mất lỗi runtime đúng như triệu chứng ban đầu.
required fix   Gọi từ main thì lấy kết quả trực tiếp; gọi từ thread khác thì chờ kết quả thật qua latch ≤ 2 s
               và trả FAILED nếu không mở được.  → ĐÃ CÓ.
```

### C3 — ĐÃ GIẢI QUYẾT: chủ dự án chọn phương án (a), cập nhật D09 (2026-09-19)

```text
file:line      docs/decisions.md:18 (D09) + docs/next-gate.md:14 + docs/permission-flow.md §4 (đã cập nhật)
               ↔  EmergencyCore.kt:266, :294-313  (giữ nguyên)
               ↔  app/src/test/.../emergency/SosCallStepTest.kt:75-87 (giữ nguyên)
               ↔  app/src/test/.../emergency/SosDispatchReportTest.kt:22-24 (giữ nguyên)
class/function EmergencyCoordinator.dispatch / dispatchSimCall
problem        Trước đây D09 chốt: SIM call CHỈ khi adapter thoại backend CHƯA tiếp nhận (`STARTED`); nếu
               backend đã tiếp nhận thì bước SIM_CALL = SKIPPED. Code + test luôn gọi handset ⇒ mâu thuẫn tài liệu.
why            Backend không phải bằng chứng handset đã gọi; cuộc gọi SIM là kênh cứu hộ vật lý trên máy.
required fix   ĐÃ CHỐT (a): cập nhật tài liệu theo hành vi hiện tại. `STARTED` của backend KHÔNG suppress
               handset call; backend lỗi/vắng mặt/SMS/vị trí đều không quyết định cuộc gọi SIM. Không sửa
               production code, không flip test.
```

### C4 — Điểm cần giữ

```text
file:line      EmergencyCore.kt:302 (thiếu quyền CALL → PERMISSION_MISSING, không crash), :303 (không có
               contact hợp lệ → UNAVAILABLE), :304-306 (SecurityException/Exception → FAILED)
problem        Không có.
why            Mọi nhánh thất bại của CALL đều chỉ ảnh hưởng bước SIM_CALL — SMS đã được gửi ở :264 trước đó.
required fix   Không sửa.
```

---

## 5. PIPELINE BLOCKER

**Blocker lịch sử (đã bị cắt trong working tree):** SMS và CALL nằm SAU nhánh location trong `dispatch` và
location cũ là GPS-only/one-shot, nên một lần GPS thất bại có thể kết thúc pipeline trước khi bất kỳ kênh cầu
cứu nào được gọi. Chuỗi hiển thị “Ra nơi thoáng hơn…” (`AndroidEmergencyLocationController.kt:66, :68`) là
triệu chứng người dùng thấy, không phải nguyên nhân — nó chỉ nằm trong `remediation`.

**Trạng thái hiện tại:** không còn điểm nào để location chặn SMS/CALL.
- `EmergencyCore.kt:260-262` chỉ ghi nhận trạng thái LOCATION, không return.
- `EmergencyCore.kt:264` gọi SMS trước và độc lập; `:266` gọi SIM call trước cả backend.
- `:253` `if(record.dispatched) return` bảo đảm một sự kiện chỉ có MỘT chuỗi cầu cứu, kể cả khi callback
  trạng thái lặp lại hoặc Compose recomposition.
- `:267-278` backend chỉ là một nhánh; lỗi/timeout backend không ảnh hưởng SMS/CALL/location local.
- `EmergencyPersistence.kt:14-20` đổi `session` mỗi process nên record đã dispatch của tiến trình cũ không
  bao giờ bị tái sử dụng.

**Blocker còn lại (nhỏ, thuộc T9):** S2 (số trống) và S3 (gate SMS bổ sung) — không làm mất kênh cầu cứu,
nhưng làm báo cáo sai và có thể gửi thêm 1 SMS không cần thiết.

---

## 6. ASTRA EDIT SCOPE

Được sửa (chỉ khi cần để hoàn tất hành vi fail-soft):

```text
app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt
app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyPersistence.kt   (chỉ nếu đổi field record)
app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSmsManagerGateway.kt
app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSimCallGateway.kt
app/src/main/java/vn/nckh27pa/fallsafe/location/LocationRepository.kt
app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidPlatformLocationSource.kt
app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidEmergencyLocationController.kt
app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt                  (chỉ wiring gateway/repository/callback)
app/src/test/java/vn/nckh27pa/fallsafe/emergency/*.kt                      (SosCallStepTest, SosDispatchReportTest, EmergencyLogicTest)
app/src/test/java/vn/nckh27pa/fallsafe/location/*.kt
```

Class/function cần sửa hoặc xác minh:

```text
EmergencyCoordinator.dispatch / dispatchSms / dispatchSimCall / updateLocation / beginVerifying / cancel / timeout
EmergencyRecord (thêm cờ “dispatch đã có fix”) + SharedPrefsEmergencyStore (tương thích Gson)
AndroidSmsManagerGateway.send (chọn subscription + multipart + result)
AndroidSimCallGateway.call / launch (ACTION_CALL, kết quả startActivity thật)
BestAvailableLocationRepository.getBestAvailableLocation (thứ tự provider + timeout)
AndroidPlatformLocationSource.permission / enabledProviders / lastKnown / current
AndroidEmergencyLocationController.onVerifyingStarted / publish / update / openMyLocation
DemoController.sos / help / refresh; SyncCoordinator callback :62-85
```

KHÔNG được đụng:

```text
.ai/luna-sos-runtime-investigation.md, .ai/astra-sos-runtime-fix.md (chỉ Hermes ghi), docs/**
app/src/main/java/vn/nckh27pa/fallsafe/HomeScreen.kt          (trừ wiring runtime bắt buộc)
MainActivity.kt, AndroidManifest.xml, app/build.gradle.kts    (đã đủ quyền/dependency — không sửa)
build/, .gradle/, generated files                             (không đọc, không sửa)
Không commit, không push, không reset/stash/checkout thay đổi của người dùng.
```

---

## 7. EXPECTED PIPELINE

```text
SOS (countdown kết thúc hoặc người dùng kích hoạt)
 ├── [async] location lookup bounded 8 s: cache → FUSED → GPS → NETWORK
 ├── [ngay] SMS khẩn cấp tới mọi contact receiveSos (kể cả vị trí chưa có)
 └── [ngay] cuộc gọi SIM thật (ACTION_CALL, contact ưu tiên) — độc lập backend/SMS/location

Location OK trước dispatch : SMS chứa tọa độ + https://maps.google.com/?q=<lat>,<lon>
Location FAIL (TIMEOUT/UNAVAILABLE/PERMISSION_DENIED/PROVIDER_DISABLED):
    → KHÔNG abort, KHÔNG return
    → SMS fallback không có tọa độ, không bịa vị trí
    → CALL vẫn chạy nếu có CALL_PHONE + contact hợp lệ
    → backend vẫn chạy nếu cấu hình
Location đến SAU khi SMS đầu đã gửi (và SMS đầu KHÔNG có vị trí):
    → đúng MỘT SMS bổ sung “Đã xác định được vị trí: <maps-link>”
    → cập nhật bước MAP_LINK trong báo cáo
Backend lỗi/timeout: không ảnh hưởng SMS/CALL/location local.
Thiếu 1 quyền: chỉ bước tương ứng báo PERMISSION_MISSING.
```

---

## 8. TEST PLAN

Case bắt buộc và nơi đang bao phủ (`app/src/test/.../emergency/SosCallStepTest.kt` trừ khi ghi khác):

```text
1  location success → SMS + CALL        gpsUnavailableNetworkFixProducesMapsSmsAndSimCall (có tọa độ + Maps URL)
2  location timeout → SMS + CALL        BestAvailableLocationRepositoryTest + missingLocationBlocksNeitherSmsNorSimCall
3  location unavailable → SMS + CALL    missingLocationBlocksNeitherSmsNorSimCall; SosDispatchReportTest.disabledLocationProvider...
4  location permission denied → SMS+CALL CẦN BỔ SUNG (chỉ có test repository cho PERMISSION_DENIED)
5  SMS failure → CALL vẫn chạy          smsFailureStillDialsAndCallExceptionIsReported
6  CALL failure → SMS vẫn chạy          everyGatewayFailureMapsDetailWithoutRetry
7  backend failure → SMS + CALL local   backendNotStartedAttemptsHandsetCall, backendStartedDoesNotSuppressHandsetCall (xem C3)
8  late location → đúng 1 supplement    lateLocationSendsOnlyOneSupplementAcrossCoordinatorRecreation
9  callback vị trí lặp → không spam     lateLocationSendsOnlyOneSupplementAcrossCoordinatorRecreation (repeat(3))
10 SOS lặp/recomposition → không nhân đôi  callsFirstEligibleContactExactlyOnceEvenOnRepeatedDispatch
   extra: no eligible recipient         noEligibleRecipientReportsUnavailableWithoutDialing
   extra: SMS permission denied → CALL   smsPermissionDeniedStillCallsSim
   CẦN BỔ SUNG: số trống không tạo lần gọi radio thứ hai; SMS đầu ĐÃ có fix → KHÔNG supplement
```

Chạy bắt buộc (Java 25 làm Gradle fail trước khi cấu hình project → dùng JDK 17):

```bash
cd /home/pdd/nckh27pa/android
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest --console=plain
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew assembleDebug --console=plain
```

Emulator/unit test KHÔNG chứng minh được: giao SMS qua nhà mạng, cuộc gọi SIM thật, đa SIM thật, GNSS ngoài
trời. Các mục đó vẫn phải kiểm thử trên điện thoại thật (xem `.ai/astra-sos-runtime-fix.md`).
