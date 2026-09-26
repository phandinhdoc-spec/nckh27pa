package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test

class PhonePressureEvidenceTest {
    private val profile = FallDetectionProfile(
        "phone", "Bảng 1", 1, 0, 0, true,
        FallDetectionConfig.DEFAULT.copy(
            postImpactStillnessDurationMs = 200,
            minimumStillnessSamples = 3,
            phonePressureEvidenceEnabled = true,
            phonePressureMinimumRisePa = 12f
        )
    )

    @Test fun pressureOnlyNeverStartsMotionSequence() {
        val detector = DemoDetector({ profile }, pressureCapability = { true })
        repeat(10) { index ->
            assertFalse(detector.accept(sample(index * 100L, 9.81f, 30f)))
        }
        assertEquals(DetectionPhase.NORMAL, detector.observation().phase)
    }

    @Test fun qualifyingPressureOnlyAnnotatesCompletedMotion() {
        val detector = DemoDetector({ profile }, pressureCapability = { true })
        assertFalse(detector.accept(sample(0, 30f, 0f)))
        assertFalse(detector.accept(sample(100, 9.81f, 5f)))
        assertFalse(detector.accept(sample(200, 9.81f, 10f)))
        assertTrue(detector.accept(sample(300, 9.81f, 12f)))
        assertEquals(true, detector.observation().pressureCorroborated)
    }

    @Test fun absentOrFailedPressureCannotVetoSameSevenFieldDecision() {
        val unavailable = DemoDetector({ profile }, pressureCapability = { false })
        val availableButFailing = DemoDetector({ profile }, pressureCapability = { true })
        listOf(0L to 30f, 100L to 9.81f, 200L to 9.81f).forEach { (t, z) ->
            assertFalse(unavailable.accept(sample(t, z, null)))
            assertFalse(availableButFailing.accept(sample(t, z, 2f)))
        }
        assertTrue(unavailable.accept(sample(300, 9.81f, null)))
        assertTrue(availableButFailing.accept(sample(300, 9.81f, 2f)))
        assertNull(unavailable.observation().pressureCorroborated)
        assertEquals(false, availableButFailing.observation().pressureCorroborated)
    }

    @Test fun normalizerUsesPaAndReturnsNullWhenStale() {
        val n = PhoneNormalizer()
        n.update(SensorKind.PRESSURE, 0, floatArrayOf(1000f))
        n.update(SensorKind.PRESSURE, 100_000_000, floatArrayOf(1000.2f))
        n.update(SensorKind.ACCEL, 100_000_000, floatArrayOf(0f, 0f, 9.81f))
        val fresh = n.packet(100_000_000, 0)!!
        assertEquals(100020f, fresh.pressurePa!!, .01f)
        assertEquals(20f, fresh.pressureWindowDeltaPa!!, .01f)
        n.update(SensorKind.ACCEL, 700_000_001, floatArrayOf(0f, 0f, 9.81f))
        val stale = n.packet(700_000_001, 0)!!
        assertNull(stale.pressurePa)
        assertNull(stale.pressureWindowDeltaPa)
    }

    private fun sample(ms: Long, z: Float, pressureDelta: Float?) = PhoneSensorPacket(
        timestampNs = ms * 1_000_000,
        wallClockTimestampMs = 0,
        accelXMs2 = 0f,
        accelYMs2 = 0f,
        accelZMs2 = z,
        pressurePa = pressureDelta?.let { 100_000f + it },
        pressureWindowDeltaPa = pressureDelta
    )
}
