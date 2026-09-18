package vn.nckh27pa.fallsafe

import core.MonotonicClock
import org.junit.Assert.*
import org.junit.Test

class DetectionProfileControllerTest {
    @Test fun controllerExposesSelectedVsActiveProfileOperationsWithoutImplicitActivation() {
        val ids = ArrayDeque(listOf("id-1", "id-2", "id-3"))
        val repository = GsonFallDetectionProfileRepository(ControllerMemoryStorage(), newId = { ids.removeFirst() })
        val controller = DemoController(
            contactRepository = InMemoryContactRepository(),
            clock = MonotonicClock { 0L },
            profileRepository = repository
        )
        val first = controller.activeProfile
        val second = controller.createDefaultProfile()!!
        val edited = second.config.copy(impactAccelerationMs2 = 37f)

        assertNotNull(controller.save(second.id, edited))
        assertEquals(first.id, controller.activeProfile.id)
        assertEquals(edited, controller.profiles.single { it.id == second.id }.config)
        val third = controller.saveAs(edited)!!
        assertEquals(3, third.profileNumber)
        assertEquals(first.id, controller.activeProfile.id)

        assertTrue(controller.activate(second.id))
        assertEquals(second.id, controller.activeProfile.id)
        assertEquals(second.id, controller.observation.activeProfileId)
        assertFalse(controller.delete(second.id))
        assertTrue(controller.delete(first.id))
        assertEquals(FallDetectionConfig.DEFAULT, controller.resetToDefault(third.id)!!.config)
        assertEquals(FallDetectionConfig.DEFAULT, controller.defaultConfig)
    }

    @Test fun savingActiveProfileAppliesImmediatelyAndResetsEvidence() {
        val repository = GsonFallDetectionProfileRepository(ControllerMemoryStorage(), newId = { "id-1" })
        val controller = DemoController(
            contactRepository = InMemoryContactRepository(),
            clock = MonotonicClock { 0L },
            profileRepository = repository
        )
        controller.acceptPhone(sample(0, 30f))
        assertEquals(DetectionPhase.IMPACT_DETECTED, controller.observation.phase)
        val changed = controller.defaultConfig.copy(impactAccelerationMs2 = 40f)
        assertNotNull(controller.save(controller.activeProfile.id, changed))
        assertEquals(DetectionPhase.NORMAL, controller.observation.phase)
        assertEquals(changed, controller.observation.config)
    }

    private fun sample(ms: Long, z: Float) = PhoneSensorPacket(
        timestampNs = ms * 1_000_000, wallClockTimestampMs = 0,
        accelXMs2 = 0f, accelYMs2 = 0f, accelZMs2 = z
    )
}

private class ControllerMemoryStorage : FallDetectionProfileStorage {
    private var value: String? = null
    override fun read(): String? = value
    override fun write(snapshotJson: String): Boolean { value = snapshotJson; return true }
}
