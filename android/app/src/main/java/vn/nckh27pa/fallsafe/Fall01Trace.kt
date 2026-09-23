package vn.nckh27pa.fallsafe

import java.util.Locale
import kotlin.math.sqrt

/**
 * Fall01Trace - Instrumentation logger for the FALL-01 real-device pipeline.
 * Confined to Android main looper / single thread.
 */
object Fall01Trace {
    const val TAG = "FALL01"

    // Internal mutable state (main-looper confined)
    private var sessionStartNs: Long? = null
    private var firstPacketNs: Long? = null
    private var lastSampleNs: Long? = null
    private var lastProfileId: String? = null
    private var lastProfileConfig: FallDetectionConfig? = null
    private val profileAnnouncements = mutableMapOf<String, Long>()
    private var sampleCount: Int = 0
    private var confirmationCount: Int = 0

    private fun emit(line: String) {
        android.util.Log.i(TAG, line)
    }

    private fun f3(v: Float?): String =
        if (v != null) String.format(Locale.US, "%.3f", v) else "null"

    private fun f1(v: Float?): String =
        if (v != null) String.format(Locale.US, "%.1f", v) else "null"

    private fun f2(v: Float?): String =
        if (v != null) String.format(Locale.US, "%.2f", v) else "null"

    private fun norm3(x: Float?, y: Float?, z: Float?): Float? {
        if (x == null || y == null || z == null) return null
        return sqrt((x.toDouble() * x + y.toDouble() * y + z.toDouble() * z)).toFloat()
    }

    private fun t(packetNs: Long): Long {
        val anchor = sessionStartNs ?: firstPacketNs ?: run {
            firstPacketNs = packetNs
            packetNs
        }
        return (packetNs - anchor) / 1_000_000L
    }

    private fun profileChanged(o: FallDetectionObservation): Boolean {
        return o.activeProfileId != lastProfileId || o.config != lastProfileConfig
    }

    private fun extractProfileNumber(displayName: String): Int {
        val match = Regex("""\d+""").find(displayName)
        return match?.value?.toIntOrNull() ?: 0
    }

    fun sessionStart(ns: Long, wallMs: Long, sensors: String) {
        sessionStartNs = ns
        firstPacketNs = null
        lastSampleNs = null
        lastProfileId = null
        lastProfileConfig = null
        profileAnnouncements.clear()
        sampleCount = 0
        confirmationCount = 0
        emit("FALL01_SESSION,event=START,tns=$ns,wallMs=$wallMs,sensors=$sensors")
    }

    fun sessionStop(ns: Long, wallMs: Long) {
        val startNs = sessionStartNs ?: return
        val elapsedMs = (ns - startNs) / 1_000_000L
        val confirmed = confirmationCount > 0
        emit("FALL01_SESSION,event=STOP,tns=$ns,wallMs=$wallMs,elapsedMs=$elapsedMs,samples=$sampleCount,confirmations=$confirmationCount,confirmed=$confirmed")
        sessionStartNs = null
        firstPacketNs = null
        lastSampleNs = null
        lastProfileId = null
        lastProfileConfig = null
        profileAnnouncements.clear()
        sampleCount = 0
        confirmationCount = 0
    }

    fun event(name: String) {
        val tMs = lastSampleNs?.let { t(it) } ?: 0L
        emit("FALL01_EVENT,event=$name,t=$tMs")
    }

    fun sample(p: PhoneSensorPacket, o: FallDetectionObservation, alertState: String, observed: Boolean) {
        sampleCount++
        val packetT = t(p.timestampNs)
        if (profileChanged(o)) {
            val cfg = o.config
            val profNum = extractProfileNumber(o.activeProfileDisplayName)
            val ann = (profileAnnouncements[o.activeProfileId] ?: 0L) + 1L
            profileAnnouncements[o.activeProfileId] = ann
            emit(
                "FALL01_PROFILE,t=$packetT," +
                    "profileId=${o.activeProfileId}," +
                    "profileNumber=$profNum," +
                    "displayName=\"${o.activeProfileDisplayName}\"," +
                    "announcements=$ann," +
                    "impact=${f3(cfg.impactAccelerationMs2)}," +
                    "stillTarget=${f3(cfg.stillnessTargetAccelerationMs2)}," +
                    "stillTol=${f3(cfg.stillnessToleranceMs2)}," +
                    "postWindowMs=${cfg.postImpactWindowMs}," +
                    "stillDurationMs=${cfg.postImpactStillnessDurationMs}," +
                    "minSamples=${cfg.minimumStillnessSamples}," +
                    "maxGapMs=${cfg.maximumSampleGapMs}," +
                    "pressureEvidence=${cfg.phonePressureEvidenceEnabled}," +
                    "minPressureRisePa=${f1(cfg.phonePressureMinimumRisePa)}"
            )
            lastProfileId = o.activeProfileId
            lastProfileConfig = o.config
        }

        val dtMs = if (lastSampleNs != null) (p.timestampNs - lastSampleNs!!) / 1_000_000L else 0L
        lastSampleNs = p.timestampNs

        val amag = norm3(p.accelXMs2, p.accelYMs2, p.accelZMs2)
        val lmag = norm3(p.linearAccelXMs2, p.linearAccelYMs2, p.linearAccelZMs2)
        val gmag = norm3(p.gyroXDps, p.gyroYDps, p.gyroZDps)

        val profNum = extractProfileNumber(o.activeProfileDisplayName)
        val pressureCorroboratedStr = o.pressureCorroborated?.toString() ?: "null"

        emit(
            "FALL01_SAMPLE,t=$packetT,tns=${p.timestampNs},dtMs=$dtMs,wallMs=${p.wallClockTimestampMs}," +
                "ax=${f3(p.accelXMs2)},ay=${f3(p.accelYMs2)},az=${f3(p.accelZMs2)},amag=${f3(amag)}," +
                "lx=${f3(p.linearAccelXMs2)},ly=${f3(p.linearAccelYMs2)},lz=${f3(p.linearAccelZMs2)},lmag=${f3(lmag)}," +
                "gx=${f3(p.gyroXDps)},gy=${f3(p.gyroYDps)},gz=${f3(p.gyroZDps)},gmag=${f3(gmag)}," +
                "gtns=${p.gyroTimestampNs ?: "null"}," +
                "pitch=${f3(p.pitchDeg)},roll=${f3(p.rollDeg)},yaw=${f3(p.yawDeg)}," +
                "pressurePa=${f1(p.pressurePa)}," +
                "alert=$alertState,observed=$observed,phase=${o.phase}," +
                "impactOver=${o.impactOverThreshold},withinStill=${o.withinStillness}," +
                "stillProgress=${f2(o.stillnessProgress)},stillSamples=${o.stillnessSampleCount}," +
                "profileNumber=$profNum,pressureCorroborated=$pressureCorroboratedStr"
        )
    }

