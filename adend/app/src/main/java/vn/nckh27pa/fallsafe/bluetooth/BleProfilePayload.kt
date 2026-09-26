package vn.nckh27pa.fallsafe.bluetooth

import com.google.gson.JsonParser
import vn.nckh27pa.fallsafe.FallDetectionConfig
import java.util.Locale

/**
 * Fall profile parameters corresponding to ESP32 FallProfile / applyProfileJson contract.
 */
data class BleFallProfile(
    val impactAccelerationMs2: Float = 25.0f,
    val stillnessTargetAccelerationMs2: Float = 9.81f,
    val stillnessToleranceMs2: Float = 1.0f,
    val postImpactWindowMs: Long = 3000L,
    val postImpactStillnessDurationMs: Long = 1000L,
    val minimumStillnessSamples: Int = 6,
    val maximumSampleGapMs: Long = 250L,
    val freeFallThresholdMs2: Float = 4.90f,
    val freeFallMinDurationMs: Long = 80L,
    val gyroTurnThresholdDps: Float = 120.0f,
    val pressureEvidenceMinRisePa: Float = 12.0f,
    val pressureWindowMs: Long = 5000L,
    val altitudeDropMinM: Float = -0.40f,
    val sampleWatchdogMs: Long = 100L
) {
    fun toFallDetectionConfig(): FallDetectionConfig {
        return FallDetectionConfig(
            impactAccelerationMs2 = impactAccelerationMs2,
            stillnessTargetAccelerationMs2 = stillnessTargetAccelerationMs2,
            stillnessToleranceMs2 = stillnessToleranceMs2,
            postImpactWindowMs = postImpactWindowMs,
            postImpactStillnessDurationMs = postImpactStillnessDurationMs,
            minimumStillnessSamples = minimumStillnessSamples,
            maximumSampleGapMs = maximumSampleGapMs,
            phonePressureEvidenceEnabled = false,
            phonePressureMinimumRisePa = pressureEvidenceMinRisePa
        )
    }

    companion object {
        val DEFAULT = BleFallProfile()

        fun fromFallDetectionConfig(
            config: FallDetectionConfig,
            freeFallThresholdMs2: Float = 4.90f,
            freeFallMinDurationMs: Long = 80L,
            gyroTurnThresholdDps: Float = 120.0f,
            pressureWindowMs: Long = 5000L,
            altitudeDropMinM: Float = -0.40f,
            sampleWatchdogMs: Long = 100L
        ): BleFallProfile {
            return BleFallProfile(
                impactAccelerationMs2 = config.impactAccelerationMs2,
                stillnessTargetAccelerationMs2 = config.stillnessTargetAccelerationMs2,
                stillnessToleranceMs2 = config.stillnessToleranceMs2,
                postImpactWindowMs = config.postImpactWindowMs,
                postImpactStillnessDurationMs = config.postImpactStillnessDurationMs,
                minimumStillnessSamples = config.minimumStillnessSamples,
                maximumSampleGapMs = config.maximumSampleGapMs,
                freeFallThresholdMs2 = freeFallThresholdMs2,
                freeFallMinDurationMs = freeFallMinDurationMs,
                gyroTurnThresholdDps = gyroTurnThresholdDps,
                pressureEvidenceMinRisePa = config.phonePressureMinimumRisePa,
                pressureWindowMs = pressureWindowMs,
                altitudeDropMinM = altitudeDropMinM,
                sampleWatchdogMs = sampleWatchdogMs
            )
        }
    }
}

/**
 * Serializer, parser, and framing helper for FallProfile JSON payloads (kind 4 / profile writing).
 */
