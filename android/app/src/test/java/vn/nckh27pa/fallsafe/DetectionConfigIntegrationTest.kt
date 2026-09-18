package vn.nckh27pa.fallsafe

import core.MonotonicClock
import core.State
import org.junit.Assert.*
import org.junit.Test

class DetectionConfigIntegrationTest {
    @Test fun detectorUsesCustomConfigAndReportsResearchObservation() {
        val custom = FallDetectionConfig.DEFAULT.copy(
            impactAccelerationMs2 = 35f,
            postImpactStillnessDurationMs = 200L,
            minimumStillnessSamples = 3
        )
        val profile = FallDetectionProfile("custom", "Bảng 9", 9, 1, 1, true, custom)
        val detector = DemoDetector { profile }

        assertFalse(detector.accept(sample(0, 30f)))
        assertEquals(DetectionPhase.NORMAL, detector.observation().phase)
        assertFalse(detector.observation().impactOverThreshold)
        assertFalse(detector.accept(sample(100, 40f)))
        val impact = detector.observation()
        assertEquals(DetectionPhase.IMPACT_DETECTED, impact.phase)
        assertEquals("custom", impact.activeProfileId)
        assertEquals("Bảng 9", impact.activeProfileDisplayName)
        assertEquals(custom, impact.config)
        assertTrue(impact.impactOverThreshold)
        assertEquals(40.0, impact.accelerationMagnitudeMs2!!, 0.001)

        assertFalse(detector.accept(sample(200, 9.81f)))
        assertEquals(DetectionPhase.POST_IMPACT_STILLNESS, detector.observation().phase)
        assertTrue(detector.observation().withinStillness)
        assertEquals(1, detector.observation().stillnessSampleCount)
        assertEquals(0f, detector.observation().stillnessProgress, 0f)
        assertFalse(detector.accept(sample(300, 9.81f)))
        assertTrue(detector.accept(sample(400, 9.81f)))
        assertEquals(DetectionPhase.FALL_CONFIRMED, detector.observation().phase)
        assertEquals(3, detector.observation().stillnessSampleCount)
        assertEquals(1f, detector.observation().stillnessProgress, 0f)
    }

    @Test fun activatingProfileChangesBehaviorImmediatelyAndClearsPartialEvidence() {
        val ids = ArrayDeque(listOf("id-1", "id-2"))
        val repo = GsonFallDetectionProfileRepository(MemoryDetectionStorage(), newId = { ids.removeFirst() })
        val stricter = repo.saveAs(FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 35f))!!
        val detector = DemoDetector(repo::activeProfile)

        assertFalse(detector.accept(sample(0, 30f)))
        assertEquals(DetectionPhase.IMPACT_DETECTED, detector.observation().phase)
        assertTrue(repo.activate(stricter.id))
        assertFalse(detector.accept(sample(100, 9.81f)))
        assertEquals(DetectionPhase.NORMAL, detector.observation().phase)
        assertEquals(stricter.id, detector.observation().activeProfileId)
        assertEquals(0, detector.observation().stillnessSampleCount)

        assertFalse(detector.accept(sample(200, 30f)))
        assertEquals(DetectionPhase.NORMAL, detector.observation().phase)
        assertFalse(detector.accept(sample(300, 40f)))
        for (t in 400L..1_300L step 100) assertFalse(detector.accept(sample(t, 9.81f)))
        assertTrue(detector.accept(sample(1_400, 9.81f)))
    }

    @Test fun sessionExposesObservationAndUsesInjectedActiveProfile() {
        val ids = ArrayDeque(listOf("id-1", "id-2"))
        val repo = GsonFallDetectionProfileRepository(MemoryDetectionStorage(), newId = { ids.removeFirst() })
        val disabledForReplay = repo.saveAs(FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 50f))!!
        assertTrue(repo.activate(disabledForReplay.id))
        val session = DemoSession(MonotonicClock { 0L }, profileRepository = repo)
        DemoReplay(0).due(1_600).forEach(session::accept)
        assertEquals(State.MONITORING, session.snapshot().state)
        assertEquals(disabledForReplay.id, session.observation().activeProfileId)
    }

    private fun sample(ms: Long, z: Float) = PhoneSensorPacket(
        timestampNs = ms * 1_000_000, wallClockTimestampMs = 0,
        accelXMs2 = 0f, accelYMs2 = 0f, accelZMs2 = z
    )
}

private class MemoryDetectionStorage : FallDetectionProfileStorage {
    private var value: String? = null
    override fun read(): String? = value
    override fun write(snapshotJson: String): Boolean { value = snapshotJson; return true }
}
