# T11 — LOC-01: Location OFF→ON must recover without restarting the app

WORKER: CommandCode CLI, model `deepseek/deepseek-v4-pro`, reasoning `high`.
ROLE: single implementer for this task. You are the ONLY worker allowed to run Gradle. Investigate with rg/sed,
do not re-read the repository, touch nothing outside WRITABLE SCOPE, do not restate this plan.

## GOAL
Fix the real-device defect LOC-01. Permission (ACCESS_FINE/COARSE) and the Android **Location Services
enabled state** are two independent facts. Today the app caches a `PROVIDER_DISABLED` location state and never
re-evaluates it, so:

  Case B (real device): Location OFF → open app → app says "Vị trí đang tắt"/"Chưa có GPS" → user turns Location
  ON from Quick Settings/Settings → return to the app **without restarting** → app STILL says no location.
  Restarting the app makes it work again.

ACCEPTANCE PATHS (owner will test on a real phone; you must make all six provable in unit tests):
  A. Location ON at startup            → app acquires a fix (no restart, no SOS needed).
  B. Location OFF at startup           → honest PROVIDER_DISABLED state, ZERO provider lookups attempted.
  C. OFF → ON after launch             → recovery WITHOUT restart: availability change is observed, location is
                                          re-acquired, state becomes a usable fix.
  D. Resume after returning from Android Settings → the same recovery as C, driven by app resume.
  E. Repeated resume                   → NO duplicate/overlapping lookups (never two concurrent requests; one
                                          bounded attempt per cooldown window).
  F. Permission DENIED                 → still correct: PERMISSION_DENIED, zero provider lookups; when permission
                                          is later granted, recovery happens the same way as C.
A green build or a green unit suite is NOT acceptance; the emulator/real-device behaviour is.

## CURRENT STATE (verified by the coordinator — treat as fact, do not re-investigate)
- Location is acquired ONLY during an SOS event: `api/SyncCoordinator.kt:68-69` calls
  `emergencyLocation?.onVerifyingStarted()` when the state is VERIFYING/ALERTING/AWAITING_HELP, and the gate
  `EventLocationRefreshGate` (`api/SyncCoordinator.kt:59`, class at the top of that file) allows only ONE request
  per local event id. There is NO startup, no periodic and no resume-driven location acquisition.
- `location/AndroidEmergencyLocationController.kt:30-35` `refreshPermissionTruth()` is the ONLY resume hook
  (`MainActivity.kt:117` → `DemoApplication.kt:181-185` `refreshCapabilityTruth()`) and it only handles one
  transition: PERMISSION_DENIED → permission now granted. A cached `LocationFailureCause.PROVIDER_DISABLED`
  (or TIMEOUT/NO_FIX) is never re-evaluated and never re-acquired. That is the root cause.
- `location/AndroidEmergencyLocationController.kt:44-62` `onVerifyingStarted()`: increments `generation`,
  cancels `activeRequest`, resets `locationState`, launches a bounded
  `BestAvailableLocationRepository(AndroidPlatformLocationSource(context), …).getBestAvailableLocation(8s)`.
  This one-shot design is correct — keep it (generation + `activeRequest?.cancel()` is the anti-duplicate guard).
- `location/AndroidEmergencyLocationController.kt:36-38` `locationPermissionGranted()` checks the runtime
  permissions only. `permissions/AndroidCapabilityPlatform.kt:44` also checks permission only — correct, do not
  change: permission and Location Services state stay separate facts.
- `location/AndroidPlatformLocationSource.kt:39-59`: `systemLocationEnabled()` (`manager.isLocationEnabled` on
  API≥28) + `enabledProviders()` via the pure `LocationProviderAvailability.enabled` (`location/LocationRepository.kt:10-23`).
  With the master Location switch OFF the provider set is empty; `BestAvailableLocationRepository`
  (`location/LocationRepository.kt:61-67`) then returns `PROVIDER_DISABLED` **without any provider call**.
- `emergency/EmergencyCore.kt:80-89` `EmergencyLocationController` interface, `:70-76` `LocationState`,
  `:25` `LocationFailureCause`. `HomeScreen.kt:114-139` `resolveLocationCardView` renders cause
  PROVIDER_DISABLED as "Vị trí đang tắt" and `HomeScreen.kt:281` renders "Chưa có GPS" while `fix == null`.
  `HomeScreen.kt` is READ-ONLY for this task — the state you publish is what the UI shows.
- `emergency/EmergencyCore.kt:374-376` `updateLocation` is a no-op when no emergency record exists, so a
  location acquisition outside an SOS cannot disturb SMS/CALL.
- Wiring: `DemoApplication.kt:85-95` builds `AndroidEmergencyLocationController` with
  `onFix = { … }`; `onTerminate` (`:117-121`) is the process-level cleanup hook.
- Duplicate-listener facts: the controller registers nothing platform-side today; `platformCurrent()`
  (`AndroidPlatformLocationSource.kt:106-124`) removes its single-update listener on cancellation. Any NEW
  platform listener you add must be registered exactly once and unregistered on close, and must never overlap.
