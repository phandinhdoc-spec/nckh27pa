# T8-fix2 — make the suite green again after the D09 restoration (test expectation only)

Your T8-fix1 run left ONE failing test, and your final report stopped with a red suite. Fix it now:

`android/app/src/test/java/vn/nckh27pa/fallsafe/emergency/SosDispatchReportTest.kt:16-25`
`allGrantedProducesSuccessfulIndependentStepsAndPersistsReport` still expects `SIM_CALL=UNAVAILABLE` while the
restored D09 guard correctly produces `SKIPPED` (`expected:<UNAVAILABLE> but was:<SKIPPED>`).

TASK (test expectation only, no production code change):
1. Update that assertion set for D09 semantics: backend `STARTED` + all permissions granted →
   `statusOf(SIM_CALL) == SKIPPED`, the other four steps stay `SUCCESS`, and the follow-up assertions about
   `failedSteps` / `allSucceeded` must be consistent with SKIPPED counting as success.
2. Keep the test's original intent (all other steps independent and successful, report persisted) — do not weaken
   it, do not delete it, do not touch production code or any other file.
3. No commit, no docs, no UI, no emulator run.

VERIFICATION: `cd android && JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon`,
then parse `app/build/test-results/testDebugUnitTest/TEST-*.xml` and report tests / failures / errors / skips.
The suite MUST be 0 failures, 0 errors, 0 skips. Do not stop while any test is red.

OUTPUT (last lines of your answer, exactly this shape):
PROVIDER: Codex
ROOT CAUSE:
CHANGED:
TEST:
DEVICE STATUS:
REMAINING FAILURE:
NEXT:
