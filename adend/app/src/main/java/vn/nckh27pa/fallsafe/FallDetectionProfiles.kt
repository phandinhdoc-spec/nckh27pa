package vn.nckh27pa.fallsafe

import android.content.Context
import com.google.gson.Gson
import vn.nckh27pa.fallsafe.espconfig.DeviceConfirmation
import vn.nckh27pa.fallsafe.espconfig.Esp32Config
import vn.nckh27pa.fallsafe.espconfig.Esp32Profile
import vn.nckh27pa.fallsafe.espconfig.Esp32ProfileRepository
import vn.nckh27pa.fallsafe.espconfig.ProfileStorageStatus
import vn.nckh27pa.fallsafe.espconfig.ProfileTarget
import vn.nckh27pa.fallsafe.espconfig.TrialRecord
import java.util.UUID

/** Experimental detector parameters only; these defaults are not medically validated. */
data class FallDetectionConfig(
    val impactAccelerationMs2: Float,
    val stillnessTargetAccelerationMs2: Float,
    val stillnessToleranceMs2: Float,
    val postImpactWindowMs: Long,
    val postImpactStillnessDurationMs: Long,
    val minimumStillnessSamples: Int,
    val maximumSampleGapMs: Long,
    val phonePressureEvidenceEnabled: Boolean = false,
    val phonePressureMinimumRisePa: Float = DEFAULT_PHONE_PRESSURE_MINIMUM_RISE_PA
) {
    init { require(isValid()) { "Invalid fall detection configuration" } }

    fun isValid(): Boolean =
        impactAccelerationMs2.isFinite() && impactAccelerationMs2 in MIN_IMPACT_ACCELERATION_MS2..MAX_IMPACT_ACCELERATION_MS2 &&
            stillnessTargetAccelerationMs2.isFinite() && stillnessTargetAccelerationMs2 in MIN_STILLNESS_TARGET_ACCELERATION_MS2..MAX_STILLNESS_TARGET_ACCELERATION_MS2 &&
            stillnessToleranceMs2.isFinite() && stillnessToleranceMs2 in MIN_STILLNESS_TOLERANCE_MS2..MAX_STILLNESS_TOLERANCE_MS2 &&
            postImpactWindowMs in MIN_POST_IMPACT_WINDOW_MS..MAX_POST_IMPACT_WINDOW_MS &&
            postImpactStillnessDurationMs in MIN_POST_IMPACT_STILLNESS_DURATION_MS..MAX_POST_IMPACT_STILLNESS_DURATION_MS &&
            postImpactStillnessDurationMs <= postImpactWindowMs &&
            minimumStillnessSamples in MINIMUM_STILLNESS_SAMPLES_MIN..MINIMUM_STILLNESS_SAMPLES_MAX &&
            maximumSampleGapMs in MIN_MAXIMUM_SAMPLE_GAP_MS..MAX_MAXIMUM_SAMPLE_GAP_MS &&
            phonePressureMinimumRisePa.isFinite() && phonePressureMinimumRisePa in 1f..200f

    companion object {
        const val MIN_IMPACT_ACCELERATION_MS2 = 1f
        const val MAX_IMPACT_ACCELERATION_MS2 = 100f
        const val MIN_STILLNESS_TARGET_ACCELERATION_MS2 = 0f
        const val MAX_STILLNESS_TARGET_ACCELERATION_MS2 = 20f
        const val MIN_STILLNESS_TOLERANCE_MS2 = 0.1f
        const val MAX_STILLNESS_TOLERANCE_MS2 = 10f
        const val MIN_POST_IMPACT_WINDOW_MS = 500L
        const val MAX_POST_IMPACT_WINDOW_MS = 10_000L
        const val MIN_POST_IMPACT_STILLNESS_DURATION_MS = 100L
        const val MAX_POST_IMPACT_STILLNESS_DURATION_MS = 10_000L
        const val MINIMUM_STILLNESS_SAMPLES_MIN = 2
        const val MINIMUM_STILLNESS_SAMPLES_MAX = 100
        const val MIN_MAXIMUM_SAMPLE_GAP_MS = 10L
        const val MAX_MAXIMUM_SAMPLE_GAP_MS = 2_000L
        const val DEFAULT_PHONE_PRESSURE_MINIMUM_RISE_PA = 12f

        // Experimental bench profile: short hand-drop -> catch -> hold still.
        // Deliberately sensitive; tune upward after the BLE/Android SOS path is verified.
        val DEFAULT = FallDetectionConfig(14f, 9.81f, 3f, 4_000L, 500L, 4, 500L)
    }
}

