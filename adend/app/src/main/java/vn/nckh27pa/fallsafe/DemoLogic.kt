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
    val pressureWindowDeltaPa: Float? = null,
    val stepCount: Long? = null, val stepDetected: Boolean? = null,
    val phoneMotionState: String = "UNKNOWN", val phonePlacementConfidence: Int = 0,
    val sensorQuality: Int = 0,
    val gyroTimestampNs: Long? = null   // gyroscope sample's own SensorEvent timestamp (ns)
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
        val gyroNs = fresh(SensorKind.GYRO)?.ns
        val o = fresh(SensorKind.ORIENTATION)?.values
        val pressure = fresh(SensorKind.PRESSURE)?.values?.get(0)
        val altitudeDelta = pressure?.let { current ->
            pressureHistory.firstOrNull()?.pressurePa?.let { baseline ->
                (44330.0 * (1.0 - (current / baseline).toDouble().pow(1.0 / 5.255))).toFloat()
                    .takeIf { it.isFinite() }
            }
        }
        val pressureWindowDelta = pressure?.let { current ->
            pressureHistory.firstOrNull()?.pressurePa?.let { oldest -> current - oldest }
        }
        return PhoneSensorPacket(a.ns, wallMs, a.values[0], a.values[1], a.values[2],
            l?.get(0), l?.get(1), l?.get(2), g?.get(0), g?.get(1), g?.get(2),
            o?.get(0), o?.get(1), o?.get(2), pressure, altitudeDelta, pressureWindowDelta,
            gyroTimestampNs = gyroNs)
    }

    private companion object {
        const val PRESSURE_WINDOW_NS = 5_000_000_000L
        const val MAX_PRESSURE_SAMPLES = 64
    }
}

/** Observable phases mirror the demo detector state machine; they are not medical claims. */
enum class DetectionPhase { NORMAL, IMPACT_DETECTED, POST_IMPACT_STILLNESS, FALL_CONFIRMED }

data class FallDetectionObservation(
    val accelerationMagnitudeMs2: Double?,
    val phase: DetectionPhase,
    val activeProfileId: String,
    val activeProfileDisplayName: String,
    val config: FallDetectionConfig,
    val impactOverThreshold: Boolean,
    val withinStillness: Boolean,
    val stillnessProgress: Float,
    val stillnessSampleCount: Int,
    val pressureCorroborated: Boolean? = null
)

