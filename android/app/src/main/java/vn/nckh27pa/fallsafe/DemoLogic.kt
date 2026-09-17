package vn.nckh27pa.fallsafe

import core.*
import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.sqrt

/** Names/types follow plan §4.5.2. Confidence 0 + UNKNOWN means unestimated. */
data class PhoneSensorPacket(
    val timestampNs: Long, val wallClockTimestampMs: Long,
    val accelXMs2: Float, val accelYMs2: Float, val accelZMs2: Float,
    val linearAccelXMs2: Float? = null, val linearAccelYMs2: Float? = null,
    val linearAccelZMs2: Float? = null,
    val gyroXDps: Float? = null, val gyroYDps: Float? = null, val gyroZDps: Float? = null,
    val pitchDeg: Float? = null, val rollDeg: Float? = null, val yawDeg: Float? = null,
    val pressurePa: Float? = null, val altitudeDeltaM: Float? = null,
    val stepCount: Long? = null, val stepDetected: Boolean? = null,
    val phoneMotionState: String = "UNKNOWN", val phonePlacementConfidence: Int = 0,
    val sensorQuality: Int = 0
)
enum class SensorKind { ACCEL, LINEAR, GYRO, ORIENTATION, PRESSURE }
class PhoneNormalizer {
    private data class Reading(val ns: Long, val values: List<Float>)
    private data class PressureReading(val ns: Long, val pressurePa: Float)
    private val readings = mutableMapOf<SensorKind, Reading>()
    private val pressureHistory = ArrayDeque<PressureReading>()
    fun clear() {
        readings.clear()
        pressureHistory.clear()
    }
    fun update(kind: SensorKind, ns: Long, values: FloatArray) {
        val count = if (kind == SensorKind.PRESSURE) 1 else 3
        if (ns < 0 || values.size < count || values.take(count).any { !it.isFinite() } ||
            (kind == SensorKind.PRESSURE && values[0] <= 0f)) {
            readings.remove(kind)
            if (kind == SensorKind.PRESSURE) pressureHistory.clear()
            return
        }
        if (ns < (readings[kind]?.ns ?: -1)) return
        val scale = when (kind) {
            SensorKind.GYRO -> (180.0 / Math.PI).toFloat()
            SensorKind.PRESSURE -> 100f
            else -> 1f
        }
        val converted = values.take(count).map { it * scale }
        if (converted.any { !it.isFinite() }) {
            readings.remove(kind)
            if (kind == SensorKind.PRESSURE) pressureHistory.clear()
            return
        }
        readings[kind] = Reading(ns, converted)
        if (kind == SensorKind.PRESSURE) {
            if (pressureHistory.lastOrNull()?.ns == ns) pressureHistory.removeLast()
            pressureHistory.addLast(PressureReading(ns, converted[0]))
            while (pressureHistory.firstOrNull()?.let { ns - it.ns > PRESSURE_WINDOW_NS } == true) {
                pressureHistory.removeFirst()
            }
            while (pressureHistory.size > MAX_PRESSURE_SAMPLES) pressureHistory.removeFirst()
        }
    }
    fun packet(nowNs: Long, wallMs: Long): PhoneSensorPacket? {
        fun fresh(kind: SensorKind) = readings[kind]?.takeIf {
            nowNs >= it.ns && nowNs - it.ns <= 500_000_000
        }
        val a = fresh(SensorKind.ACCEL) ?: return null
        val l = fresh(SensorKind.LINEAR)?.values
        val g = fresh(SensorKind.GYRO)?.values
        val o = fresh(SensorKind.ORIENTATION)?.values
        val pressure = fresh(SensorKind.PRESSURE)?.values?.get(0)
        val altitudeDelta = pressure?.let { current ->
            pressureHistory.firstOrNull()?.pressurePa?.let { baseline ->
                (44330.0 * (1.0 - (current / baseline).toDouble().pow(1.0 / 5.255))).toFloat()
                    .takeIf { it.isFinite() }
            }
        }
        return PhoneSensorPacket(a.ns, wallMs, a.values[0], a.values[1], a.values[2],
            l?.get(0), l?.get(1), l?.get(2), g?.get(0), g?.get(1), g?.get(2),
            o?.get(0), o?.get(1), o?.get(2), pressure, altitudeDelta)
    }

    private companion object {
        const val PRESSURE_WINDOW_NS = 5_000_000_000L
        const val MAX_PRESSURE_SAMPLES = 64
    }
}

/** DEMO only: impact >=25 m/s², then >=1 s quiet at 9.81±1 within 3 s.
 * At least 6 quiet samples; gaps >250 ms, invalid or reversed times break evidence.
 * These illustrative thresholds are not a validated fall detector.
 */
