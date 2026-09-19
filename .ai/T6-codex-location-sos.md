# T6 — Codex (Level 2 implementation): location pipeline + SOS → SMS/CALL resilience

STATUS: DISPATCHED
OWNER: Codex (single primary implementer for Android logic)
GOAL: SOS must obtain a usable location without a satellite fix and must always attempt SMS + CALL
regardless of location outcome.

## Baseline
- Branch `main`, commit a995d8b. Work in `/home/pdd/nckh27pa/android`.
- Build/test command (do NOT add `--rerun-tasks`, keep `--no-daemon`):
  `cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon`
- Assemble: `./gradlew assembleDebug --no-daemon`
- Baseline before your work: 147 unit tests PASS, assembleDebug PASS.
- You own the ONLY Gradle build in this phase. No other worker runs Gradle while you work.

## Root cause you are fixing (verified by targeted reading, do not re-discover)
1. `location/LocationResolution.kt:11-16` `LocationProviderSelector.choose` returns `GPS` whenever
   `hasFine && gpsEnabled`. The master location switch makes `gpsEnabled` true, so `NETWORK_PROVIDER`
   and the `fused` provider are effectively dead code on a real phone. One single-shot provider attempt.
2. `location/AndroidEmergencyLocationController.kt:70-86` requests ONLY that provider with an 8 s timeout;
   `getLastKnownLocation` (line 107) is consulted only AFTER failure, never first.
3. No FusedLocationProviderClient at all: `com.google.android.gms:play-services-location` is absent from
   `app/build.gradle.kts`. The string `"fused"` at line 173 is the platform `LocationManager` provider.
4. Failure text is presented as a blocker: lines 69, 133, 135 and `EmergencyCore.kt:44`
   ("Di chuyển ra nơi thoáng và thử lại."). Indoors (no satellite fix) this is exactly the reported
   symptom "Ra chỗ thoáng và thử lại".
5. SOS flow never dials the handset: `EmergencyCoordinator.dispatch` (EmergencyCore.kt:198-232) runs only
   `backend.startVoice(eventId)`. `ManualSimCallFallback` (ACTION_CALL) exists but is reachable only from
   the manual "GỌI NGƯỜI THÂN" dialog.