data class FallDetectionProfile(
    val id: String,
    val displayName: String,
    val profileNumber: Int,
    val createdAt: Long,
    val updatedAt: Long,
    val isActive: Boolean,
    val config: FallDetectionConfig,
    val profileTarget: ProfileTarget = ProfileTarget.PHONE,
    val profileRevision: Long = 1L
)

interface FallDetectionProfileRepository {
    val defaultConfig: FallDetectionConfig
    fun listProfiles(): List<FallDetectionProfile>
    fun getProfile(id: String): FallDetectionProfile?
    fun activeProfile(): FallDetectionProfile
    fun createDefault(): FallDetectionProfile?
    fun save(id: String, config: FallDetectionConfig): FallDetectionProfile?
    fun saveAs(config: FallDetectionConfig): FallDetectionProfile?
    fun activate(id: String): Boolean
    fun delete(id: String): Boolean
    fun resetToDefault(id: String): FallDetectionProfile?
}

interface FallDetectionProfileStorage {
    fun read(): String?
    fun write(snapshotJson: String): Boolean
}

class InMemoryFallDetectionProfileStorage(initialSnapshotJson: String? = null) : FallDetectionProfileStorage {
    private var snapshotJson = initialSnapshotJson
    override fun read(): String? = snapshotJson
    override fun write(snapshotJson: String): Boolean { this.snapshotJson = snapshotJson; return true }
}

class SharedPreferencesFallDetectionProfileStorage(context: Context, userId: String = BuildConfig.FALLSAFE_USER_ID) : FallDetectionProfileStorage {
    private val preferences = context.getSharedPreferences("fallsafe_detection_profiles_v1_$userId", Context.MODE_PRIVATE)
    override fun read(): String? = preferences.getString(SNAPSHOT_KEY, null)
    override fun write(snapshotJson: String): Boolean = preferences.edit().putString(SNAPSHOT_KEY, snapshotJson).commit()
    private companion object { const val SNAPSHOT_KEY = "profiles_snapshot_json" }
}

