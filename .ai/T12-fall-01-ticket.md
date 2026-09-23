# T12 — FALL-01 real-device instrumentation (logging only)

WORKER: Codex CLI (Level 2 implementation), single worker, single pass.
REPO: `/Users/phananh/TEMP/nckh27pa/nckh27pa` (git, branch `main`, HEAD `8761541`, clean at ticket time).
ROLE: add instrumentation ONLY. You are not the reviewer and not the threshold tuner.

## GOAL

Make the existing phone-side FALL pipeline observable on a real device through ONE logcat tag (`FALL01`),
without changing any detection behaviour. After your edits, `adb logcat -v threadtime FALL01:V '*:S'`
must return the whole FALL-01 data set (per-sample values, state transitions, final decision).

## HARD CONSTRAINTS (violating any of these fails the task)

- DO NOT change any threshold, profile value, validation range or default in
  `FallDetectionProfiles.kt`, `espconfig/Models.kt`, or `DemoDetector.DEFAULT_DETECTION_PROFILE`.
- DO NOT change any decision condition, comparison, ordering, timing constant, or return value in
  `DemoDetector.accept()`. Instrumentation must be PURELY ADDITIVE: you may add new statements and new
  files; you may not rewrite, reorder, reformat or delete existing statements or comments.
- DO NOT change sampling frequency, sensor registration, `SensorManager.SENSOR_DELAY_*`, or the 250 ms
  freshness window in `PhoneNormalizer`.
- DO NOT add interpolation/resampling of sensor data.
- DO NOT touch anything under `emergency/`, `location/`, `api/`, UI files (`HomeScreen.kt`,
  `MainActivity.kt`, `FallDetectionCalibrationScreen.kt`), Gradle files, or `core/src/Core.kt`.
- DO NOT run or modify Gradle files. DO NOT run `git commit/push/checkout/stash/reset/clean`. DO NOT commit.
- DO NOT create or modify any `.md` file. Only the three Kotlin files listed below.
- Keep `DemoDetector.accept()` behaviour bit-identical: same branches, same order, same state writes.

## FILES TO MODIFY (only these three)

1. `android/app/src/main/java/vn/nckh27pa/fallsafe/Fall01Trace.kt` — NEW FILE, the whole logger.
2. `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt` — additive hooks.
3. `android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt` — 2 additive session markers.

## PIPELINE FACTS (already verified — do not re-investigate)

```
PhoneSensorCollector.onSensorChanged        (PhoneSensorCollector.kt:43-63)
  -> PhoneInputPipeline.update              (DemoLogic.kt:323-326)   one packet per ACCEL event
  -> DemoController.acceptPhone             (DemoApplication.kt:434-440)
  -> DemoInputAdapter.acceptPhone           (DemoLogic.kt:335-339)   PHONE_ONLY path only; replay excluded
  -> DemoSession.accept                     (DemoLogic.kt:289-293)   detector runs ONLY while AlertCore == MONITORING
  -> DemoDetector.accept                    (DemoLogic.kt:129-201)   phase machine
  -> AlertCore.suspected()/evidenceConfirmed()  (core/src/Core.kt:73-92)  MONITORING -> SUSPECTED -> VERIFYING
```
- `PhoneSensorPacket.timestampNs` = the ACCEL `SensorEvent.timestamp` (ns, elapsedRealtime base).
- `accelXMs2/YMs2/ZMs2` = TYPE_ACCELEROMETER in m/s^2 (gravity included) — these are what the detector uses.
- `linearAccel*` = TYPE_LINEAR_ACCELERATION, `gyroXDps/YDps/ZDps` = gyroscope in deg/s (already scaled by
  180/pi in `PhoneNormalizer.update`), `pitchDeg/rollDeg/yawDeg` = degrees, `pressurePa` = Pa. All optional.
- Existing detector phases (`DetectionPhase`, `DemoLogic.kt:92`): `NORMAL`, `IMPACT_DETECTED`,
  `POST_IMPACT_STILLNESS`, `FALL_CONFIRMED`. Use these exact names — invent no new state.
- Existing AlertCore states (`core.State`, `core/src/Core.kt:9`): `MONITORING`, `SUSPECTED`, `VERIFYING`,
  `ALERTING`, `AWAITING_HELP`.