    fun transition(from: DetectionPhase, o: FallDetectionObservation, reason: String, p: PhoneSensorPacket?) {
        if (from == o.phase) return
        val packetT = p?.let { t(it.timestampNs) } ?: (lastSampleNs?.let { t(it) } ?: 0L)
        val packetTns = p?.timestampNs?.toString() ?: "null"
        val amag = p?.let { norm3(it.accelXMs2, it.accelYMs2, it.accelZMs2) }
        val gmag = p?.let { norm3(it.gyroXDps, it.gyroYDps, it.gyroZDps) }
        val cfg = o.config
        val profNum = extractProfileNumber(o.activeProfileDisplayName)

        emit(
            "FALL01_STATE,t=$packetT,tns=$packetTns,from=$from,to=${o.phase},reason=$reason," +
                "amag=${f3(amag)},gmag=${f3(gmag)}," +
                "stillProgress=${f2(o.stillnessProgress)},stillSamples=${o.stillnessSampleCount}," +
                "profileNumber=$profNum," +
                "impact=${f3(cfg.impactAccelerationMs2)}," +
                "stillTarget=${f3(cfg.stillnessTargetAccelerationMs2)}," +
                "stillTol=${f3(cfg.stillnessToleranceMs2)}," +
                "postWindowMs=${cfg.postImpactWindowMs}," +
                "stillDurationMs=${cfg.postImpactStillnessDurationMs}," +
                "minSamples=${cfg.minimumStillnessSamples}," +
                "maxGapMs=${cfg.maximumSampleGapMs}"
        )
    }

    fun decision(p: PhoneSensorPacket?, o: FallDetectionObservation, detected: Boolean, reason: String) {
        if (detected) {
            confirmationCount++
        }
        val packetT = p?.let { t(it.timestampNs) } ?: (lastSampleNs?.let { t(it) } ?: 0L)
        val packetTns = p?.timestampNs?.toString() ?: "null"
        val amag = p?.let { norm3(it.accelXMs2, it.accelYMs2, it.accelZMs2) }
        val gmag = p?.let { norm3(it.gyroXDps, it.gyroYDps, it.gyroZDps) }
        val cfg = o.config
        val profNum = extractProfileNumber(o.activeProfileDisplayName)

        emit(
            "FALL01_DECISION,t=$packetT,tns=$packetTns,detected=$detected,reason=$reason," +
                "amag=${f3(amag)},gmag=${f3(gmag)},phase=${o.phase}," +
                "impactOver=${o.impactOverThreshold},withinStill=${o.withinStillness}," +
                "stillProgress=${f2(o.stillnessProgress)},stillSamples=${o.stillnessSampleCount}," +
                "profileNumber=$profNum,profileId=${o.activeProfileId}," +
                "impact=${f3(cfg.impactAccelerationMs2)}," +
                "stillTarget=${f3(cfg.stillnessTargetAccelerationMs2)}," +
                "stillTol=${f3(cfg.stillnessToleranceMs2)}," +
                "postWindowMs=${cfg.postImpactWindowMs}," +
                "stillDurationMs=${cfg.postImpactStillnessDurationMs}," +
                "minSamples=${cfg.minimumStillnessSamples}," +
                "maxGapMs=${cfg.maximumSampleGapMs}"
        )
    }

    fun alertState(from: core.State, to: core.State, reason: String, o: FallDetectionObservation?, p: PhoneSensorPacket?) {
        if (from == to) return
        val tStr = p?.let { t(it.timestampNs).toString() } ?: "null"
        val amagStr = if (p != null) f3(norm3(p.accelXMs2, p.accelYMs2, p.accelZMs2)) else "null"
        val profNum = o?.let { extractProfileNumber(it.activeProfileDisplayName).toString() } ?: "null"

        emit(
            "FALL01_ALERT_STATE,t=$tStr,from=$from,to=$to,reason=$reason,profileNumber=$profNum,amag=$amagStr"
        )
    }
}