/** One atomic schema-v2 snapshot keeps PHONE and ESP32 numbering/configuration independent. */
class GsonFallDetectionProfileRepository(
    private val storage: FallDetectionProfileStorage,
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val newId: () -> String = { UUID.randomUUID().toString() },
    private val gson: Gson = Gson()
) : FallDetectionProfileRepository, Esp32ProfileRepository {
    private data class Snapshot(
        val schemaVersion: Int,
        val lastAssignedProfileNumber: Int,
        val profiles: List<FallDetectionProfile>,
        val esp32LastAssignedProfileNumber: Int = 0,
        val esp32Profiles: List<Esp32Profile> = emptyList(),
        val confirmations: List<DeviceConfirmation> = emptyList(),
        val trials: List<TrialRecord> = emptyList()
    )
    private data class LegacyConfig(
        val impactAccelerationMs2: Float,
        val stillnessTargetAccelerationMs2: Float,
        val stillnessToleranceMs2: Float,
        val postImpactWindowMs: Long,
        val postImpactStillnessDurationMs: Long,
        val minimumStillnessSamples: Int,
        val maximumSampleGapMs: Long
    )
    private data class LegacyProfile(
        val id: String?, val displayName: String?, val profileNumber: Int,
        val createdAt: Long, val updatedAt: Long, val isActive: Boolean,
        val config: LegacyConfig?
    )
    private data class LegacySnapshot(
        val schemaVersion: Int,
        val lastAssignedProfileNumber: Int,
        val profiles: List<LegacyProfile>?
    )

    override val defaultConfig: FallDetectionConfig get() = FallDetectionConfig.DEFAULT
    var storageStatus: ProfileStorageStatus = ProfileStorageStatus.READY
        private set
    private var snapshot: Snapshot = loadOrRecover()

    @Synchronized override fun listProfiles() = snapshot.profiles.toList()
    @Synchronized override fun getProfile(id: String) = snapshot.profiles.firstOrNull { it.id == id }
    @Synchronized override fun activeProfile() = snapshot.profiles.single { it.isActive }
    @Synchronized fun phoneHighWaterMark() = snapshot.lastAssignedProfileNumber
    @Synchronized override fun listEsp32Profiles() = snapshot.esp32Profiles.toList()
    @Synchronized override fun listConfirmations() = snapshot.confirmations.toList()
    @Synchronized override fun listTrials() = snapshot.trials.toList()

    @Synchronized override fun createDefault() = createPhone(FallDetectionConfig.DEFAULT)

    @Synchronized override fun save(id: String, config: FallDetectionConfig): FallDetectionProfile? {
        require(config.isValid())
        val existing = snapshot.profiles.firstOrNull { it.id == id } ?: return null
        val updated = existing.copy(config = config, profileRevision = existing.profileRevision + 1,
            updatedAt = maxOf(safeNow(), existing.createdAt, existing.updatedAt))
        return if (commit(snapshot.copy(profiles = snapshot.profiles.map { if (it.id == id) updated else it }))) updated else null
    }

    @Synchronized override fun saveAs(config: FallDetectionConfig) = createPhone(config)

    @Synchronized override fun activate(id: String): Boolean {
        if (snapshot.profiles.none { it.id == id }) return false
        if (snapshot.profiles.single { it.isActive }.id == id) return true
        val time = safeNow()
        return commit(snapshot.copy(profiles = snapshot.profiles.map {
            val active = it.id == id
            if (it.isActive == active) it else it.copy(isActive = active, updatedAt = maxOf(time, it.createdAt, it.updatedAt))
        }))
    }

    @Synchronized override fun delete(id: String): Boolean {
        val target = snapshot.profiles.firstOrNull { it.id == id } ?: return false
        if (target.isActive || snapshot.profiles.size <= 1) return false
        return commit(snapshot.copy(profiles = snapshot.profiles.filterNot { it.id == id }))
    }

    @Synchronized override fun resetToDefault(id: String) = save(id, FallDetectionConfig.DEFAULT)

    @Synchronized override fun createEsp32Default() = createEsp32(Esp32Config.DEFAULT)

    @Synchronized override fun saveEsp32(id: String, config: Esp32Config): Esp32Profile? {
        require(config.isValid())
        val existing = snapshot.esp32Profiles.firstOrNull { it.id == id } ?: return null
        val updated = existing.copy(config = config, profileRevision = existing.profileRevision + 1,
            updatedAt = maxOf(safeNow(), existing.createdAt, existing.updatedAt))
        return if (commit(snapshot.copy(esp32Profiles = snapshot.esp32Profiles.map { if (it.id == id) updated else it }))) updated else null
    }

    @Synchronized override fun saveEsp32As(config: Esp32Config) = createEsp32(config)

    @Synchronized override fun deleteEsp32(id: String): Boolean {
        if (snapshot.esp32Profiles.none { it.id == id }) return false
        return commit(snapshot.copy(esp32Profiles = snapshot.esp32Profiles.filterNot { it.id == id }))
    }

    @Synchronized override fun recordConfirmation(confirmation: DeviceConfirmation): Boolean =
        commit(snapshot.copy(confirmations = snapshot.confirmations + confirmation))

    @Synchronized override fun recordTrial(trial: TrialRecord): Boolean =
        commit(snapshot.copy(trials = snapshot.trials + trial))

    private fun createPhone(config: FallDetectionConfig): FallDetectionProfile? {
        require(config.isValid())
        val number = maxOf(snapshot.lastAssignedProfileNumber, snapshot.profiles.maxOfOrNull { it.profileNumber } ?: 0) + 1
        val time = safeNow()
        val profile = FallDetectionProfile(allocateId(), "Bảng $number", number, time, time, false, config)
        return if (commit(snapshot.copy(lastAssignedProfileNumber = number, profiles = snapshot.profiles + profile))) profile else null
    }

    private fun createEsp32(config: Esp32Config): Esp32Profile? {
        require(config.isValid())
        val number = maxOf(snapshot.esp32LastAssignedProfileNumber, snapshot.esp32Profiles.maxOfOrNull { it.profileNumber } ?: 0) + 1
        val time = safeNow()
        val profile = Esp32Profile(allocateId(), "Bảng $number", number, time, time, 1, config)
        return if (commit(snapshot.copy(esp32LastAssignedProfileNumber = number, esp32Profiles = snapshot.esp32Profiles + profile))) profile else null
    }

    private fun loadOrRecover(): Snapshot {
        val raw = try { storage.read() } catch (_: Exception) { null }
        if (raw != null) {
            val v2 = try { gson.fromJson(raw, Snapshot::class.java) } catch (_: Exception) { null }
            if (v2 != null && validSnapshot(v2)) return v2
            val migrated = migrateV1(raw)
            if (migrated != null) {
                val written = try { storage.write(gson.toJson(migrated)) } catch (_: Exception) { false }
                storageStatus = if (written) ProfileStorageStatus.READY else ProfileStorageStatus.STORAGE_ERROR
                return migrated
            }
        }
        val recovered = defaultSnapshot()
        val written = try { storage.write(gson.toJson(recovered)) } catch (_: Exception) { false }
        if (!written) storageStatus = ProfileStorageStatus.STORAGE_ERROR
        return recovered
    }

    private fun migrateV1(raw: String): Snapshot? {
        val legacy = try { gson.fromJson(raw, LegacySnapshot::class.java) } catch (_: Exception) { return null }
        val values = legacy.profiles ?: return null
        if (legacy.schemaVersion != 1 || values.isEmpty() || values.count { it.isActive } != 1) return null
        val profiles = try {
            values.map { old ->
                val c = old.config ?: return null
                val config = FallDetectionConfig(c.impactAccelerationMs2, c.stillnessTargetAccelerationMs2,
                    c.stillnessToleranceMs2, c.postImpactWindowMs, c.postImpactStillnessDurationMs,
                    c.minimumStillnessSamples, c.maximumSampleGapMs)
                FallDetectionProfile(old.id ?: return null, old.displayName ?: return null, old.profileNumber,
                    old.createdAt, old.updatedAt, old.isActive, config)
            }
        } catch (_: IllegalArgumentException) { return null }
        val migrated = Snapshot(2, legacy.lastAssignedProfileNumber, profiles)
        return migrated.takeIf(::validSnapshot)
    }

    private fun defaultSnapshot(): Snapshot {
        val time = safeNow()
        return Snapshot(2, 1, listOf(FallDetectionProfile(allocateId(), "Bảng 1", 1, time, time, true, FallDetectionConfig.DEFAULT)))
    }

    private fun validSnapshot(value: Snapshot): Boolean {
        val profiles = value.profiles
        if (value.schemaVersion != 2 || profiles.isEmpty() || profiles.count { it.isActive } != 1) return false
        if (value.lastAssignedProfileNumber < profiles.maxOf { it.profileNumber } || value.esp32LastAssignedProfileNumber < (value.esp32Profiles.maxOfOrNull { it.profileNumber } ?: 0)) return false
        if (profiles.map { it.id }.toSet().size != profiles.size || profiles.map { it.profileNumber }.toSet().size != profiles.size) return false
        if (value.esp32Profiles.map { it.id }.toSet().size != value.esp32Profiles.size || value.esp32Profiles.map { it.profileNumber }.toSet().size != value.esp32Profiles.size) return false
        return profiles.all { it.id.isNotBlank() && it.profileTarget == ProfileTarget.PHONE && it.profileRevision > 0 && it.profileNumber > 0 &&
            it.displayName == "Bảng ${it.profileNumber}" && it.createdAt >= 0 && it.updatedAt >= it.createdAt && it.config.isValid() } &&
            value.esp32Profiles.all { it.id.isNotBlank() && it.profileRevision > 0 && it.profileNumber > 0 &&
                it.displayName == "Bảng ${it.profileNumber}" && it.createdAt >= 0 && it.updatedAt >= it.createdAt && it.config.isValid() }
    }

    private fun commit(next: Snapshot): Boolean {
        val written = try { storage.write(gson.toJson(next)) } catch (_: Exception) { false }
        storageStatus = if (written) ProfileStorageStatus.READY else ProfileStorageStatus.STORAGE_ERROR
        if (written) snapshot = next
        return written
    }

    private fun safeNow() = nowMs().coerceAtLeast(0L)
    private fun allocateId() = try { newId() } catch (_: NoSuchElementException) { UUID.randomUUID().toString() }
}
