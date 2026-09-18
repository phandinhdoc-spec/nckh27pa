package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test

class FallDetectionProfilesTest {
    @Test fun defaultConfigPreservesCurrentExperimentalDetectorValues() {
        val c = FallDetectionConfig.DEFAULT
        assertEquals(25f, c.impactAccelerationMs2, 0f)
        assertEquals(9.81f, c.stillnessTargetAccelerationMs2, 0f)
        assertEquals(1f, c.stillnessToleranceMs2, 0f)
        assertEquals(3_000L, c.postImpactWindowMs)
        assertEquals(1_000L, c.postImpactStillnessDurationMs)
        assertEquals(6, c.minimumStillnessSamples)
        assertEquals(250L, c.maximumSampleGapMs)
    }

    @Test fun configAcceptsDocumentedBoundariesAndRejectsInvalidValues() {
        assertEquals(1f, FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2, 0f)
        assertEquals(100f, FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2, 0f)
        assertEquals(0f, FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2, 0f)
        assertEquals(20f, FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2, 0f)
        assertEquals(0.1f, FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2, 0f)
        assertEquals(10f, FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2, 0f)
        assertEquals(500L, FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS)
        assertEquals(10_000L, FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS)
        assertEquals(100L, FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS)
        assertEquals(10_000L, FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS)
        assertEquals(2, FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN)
        assertEquals(100, FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX)
        assertEquals(10L, FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS)
        assertEquals(2_000L, FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS)

        val min = FallDetectionConfig(
            impactAccelerationMs2 = FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2,
            stillnessTargetAccelerationMs2 = FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2,
            stillnessToleranceMs2 = FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2,
            postImpactWindowMs = FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS,
            postImpactStillnessDurationMs = FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS,
            minimumStillnessSamples = FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN,
            maximumSampleGapMs = FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS
        )
        assertTrue(min.isValid())
        val max = FallDetectionConfig(
            impactAccelerationMs2 = FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2,
            stillnessTargetAccelerationMs2 = FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2,
            stillnessToleranceMs2 = FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2,
            postImpactWindowMs = FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS,
            postImpactStillnessDurationMs = FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS,
            minimumStillnessSamples = FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX,
            maximumSampleGapMs = FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS
        )
        assertTrue(max.isValid())

        fun invalid(block: () -> Unit) = assertThrows(IllegalArgumentException::class.java, block)
        invalid { FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 0f) }
        invalid { FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 100.1f) }
        invalid { FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = Float.NaN) }
        invalid { FallDetectionConfig.DEFAULT.copy(stillnessTargetAccelerationMs2 = -0.1f) }
        invalid { FallDetectionConfig.DEFAULT.copy(stillnessTargetAccelerationMs2 = 20.1f) }
        invalid { FallDetectionConfig.DEFAULT.copy(stillnessTargetAccelerationMs2 = Float.POSITIVE_INFINITY) }
        invalid { FallDetectionConfig.DEFAULT.copy(stillnessToleranceMs2 = 0f) }
        invalid { FallDetectionConfig.DEFAULT.copy(stillnessToleranceMs2 = 10.1f) }
        invalid { FallDetectionConfig.DEFAULT.copy(postImpactWindowMs = 0L) }
        invalid { FallDetectionConfig.DEFAULT.copy(postImpactWindowMs = 10_001L) }
        invalid { FallDetectionConfig.DEFAULT.copy(postImpactStillnessDurationMs = 0L) }
        invalid { FallDetectionConfig.DEFAULT.copy(postImpactStillnessDurationMs = 10_001L, postImpactWindowMs = 10_001L) }
        invalid { FallDetectionConfig.DEFAULT.copy(postImpactStillnessDurationMs = 3_001L) }
        invalid { FallDetectionConfig.DEFAULT.copy(minimumStillnessSamples = 0) }
        invalid { FallDetectionConfig.DEFAULT.copy(minimumStillnessSamples = 101) }
        invalid { FallDetectionConfig.DEFAULT.copy(maximumSampleGapMs = 0L) }
        invalid { FallDetectionConfig.DEFAULT.copy(maximumSampleGapMs = 2_001L) }
    }

    @Test fun repositoryCreatesDefaultAndPersistsEditsAcrossReload() {
        val storage = MemoryProfileStorage()
        val repo = GsonFallDetectionProfileRepository(storage, nowMs = { 100L }, newId = { "id-1" })
        val initial = repo.listProfiles()
        assertEquals(1, initial.size)
        assertEquals("Bảng 1", initial.single().displayName)
        assertEquals(1, initial.single().profileNumber)
        assertTrue(initial.single().isActive)

        val custom = FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 35f)
        assertNotNull(repo.save(initial.single().id, custom))
        val reloaded = GsonFallDetectionProfileRepository(storage, nowMs = { 200L }, newId = { "unused" })
        assertEquals(custom, reloaded.activeProfile().config)
        assertEquals(initial.single().id, reloaded.activeProfile().id)
    }

    @Test fun saveSurvivesRepositoryRecreationWhenClockMovesBackward() {
        val storage = MemoryProfileStorage()
        var clock = 1_000L
        var repo = GsonFallDetectionProfileRepository(storage, nowMs = { clock }, newId = { "id-1" })
        val original = repo.activeProfile()
        val custom = original.config.copy(impactAccelerationMs2 = 35f)

        clock = 500L
        val saved = repo.save(original.id, custom)!!
        assertEquals(original.createdAt, saved.updatedAt)

        repo = GsonFallDetectionProfileRepository(storage, nowMs = { clock }, newId = { "recovered" })
        assertEquals(original.id, repo.activeProfile().id)
        assertEquals(custom, repo.activeProfile().config)
        assertEquals(original.createdAt, repo.activeProfile().updatedAt)
    }

    @Test fun saveOnlyUpdatesExistingAndFailedWriteDoesNotMutateMemory() {
        val storage = MemoryProfileStorage()
        val repo = GsonFallDetectionProfileRepository(storage, nowMs = { 10L }, newId = { "id-1" })
        assertNull(repo.save("missing", FallDetectionConfig.DEFAULT))
        val before = repo.activeProfile()
        storage.acceptWrites = false
        assertNull(repo.save(before.id, before.config.copy(impactAccelerationMs2 = 40f)))
        assertEquals(before, repo.activeProfile())
    }

    @Test fun saveAsEditingTable2CreatesTable4WithoutChangingTable2() {
        val ids = ArrayDeque(listOf("id-1", "id-2", "id-3", "id-4"))
        val repo = GsonFallDetectionProfileRepository(MemoryProfileStorage(), nowMs = { 10L }, newId = { ids.removeFirst() })
        repo.createDefault()
        val table2 = repo.listProfiles().single { it.profileNumber == 2 }
        repo.createDefault()
        val draft = table2.config.copy(impactAccelerationMs2 = 42f)
        val table4 = repo.saveAs(draft)!!
        assertEquals(4, table4.profileNumber)
        assertEquals("Bảng 4", table4.displayName)
        assertEquals(draft, table4.config)
        assertEquals(FallDetectionConfig.DEFAULT, repo.getProfile(table2.id)!!.config)
    }

    @Test fun deletedProfileNumbersAreNeverReused() {
        val ids = ArrayDeque(listOf("id-1", "id-2", "id-3", "id-4", "id-5"))
        val storage = MemoryProfileStorage()
        var repo = GsonFallDetectionProfileRepository(storage, nowMs = { 10L }, newId = { ids.removeFirst() })
        repeat(3) { repo.createDefault() }
        val table2 = repo.listProfiles().single { it.profileNumber == 2 }
        assertTrue(repo.delete(table2.id))
        repo = GsonFallDetectionProfileRepository(storage, nowMs = { 20L }, newId = { ids.removeFirst() })
        assertEquals(5, repo.createDefault()!!.profileNumber)
    }

    @Test fun activateMaintainsExactlyOneActiveAndSurvivesRestart() {
        val ids = ArrayDeque(listOf("id-1", "id-2"))
        val storage = MemoryProfileStorage()
        var repo = GsonFallDetectionProfileRepository(storage, nowMs = { 10L }, newId = { ids.removeFirst() })
        val second = repo.createDefault()!!
        assertTrue(repo.activate(second.id))
        assertEquals(1, repo.listProfiles().count { it.isActive })
        assertEquals(second.id, repo.activeProfile().id)
        repo = GsonFallDetectionProfileRepository(storage, nowMs = { 20L }, newId = { "unused" })
        assertEquals(second.id, repo.activeProfile().id)
        assertEquals(1, repo.listProfiles().count { it.isActive })
    }

    @Test fun activateSurvivesRepositoryRecreationWhenClockMovesBackward() {
        val ids = ArrayDeque(listOf("id-1", "id-2"))
        val storage = MemoryProfileStorage()
        var clock = 1_000L
        var repo = GsonFallDetectionProfileRepository(storage, nowMs = { clock }, newId = { ids.removeFirst() })
        val first = repo.activeProfile()
        clock = 2_000L
        val second = repo.createDefault()!!
        clock = 3_000L
        repo.save(first.id, first.config)

        clock = 500L
        assertTrue(repo.activate(second.id))
        assertEquals(3_000L, repo.getProfile(first.id)!!.updatedAt)
        assertEquals(second.createdAt, repo.getProfile(second.id)!!.updatedAt)

        repo = GsonFallDetectionProfileRepository(storage, nowMs = { clock }, newId = { "recovered" })
        assertEquals(second.id, repo.activeProfile().id)
        assertEquals(1, repo.listProfiles().count { it.isActive })
        assertEquals(3_000L, repo.getProfile(first.id)!!.updatedAt)
        assertEquals(second.createdAt, repo.getProfile(second.id)!!.updatedAt)
    }

    @Test fun deleteRejectsActiveAndLastProfile() {
        val ids = ArrayDeque(listOf("id-1", "id-2"))
        val repo = GsonFallDetectionProfileRepository(MemoryProfileStorage(), newId = { ids.removeFirst() })
        val first = repo.activeProfile()
        assertFalse(repo.delete(first.id))
        val second = repo.createDefault()!!
        assertFalse(repo.delete(first.id))
        assertTrue(repo.activate(second.id))
        assertTrue(repo.delete(first.id))
        assertFalse(repo.delete(second.id))
    }

    @Test fun resetRestoresExperimentalDefaults() {
        val repo = GsonFallDetectionProfileRepository(MemoryProfileStorage(), nowMs = { 10L }, newId = { "id-1" })
        val id = repo.activeProfile().id
        repo.save(id, FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 44f))
        assertEquals(FallDetectionConfig.DEFAULT, repo.resetToDefault(id)!!.config)
    }

    @Test fun corruptInvalidEmptyAndMultipleActiveSnapshotsRecoverSafely() {
        val corrupt = listOf(
            "not json",
            "{\"lastAssignedProfileNumber\":0,\"profiles\":[]}",
            "{\"lastAssignedProfileNumber\":1,\"profiles\":null}",
            "{\"lastAssignedProfileNumber\":1,\"profiles\":[{\"id\":\"a\",\"displayName\":\"Bảng 1\",\"profileNumber\":1,\"createdAt\":1,\"updatedAt\":1,\"isActive\":true}]}",
            "{\"lastAssignedProfileNumber\":1,\"profiles\":[" + profileJson("a", 1, true, 25f) + "]}",
            "{\"lastAssignedProfileNumber\":2,\"profiles\":[" +
                profileJson("a", 1, true, 25f) + "," + profileJson("b", 2, true, 25f) + "]}",
            "{\"lastAssignedProfileNumber\":1,\"profiles\":[" + profileJson("a", 1, true, "NaN") + "]}"
        )
        corrupt.forEachIndexed { index, json ->
            val storage = MemoryProfileStorage(json)
            val repo = GsonFallDetectionProfileRepository(storage, nowMs = { 50L }, newId = { "recovered-$index" })
            assertEquals(1, repo.listProfiles().size)
            assertTrue(repo.activeProfile().isActive)
            assertEquals(FallDetectionConfig.DEFAULT, repo.activeProfile().config)
            assertNotEquals(json, storage.value)
        }
    }

    @Test fun snapshotWithNonSystemDisplayNameRecoversSafely() {
        val json = "{\"schemaVersion\":1,\"lastAssignedProfileNumber\":1,\"profiles\":[" +
            profileJson("a", 1, true, 25f, displayName = "Bảng nghiên cứu") + "]}"
        val storage = MemoryProfileStorage(json)

        val repo = GsonFallDetectionProfileRepository(storage, nowMs = { 50L }, newId = { "recovered" })

        assertEquals("recovered", repo.activeProfile().id)
        assertEquals("Bảng 1", repo.activeProfile().displayName)
        assertNotEquals(json, storage.value)
    }

    private fun profileJson(
        id: String,
        number: Int,
        active: Boolean,
        impact: Any,
        displayName: String = "Bảng $number"
    ) =
        """{"id":"$id","displayName":"$displayName","profileNumber":$number,"createdAt":1,"updatedAt":1,"isActive":$active,"config":{"impactAccelerationMs2":$impact,"stillnessTargetAccelerationMs2":9.81,"stillnessToleranceMs2":1.0,"postImpactWindowMs":3000,"postImpactStillnessDurationMs":1000,"minimumStillnessSamples":6,"maximumSampleGapMs":250}}"""
}

private class MemoryProfileStorage(initial: String? = null) : FallDetectionProfileStorage {
    var value: String? = initial
    var acceptWrites = true
    override fun read(): String? = value
    override fun write(snapshotJson: String): Boolean {
        if (!acceptWrites) return false
        value = snapshotJson
        return true
    }
}
