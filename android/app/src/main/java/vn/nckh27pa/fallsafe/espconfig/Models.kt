package vn.nckh27pa.fallsafe.espconfig

import java.math.BigDecimal
import java.security.MessageDigest
import java.util.Locale
import java.util.UUID

enum class ProfileTarget { PHONE, ESP32 }
enum class ProfileStorageStatus { READY, STORAGE_ERROR }

data class Esp32Config(
    val impactAccelerationMs2: Double = 25.000,
    val stillnessTargetAccelerationMs2: Double = 9.810,
    val stillnessToleranceMs2: Double = 1.000,
    val postImpactWindowMs: Long = 3000,
    val postImpactStillnessDurationMs: Long = 1000,
    val minimumStillnessSamples: Int = 6,
    val maximumSampleGapMs: Long = 250,
    val pressureEvidenceEnabled: Boolean = false,
    val pressureMinimumRisePa: Double = 12.00,
    val pressureWindowMs: Long = 3000,
    val pressureMinimumSamples: Int = 5,
    val pressureFilterAlpha: Double = 0.200,
    val pressureStaleAfterMs: Long = 1000
) {
    fun validationError(): String? = when {
        !validDecimal(impactAccelerationMs2, 1.0, 100.0, 3) -> "config.impactAccelerationMs2"
        !validDecimal(stillnessTargetAccelerationMs2, 0.0, 20.0, 3) -> "config.stillnessTargetAccelerationMs2"
        !validDecimal(stillnessToleranceMs2, 0.1, 10.0, 3) -> "config.stillnessToleranceMs2"
        postImpactWindowMs !in 500L..10_000L -> "config.postImpactWindowMs"
        postImpactStillnessDurationMs !in 100L..10_000L -> "config.postImpactStillnessDurationMs"
        postImpactStillnessDurationMs > postImpactWindowMs -> "config.postImpactStillnessDurationMs"
        minimumStillnessSamples !in 2..100 -> "config.minimumStillnessSamples"
        maximumSampleGapMs !in 10L..2_000L -> "config.maximumSampleGapMs"
        !validDecimal(pressureMinimumRisePa, 1.0, 200.0, 2) -> "config.pressureMinimumRisePa"
        pressureWindowMs !in 500L..10_000L -> "config.pressureWindowMs"
        pressureMinimumSamples !in 2..100 -> "config.pressureMinimumSamples"
        !validDecimal(pressureFilterAlpha, .01, 1.0, 3) -> "config.pressureFilterAlpha"
        pressureStaleAfterMs !in 100L..5_000L || pressureStaleAfterMs > pressureWindowMs -> "config.pressureStaleAfterMs"
        else -> null
    }
    fun isValid() = validationError() == null
    companion object {
        val DEFAULT = Esp32Config()
        private fun validDecimal(value: Double, min: Double, max: Double, places: Int): Boolean {
            if (!value.isFinite() || value !in min..max) return false
            return try { BigDecimal.valueOf(value).stripTrailingZeros().scale().coerceAtLeast(0) <= places } catch (_: Exception) { false }
        }
    }
}

data class Esp32Profile(
    val id: String,
    val displayName: String,
    val profileNumber: Int,
    val createdAt: Long,
    val updatedAt: Long,
    val profileRevision: Long,
    val config: Esp32Config
)

data class DeviceConfirmation(
    val deviceId: String,
    val profileId: String,
    val profileRevision: Long,
    val configRevision: Long,
    val configHash: String,
    val config: Esp32Config,
    val confirmedAtMs: Long
)

data class TrialRecord(
    val source: ProfileTarget,
    val deviceId: String?,
    val profileId: String,
    val profileRevision: Long,
    val appliedConfigRevision: Long?,
    val appliedConfigHash: String?,
    val timestampMs: Long
)

object EspCanonical {
    fun document(profile: Esp32Profile): String = document(profile.id, profile.profileRevision, profile.config)
    fun document(profileId: String, profileRevision: Long, c: Esp32Config): String = buildString {
        append("{\"schemaVersion\":1,\"profileTarget\":\"ESP32\",\"profileId\":\"")
        append(profileId)
        append("\",\"profileRevision\":").append(profileRevision).append(",\"config\":{")
        append("\"impactAccelerationMs2\":").append(decimal(c.impactAccelerationMs2, 3))
        append(",\"stillnessTargetAccelerationMs2\":").append(decimal(c.stillnessTargetAccelerationMs2, 3))
        append(",\"stillnessToleranceMs2\":").append(decimal(c.stillnessToleranceMs2, 3))
        append(",\"postImpactWindowMs\":").append(c.postImpactWindowMs)
        append(",\"postImpactStillnessDurationMs\":").append(c.postImpactStillnessDurationMs)
        append(",\"minimumStillnessSamples\":").append(c.minimumStillnessSamples)
        append(",\"maximumSampleGapMs\":").append(c.maximumSampleGapMs)
        append(",\"pressureEvidenceEnabled\":").append(c.pressureEvidenceEnabled)
        append(",\"pressureMinimumRisePa\":").append(decimal(c.pressureMinimumRisePa, 2))
        append(",\"pressureWindowMs\":").append(c.pressureWindowMs)
        append(",\"pressureMinimumSamples\":").append(c.pressureMinimumSamples)
        append(",\"pressureFilterAlpha\":").append(decimal(c.pressureFilterAlpha, 3))
        append(",\"pressureStaleAfterMs\":").append(c.pressureStaleAfterMs).append("}}")
    }
    fun hash(profile: Esp32Profile): String = hash(profile.id, profile.profileRevision, profile.config)
    fun hash(profileId: String, profileRevision: Long, config: Esp32Config): String =
        MessageDigest.getInstance("SHA-256").digest(document(profileId, profileRevision, config).toByteArray(Charsets.UTF_8))
            .joinToString("") { "%02x".format(Locale.US, it.toInt() and 0xff) }
    fun validUuid(value: String): Boolean = try {
        UUID.fromString(value).toString().equals(value, ignoreCase = true)
    } catch (_: IllegalArgumentException) { false }
    private fun decimal(value: Double, places: Int) = String.format(Locale.US, "%.${places}f", value)
}

interface Esp32ProfileRepository {
    fun listEsp32Profiles(): List<Esp32Profile>
    fun createEsp32Default(): Esp32Profile?
    fun saveEsp32(id: String, config: Esp32Config): Esp32Profile?
    fun saveEsp32As(config: Esp32Config): Esp32Profile?
    fun deleteEsp32(id: String): Boolean
    fun listConfirmations(): List<DeviceConfirmation>
    fun recordConfirmation(confirmation: DeviceConfirmation): Boolean
    fun listTrials(): List<TrialRecord>
    fun recordTrial(trial: TrialRecord): Boolean
}
