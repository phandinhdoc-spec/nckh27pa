package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.espconfig.*

class EspConfigBackendTest {
    @Test fun v1MigrationPreservesEveryPhoneFieldAndCreatesNoEspProfile() {
        val legacy = """{"schemaVersion":1,"lastAssignedProfileNumber":7,"profiles":[{"id":"phone-a","displayName":"Bảng 3","profileNumber":3,"createdAt":11,"updatedAt":12,"isActive":false,"config":{"impactAccelerationMs2":31.0,"stillnessTargetAccelerationMs2":9.7,"stillnessToleranceMs2":1.2,"postImpactWindowMs":3200,"postImpactStillnessDurationMs":1100,"minimumStillnessSamples":7,"maximumSampleGapMs":240}},{"id":"phone-b","displayName":"Bảng 7","profileNumber":7,"createdAt":21,"updatedAt":22,"isActive":true,"config":{"impactAccelerationMs2":32.0,"stillnessTargetAccelerationMs2":9.8,"stillnessToleranceMs2":1.3,"postImpactWindowMs":3300,"postImpactStillnessDurationMs":1200,"minimumStillnessSamples":8,"maximumSampleGapMs":230}}]}"""
        val storage = MutableProfileStorage(legacy)
        val repo = GsonFallDetectionProfileRepository(storage, newId = { "unused" })

        assertEquals(listOf("phone-a", "phone-b"), repo.listProfiles().map { it.id })
        assertEquals("phone-b", repo.activeProfile().id)
        assertEquals(7, repo.phoneHighWaterMark())
        assertTrue(repo.listProfiles().all { it.profileTarget == ProfileTarget.PHONE && it.profileRevision == 1L })
        assertTrue(repo.listProfiles().all { !it.config.phonePressureEvidenceEnabled })
        assertEquals(12f, repo.listProfiles().first().config.phonePressureMinimumRisePa, 0f)
        assertTrue(repo.listEsp32Profiles().isEmpty())
        assertTrue(storage.value!!.contains("\"schemaVersion\":2"))
        assertTrue(storage.value!!.contains("\"esp32Profiles\":[]"))
    }

    @Test fun failedV1MigrationKeepsLegacyReadableAndReportsStorageFailure() {
        val legacy = """{"schemaVersion":1,"lastAssignedProfileNumber":1,"profiles":[{"id":"phone-a","displayName":"Bảng 1","profileNumber":1,"createdAt":1,"updatedAt":1,"isActive":true,"config":{"impactAccelerationMs2":25.0,"stillnessTargetAccelerationMs2":9.81,"stillnessToleranceMs2":1.0,"postImpactWindowMs":3000,"postImpactStillnessDurationMs":1000,"minimumStillnessSamples":6,"maximumSampleGapMs":250}}]}"""
        val storage = MutableProfileStorage(legacy, acceptWrites = false)
        val repo = GsonFallDetectionProfileRepository(storage)
        assertEquals("phone-a", repo.activeProfile().id)
        assertEquals(ProfileStorageStatus.STORAGE_ERROR, repo.storageStatus)
        assertEquals(legacy, storage.value)
    }

    @Test fun espProfilesUseIndependentNumbersAndRevisionRules() {
        val ids = ArrayDeque(listOf(
            "4ce38b32-baa5-4f3f-99be-8a920bd2d17d",
            "11111111-1111-4111-8111-111111111111",
            "22222222-2222-4222-8222-222222222222"
        ))
        val storage = MutableProfileStorage()
        var repo = GsonFallDetectionProfileRepository(storage, nowMs = { 100 }, newId = { ids.removeFirst() })
        val first = repo.createEsp32Default()!!
        assertEquals(1, first.profileNumber)
        assertEquals(1L, first.profileRevision)
        val saved = repo.saveEsp32(first.id, first.config)!!
        assertEquals(2L, saved.profileRevision)
        val second = repo.saveEsp32As(saved.config)!!
        assertEquals(2, second.profileNumber)
        assertEquals(1L, second.profileRevision)
        assertTrue(repo.deleteEsp32(first.id))
        repo = GsonFallDetectionProfileRepository(storage, newId = { ids.removeFirst() })
        assertEquals(3, repo.createEsp32Default()!!.profileNumber)
        assertEquals(1, repo.activeProfile().profileNumber)
    }

    @Test fun confirmationIsImmutableWhenProfileChangesOrIsDeleted() {
        val id = "4ce38b32-baa5-4f3f-99be-8a920bd2d17d"
        val repo = GsonFallDetectionProfileRepository(MutableProfileStorage(), nowMs = { 500 }, newId = { id })
        val profile = repo.createEsp32Default()!!
        val confirmation = DeviceConfirmation("FALLSAFE-01A2", profile.id, profile.profileRevision, 8,
            EspCanonical.hash(profile), profile.config, 500)
        assertTrue(repo.recordConfirmation(confirmation))
        repo.saveEsp32(profile.id, profile.config.copy(impactAccelerationMs2 = 30.0))
        assertTrue(repo.deleteEsp32(profile.id))
        assertEquals(confirmation, repo.listConfirmations().single())
    }

    @Test fun canonicalHashMatchesFrozenVector() {
        val profile = Esp32Profile("4ce38b32-baa5-4f3f-99be-8a920bd2d17d", "Bảng 1", 1, 1, 1, 3, Esp32Config.DEFAULT)
        assertEquals("88dcc136a42afb992df14823614f9f9f4459e4093ade82be7e5624dd5a4b11fe", EspCanonical.hash(profile))
    }
}

private class MutableProfileStorage(initial: String? = null, var acceptWrites: Boolean = true) : FallDetectionProfileStorage {
    var value: String? = initial
    override fun read(): String? = value
    override fun write(snapshotJson: String): Boolean {
        if (!acceptWrites) return false
        value = snapshotJson
        return true
    }
}
