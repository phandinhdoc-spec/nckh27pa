# T7 — AGY / Gemini (Level 1): location card status + SOS SIM_CALL step in the UI

STATUS: QUEUED (starts after T6 lands — one Gradle build at a time)
OWNER: AGY (single primary worker for `HomeScreen.kt`)
GOAL: the "VỊ TRÍ CỦA TÔI" area shows a real location state (accuracy + age + source), an in-progress
state instead of a failure, and the improvement hint only as a hint. The SOS progress dialog shows the
new `SIM_CALL` step.

## Frozen contract — already in the repository, do NOT edit these files
`emergency/EmergencyCore.kt` (owned by T6):
```kotlin
enum class LocationSource { PHONE, ESP32_GNSS, FUSED, GPS, NETWORK, CACHED, UNKNOWN }
enum class LocationFreshness { FRESH, STALE, UNAVAILABLE }
enum class LocationFailureCause { PERMISSION_DENIED, PROVIDER_DISABLED, TIMEOUT, NO_FIX, INVALID_FIX }
data class LocationState(val fix: LocationFix?, val freshness: LocationFreshness,
                         val cause: LocationFailureCause?, val explanation: String, val remediation: String?)
enum class SosStep { LOCATION, SMS, VOICE_CALL, SIM_CALL, MAP_LINK }  // SIM_CALL is new
```
`LocationFix` exposes `latitude`, `longitude`, `accuracyM: Float?`, `fixTimeMs: Long`, `source`,
`mapsUrl`, `geoUri`. After T6 the waiting state is `explanation="Đang xác định vị trí..."` with
`remediation=null`, and remediation only ever holds an improvement hint such as
"Ra nơi thoáng hơn có thể tăng độ chính xác; cảnh báo vẫn được gửi."

## Files you own (touch NOTHING else)
- `app/src/main/java/vn/nckh27pa/fallsafe/HomeScreen.kt`
- `app/src/test/java/vn/nckh27pa/fallsafe/HomeScreenPureUiTest.kt`

## Work
1. Replace `resolveLocationCardStatus(loc: LocationState)` (HomeScreen.kt:107) with a pure helper that
   returns the three facts the owner asked for, keeping the project's existing style of pure string
   helpers so it stays unit-testable without Android:
   ```kotlin
   internal data class LocationCardView(val title: String, val detail: String, val hint: String?)
   internal fun resolveLocationCardView(loc: LocationState, nowMs: Long): LocationCardView
   ```
   Rules:
   - no fix + cause `NO_FIX`/`TIMEOUT` → title `Đang xác định...`, detail `Vị trí sẽ được gửi ngay khi có`,
     hint null.
   - no fix + cause `PERMISSION_DENIED` → title `Chưa cấp quyền vị trí`, detail `Ứng dụng chưa được phép
     dùng vị trí`, hint = the remediation text when present.
   - no fix + cause `PROVIDER_DISABLED` → title `Vị trí đang tắt`, detail `Dịch vụ vị trí của máy đang tắt`,
     hint = the remediation text when present.
   - fix + `FRESH` → title `Đã xác định`, detail = `± N m` when `accuracyM != null` else `Độ chính xác
     chưa rõ`, plus an age line `Cập nhật N giây trước` (`vừa xong` when age < 5 s, `N phút trước` when
     ≥ 60 s), hint null.
   - fix + `STALE` → title `Vị trí gần đúng`, detail = `± N m` + `Cập nhật N phút trước`, hint =
     remediation when present.
   Never say a fix is absent when one exists, and never present the improvement hint as an error or as a
   reason SOS cannot be sent.
2. Render it where the location card is built (HomeScreen.kt:216-221 and the `PeripheralCardContent`
   call at ~line 311): `statusText = view.title`, `subline = view.detail`, and keep tap-to-open-maps.
   Keep high contrast and the existing ≥16sp text / ≥56dp touch rules.
3. In the SOS per-step dialog (HomeScreen.kt:734-800 area, note lines 744-747 currently print
   `loc.explanation` + `loc.remediation` in `WarningOrange`): show explanation at 16sp and, when present,
   the hint in a NEUTRAL colour with a lightbulb-free plain label, e.g. `Gợi ý: <hint>`, at ≥15sp.
   The hint must not read as a blocker. No colour change when the hint is the waiting text.
4. Confirm the per-step SOS list renders `SIM_CALL` correctly (it iterates `report.steps`, so this is
   usually automatic at line ~1135): its label must come from `SosStep.SIM_CALL.vietnameseLabel`
   ("Cuộc gọi SIM") and every `SosStepStatus` must already map in `resolveSosStepStatusLabel`
   (line 91). Add the mapping only if something is missing.
5. Update `HomeScreenPureUiTest.kt` for the new helper: cover FRESH with accuracy (age in seconds),
   FRESH vừa xong, STALE, NO_FIX in-progress, PERMISSION_DENIED, PROVIDER_DISABLED, and a fix with
   `accuracyM == null`. Assert the exact strings the helper produces and that no state returns a string
   containing `thoáng` as an error title.
   You may keep the old `resolveLocationCardStatus` only if nothing references it; otherwise delete it.

## Acceptance for T7
- `cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon` PASS.
- `./gradlew assembleDebug --no-daemon` PASS.
- No change outside your two files. Do NOT commit.

## Report back (short)
DONE / CHANGED / TEST (exact command + counts) / NEXT / BLOCKER