object BleProfilePayload {
    fun buildJson(profile: BleFallProfile = BleFallProfile.DEFAULT): String {
        return buildString {
            append("{")
            append("\"impactAccelerationMs2\":").append(String.format(Locale.US, "%.3f", profile.impactAccelerationMs2)).append(",")
            append("\"stillnessTargetAccelerationMs2\":").append(String.format(Locale.US, "%.3f", profile.stillnessTargetAccelerationMs2)).append(",")
            append("\"stillnessToleranceMs2\":").append(String.format(Locale.US, "%.3f", profile.stillnessToleranceMs2)).append(",")
            append("\"postImpactWindowMs\":").append(profile.postImpactWindowMs).append(",")
            append("\"postImpactStillnessDurationMs\":").append(profile.postImpactStillnessDurationMs).append(",")
            append("\"minimumStillnessSamples\":").append(profile.minimumStillnessSamples).append(",")
            append("\"maximumSampleGapMs\":").append(profile.maximumSampleGapMs).append(",")
            append("\"freeFallThresholdMs2\":").append(String.format(Locale.US, "%.3f", profile.freeFallThresholdMs2)).append(",")
            append("\"freeFallMinDurationMs\":").append(profile.freeFallMinDurationMs).append(",")
            append("\"gyroTurnThresholdDps\":").append(String.format(Locale.US, "%.3f", profile.gyroTurnThresholdDps)).append(",")
            append("\"pressureEvidenceMinRisePa\":").append(String.format(Locale.US, "%.3f", profile.pressureEvidenceMinRisePa)).append(",")
            append("\"pressureWindowMs\":").append(profile.pressureWindowMs).append(",")
            append("\"altitudeDropMinM\":").append(String.format(Locale.US, "%.3f", profile.altitudeDropMinM)).append(",")
            append("\"sampleWatchdogMs\":").append(profile.sampleWatchdogMs)
            append("}")
        }
    }

    fun parseJson(json: String): BleFallProfile? {
        if (json.isBlank()) return null
        return try {
            val element = JsonParser.parseString(json)
            if (!element.isJsonObject) return null
            val obj = element.asJsonObject

            BleFallProfile(
                impactAccelerationMs2 = obj.get("impactAccelerationMs2")?.asFloat ?: return null,
                stillnessTargetAccelerationMs2 = obj.get("stillnessTargetAccelerationMs2")?.asFloat ?: return null,
                stillnessToleranceMs2 = obj.get("stillnessToleranceMs2")?.asFloat ?: return null,
                postImpactWindowMs = obj.get("postImpactWindowMs")?.asLong ?: return null,
                postImpactStillnessDurationMs = obj.get("postImpactStillnessDurationMs")?.asLong ?: return null,
                minimumStillnessSamples = obj.get("minimumStillnessSamples")?.asInt ?: return null,
                maximumSampleGapMs = obj.get("maximumSampleGapMs")?.asLong ?: return null,
                freeFallThresholdMs2 = obj.get("freeFallThresholdMs2")?.asFloat ?: return null,
                freeFallMinDurationMs = obj.get("freeFallMinDurationMs")?.asLong ?: return null,
                gyroTurnThresholdDps = obj.get("gyroTurnThresholdDps")?.asFloat ?: return null,
                pressureEvidenceMinRisePa = obj.get("pressureEvidenceMinRisePa")?.asFloat ?: return null,
                pressureWindowMs = obj.get("pressureWindowMs")?.asLong ?: return null,
                altitudeDropMinM = obj.get("altitudeDropMinM")?.asFloat ?: return null,
                sampleWatchdogMs = obj.get("sampleWatchdogMs")?.asLong ?: return null
            )
        } catch (_: Exception) {
            null
        }
    }

    /**
     * Encode profile payload as IF-003 frames (kind 4 / command) if framed transmission is requested.
     */
    fun buildFramed(mtu: Int, messageId: Long, profile: BleFallProfile = BleFallProfile.DEFAULT): List<ByteArray>? {
        val payload = buildJson(profile).toByteArray(Charsets.UTF_8)
        return Framing.encode(mtu, BleGattUuids.KIND_COMMAND, messageId, payload)
    }
}
