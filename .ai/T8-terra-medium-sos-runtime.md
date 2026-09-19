# T8 — SOS runtime defect hunt & fix (Terra Medium / Codex)

WORKER: Codex CLI, model `gpt-5.6-terra`, reasoning medium (provider #1 — CommandCode is fallback ONLY on quota exhaustion).
ROLE: you are the single implementer for this task. Do the root-cause investigation yourself with rg/sed; do not
re-read the repository; do not restate the plan; do not touch files outside WRITABLE SCOPE.

## GOAL
Find and fix the REAL runtime defects in the Android SOS path so that, on a running device, all four owner
acceptance paths work:
  A. LOCATION → MAPS      (a usable fix exists and "mở bản đồ" opens it)
  B. LOCATION → SMS       (the SMS body carries the correct maps link)
  C. CALL                 (a SIM call to the priority contact is really launched)
  D. SOS → LOCATION → SMS → CALL  (one orchestration run produces a per-step report where each step's
     status matches what actually happened on the device)
Previous "7/7 permission PASS" is NOT acceptance. Unit/build green is NOT acceptance.

## CURRENT STATE (verified by Hermes, targeted reads — treat as fact, not as a substitute for your own check)
- `location/LocationRepository.kt:16-68` `BestAvailableLocationRepository`: permission → fresh cached → FUSED/GPS/NETWORK → stale cache → failure; total bounded by `SOS_LOCATION_TIMEOUT_MS=8_000` (`emergency/EmergencyCore.kt:22`). Pass budget halves remaining for FUSED and GPS.
- `location/AndroidPlatformLocationSource.kt`: Play Services fused + LocationManager fallback; logs `FallSafe/Location`.
- `emergency/EmergencyCore.kt:235-313` `dispatch()`: steps LOCATION, SMS, VOICE_CALL, SIM_CALL, MAP_LINK; `dispatchSimCall` at `:273-290`; SMS loop at `:293-313`.
- `emergency/AndroidSmsManagerGateway.kt`: feature check, SEND_SMS check, multi-SIM branch at `:51-56`, PendingIntent per part, aggregation in `SmsPartAggregation`.
- `emergency/AndroidSimCallGateway.kt:15-48`: ACTION_CALL; off-main-thread path returns STARTED from a `Handler.post` return value.
- Wiring in `DemoApplication.kt:74-90`; capability snapshot from `CapabilityAccessController`; `logStatus` → `android.util.Log.i`.
- Permissions declared in `app/src/main/AndroidManifest.xml:3-18`; runtime requests in `MainActivity.kt:97-105` (CALL_PHONE, SEND_SMS, FINE+COARSE only).
- Previous session checkpoint: `.ai/task_on_progress.md` (read §1–§2 and §7 before starting; it is the durable memory for this task).
- Emulator only: `emulator-5554` (API 36). No physical device, no SIM, no real GNSS. State that honestly, never claim device verification.

## HYPOTHESES TO CONFIRM OR REFUTE (each one only becomes a fix after you have evidence)
- H1 `AndroidPlatformLocationSource.kt:40-45` adds FUSED whenever Play Services is present, without checking that any location provider is enabled ⇒ `LocationRepository.kt:32` can never return `PROVIDER_DISABLED`; with the system location toggle OFF the user sees TIMEOUT after 8 s instead of the correct cause and the remediation copy is wrong.
- H2 multi-SIM/default SIM (`AndroidSmsManagerGateway.kt:51-56`): READ_PHONE_STATE is declared in the manifest but never requested at runtime (`MainActivity.kt:97-105`) ⇒ `hasPhoneStateAccess` is false in practice; and when it IS granted on a dual-SIM phone the branch returns a hard FAILURE ("Có nhiều SIM; cần chọn SIM gửi rõ ràng") which BLOCKS the emergency SMS entirely. An emergency must still send via the default SMS subscription, and the chosen subscription must be visible in the diagnostics.
- H3 `AndroidSimCallGateway.kt:29-36`: off-main-thread call reports `CallStatus.STARTED` because only `Handler.post` succeeded; the real `startActivity` result is only logged ⇒ `SosStep.SIM_CALL` can be reported SUCCESS while no call was launched.
- H4 `EmergencyCore.kt:280`: SIM call is SKIPPED whenever the backend voice step is SUCCESS — confirm against the runtime truth of `SyncCoordinator.requestVoice` (`api/SyncCoordinator.kt:97-103`, returns CONFIGURED, never STARTED) and decide whether the owner's "SOS → LOCATION → SMS → CALL" requirement is met; do not weaken the backend voice path.
- H5 `SmsPartAggregation` (`EmergencyCore.kt:155-180`) `require(index in 0 until total)` is called from a BroadcastReceiver (`AndroidSmsManagerGateway.kt:26-38`) whose `parts[key]` entry can be replaced by a later send with a different part count ⇒ possible crash / wrong status from a late PendingIntent.
- H6 `AndroidPlatformLocationSource.kt:53-64` `lastKnown()` reads every provider's cache, including fixes recorded while location was off or from another app's passive updates; confirm a stale/garbage cached fix can never be published as a fresh usable fix.
- H7 diagnostic logging is fragmented (`FallSafe/Location` at `AndroidEmergencyLocationController.kt:47`, `FallSafe/CALL`/`FallSafe/SMS` from `EmergencyCore.kt:262,276,304`): one SOS run cannot be reconstructed from logcat. Required: a single structured diagnostic stream for one event (event id, step, status, elapsed ms, source/accuracy, subscription, capability snapshot), readable with one `adb logcat -s` command, with no secret/contact phone number in full.
- H8 capability detection (`permissions/CapabilityAccess.kt:136-141`, `permissions/AndroidCapabilityPlatform.kt:32-44`): "messaging granted" is SEND_SMS only; the device feature `FEATURE_TELEPHONY_MESSAGING`/`FEATURE_TELEPHONY_CALLING` is checked only inside the gateways, so SOS readiness can promise something the device cannot do. Confirm and make capability facts and gateway behaviour agree.

## RULES
1. TDD, red first: for every defect you fix, add/extend a JVM unit test that FAILS before the fix (record the
   failing output to `.ai/T8-red.txt`), then fix, then show it passing. Use the existing style in
   `app/src/test/java/vn/nckh27pa/fallsafe/{location,emergency,permissions}/`.
2. Targeted fixes only: no rewrites, no renames, no reformatting, no architecture change. Keep the contract in
   `emergency/EmergencyCore.kt` (`LocationLookup`, `SosDispatchReport`, `CallStatus`, `SosStep`) unless a defect
   cannot be fixed without it — if so, stop and report instead of changing it silently.
3. Do NOT change the backend contract (`api/ApiService.kt`, `core/**`) and do NOT add new Gradle dependencies.
4. Only ONE Gradle invocation at a time. Do not leave background Gradle daemons running.
5. FORBIDDEN: `git commit|push|reset|checkout|clean`, touching `.env`/secrets, reading the whole repo, editing
   `HomeScreen.kt` or any other UI file (report to Hermes if you believe a UI change is needed — AGY owns UI),
   deleting existing user changes.
6. If a hypothesis turns out false, write one line in your report saying so and move on. Do not invent defects.

## WRITABLE SCOPE
- `android/app/src/main/java/vn/nckh27pa/fallsafe/location/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/emergency/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/permissions/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt` (wiring only), `MainActivity.kt` (only if a
  runtime permission request must be added — justify it)
- `android/app/src/test/java/vn/nckh27pa/fallsafe/{location,emergency,permissions}/**`
- `android/scripts/**`, `docs/evidence/sos-runtime/**`, `.ai/T8-*.txt`
Everything else is read-only.

## COMMANDS (run from `android/`)
- red:  `./gradlew testDebugUnitTest --tests 'vn.nckh27pa.fallsafe.location.*' --tests 'vn.nckh27pa.fallsafe.emergency.*' --rerun-tasks --no-daemon`
- full: `./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`   (then count tests from
  `app/build/test-results/testDebugUnitTest/TEST-*.xml` — an UP-TO-DATE build proves nothing)
- emulator acceptance: `python3 scripts/sos-location-acceptance.py` (writes `docs/evidence/sos-location/`)
- location toggle OFF scenario: `adb shell settings put secure location_mode 0` (expect the correct failure
  cause/remediation in the UI/log, then restore with `location_mode 3`)
- logs: `adb logcat -d -s FallSafe/SOS:I FallSafe/Location:I FallSafe/SMS:I FallSafe/CALL:I`
ADB: `/home/pdd/Android/Sdk/platform-tools/adb`, device `emulator-5554` only.

## ACCEPTANCE TO PROVE (report the raw evidence for each)
- G1: `./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon` green, with the exact PASS/FAIL/ERROR
  count parsed from the XML, and the new tests named.
- G2: emulator run of `scripts/sos-location-acceptance.py` reproducing paths A and B (real `adb emu geo fix` /
  sensor-driven countdown, coordinates in the SMS body match the injected values, `source=` line in the log).
- G3: path C — the SIM-call branch really launches (log + `dumpsys`/activity evidence on the emulator; if the
  emulator cannot place a SIM call, say exactly which part is unverifiable and why).
- G4: path D — one SOS run whose per-step report matches the observed device behaviour, including at least one
  degraded case (GPS unavailable, or location toggle OFF) where SMS and CALL still run.
- G5: the single structured diagnostic stream from H7, shown by one logcat excerpt for one event id.
DEVICE STATUS must be explicit: emulator evidence ≠ physical-device verification.

## CHECKPOINT (mandatory)
After each iteration, rewrite `.ai/task_on_progress.md` §T8 with:
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
PROVIDER: Codex
ROOT CAUSE:
CHANGED:
TEST:
DEVICE STATUS:
REMAINING FAILURE:
NEXT:
```