- Unit tests run on the JVM with `unitTests.isReturnDefaultValues = true`, so `android.util.Log` calls are
  safe no-ops there. Call `android.util.Log.i` directly; no availability probe needed.

## 1) NEW FILE `Fall01Trace.kt`

Package `vn.nckh27pa.fallsafe`. `object Fall01Trace` with `const val TAG = "FALL01"`. One private helper
`emit(line: String)` that calls `android.util.Log.i(TAG, line)`. All `[FALL01_*]` lines below MUST be single
`Log.i` calls (one log record per line). Values: floats formatted with `String.format(Locale.US, "%.3f", v)`
(accel/gyro/mag), `%.1f` (pressure, Pa), `%.2f` (progress). Null/absent optional values print the literal
token `null`. Never log contact data, phone numbers, location, or anything else private.

Internal mutable state (main-looper confined; document with a comment):
- `sessionStartNs: Long?`, `lastSampleNs: Long?`, `lastProfileId: String?`.
- `t(packetNs)`: `elapsedMs = (packetNs - (sessionStartNs ?: firstPacketNs)) / 1_000_000` (Long, integer ms).
  Anchor = `sessionStartNs` when a session marker was written, otherwise the first sample seen after that
  anchor was cleared. Never interpolate or alter timestamps.
- `profileChanged(o: FallDetectionObservation): Boolean` — compares `o.activeProfileId` with `lastProfileId`.

Public API (exact names/signatures):

```kotlin
fun sessionStart(ns: Long, wallMs: Long, sensors: String)   // resets anchor, lastSampleNs, lastProfileId
fun sessionStop(ns: Long, wallMs: Long)
fun sample(p: PhoneSensorPacket, o: FallDetectionObservation, alertState: String, observed: Boolean)
fun event(name: String)                                     // e.g. PIPELINE_STALE_RESET
fun transition(from: DetectionPhase, o: FallDetectionObservation, reason: String, p: PhoneSensorPacket?)
fun decision(p: PhoneSensorPacket?, o: FallDetectionObservation, detected: Boolean, reason: String)
fun alertState(from: core.State, to: core.State, reason: String, o: FallDetectionObservation?, p: PhoneSensorPacket?)
```

`sample()` emits `FALL01_PROFILE` first when the active profile changed (or was never logged), then
`FALL01_SAMPLE`. `transition()` emits nothing when `from == o.phase`. `alertState()` emits nothing when
`from == to`.

Line formats (field order fixed; every line starts with the `FALL01_` token; no field may be dropped):

```text
FALL01_SESSION,event=START,tns=<ns>,wallMs=<ms>,sensors=<ACCEL+GYRO+...>
FALL01_SESSION,event=STOP,tns=<ns>,wallMs=<ms>,elapsedMs=<ms>
FALL01_EVENT,event=PIPELINE_STALE_RESET,t=<ms>
FALL01_PROFILE,t=<ms>,profileId=<uuid>,profileNumber=<n>,displayName="<name>",revision=<n>,impact=<f>,stillTarget=<f>,stillTol=<f>,postWindowMs=<n>,stillDurationMs=<n>,minSamples=<n>,maxGapMs=<n>,pressureEvidence=<bool>,minPressureRisePa=<f>
FALL01_SAMPLE,t=<ms>,tns=<ns>,dtMs=<ms>,wallMs=<ms>,ax=<f>,ay=<f>,az=<f>,amag=<f>,lx=<f>,ly=<f>,lz=<f>,lmag=<f>,gx=<f>,gy=<f>,gz=<f>,gmag=<f>,gtns=<ns>,pitch=<f>,roll=<f>,yaw=<f>,pressurePa=<f>,alert=<State>,observed=<bool>,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,pressureCorroborated=<bool|null>
FALL01_STATE,t=<ms>,tns=<ns>,from=<DetectionPhase>,to=<DetectionPhase>,reason=<REASON>,amag=<f>,gmag=<f>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,impact=<f>,stillTarget=<f>,stillTol=<f>,postWindowMs=<n>,stillDurationMs=<n>,minSamples=<n>,maxGapMs=<n>
FALL01_DECISION,t=<ms>,tns=<ns>,detected=<bool>,reason=<REASON>,amag=<f>,gmag=<f>,phase=<DetectionPhase>,impactOver=<bool>,withinStill=<bool>,stillProgress=<f>,stillSamples=<n>,profileNumber=<n>,profileId=<uuid>,impact=<f>,stillTarget=<f>,stillTol=<f>,postWindowMs=<n>,stillDurationMs=<n>,minSamples=<n>,maxGapMs=<n>
FALL01_ALERT_STATE,t=<ms>,from=<State>,to=<State>,reason=<REASON>,profileNumber=<n>,amag=<f>
```
- `dtMs` = current packet ns minus previous logged packet ns (ms). `amag`/`gmag`/`lmag` = Euclidean norms of
  the three components (computed in the logger only; never fed back into detection).
