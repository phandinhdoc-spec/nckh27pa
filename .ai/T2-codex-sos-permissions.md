# T2 (Codex) — SOS permission/logic hardening, maps intent, per-step SOS reporting

STATUS: DONE (accepted by Hermes after host Gradle run + emulator verification; no commits made)
OWNER: Codex Sol (codex exec, workspace-write)
GOAL: Complete the EXISTING local SOS permission implementation (no rewrite, no new services) so that
(a) every SOS step is permission-checked and returns its own result, (b) location works with
approximate-only grant, (c) maps opening prefers Google Maps with working fallback, (d) the controller
exposes a UI-ready, non-technical permission-setup interface. UI/Compose files are NOT yours.

## Baseline (verified by Hermes before dispatch)
- `git status` clean at commit b40ecf3.
- Android unit tests PASS: `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon`
- Existing code you must extend, not replace:
  - `android/app/src/main/java/vn/nckh27pa/fallsafe/permissions/{CapabilityAccess,AndroidCapabilityPlatform}.kt`
  - `android/app/src/main/java/vn/nckh27pa/fallsafe/location/AndroidEmergencyLocationController.kt`
  - `android/app/src/main/java/vn/nckh27pa/fallsafe/emergency/{EmergencyCore,AndroidSmsManagerGateway,ManualSimCallFallback,EmergencyPersistence}.kt`
  - `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` (Activity class + permission launchers only)
  - `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt` (DemoController wiring)
  - tests under `android/app/src/test/java/vn/nckh27pa/fallsafe/**`

## WRITABLE SCOPE (exclusive ownership — do not write anything else)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/permissions/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/location/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/emergency/**`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/api/SyncCoordinator.kt` (only the voice-gateway call site)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` — ONLY the `MainActivity` class body (lines ~38-157). Do NOT touch `DemoScreen`, `SettingsScreen`, `EventsScreen` or any @Composable.
- `android/app/src/main/AndroidManifest.xml` (only if a permission/feature is genuinely required)
- `android/app/src/test/java/vn/nckh27pa/fallsafe/{permissions,emergency,location,api}/**`

FORBIDDEN: `HomeScreen.kt`, `ContactsScreen.kt`, `FallDetection*.kt`, `DeviceDetails`, `docs/**`, `backend/**`, ESP files, any commit.

## FROZEN CONTRACT (do not redesign)

1. Capabilities are independent: CALLING=`CALL_PHONE`, MESSAGING=`SEND_SMS`, LOCATION=foreground
   FINE/COARSE. Never request `ACCESS_BACKGROUND_LOCATION`. No runtime permission request at startup.
   `READ_PHONE_STATE` stays in the manifest (existing multi-SIM gateway) but:
   - `Capability.MESSAGING` display state = GRANTED iff `SEND_SMS` granted.
   - When `READ_PHONE_STATE` is missing, expose a factual degradation note (e.g. "Chưa cho phép kiểm tra
     SIM; nếu máy có nhiều SIM, ứng dụng cần chọn SIM để gửi.") — never block SMS on it.
2. Display states stay `GRANTED | CAN_REQUEST | NEEDS_SETTINGS`, derived from
   `wasAttempted` + `shouldShowRequestPermissionRationale`. A permanent denial must never re-loop the
   system dialog; opening App Settings happens only from a separate explicit call.
3. NEW: location precision must be surfaced. Add to the location capability display/readiness whether the
   grant is precise (`ACCESS_FINE_LOCATION`) or approximate-only (`ACCESS_COARSE_LOCATION` only), including
   a non-technical Vietnamese explanation. Approximate-only is a WORKING state, not an error.
4. FIX (bug): `AndroidEmergencyLocationController.onVerifyingStarted()` currently selects
   `GPS_PROVIDER` whenever it is enabled, even when only COARSE is granted → `SecurityException` →
   `PERMISSION_DENIED`. Provider choice must respect the granted precision: FINE → GPS preferred;
   coarse-only → NETWORK/FUSED `getCurrentLocation` (API 30+) or a coarse-capable provider, never a
   provider requiring FINE. Keep the existing bounded 8s timeout + validated last-known fallback, keep
   `LocationFailureCause` semantics, never fabricate coordinates.
5. Maps opening must prefer Google Maps and never crash:
   order = (a) `geo:` intent explicitly targeting `com.google.android.apps.maps`, (b) generic `geo:`
   (any installed maps app), (c) `https` maps URL in a browser. Missing Maps app, no maps app at all and
   `SecurityException` all return a factual false+reason, never a crash. Extract the resolution into a
   testable seam so unit tests can cover "Google Maps installed", "Google Maps missing", "no maps app".
   The SMS body keeps its clickable maps URL (`LocationFix.mapsUrl`) so the recipient can always open it.
6. NEW: per-step SOS result reporting. Add to `emergency/EmergencyCore.kt` (Android-free, unit-testable):
   - `enum class SosStep { LOCATION, SMS, VOICE_CALL, MAP_LINK }`
   - `enum class SosStepStatus { SUCCESS, PARTIAL, PERMISSION_MISSING, UNAVAILABLE, FAILED, SKIPPED }`
   - `data class SosStepResult(step, status, detail: String?)` + `data class SosDispatchReport(eventId, steps)`
     with helpers (`statusOf(step)`, `failedSteps`, `allSucceeded`, and a Vietnamese label per step).
   - `EmergencyCoordinator` builds and stores this report per event (persist it with the record through the
     existing store contract; keep it readable after the fact for UI/logs). Each step resolves INDEPENDENTLY:
     * LOCATION: SUCCESS with fix / `UNAVAILABLE` when no fix (never invent coordinates).
     * SMS: per-contact results aggregated — SUCCESS when all parts submitted, PARTIAL/FAILED otherwise,
       `PERMISSION_MISSING` with the existing `EmergencyFailureMessages.messagingPermissionMissing` copy
       when `SEND_SMS` is absent.
     * VOICE_CALL: `SUCCESS` when the backend voice dispatch was accepted; `UNAVAILABLE` when the provider
       is not configured; `PERMISSION_MISSING` with the "không thể tự động gọi" copy when `CALL_PHONE` is
       absent AND the backend path is unavailable. Change `EmergencyBackendGateway` to return a small status
       (configured/started/unavailable/disabled) instead of `Unit`, and update the single caller in
       `api/SyncCoordinator.kt`. Do NOT add SIM auto-dial — SIM calling stays manual
       (`ManualSimCallFallback`), which already checks `CALL_PHONE` and returns a factual failure.
     * MAP_LINK: SUCCESS when the dispatched text carried a maps URL, SKIPPED when no fix.
   - The coordinator must remain Android-free: inject the permission facts (e.g. a
     `CapabilitySnapshot(calling, messaging, location)`-style provider) so tests use fakes.
   - Keep all existing behavior: cancellation before dispatch blocks dispatch, idempotent single dispatch,
     one late location supplement per contact, backend/SMS failure never blocks the other.
7. Fail-safe: no SOS path may throw on a missing permission. `SosDispatchReport` must always be produced
   for a dispatched event so UI/logs know exactly which step failed.
8. NEW (non-UI, for the UI worker): `DemoController` must expose
   - ordered first-run setup: `nextPermissionSetupStep(): Capability?` (order LOCATION → MESSAGING → CALLING,
     `null` when all granted),
   - `permissionSetupSeen` (persisted flag) + `markPermissionSetupSeen()`,
   - a short non-technical explanation string constant for the first-run dialog,
   - the latest `SosDispatchReport` for the current event,
   - per-capability `CapabilityDisplay` already exposed (calling/messaging/location) plus the precision note.
   No Android permission constant name (`ACCESS_FINE_LOCATION`, etc.) may appear in any user-visible string.
   All user-visible copy must be Vietnamese, short, elderly-friendly.

## TDD + TESTS REQUIRED (unit tests only; fakes; never send real SMS or dial a real number)
Add a focused failing test before each new behavior. Cover at minimum:
1. No permission granted at all → readiness reports all three missing, nothing throws, request path returns
   `EXPLANATION_REQUIRED` before acknowledgement.
2. All granted → all steps SUCCESS-capable.
3. Location only granted → SMS reports PERMISSION_MISSING, VOICE_CALL reports PERMISSION_MISSING/UNAVAILABLE,
   LOCATION step unaffected.
4. Location + SMS, CALL_PHONE missing → SMS still sent, VOICE_CALL reports PERMISSION_MISSING with the
   "cannot auto-call" copy, no crash.
5. Location + CALL_PHONE, SMS missing → manual SIM call path succeeds (permission granted) while SMS step
   reports PERMISSION_MISSING; no crash.
6. Denied once (`shouldShowRationale == true`) → state CAN_REQUEST, request allowed again.
7. Permanently denied / Don't ask again (`wasAttempted && !shouldShowRationale`) → NEEDS_SETTINGS, `request`
   returns `SETTINGS_REQUIRED` and never launches a system dialog; only the separate settings call opens
   App Settings.
8. Granted later in Android Settings → `refreshCapabilityTruth()` flips state to GRANTED (simulate by
   changing the fake platform between calls).
9. Location provider disabled → LOCATION step UNAVAILABLE, SMS/voice still dispatched, message says
   "Chưa xác định được vị trí." (no fabricated coordinates).
10/11. Maps seam: Google Maps installed → targets it; Google Maps missing → falls back; no maps at all →
    returns false with reason (no crash).
12. Coarse-only grant → approximate precision surfaced AND controller picks a coarse-capable provider.
13. SMS gateway failure (fake throwing / FAILED) → SMS step FAILED, LOCATION + VOICE_CALL steps still run.
14. SIM call failure (fake) → returns factual failure, SOS report unaffected.
Also keep the existing suites green (CapabilityAccessTest, EmergencyLogicTest, BoundedLocationResolutionTest,
api tests).

## VERIFICATION COMMANDS (run them, paste real output in the report)
```
cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon
cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew assembleDebug --no-daemon
cd /home/pdd/nckh27pa && git diff --check && git status --short
```
Do NOT commit. Do NOT reformat or refactor unrelated code. Keep the existing terse/compressed Kotlin style
where you edit lightly; new code may be normally formatted.

## REPORT BACK (exact format)
STATUS / OWNER / FILES / GOAL / RESULT / NEXT_ACTION, plus:
- exact list of changed files (`git status --short`),
- test evidence: RED line for each new test before implementation and the GREEN run after,
- which of the 14 scenarios above are covered by which test name,
- any limitation or assumption you had to make.


---

## RESULT (Hermes acceptance, 2026-09-18)
- Host Gradle (agent sandbox blocked it, so Hermes ran it): 147 unit tests, 0 failures
  (`testDebugUnitTest --rerun-tasks`) and `assembleDebug` → `android/app/build/outputs/apk/debug/app-debug.apk`.
- Every in-scope acceptance criterion is covered by tests (see scenario mapping above) and the SOS step report
  was additionally observed on the emulator: "Trạng thái SOS: Vị trí: Thành công • Tin nhắn: Không khả dụng •
  Cuộc gọi trợ giúp: Không khả dụng • Liên kết bản đồ: Bỏ qua" with a real fix, and
  "… • Cuộc gọi trợ giúp: Chưa cấp quyền • …" when `CALL_PHONE` is missing.
- Defects found in review and corrected: none in T2 scope. Note for the record: TDD RED evidence was not
  captured because the worker sandbox could not run Gradle; the tests themselves are real assertions on
  behaviour (verified by reading them and by re-running them in the host).