- Environment: emulator `emulator-5554` is running and is owned by the COORDINATOR — do not use adb, do not
  install anything, do not run the app. JDK 17 only: `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64`.
  Java 25 breaks Gradle in this environment. No physical device is available to you.

## DESIGN CONTRACT (coordinator-owned — implement exactly this shape)
New pure file `location/LocationRecovery.kt` (NO Android imports, NO Compose imports — it must be unit-testable
in plain JVM):

```kotlin
enum class LocationSignal { FOREGROUND_RESUMED, SYSTEM_LOCATION_CHANGED, EMERGENCY_STARTED }

data class LocationAvailability(val permission: LocationPermission, val providers: Set<LocationProviderKind>) {
    val usable: Boolean get() = permission != LocationPermission.DENIED && providers.isNotEmpty()
}

sealed interface LocationRecoveryDecision {
    data object None : LocationRecoveryDecision
    data object PermissionDenied : LocationRecoveryDecision
    data object ProviderDisabled : LocationRecoveryDecision
    data object Acquire : LocationRecoveryDecision
}

data class LocationRecoveryInput(
    val signal: LocationSignal,
    val availability: LocationAvailability,
    val availabilityChanged: Boolean,
    val requestInFlight: Boolean,
    val hasFix: Boolean,
    val fixIsFresh: Boolean
)

class LocationRecoveryPolicy(
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val cooldownMs: Long = RECOVERY_COOLDOWN_MS
) {
    fun decide(input: LocationRecoveryInput): LocationRecoveryDecision
    fun markAttempt()        // called by the caller when an attempt is really started
    fun reset()              // clears the cooldown bookkeeping
    companion object { const val RECOVERY_COOLDOWN_MS = 15_000L }
}
```

`decide` rules, in this exact order:
1. `requestInFlight` → `None`.
2. permission DENIED → `hasFix ? None : PermissionDenied`.
3. `providers.isEmpty()` → `hasFix ? None : ProviderDisabled`.
4. otherwise usable → attempt only when `availabilityChanged || !hasFix || !fixIsFresh`; when
   `availabilityChanged` is false, an attempt is additionally gated by the cooldown
   (`nowMs() - lastAttempt >= cooldownMs`, and no attempt when no attempt was ever recorded and no cooldown is
   running). When step 4 decides to attempt, record the attempt time inside `decide` and return `Acquire`.
   Otherwise `None`.
`availabilityChanged` is computed by the caller by comparing the CURRENT availability with the availability it
saw on the previous signal; the very first evaluation counts as changed.

Integration in `location/AndroidEmergencyLocationController.kt`:
- Keep the public interface from `emergency/EmergencyCore.kt` unchanged (`refreshPermissionTruth()`,
  `onVerifyingStarted()`, …). Do NOT edit `emergency/EmergencyCore.kt`, `api/SyncCoordinator.kt`,
  `MainActivity.kt`, `HomeScreen.kt`.
- `refreshPermissionTruth()` keeps its name but becomes the full re-check: read `permission()` +
  `enabledProviders()` through a `PlatformLocationSource`, compute `availabilityChanged`, run
  `LocationRecoveryPolicy.decide`, and apply:
  * `PermissionDenied` → publish `LocationState(cause = PERMISSION_DENIED, …)` using the existing
    `explanation()`/`remediation()` helpers (keep their Vietnamese copy).
  * `ProviderDisabled` → publish `LocationState(cause = PROVIDER_DISABLED, …)` via the same helpers.
  * `Acquire` → start ONE bounded lookup (same as the SOS path: fresh repository, `SOS_LOCATION_TIMEOUT_MS`,
    `generation` guard, cancels/replaces any previous `activeRequest`; a new attempt must never run beside an
    old one). Do not loop, do not poll, do not register a continuous location subscription.
  * `None` → change nothing.
- `requestInFlight` comes from the controller's single source of truth (`activeRequest?.isActive == true`), never
  from a second flag.
- `onVerifyingStarted()` keeps today's behaviour and additionally calls `policy.markAttempt()` so the cooldown
  accounting matches reality.
- Add a diagnostic line per evaluation on the existing tag `FallSafe/Location`:
  `Log.i(TAG, "recovery signal=$signal availability=$availability changed=$changed decision=$decision")`.
  No secrets, no phone numbers.
- Add `fun onSystemLocationChanged()` that performs the same re-check with signal `SYSTEM_LOCATION_CHANGED`
  (main thread; reuse the same path as `refreshPermissionTruth` — one implementation, two entry points).
- Add `fun close()` that cancels `activeRequest`, unregisters the observer (if it owns one) and clears state
  bookkeeping; call it from `DemoApplication.onTerminate()`.

New `location/SystemLocationAvailabilityObserver.kt` (best-effort, small, no new dependency):
- A `ContentObserver` on the main looper that fires the callback when the system location setting changes.
  Register it on `Settings.Secure.LOCATION_PROVIDERS_ALLOWED` and on the `"location_mode"` key
  (`Settings.Secure.getUriFor("location_mode")`); both registrations wrapped in try/catch and treated as
  best-effort. `register(): Boolean` is idempotent (a second call returns false and registers nothing),
  `close()` unregisters exactly the observers it registered and never throws.