- `amag`/`gmag` in `FALL01_STATE`/`FALL01_DECISION`/`FALL01_ALERT_STATE` come from the packet passed in
  (print `null` when no packet is available). `amag` in those lines must equal the detector's magnitude for
  that packet when a packet is passed.
- `FALL01_ALERT_STATE` has no `tns` (AlertCore transitions may not carry a packet); use `t=<ms>` from
  `(packet.timestampNs or currentElapsed)`; when `p == null`, print `t=null` and `amag=null`.

## 2) `DemoLogic.kt` — additive hooks

### 2a. `PhoneSensorPacket` (line 9-21)
Append ONE new parameter at the END of the data class (default null — keeps every existing positional
constructor call source-compatible):
`val gyroTimestampNs: Long? = null   // gyroscope sample's own SensorEvent timestamp (ns)`
In `PhoneNormalizer.packet()` (line 80-82) pass `g?.ns` for that new parameter, keeping the existing
positional arguments untouched. No other change to the normalizer.

### 2b. `DemoDetector.accept()` (lines 129-201) — instrument, never modify
Add `val previousPhase = currentObservation.phase` as the FIRST new statement inside `accept()` (before the
existing `val profile = ...` block is fine, or immediately after it — do not move existing lines).
Then, in each existing branch, append hook calls AFTER the existing `currentObservation = ...` assignment
(additions only; do not touch the existing statements):

- profile-changed branch (131-135): `Fall01Trace.transition(previousPhase, currentObservation, "PROFILE_CHANGED", p)`
  and `Fall01Trace.decision(p, currentObservation, false, "PROFILE_CHANGED")`.
- invalid-input branch (139-142, after `reset()`): `Fall01Trace.transition(previousPhase, currentObservation, "INVALID_SAMPLE_INPUT", p)`
  and `Fall01Trace.decision(p, currentObservation, false, "INVALID_SAMPLE_INPUT")`.
- sample-gap branch (146-151): BEFORE `clearEvidence()` add `val hadActiveImpact = impact != null`; after the
  existing `currentObservation = ...` add `Fall01Trace.transition(previousPhase, currentObservation, "SAMPLE_GAP_RESET", p)`
  and, only when `hadActiveImpact`, `Fall01Trace.decision(p, currentObservation, false, "SAMPLE_GAP_ABORTED_IMPACT_EPISODE")`.
- impact branch (154-162): `Fall01Trace.transition(previousPhase, currentObservation, "IMPACT_THRESHOLD_REACHED", p)`.
- no-active-impact branch (163-167): `Fall01Trace.transition(previousPhase, currentObservation, "NO_ACTIVE_IMPACT", p)`.
- window-expired branch (168-174): `Fall01Trace.transition(previousPhase, currentObservation, "POST_IMPACT_WINDOW_EXPIRED", p)`
  and `Fall01Trace.decision(p, currentObservation, false, "POST_IMPACT_WINDOW_EXPIRED")`.
- stillness-interrupted branch (175-181): `Fall01Trace.transition(previousPhase, currentObservation, "STILLNESS_INTERRUPTED", p)`
  and `Fall01Trace.decision(p, currentObservation, false, "STILLNESS_INTERRUPTED")`.
