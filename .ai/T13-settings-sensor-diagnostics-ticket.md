# T13 — TICKET: Settings "DỮ LIỆU CẢM BIẾN" (sensor diagnostics UI)

STATUS: ready for implementation
OWNER (implementation): one worker only — AGY CLI `gemini-3.8-flash-high` (Level 1: simple UI / simple code generation).
OWNER (verification): Hermes (project manager) — Gradle + real device + git diff.
REPO: /Users/phananh/TEMP/nckh27pa/nckh27pa (Android app under `android/`)

## GOAL

Show the live phone sensor values that TODAY feed FALL-01 inside the existing **Settings** tab
(`MainActivity.kt` -> `SettingsScreen`), as a new card section headed `DỮ LIỆU CẢM BIẾN`.

## HARD CONSTRAINTS (violation = ticket failed)

```
THRESHOLD_CHANGED = NO
DETECTION_LOGIC_CHANGED = NO
SAMPLING_CHANGED = NO
```

- Do NOT touch `FallDetectionProfiles.kt`, `espconfig/Models.kt`, `core/src/Core.kt`.
- Do NOT change any threshold/profile value, timing window, `SENSOR_DELAY_GAME`, the 500 ms freshness
  window in `PhoneNormalizer.packet`, or interpolation.
- Do NOT register any new `SensorManager` listener and do NOT create a second `PhoneSensorCollector`.
  The UI must READ the already-shared pipeline state only:
  `controller.packet` (last `PhoneSensorPacket` produced by `PhoneInputPipeline`/`PhoneNormalizer`) and
  `controller.observation` (detector observation). Both are Compose `mutableStateOf` in
  `DemoApplication.kt` (`var packet ...; private set`, `var observation ...; private set`).
- Changes must be ADDITIVE. Do not reorder or restructure existing statements in `SettingsScreen`.
- Do NOT touch `Fall01Trace.kt`, `PhoneSensorCollector.kt`, `MonitoringService.kt`, `DemoLogic.kt`.
- No emulator/device commands by the worker. Gradle compile check is allowed.

## FIRST ACTION = EDIT. Do NOT rescan the repository.

Read ONLY these files if you need context (they are small and already located):
- `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` (line ~293 `SettingsScreen`, line ~248 call site)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionCalibrationScreen.kt`
  (lines 300-365: `detectionPhaseVietnamese`, `SensorUiSnapshot`, `takeSensorUiSnapshot`; lines 404-412:
  the ~4 Hz throttled snapshot pattern to copy)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt` lines 9-22 (`PhoneSensorPacket`),
  346-354 (`PhoneInputPipeline`)
- `android/app/src/test/java/vn/nckh27pa/fallsafe/FallDetectionCalibrationUiTest.kt` lines 330-375
  (test style to copy)

## FILES TO CHANGE (exactly two new + one edit)

### 1. NEW `android/app/src/main/java/vn/nckh27pa/fallsafe/SettingsSensorDiagnostics.kt`

Pure, display-only mapping. No Android/sensor imports.

```kotlin
package vn.nckh27pa.fallsafe

import java.util.Locale
import kotlin.math.sqrt

/** Display-only state of the Settings sensor diagnostics card. No detection logic lives here. */
enum class SensorDataState { LIVE, LOST, UNSUPPORTED }

const val SENSOR_SOURCE_PHONE = "PHONE"

data class SettingsSensorDiagnostics(
    val source: String,
    val dataState: SensorDataState,
    val fallPhase: DetectionPhase,
    val accelX: Float?,
    val accelY: Float?,
    val accelZ: Float?,
    val accelMagnitudeMs2: Double?,
    val gyroXRadS: Float?,
    val gyroYRadS: Float?,
    val gyroZRadS: Float?,
    val gyroMagnitudeRadS: Double?
)

private const val DEG_TO_RAD = (Math.PI / 180.0)

/**
 * Builds the diagnostics snapshot from the SAME packet the fall detector consumes.
 * Gyro is converted deg/s -> rad/s for DISPLAY ONLY (the pipeline keeps deg/s).
 * `packet == null` means the pipeline has no fresh accelerometer sample (its own 500 ms rule).
 */
fun settingsSensorDiagnostics(
    packet: PhoneSensorPacket?,
    observation: FallDetectionObservation,
    sensorsAvailable: Boolean
): SettingsSensorDiagnostics {
    val gx = packet?.gyroXDps?.let { (it * DEG_TO_RAD).toFloat() }
    val gy = packet?.gyroYDps?.let { (it * DEG_TO_RAD).toFloat() }
    val gz = packet?.gyroZDps?.let { (it * DEG_TO_RAD).toFloat() }
    val magnitude = packet?.let {
        sqrt(
            it.accelXMs2.toDouble() * it.accelXMs2 +
                it.accelYMs2.toDouble() * it.accelYMs2 +
                it.accelZMs2.toDouble() * it.accelZMs2
        )
    }
    val gyroMagnitude = if (gx != null && gy != null && gz != null) {
        sqrt(gx.toDouble() * gx + gy.toDouble() * gy + gz.toDouble() * gz)
    } else null
    val state = when {
        packet != null -> SensorDataState.LIVE
        sensorsAvailable -> SensorDataState.LOST
        else -> SensorDataState.UNSUPPORTED
    }
    return SettingsSensorDiagnostics(
        source = SENSOR_SOURCE_PHONE,
        dataState = state,
        fallPhase = observation.phase,
        accelX = packet?.accelXMs2,
        accelY = packet?.accelYMs2,
        accelZ = packet?.accelZMs2,
        accelMagnitudeMs2 = magnitude,
        gyroXRadS = gx,
        gyroYRadS = gy,
        gyroZRadS = gz,
        gyroMagnitudeRadS = gyroMagnitude
    )
}

/** 2 decimals for acceleration, 3 for gyro; "—" when the axis is missing. */
fun formatDiagnosticNumber(value: Double?, decimals: Int): String =
    if (value == null || !value.isFinite()) "—"
    else String.format(Locale.US, "%.${decimals}f", value)

fun formatDiagnosticNumber(value: Float?, decimals: Int): String =
    formatDiagnosticNumber(value?.toDouble(), decimals)

fun sensorDataStateVietnamese(state: SensorDataState): String = when (state) {
    SensorDataState.LIVE -> "Đang nhận dữ liệu"
    SensorDataState.LOST -> "Mất dữ liệu"
    SensorDataState.UNSUPPORTED -> "Không hỗ trợ"
}
```