## Frozen contract — already applied by Hermes, DO NOT change names or semantics
`app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt` now has:
```kotlin
const val SOS_LOCATION_TIMEOUT_MS = 8_000L
enum class LocationSource { PHONE, ESP32_GNSS, FUSED, GPS, NETWORK, CACHED, UNKNOWN }
data class LocationLookup(val fix: LocationFix?, val cause: LocationFailureCause?, val fromCache: Boolean, val elapsedMs: Long)
interface LocationRepository { suspend fun getBestAvailableLocation(timeoutMs: Long = SOS_LOCATION_TIMEOUT_MS): LocationLookup }
enum class CallStatus { STARTED, PERMISSION_MISSING, UNAVAILABLE, FAILED }
data class CallDispatchState(val status: CallStatus, val detail: String? = null)
fun interface EmergencyCallGateway { fun call(phone: String): CallDispatchState }
enum class SosStep { LOCATION, SMS, VOICE_CALL, SIM_CALL, MAP_LINK }   // SIM_CALL added
```
`LocationFix` now exposes `mapsUrl` = `https://maps.google.com/?q=$lat,$lon` and `geoUri` =
`geo:$lat,$lon?q=$lat,$lon`. These are the ONLY definitions of those links; delete any duplicated
inline link building (e.g. the inline `geo:` in the controller's `openMyLocation`).

## Deliverables

### A. Pure, JVM-testable location pipeline
Create `app/src/main/java/vn/nckh27pa/fallsafe/location/LocationRepository.kt`:
```kotlin
enum class LocationProviderKind { FUSED, GPS, NETWORK }
enum class LocationPermission { PRECISE, APPROXIMATE, DENIED }

/** Port for the Android layer (and fakes in tests). No android.* imports in this file. */
interface PlatformLocationSource {
    fun permission(): LocationPermission
    fun enabledProviders(): Set<LocationProviderKind>
    suspend fun lastKnown(): LocationFix?                                   // best cached across sources
    suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix?  // null on timeout/error
}

class BestAvailableLocationRepository(
    private val source: PlatformLocationSource,
    private val nowMs: () -> Long = System::currentTimeMillis
) : LocationRepository { /* exact algorithm below */ }
```
Algorithm, in this order (each step bounded by the remaining budget; the whole call must never exceed
`timeoutMs` and must never throw):
1. `permission() == DENIED` → `LocationLookup(null, PERMISSION_DENIED, fromCache=false, elapsed)`.
2. `enabledProviders()` empty → `LocationLookup(null, PROVIDER_DISABLED, false, elapsed)`.
3. `lastKnown()` non-null AND `freshness(nowMs()) == FRESH` → return it immediately,
   `fromCache=true, cause=null`. (Cached-first fast path.)
4. Request current location over the enabled kinds in priority order **FUSED, then GPS, then NETWORK**
   (skip kinds not in `enabledProviders()`), giving each attempt the remaining budget. First valid fix
   returned wins: `fromCache=false, cause=null`, and `LocationSource` set per kind.
   A cached-but-stale fix MUST NOT be returned before step 4 has been attempted.
5. No current fix but a stale `lastKnown()` exists → `LocationLookup(stale, TIMEOUT, true, elapsed)`.
6. Nothing at all → `LocationLookup(null, TIMEOUT if any attempt timed out else NO_FIX, false, elapsed)`.

Never apply an accuracy threshold: any fix with finite accuracy (including 100–200 m) is usable.
Use `LocationFix.validated(...)` for every value you accept.

### B. Android implementation of the port
Create `app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidPlatformLocationSource.kt`:
- Primary: `com.google.android.gms.location.FusedLocationProviderClient`
  - cached: `lastLocation` (await via `kotlinx.coroutines.tasks.await`-style helper or a
    `suspendCancellableCoroutine` + `addOnSuccessListener`; do not add kotlinx-coroutines-play-services
    unless you actually add the dependency and it resolves).
  - current: `getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, cancellationToken)` for FUSED, and a
    second, cheaper `Priority.PRIORITY_BALANCED_POWER_ACCURACY` pass must be reachable through the
    NETWORK kind so Wi-Fi/cell location is used when satellite positioning is unavailable.
  - Report Play Services availability with `GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)`.
- Fallback (no Play Services): platform `LocationManager` — `GPS_PROVIDER` for GPS,
  `NETWORK_PROVIDER` for NETWORK, `LocationManager.FUSED_PROVIDER` (API ≥ 31, provider name `"fused"`)
  for FUSED, using `getCurrentLocation(...)` on API ≥ 30 and `requestSingleUpdate` below.
- `permission()`: PRECISE if ACCESS_FINE_LOCATION granted, APPROXIMATE if only COARSE, else DENIED.
- `enabledProviders()` must reflect `isProviderEnabled` per provider; FUSED counts as enabled when Play
  Services is available (or the platform `fused` provider exists) — never require GPS to be enabled.
- Map raw fixes to `LocationSource.FUSED / GPS / NETWORK`; cached fixes returned from `lastKnown()`
  carry `LocationSource.CACHED`. Keep `ESP32_GNSS` as the external-source value (still unused).
- Add `implementation("com.google.android.gms:play-services-location:21.4.0")` to `app/build.gradle.kts`
  (verified available on Google Maven; change the version only if that exact one fails to resolve).

### C. Controller rewrite (keep the class and its interface)
`location/AndroidEmergencyLocationController.kt` keeps implementing `EmergencyLocationController` with the
same public members (`locationState`, `lastMapOpenReason`, `onVerifyingStarted`, `acceptEsp32Gnss`,
`openMyLocation`, `requestManualShare`, `confirmManualShare`) and same constructor parameter shape, but:
- `onVerifyingStarted()` calls `LocationRepository.getBestAvailableLocation(SOS_LOCATION_TIMEOUT_MS)` on a
  coroutine scope, keeps the existing generation/guard so a late result from a cancelled attempt is ignored,
  then publishes `LocationState`.
- `BoundedLocationResolutionAttempt` is no longer needed by the controller; keep the class in
  `EmergencyCore.kt` only if it is still used (it must keep its passing tests
  `BoundedLocationResolutionTest`), otherwise delete both together — your choice, but leave no unused code.
- Publishing rules: with a fix, `cause=null` when fresh/current; `cause=TIMEOUT` plus
  `explanation="Đang dùng vị trí hợp lệ gần nhất; cảnh báo vẫn tiếp tục."` when served from cache.
  With no fix, `explanation(...)` keeps its truthful wording but the `remediation` for `NO_FIX`/`TIMEOUT`
  MUST become an improvement hint, not a blocker: `"Ra nơi thoáng hơn có thể tăng độ chính xác; cảnh báo vẫn được gửi."`
- `openMyLocation()` must use `fix.geoUri` / `fix.mapsUrl` (no inline URI building).
- Location card default text in `LocationState`: `explanation="Đang xác định vị trí..."`, `remediation=null`
  (the waiting text must not read as a failure).

### D. SOS flow: never let location failure block SMS or CALL
In `EmergencyCoordinator` (`emergency/EmergencyCore.kt`):
- New constructor parameter `private val call: EmergencyCallGateway = EmergencyCallGateway { CallDispatchState(CallStatus.UNAVAILABLE, "Chưa cấu hình cuộc gọi SIM") }`
  placed after `backend` and before `store` — keep every existing parameter working with its current default.
- Step order in the produced report MUST be: LOCATION, SMS, VOICE_CALL, SIM_CALL, MAP_LINK.
- SMS and the two call steps are INDEPENDENT: a null location may only change the message text, never skip or
  disable SMS/CALL. SMS dispatch must not wait for delivery reports.
- New SIM_CALL step rules (this implements decision D08 below):
  1. Choose the primary recipient: `record.contacts.firstOrNull { it.receiveSos && it.phone.isNotBlank() }`.
  2. If `!permissionFacts.calling` → `PERMISSION_MISSING`, detail `EmergencyFailureMessages.callPermissionMissing`.
  3. Else if no primary recipient → `UNAVAILABLE`, detail "Chưa có người thân để gọi."
  4. Else if the backend voice step already reported `SUCCESS` (STARTED) → `SKIPPED`,
     detail "Máy chủ đã tiếp nhận cuộc gọi trợ giúp."
  5. Else call `call.call(primary.phone)` exactly ONCE and map `CallStatus`:
     STARTED→SUCCESS ("Đã gọi SIM tới <tên người nhận>."), PERMISSION_MISSING→PERMISSION_MISSING,
     UNAVAILABLE→UNAVAILABLE, FAILED→FAILED, each carrying the gateway's truthful detail.
  6. Never retry, never dial more than one number, and swallow nothing silently — every branch has a detail.
- Keep the injected `capabilities` snapshot usage; do not read Android permissions from this class.
- `EmergencyMessageFormatter.emergency` copy must follow the owner-specified wording:
  - with fix: starts with `"CẢNH BÁO SOS: Tôi có thể đã bị ngã và cần hỗ trợ."`, includes the helper name,
    `Thời điểm sự kiện:`, `Tọa độ: <lat>,<lon>`, `fix.mapsUrl`, the accuracy when known, and
    `Nguồn vị trí: điện thoại|ESP32 GNSS`.
  - without fix: same header, then `"Hiện chưa xác định được vị trí chính xác. Vui lòng gọi lại hoặc kiểm tra ứng dụng theo dõi."`
    — it must still contain the substring `xác định được vị trí`, must never invent coordinates,
    and must not mention SMS/CALL permission constant names.
  - Map the new sources for the "Nguồn vị trí" line: CACHED/FUSED/GPS/NETWORK/PHONE → "điện thoại",
    ESP32_GNSS → "ESP32 GNSS", UNKNOWN → "không xác định".

### E. Handset call gateway
Create `app/src/main/java/vn/nckh27pa/fallsafe/emergency/AndroidSimCallGateway.kt` implementing
`EmergencyCallGateway`: check `CALL_PHONE` with `ContextCompat.checkSelfPermission` before firing,
then `Intent(Intent.ACTION_CALL, Uri.parse("tel:$encodedPhone"))` with `FLAG_ACTIVITY_NEW_TASK`;
map results to `CallStatus`. Catch `SecurityException`/`ActivityNotFoundException`/`RuntimeException` and
return `PERMISSION_MISSING`/`FAILED`/`UNAVAILABLE` with truthful Vietnamese details — never throw.
Leave `ManualSimCallFallback` and `DemoController.callContactViaSim` working exactly as they do today
(manual path unchanged).
Wire it in `DemoApplication.onCreate()` (constructor arg `call = AndroidSimCallGateway(this)`) and wire the
new location repository into the location controller. Do not touch `HomeScreen.kt` — another worker owns it.

### F. Logging (requirement §18)
Use `android.util.Log` with tags `FallSafe/Location`, `FallSafe/SMS`, `FallSafe/CALL` at INFO/WARN:
log source, accuracy, age, cause, per-recipient SMS status, call status. NEVER log phone numbers or message
bodies — log `contactId` only.

### G. Tests (required, all must be real assertions on behavior)
Extend/add JVM tests (no Robolectric; keep everything pure):
- `location/BestAvailableLocationRepositoryTest.kt` — fake `PlatformLocationSource`:
  1. fine granted + cached FRESH → returned immediately, `fromCache=true`, `current()` never called;
  2. cached STALE + current fix available → current fix returned, `source` from the kind used;
  3. GPS disabled/returns null but FUSED (or NETWORK) returns a fix → lookup succeeds
     — this is the "GPS unavailable ≠ location unavailable" acceptance case;
  4. accuracy 150 m → still returned, `cause=null`;
  5. no fix anywhere (stale cache absent) → `fix=null`, non-null cause;
  6. permission DENIED → `PERMISSION_DENIED` and `lastKnown`/`current` never called;
  7. enabled providers empty → `PROVIDER_DISABLED`;
  8. stale cache + every current attempt times out → stale fix returned with `cause=TIMEOUT`.
- `emergency/SosCallStepTest.kt` (or extend `SosDispatchReportTest.kt`):
  9. CALL_PHONE granted + backend voice not STARTED → gateway invoked once with the primary contact's phone,
     step SIM_CALL SUCCESS;
  10. backend voice STARTED → SIM_CALL SKIPPED and gateway NOT invoked;
  11. CALL_PHONE denied → SIM_CALL PERMISSION_MISSING, no crash, SMS step still SUCCESS;
  12. no location at all → SMS step still SUCCESS (message says the location is unknown) AND SIM_CALL still
      attempted — the critical "location failure blocks nothing" acceptance;
  13. GPS unavailable + network fix available → SMS message contains `https://maps.google.com/?q=<lat>,<lon>`
      AND SIM_CALL SUCCESS.
- Update the existing tests your changes invalidate, and say so explicitly in the report:
  `location/LocationIntentAndProviderTest.kt` (its `coarseOnlyNeverChoosesGpsAndUsesCoarseCapableProvider`
  currently asserts GPS-first, which is the defect — replace it with best-available/fused-first expectations),
  `emergency/EmergencyLogicTest.kt` (mapsUrl + `startsWith(...ngã và cần hỗ trợ.)`),
  `emergency/SosDispatchReportTest.kt` (new step), `emergency/BoundedLocationResolutionTest.kt` only if you
  removed that class.
- Do NOT edit `HomeScreenPureUiTest.kt` or `HomeScreen.kt` (owned by T7).
- No test may be deleted to make the suite green without stating why in the report.

## Acceptance for T6
- `./gradlew testDebugUnitTest --no-daemon` PASS (all pre-existing tests + your new ones).
- `./gradlew assembleDebug --no-daemon` PASS.
- No `GPS_PROVIDER`-only path remains reachable; `play-services-location` is actually used.
- `git status` tells exactly which files you changed; do NOT commit.

## Report back (short, no logs)
DONE / CHANGED (file: reason) / TEST (exact commands + real counts) / NEXT / BLOCKER
Plus: the list of pre-existing tests you modified and why.