- The deterministic recovery path is the resume re-check; the observer is an enhancement for the case where the
  user flips Quick Settings without the activity resuming. Do not rely on it for correctness.
- `DemoApplication.kt`: build the observer with `LocationSignal.SYSTEM_LOCATION_CHANGED` →
  `AndroidEmergencyLocationController.onSystemLocationChanged()`, keep a reference, close it in `onTerminate()`.
  Wiring only — no other change in that file.

## RULES
1. TDD, red first: write the failing tests FIRST, capture the failing output to `.ai/T11-red.txt`, then fix.
2. Use JUnit4 + `kotlinx-coroutines-test` only — this project has NO Robolectric and no instrumented tests.
   `Dispatchers.Main` is not available in unit tests: keep every tested class free of Android/Compose/Dispatchers
   dependencies (that is why the policy is pure).
3. Targeted patches only: no rewrites, no renames, no reformatting, no architecture change beyond the contract
   above. Keep the existing Vietnamese error copy and the existing log format.
4. Do NOT touch the fall-detection code, thresholds, `HomeScreen.kt`, `MainActivity.kt`,
   `emergency/EmergencyCore.kt`, `api/SyncCoordinator.kt`, `permissions/**`, `core/**`, `app/build.gradle.kts`,
   `AndroidManifest.xml`, `docs/**`, `.ai/T10-*`.
5. No new Gradle dependencies. Exactly ONE Gradle invocation at a time; leave no Gradle daemon running.
6. FORBIDDEN: `git commit|push|reset|checkout|restore|stash|clean`, reading the whole repository, touching
   `.env`/secrets, using adb or the emulator, editing files owned by another worker.
7. If a rule conflicts with reality, STOP and report instead of improvising.

## WRITABLE SCOPE
- `android/app/src/main/java/vn/nckh27pa/fallsafe/location/**`  (new `LocationRecovery.kt`,
  `SystemLocationAvailabilityObserver.kt` allowed here)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt` (wiring only)
- `android/app/src/test/java/vn/nckh27pa/fallsafe/location/**`
- `.ai/T11-*.txt|md` (your own evidence/checkpoint files)
Everything else is read-only.

## COMMANDS (run from `/home/pdd/nckh27pa/android`, always export JAVA_HOME first)
- red:  `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest --tests 'vn.nckh27pa.fallsafe.location.*' --rerun-tasks --no-daemon --console=plain`
- full: `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon --console=plain`
  then parse the REAL totals from `app/build/test-results/testDebugUnitTest/TEST-*.xml` (an UP-TO-DATE build
  proves nothing). Baseline before your change: 27 suites / 175 tests / 0 failures / 0 errors.
- If the same error repeats twice, STOP and report it unresolved instead of guessing.

## ACCEPTANCE TO PROVE (report raw evidence for each; unit tests + Gradle XML are the artifact)
- G1: build + full suite green, with the exact suite/test/failure/error/skip counts parsed from the XML, and the
  new test names listed. No existing test may be weakened or deleted.
- G2: named test for path A (usable availability at startup → `Acquire` and a fix is published).
- G3: named test for path B (providers empty → `ProviderDisabled`, and a counting fake `PlatformLocationSource`
  proves `current()`/`lastKnown()` were called ZERO times).
- G4: named test for path C (OFF → ON: first `ProviderDisabled`, then with `availabilityChanged = true` exactly
  ONE `Acquire` → fix published; assert the lookup count is exactly 1).
- G5: named test for path D (resume signal after the system setting changed → same single recovery).
- G6: named test for path E (repeated resumes: no acquisition while a request is in flight; after a fresh fix,
  repeated resumes cause ZERO further lookups; after a failed attempt, resumes inside the cooldown cause none and
  only after the cooldown exactly one).
- G7: named test for path F (permission DENIED → `PermissionDenied`, zero provider calls; then granted +
  `availabilityChanged` → one `Acquire`).
- G8: state that the emulator/real-device step is NOT verified by you: the coordinator runs the OFF→ON
  acceptance on `emulator-5554` and the owner runs it on a real phone. Say which parts stay unverified (real GPS,
  real Settings app, Quick Settings toggle).
DEVICE STATUS must be explicit: unit/build evidence is not device verification.

## CHECKPOINT (mandatory)
Rewrite the `T11` section of `.ai/task_on_progress.md` (append if the section does not exist; do not touch other
sections) with:
```
ROOT_CAUSE:
FILES_INSPECTED:
FILES_CHANGED:
TESTS_RUN:
TEST_RESULTS:
CURRENT_FAILURE:
NEXT_ACTION:
DO_NOT_REPEAT:
```

## FINAL REPORT (last thing in your output, exactly this shape)
```
PROVIDER: CommandCode deepseek/deepseek-v4-pro
ROOT CAUSE:
CHANGED:
TEST:
DEVICE STATUS:
REMAINING FAILURE:
NEXT:
```
