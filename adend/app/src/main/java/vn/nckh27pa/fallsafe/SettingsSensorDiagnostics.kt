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
    if (value == null || !value.isFinite()) "—"
    else value.toBigDecimal().setScale(decimals, java.math.RoundingMode.HALF_UP).toPlainString()

fun sensorDataStateVietnamese(state: SensorDataState): String = when (state) {
    SensorDataState.LIVE -> "Đang nhận dữ liệu"
    SensorDataState.LOST -> "Mất dữ liệu"
    SensorDataState.UNSUPPORTED -> "Không hỗ trợ"
}