class DemoDetector {
    private var last: Long? = null
    private var impact: Long? = null
    private var quiet: Long? = null
    private var count = 0
    fun reset() { last = null; impact = null; quiet = null; count = 0 }
    fun accept(p: PhoneSensorPacket): Boolean {
        val t = p.timestampNs
        val a = listOf(p.accelXMs2, p.accelYMs2, p.accelZMs2)
        if (t < 0 || a.any { !it.isFinite() }) { reset(); return false }
        val previous = last
        if (previous != null && (t <= previous || t - previous > 250_000_000)) {
            reset(); last = t; return false
        }
        last = t
        val magnitude = sqrt(a.sumOf { it.toDouble() * it.toDouble() })
        if (magnitude >= 25) { impact = t; quiet = null; count = 0; return false }
        val hit = impact ?: return false
        if (t - hit > 3_000_000_000L) { impact = null; quiet = null; count = 0; return false }
        if (abs(magnitude - 9.81) > 1) { quiet = null; count = 0; return false }
        if (quiet == null) quiet = t
        count++
        if (count >= 6 && t - quiet!! >= 1_000_000_000) { reset(); return true }
        return false
    }
}
class DemoReplay(private val startMs: Long) {
    private var index = 0
    val finished get() = index > 16
    fun due(nowMs: Long): List<PhoneSensorPacket> {
        val result = mutableListOf<PhoneSensorPacket>()
        while (!finished && nowMs >= startMs && nowMs - startMs >= index * 100L) {
            result += PhoneSensorPacket((startMs + index * 100L) * 1_000_000, 0,
                0f, 0f, if (index == 2) 30f else 9.81f)
            index++
        }
        return result
    }
}
class LocalDemoSink : AlertSink {
    var fail = false
    private val received = ArrayDeque<Alert>()
    override fun send(alert: Alert) {
        if (fail) error("Lỗi mô phỏng")
        received.addLast(alert)
        while (received.size > 32) received.removeFirst()
    }
    fun alerts() = received.toList()
}
class DemoSession(clock: MonotonicClock, val sink: LocalDemoSink = LocalDemoSink()) {
    private val core = AlertCore(clock, sink)
    private val detector = DemoDetector()
    fun resetDetection() = detector.reset()
    fun accept(packet: PhoneSensorPacket) {
        if (core.snapshot().state == State.MONITORING && detector.accept(packet)) {
            core.suspected(); core.evidenceConfirmed()
        }
    }
    fun snapshot() = core.snapshot()
    fun events() = core.events()
    fun tick() = core.tick()
    fun safe() { core.safe(); detector.reset() }
    fun needHelp() { core.needHelp(); detector.reset() }
    fun complete() { core.complete(); detector.reset() }
}
class SosHold(val requiredMs: Long = 2000L) {
    private var started: Long? = null
    fun start(nowMs: Long) { started = nowMs }
    fun cancel() { started = null }
    fun ready(nowMs: Long): Boolean {
        val start = started ?: return false
        if (nowMs >= start && nowMs - start >= requiredMs) { cancel(); return true }
        return false
    }
    fun isHolding(): Boolean = started != null
    fun progress(nowMs: Long): Float {
        val start = started ?: return 0f
        if (nowMs <= start) return 0f
        return ((nowMs - start).toFloat() / requiredMs).coerceIn(0f, 1f)
    }
}

/** Shared by the Android collector and offline pipeline tests. */
class PhoneInputPipeline(private val onPacket: (PhoneSensorPacket?) -> Unit) {
    private val normalizer = PhoneNormalizer()
    fun clear() = normalizer.clear()
    fun latest(nowNs: Long, wallMs: Long) = normalizer.packet(nowNs, wallMs)
    fun update(kind: SensorKind, ns: Long, values: FloatArray, nowNs: Long, wallMs: Long) {
        normalizer.update(kind, ns, values)
        if (kind == SensorKind.ACCEL) onPacket(latest(nowNs, wallMs))
    }
}

/** Input and pause policy shared with the process-scoped controller. */
class DemoInputAdapter(
    private val session: DemoSession,
    private val phoneOnly: () -> Boolean,
    private val display: (PhoneSensorPacket?) -> Unit
) {
    fun acceptPhone(packet: PhoneSensorPacket?) {
        if (!phoneOnly()) return
        display(packet)
        if (packet != null) session.accept(packet) else session.resetDetection()
    }
    fun acceptReplay(packet: PhoneSensorPacket) { display(packet); session.accept(packet) }
    fun paused(backgroundMonitoring: Boolean = false) {
        if (phoneOnly() && !backgroundMonitoring) { display(null); session.resetDetection() }
    }
}
