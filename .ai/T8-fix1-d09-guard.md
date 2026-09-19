# T8-fix1 — restore owner decision D09 (SIM_CALL guard) + keep TDD evidence

CONTEXT: your T8 run changed `EmergencyCoordinator.dispatch` so `dispatchSimCall` runs even when the backend
voice step already reported SUCCESS (`"Máy chủ đã tiếp nhận cuộc gọi trợ giúp."` branch removed). Owner decision
`docs/decisions.md` **D09** says the SOS SIM call happens **only when the backend voice adapter has NOT accepted
(`STARTED`)**, and `docs/next-gate.md` §4 repeats it. Owner decisions can only be changed by the project owner, so
the code must go back to the documented behaviour.

TASK (bounded, no other changes):
1. In `android/app/src/main/java/vn/nckh27pa/fallsafe/emergency/EmergencyCore.kt` restore in `dispatchSimCall`:
   if the VOICE_CALL step result is `SosStepStatus.SUCCESS`, return
   `SosStepResult(SosStep.SIM_CALL, SosStepStatus.SKIPPED, "Máy chủ đã tiếp nhận cuộc gọi trợ giúp.")` — keep the
   `logStatus("FallSafe/CALL", …)` call for that branch, keep the ordering (SMS before call) and keep everything
   else you changed in T8 untouched.
2. Update `SosCallStepTest.backendStartedStillLaunchesHandsetCallForTheSosCallPath` so it asserts the D09 contract
   instead: with the backend returning `STARTED`, the SIM call must be SKIPPED and the gateway must NOT be invoked;
   keep/add a test that shows the call IS attempted when the backend is CONFIGURED/UNAVAILABLE/DISABLED.
3. Do NOT change docs, UI, permissions, location or SMS files. Do NOT commit anything.

VERIFICATION (report the raw result):
- `JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`
  (Java 25 breaks Gradle; use JDK 17) → then parse `app/build/test-results/testDebugUnitTest/TEST-*.xml` and report
  tests/failures/errors/skips.
- No emulator run needed for this fix.

OUTPUT (last lines of your answer, exactly this shape):
PROVIDER: Codex
ROOT CAUSE:
CHANGED:
TEST:
DEVICE STATUS:
REMAINING FAILURE:
NEXT:
