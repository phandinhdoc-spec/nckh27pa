# T10 — FALL-01 investigation: why a soft-surface fall (onto a bed) is not detected + instrumentation design

WORKER: AGY CLI, model `gemini-3.8-flash-medium` (Level 1, cheap). This is an INVESTIGATION-ONLY task.
ROLE: single investigator. Read only the files listed under "FILES TO READ" with `rg`/`sed`/bounded windows.
Do NOT re-read the repository, do NOT change any source file, do NOT run Gradle, do NOT use adb, do NOT install
anything. Your ONLY writable artifact is the report file named below.

## GOAL
The owner dropped a real phone onto a BED and the app did not detect a fall. This is NOT a licence to lower a
threshold. A bed absorbs energy, so the impact peak on a soft surface can be far below a hard-floor fall, and the
phone often ends up in a stable, still post-impact posture (face-down/face-up on a soft surface) for a long time.
Deliver an evidence-based investigation and an instrumentation design that makes real-device threshold selection
possible. Apply no threshold change.

## WHAT TO DELIVER (exactly one file, markdown, Vietnamese or English body is both fine)
`.ai/T10-fall-01-investigation.md` with these sections:

1. `## Current detector logic` — reconstruct the real pipeline with `path:line` anchors:
   sensor registration/sampling (`PhoneSensorCollector.kt`), the phone-side detector state machine
   (`DemoLogic.kt`), the threshold/profile table (`FallDetectionProfiles.kt`), the ESP32-side config
   (`espconfig/Models.kt`), and the shared emergency state machine (`core/src/Core.kt`). State explicitly:
   which constant is compared with what, in which units (m/s² vs g), over which window, in which phase order.
2. `## Suspected reason a soft-surface fall is missed` — a ranked list (most likely first) of concrete,
   falsifiable hypotheses, each anchored with `path:line` and each with the observation that would confirm or
   refute it. Cover at least: impact peak below the impact threshold; free-fall/minimum-acceleration phase too
   short or never below its threshold because the phone is held/braced; post-impact stillness detected too early
   (stillness during the airborne phase) or never satisfied; sampling rate/registration rate too low to catch a
   short peak; gyroscope/orientation evidence not used at all or not used in the decision; timing windows
   (`postImpactWindowMs`, `postImpactStillnessDurationMs`) sized for hard surfaces; whether the accelerometer is
   registered with a fixed delay (e.g. SENSOR_DELAY_*) that aliases short transients; whether a "suspect" state
   needs a second confirmation that a bed fall cannot produce. If the code shows a hypothesis is impossible, say
   so in one line and move on — do NOT invent defects and do NOT propose a threshold value.
3. `## Logging / instrumentation proposal` — the design of a real-device logger that records, per sample and per
   state transition, at least: `timestamp` (wall clock ms + elapsedRealtime ns), `ax ay az`, `|a|`, `gx gy gz`,
   gyro magnitude, running `min |a|` (free-fall candidate) per window, running `max |a|` (impact peak) per
   window, orientation/posture change if the system can compute it (`azimuth/pitch/roll` — say exactly which of
   these the code already computes), the fall-state transitions with their timestamps, and the **threshold
   profile actually in use** (all threshold fields + profile name) on every emitted row or as a header line.
   Specify: the exact hook points (`path:line` of the functions that must emit), the CSV/column layout with a
   header row, the file path and rotation policy on the device, how it is retrieved with one `adb` command, how
   the owner starts/stops a recording, what it costs (bytes per minute at the current sensor rate), and the
   concrete experiment list it must make measurable: drop onto bed, sit down fast, lie down, place phone firmly
   on a table, walk/run, normal posture changes. State which existing code already computes each field and which
   fields need new computation.
4. `## Files that would need modification` — the minimal file list for the instrumentation, one line per file
   with why, and an explicit note of what must NOT be touched in this investigation (thresholds, detector
   decision logic, SOS/SMS/CALL, UI copy).
5. `## Recommended next experiment` — the shortest real-device procedure (number the steps) that produces
   comparable CSV runs for the six situations, plus what would be compared afterwards to choose a profile.
   Say plainly which conclusions CANNOT be drawn without real-device data.

## FILES TO READ (targeted only; nothing else)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt` (65 lines — read it fully)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt` (pay attention to lines ~90-250: the detector and
  its phase/timing logic)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt` (threshold table, validation ranges,
  default profiles)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/espconfig/Models.kt` (ESP32-side config defaults)
- `android/core/src/Core.kt` (shared state machine: Monitoring/Suspected/Verifying/Alerting)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/MonitoringService.kt` and
  `android/app/src/main/java/vn/nckh27pa/fallsafe/SensorOwnership.kt` (only to answer: does detection keep running
  when the app is not in the foreground, and at what rate)
- `.ai/architecture.md` (durable project memory — read first, do not edit)
- `.ai/task_on_progress.md` (read §1 and §10 only; do not edit)

## KNOWN FACTS (verify, do not re-discover)
- The phone-side detector lives in `DemoLogic.kt`; the impact test is a single comparison against
  `config.impactAccelerationMs2` (`DemoLogic.kt:153`) and the ESP32 default for that field is 25.0 m/s²
  (`espconfig/Models.kt:12`), with `postImpactWindowMs = 3000` and `postImpactStillnessDurationMs = 1000`.
- Accelerometer values are already reported in m/s²; gyroscope fields exist in the sample model
  (`DemoLogic.kt:14`) — determine whether any decision actually uses them.
- `PhoneSensorCollector.kt` already computes pitch/roll/azimuth via `SensorManager.getOrientation`
  (`:51-57`) — determine whether that value reaches the detector or only the display/log.
- There is no emulator/real-device evidence for this defect yet; the owner's observation on a real phone is the
  only evidence. Never state a hypothesis as measured fact.
- NO physical device is available to you. You cannot measure anything. Your output is a code-grounded analysis
  and an instrumentation design, nothing more.

## RULES
- Read-only for every source file. The ONLY file you may create or modify is `.ai/T10-fall-01-investigation.md`.
- Do NOT change any threshold, do NOT change detector logic, do NOT touch SOS/SMS/CALL, UI, or Gradle files.
- Do NOT run `./gradlew`, do NOT use adb, do NOT run `git commit|push|reset|checkout|stash|clean`.
- Every claim needs `path:line`. If you cannot verify something from the listed files, write
  "UNVERIFIED — needs real-device data" instead of guessing.
- Do NOT propose a specific threshold value in this round; the owner will choose a profile after real-device data.

## FINAL REPORT (last thing in your output, exactly this shape)
```
PROVIDER: AGY gemini-3.8-flash-medium
REPORT FILE:
DETECTOR SUMMARY:
TOP HYPOTHESES (ranked, with path:line):
INSTRUMENTATION HOOK POINTS:
FILES THAT WOULD CHANGE:
UNVERIFIED / NEEDS DEVICE:
NEXT:
```