### 2. EDIT `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt`

(a) Add `import java.util.Locale` is NOT needed (formatting lives in the new file).
Add nothing else to imports unless the compiler asks (`Modifier.weight`, `TextAlign`, `delay`,
`isActive`, `TextAlign` may be needed — `androidx.compose.foundation.layout.*`,
`androidx.compose.runtime.*` are already imported). Add `import androidx.compose.ui.text.style.TextAlign`
and `import kotlinx.coroutines.delay` + `import kotlinx.coroutines.isActive` ONLY if not already present
(check first: `androidx.compose.runtime.*` is present, `isActive`/`delay` are NOT).

(b) In `DemoScreen`, the call site (currently ~line 248-256) passes `sensorSummary`. Keep it unchanged.

(c) At the END of `SettingsScreen`'s `Column` (currently the last two items are
`Action("Dùng cảm biến điện thoại thật", ...)` and `Text("Cảm biến đã đăng ký: ...", ...)`, ~line 411-412),
APPEND one call — inserted directly AFTER the existing `Text("Cảm biến đã đăng ký: ...")` line and
before the closing `}` of the Column:

```kotlin
        SensorDiagnosticsSection(
            controller = c,
            sensorsAvailable = sensorSummary.isNotBlank() && sensorSummary != NO_SENSOR_SUMMARY
        )
```

(d) Add the shared literal next to the new section (private to this file):

```kotlin
private const val NO_SENSOR_SUMMARY = "Không có cảm biến khả dụng"
```

and replace the two existing occurrences of the literal `"Không có cảm biến khả dụng"` in
`onResume` / `onMonitoringChanged` with `NO_SENSOR_SUMMARY` (behaviour identical).

(e) Add the new composable (place it directly below `SettingsScreen`, above `private fun Action(...)`):