- stillness/confirmed branch (182-201): after the existing `currentObservation = observationFor(...)` add
  `Fall01Trace.transition(previousPhase, currentObservation, if (confirmed) "STILLNESS_CONFIRMED" else "STILLNESS_IN_PROGRESS", p)`
  and `Fall01Trace.decision(p, currentObservation, confirmed, if (confirmed) "STILLNESS_CONFIRMED" else "STILLNESS_IN_PROGRESS")`.
  `detected` MUST be the existing `confirmed` value — do not recompute the decision.
  (Note `clearEvidence()` on line 199 must stay exactly where it is.)

### 2c. `DemoSession` (lines 279-300) — additive alert-state hooks
Add a private helper (new code only):
```kotlin
private fun alertStep(reason: String, packet: PhoneSensorPacket?, step: () -> Unit) {
    val before = core.snapshot().state
    step()
    Fall01Trace.alertState(before, core.snapshot().state, reason, detector.observation(), packet)
}
```
Use it so that AlertCore transitions become visible while the existing call order is preserved:
- `accept()`: keep the existing condition/return untouched; inside the `if` body call
  `alertStep("FALL_CONFIRMED_BY_DETECTOR", packet) { core.suspected() }` then
  `alertStep("FALL_EVIDENCE_CONFIRMED", packet) { core.evidenceConfirmed() }`.
- `safe()`  -> `alertStep("USER_SAFE", null) { core.safe() }` then `detector.reset()`.
- `needHelp()` -> `alertStep("USER_NEED_HELP", null) { core.needHelp() }` then `detector.reset()`.
- `complete()` -> `alertStep("EVENT_COMPLETE", null) { core.complete() }` then `detector.reset()`.
- `tick()` -> `alertStep("VERIFICATION_TICK", null) { core.tick() }`.
Each block must still call the same `core.*` / `detector.reset()` functions exactly once, in the same order.

### 2d. `DemoInputAdapter.acceptPhone()` (lines 335-339) — sample stream
Every packet that reaches the phone pipeline must produce exactly ONE `FALL01_SAMPLE` line, whatever the
AlertCore state. Do it additively:
```kotlin
fun acceptPhone(packet: PhoneSensorPacket?) {
    if (!phoneOnly()) return
    display(packet)
    if (packet != null) {
        val observed = session.snapshot().state == State.MONITORING
        session.accept(packet)
        Fall01Trace.sample(packet, session.observation(), session.snapshot().state.name, observed)
    } else {
        session.resetDetection()
        Fall01Trace.event("PIPELINE_STALE_RESET")
    }
}
```
(`State` is already imported via `import core.*`.) `acceptReplay` must stay untouched so the simulated
replay never pollutes the FALL-01 log.

## 3) `PhoneSensorCollector.kt` — session markers (additive)

- In `start()`: after `activeSensors` is assigned, add
  `Fall01Trace.sessionStart(SystemClock.elapsedRealtimeNanos(), System.currentTimeMillis(), activeSensors.joinToString("+") { it.name })`.
- In `stop()`: after `activeSensors = emptySet()`, add
  `Fall01Trace.sessionStop(SystemClock.elapsedRealtimeNanos(), System.currentTimeMillis())`.
(Guard against a STOP without a START inside `sessionStop` in the logger — just skip logging then.)
Do not change registration, rate, or callback handling.

## ACCEPTANCE

- `cd android && ./gradlew --offline --no-daemon --max-workers=2 testDebugUnitTest assembleDebug` passes
  (206 pre-existing tests must stay green; you may run this compile/test check yourself).
- `git diff --stat` touches only the three files above.
- `git diff` shows ONLY added lines plus the single appended `gyroTimestampNs` data-class parameter and the
  matching `g?.ns` argument — no changed comparison, constant, or control-flow line.

## FINAL REPORT (last thing in your output, exactly this shape)

```
FILES: <path:line ranges changed>
ADDED LINES: <count>
LOG LINES IMPLEMENTED: FALL01_SESSION / FALL01_EVENT / FALL01_PROFILE / FALL01_SAMPLE / FALL01_STATE / FALL01_DECISION / FALL01_ALERT_STATE
GRADLE: <command> -> <result>
THRESHOLD_CHANGED: YES|NO
DETECTION_LOGIC_CHANGED: YES|NO
BLOCKERS:
```