/** DEMO detector driven only by the seven active profile parameters. */
class DemoDetector(
    private val activeProfileProvider: () -> FallDetectionProfile,
    private val pressureCapability: () -> Boolean
) {
    constructor() : this({ DEFAULT_DETECTION_PROFILE }, { false })
    constructor(activeProfileProvider: () -> FallDetectionProfile) : this(activeProfileProvider, { false })
    private var last: Long? = null
    private var impact: Long? = null
    private var quiet: Long? = null
    private var count = 0
    private var observedProfile = activeProfileProvider()
    private var currentObservation = normalObservation(observedProfile)

    fun reset() {
        clearEvidence()
        observedProfile = activeProfileProvider()
        currentObservation = normalObservation(observedProfile)
    }

    fun observation(): FallDetectionObservation = currentObservation

    fun accept(p: PhoneSensorPacket): Boolean {
        val previousPhase = currentObservation.phase
        val profile = activeProfileProvider()
        if (profile.id != observedProfile.id || profile.config != observedProfile.config) {
            clearEvidence()
            observedProfile = profile
            currentObservation = normalObservation(profile)
            Fall01Trace.transition(previousPhase, currentObservation, "PROFILE_CHANGED", p)
            Fall01Trace.decision(p, currentObservation, false, "PROFILE_CHANGED")
        }
        val config = profile.config
        val t = p.timestampNs
        val acceleration = listOf(p.accelXMs2, p.accelYMs2, p.accelZMs2)
        if (t < 0 || acceleration.any { !it.isFinite() }) {
            reset()
            Fall01Trace.transition(previousPhase, currentObservation, "INVALID_SAMPLE_INPUT", p)
            Fall01Trace.decision(p, currentObservation, false, "INVALID_SAMPLE_INPUT")
            return false
        }
        val magnitude = sqrt(acceleration.sumOf { it.toDouble() * it.toDouble() })
        val previous = last
        val maximumGapNs = config.maximumSampleGapMs * 1_000_000L
        if (previous != null && (t <= previous || t - previous > maximumGapNs)) {
            val hadActiveImpact = impact != null
            clearEvidence()
            last = t
            currentObservation = observationFor(profile, magnitude, DetectionPhase.NORMAL)
            Fall01Trace.transition(previousPhase, currentObservation, "SAMPLE_GAP_RESET", p)
            if (hadActiveImpact) {
                Fall01Trace.decision(p, currentObservation, false, "SAMPLE_GAP_ABORTED_IMPACT_EPISODE")
            }
            return false
        }
        last = t
        val overImpact = magnitude >= config.impactAccelerationMs2
        if (overImpact) {
            impact = t
            quiet = null
            count = 0
            currentObservation = observationFor(
                profile, magnitude, DetectionPhase.IMPACT_DETECTED, impactOverThreshold = true
            )
            Fall01Trace.transition(previousPhase, currentObservation, "IMPACT_THRESHOLD_REACHED", p)
            return false
        }
        val hit = impact
        if (hit == null) {
            currentObservation = observationFor(profile, magnitude, DetectionPhase.NORMAL)
            Fall01Trace.transition(previousPhase, currentObservation, "NO_ACTIVE_IMPACT", p)
            return false
        }
        if (t - hit > config.postImpactWindowMs * 1_000_000L) {
            impact = null
            quiet = null
            count = 0
            currentObservation = observationFor(profile, magnitude, DetectionPhase.NORMAL)
            Fall01Trace.transition(previousPhase, currentObservation, "POST_IMPACT_WINDOW_EXPIRED", p)
            Fall01Trace.decision(p, currentObservation, false, "POST_IMPACT_WINDOW_EXPIRED")
            return false
        }
        val still = abs(magnitude - config.stillnessTargetAccelerationMs2) <= config.stillnessToleranceMs2
        if (!still) {
            quiet = null
            count = 0
            currentObservation = observationFor(profile, magnitude, DetectionPhase.IMPACT_DETECTED)
            Fall01Trace.transition(previousPhase, currentObservation, "STILLNESS_INTERRUPTED", p)
            Fall01Trace.decision(p, currentObservation, false, "STILLNESS_INTERRUPTED")
            return false
        }
        if (quiet == null) quiet = t
        count++
        val elapsedNs = t - quiet!!
        val requiredNs = config.postImpactStillnessDurationMs * 1_000_000L
        val progress = if (requiredNs == 0L) 1f else (elapsedNs.toDouble() / requiredNs).toFloat().coerceIn(0f, 1f)
        val confirmed = count >= config.minimumStillnessSamples && elapsedNs >= requiredNs
        currentObservation = observationFor(
            profile = profile,
            magnitude = magnitude,
            phase = if (confirmed) DetectionPhase.FALL_CONFIRMED else DetectionPhase.POST_IMPACT_STILLNESS,
            withinStillness = true,
            progress = progress,
            samples = count,
            pressureCorroborated = if (confirmed && config.phonePressureEvidenceEnabled && pressureCapability()) {
                (p.pressureWindowDeltaPa ?: Float.NEGATIVE_INFINITY) >= config.phonePressureMinimumRisePa
            } else null
        )
        Fall01Trace.transition(previousPhase, currentObservation, if (confirmed) "STILLNESS_CONFIRMED" else "STILLNESS_IN_PROGRESS", p)
        Fall01Trace.decision(p, currentObservation, confirmed, if (confirmed) "STILLNESS_CONFIRMED" else "STILLNESS_IN_PROGRESS")
        if (confirmed) clearEvidence()
        return confirmed
    }

    private fun clearEvidence() {
        last = null
        impact = null
        quiet = null
        count = 0
    }

    private fun observationFor(
        profile: FallDetectionProfile,
        magnitude: Double,
        phase: DetectionPhase,
        impactOverThreshold: Boolean = false,
        withinStillness: Boolean = false,
        progress: Float = 0f,
        samples: Int = 0,
        pressureCorroborated: Boolean? = null
    ) = FallDetectionObservation(
        accelerationMagnitudeMs2 = magnitude,
        phase = phase,
        activeProfileId = profile.id,
        activeProfileDisplayName = profile.displayName,
        config = profile.config,
        impactOverThreshold = impactOverThreshold,
        withinStillness = withinStillness,
        stillnessProgress = progress,
        stillnessSampleCount = samples,
        pressureCorroborated = pressureCorroborated
    )

    private fun normalObservation(profile: FallDetectionProfile) = FallDetectionObservation(
        accelerationMagnitudeMs2 = null,
        phase = DetectionPhase.NORMAL,
        activeProfileId = profile.id,
        activeProfileDisplayName = profile.displayName,
        config = profile.config,
        impactOverThreshold = false,
        withinStillness = false,
        stillnessProgress = 0f,
        stillnessSampleCount = 0
    )

    private companion object {
        val DEFAULT_DETECTION_PROFILE = FallDetectionProfile(
            id = "experimental-default",
            displayName = "Bảng mặc định thử nghiệm",
            profileNumber = 0,
            createdAt = 0,
            updatedAt = 0,
            isActive = true,
            config = FallDetectionConfig.DEFAULT
        )
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
class DemoSession(
    clock: MonotonicClock,
    val sink: LocalDemoSink = LocalDemoSink(),
    profileRepository: FallDetectionProfileRepository? = null
) {
    private val core = AlertCore(clock, sink)
    private val detector = if (profileRepository == null) DemoDetector()
        else DemoDetector(activeProfileProvider = { profileRepository.activeProfile() })
    private fun alertStep(reason: String, packet: PhoneSensorPacket?, step: () -> Unit) {
        val before = core.snapshot().state
        step()
        Fall01Trace.alertState(before, core.snapshot().state, reason, detector.observation(), packet)
    }
    fun resetDetection() = detector.reset()
    fun observation() = detector.observation()
    fun accept(packet: PhoneSensorPacket) {
        if (core.snapshot().state == State.MONITORING && detector.accept(packet)) {
            alertStep("FALL_CONFIRMED_BY_DETECTOR", packet) { core.suspected() }
            alertStep("FALL_EVIDENCE_CONFIRMED", packet) { core.evidenceConfirmed() }
        }
    }
    fun snapshot() = core.snapshot()
    fun events() = core.events()
    fun tick() = alertStep("VERIFICATION_TICK", null) { core.tick() }
    fun safe() { alertStep("USER_SAFE", null) { core.safe() }; detector.reset() }
    fun needHelp() { alertStep("USER_NEED_HELP", null) { core.needHelp() }; detector.reset() }
    fun complete() { alertStep("EVENT_COMPLETE", null) { core.complete() }; detector.reset() }
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
        if (packet != null) {
            val observed = session.snapshot().state == State.MONITORING
            session.accept(packet)
            Fall01Trace.sample(packet, session.observation(), session.snapshot().state.name, observed)
        } else {
            session.resetDetection()
            Fall01Trace.event("PIPELINE_STALE_RESET")
        }
    }
    fun acceptReplay(packet: PhoneSensorPacket) { display(packet); session.accept(packet) }
    fun paused(backgroundMonitoring: Boolean = false) {
        if (phoneOnly() && !backgroundMonitoring) { display(null); session.resetDetection() }
    }
}