```kotlin
/**
 * Diagnostics only: reads the SAME packet/observation the fall detector consumes.
 * No new sensor listener; no detection logic; updates at ~4 Hz so the page does not jump.
 */
@Composable
private fun SensorDiagnosticsSection(
    controller: vn.nckh27pa.fallsafe.DemoController,
    sensorsAvailable: Boolean
) {
    var diagnostics by remember {
        mutableStateOf(
            settingsSensorDiagnostics(controller.packet, controller.observation, sensorsAvailable)
        )
    }
    LaunchedEffect(sensorsAvailable) {
        while (isActive) {
            diagnostics = settingsSensorDiagnostics(controller.packet, controller.observation, sensorsAvailable)
            delay(250L)
        }
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        border = BorderStroke(1.5.dp, MaterialTheme.colorScheme.outlineVariant),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(14.dp)
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(
                "DỮ LIỆU CẢM BIẾN",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            // Gia tốc kế
            Text("Gia tốc kế", fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
            DiagnosticAxisRow("X", formatDiagnosticNumber(diagnostics.accelX, 2) + " m/s²")
            DiagnosticAxisRow("Y", formatDiagnosticNumber(diagnostics.accelY, 2) + " m/s²")
            DiagnosticAxisRow("Z", formatDiagnosticNumber(diagnostics.accelZ, 2) + " m/s²")
            DiagnosticAxisRow("Độ lớn", formatDiagnosticNumber(diagnostics.accelMagnitudeMs2, 2) + " m/s²")

            // Con quay hồi chuyển
            Text("Con quay hồi chuyển", fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
            DiagnosticAxisRow("X", formatDiagnosticNumber(diagnostics.gyroXRadS, 3) + " rad/s")
            DiagnosticAxisRow("Y", formatDiagnosticNumber(diagnostics.gyroYRadS, 3) + " rad/s")
            DiagnosticAxisRow("Z", formatDiagnosticNumber(diagnostics.gyroZRadS, 3) + " rad/s")
            DiagnosticAxisRow("Độ lớn", formatDiagnosticNumber(diagnostics.gyroMagnitudeRadS, 3) + " rad/s")

            Text("Nguồn cảm biến: ${diagnostics.source}", fontSize = 15.sp)
            Text(
                "Trạng thái dữ liệu: ${sensorDataStateVietnamese(diagnostics.dataState)} (${diagnostics.dataState.name})",
                fontSize = 15.sp
            )
            Text(
                "FALL state: ${diagnostics.fallPhase.name} · ${detectionPhaseVietnamese(diagnostics.fallPhase)}",
                fontSize = 15.sp
            )
            Text(
                "Hệ thống: ${controller.snapshot.state.name}",
                fontSize = 15.sp
            )
        }
    }
}

@Composable
private fun DiagnosticAxisRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, modifier = Modifier.weight(0.35f), fontSize = 16.sp)
        Text(
            value,
            modifier = Modifier.weight(0.65f),
            fontSize = 16.sp,
            textAlign = TextAlign.End,
            fontWeight = FontWeight.Medium
        )
    }
}
```

Notes for the implementer:
- Row child COUNT is constant, the value column is right-aligned -> no layout jump at 4 Hz.
- `textAlign` needs `import androidx.compose.ui.text.style.TextAlign`.
- If `controller.snapshot` is not accessible from that scope, drop ONLY that last `Text` line
  and say so in your summary. Do not invent another data source.
- `DemoController` is declared in `DemoApplication.kt`; reference it directly
  (`controller: DemoController`) if it resolves in this file without a fully-qualified name
  (it does: same package). Prefer the plain name.

### 3. NEW `android/app/src/test/java/vn/nckh27pa/fallsafe/SettingsSensorDiagnosticsTest.kt`

JUnit4 + `org.junit.Assert.*`, same style as `FallDetectionCalibrationUiTest.kt`. Build the
`FallDetectionObservation` exactly like that file does (`FallDetectionConfig.DEFAULT`, id "p1",
displayName "Bảng 1", phase NORMAL/IMPACT_DETECTED). Required cases:

1. still phone, accel `(0f, 0f, 9.80665f)`, no gyro -> `dataState == LIVE`,
   `source == "PHONE"`, magnitude ≈ 9.80665 (±0.01), gyro values null and `gyroMagnitudeRadS == null`.
2. gyro given in deg/s `(180f, -90f, 0f)` -> rad/s ≈ `(3.141593, -1.570796, 0.0)` (±1e-4);
   gyro magnitude ≈ 3.512407 (±1e-4). Proves the rad/s conversion is display-only arithmetic.
3. `packet = null`, `sensorsAvailable = true` -> `LOST`; `packet = null`, `sensorsAvailable = false`
   -> `UNSUPPORTED`; packet present -> `LIVE` even when `sensorsAvailable = false`.
4. `fallPhase` mirrors `observation.phase` (pass `IMPACT_DETECTED`).
5. formatting: `formatDiagnosticNumber(9.80665, 2) == "9.81"`,
   `formatDiagnosticNumber(0.0865f, 3) == "0.087"`, `formatDiagnosticNumber(null, 2) == "—"`,
   `formatDiagnosticNumber(Float.NaN, 3) == "—"`.
6. `sensorDataStateVietnamese` returns "Đang nhận dữ liệu" / "Mất dữ liệu" / "Không hỗ trợ".

## ACCEPTANCE (Hermes runs these, not the worker)

```bash
cd /Users/phananh/TEMP/nckh27pa/nckh27pa/android
./gradlew testDebugUnitTest assembleDebug --rerun-tasks --no-daemon --max-workers=2
```
Pre-existing baseline: 30 suites / 206 tests / 0 failures. New tests must ADD, none may be deleted or
weakened. Then Hermes installs on the real device (2201117SG) and checks:
still -> |a| ≈ 9.8, gyro ≈ 0; rotate -> gyro changes; shake -> accel changes;
UI shows `Nguồn cảm biến: PHONE`, `Trạng thái dữ liệu: Đang nhận dữ liệu (LIVE)`;
`adb logcat FALL01:V '*:S'` still shows `FALL01_SAMPLE`.

## DELIVERABLE FROM THE WORKER

Edit the files, then report (short): files changed, the exact Gradle command + result, and any
deviation you were forced to make. Do not claim device results — you have no device.
