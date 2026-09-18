package vn.nckh27pa.fallsafe.permissions

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CapabilityAccessTest {
    @Test fun allGrantedAndEachMissingCapabilityAreReportedIndependently() {
        val platform = FakePlatform(granted = Capability.entries.toMutableSet())
        val access = CapabilityAccessController(platform, MemoryCapabilityAttemptStore())

        assertTrue(access.checkSosReadiness(eligibleContactCount = 2).allCapabilitiesGranted)
        Capability.entries.forEach { missing ->
            platform.granted.remove(missing)
            val readiness = access.checkSosReadiness(eligibleContactCount = 2)
            assertEquals(CapabilityDisplayState.CAN_REQUEST, access.display(missing).state)
            assertEquals(setOf(missing), readiness.missingCapabilities)
            platform.granted.add(missing)
        }
    }

    @Test fun deniedCanBeRequestedAgainButPermanentDenialRequiresSeparateSettingsAction() {
        val platform = FakePlatform()
        val attempts = MemoryCapabilityAttemptStore()
        val access = CapabilityAccessController(platform, attempts)

        assertEquals(CapabilityDisplayState.CAN_REQUEST, access.display(Capability.MESSAGING).state)
        assertEquals(PermissionRequestResult.EXPLANATION_REQUIRED, access.request(Capability.MESSAGING, explanationAcknowledged = false))
        assertTrue(platform.requests.isEmpty())

        assertEquals(PermissionRequestResult.SYSTEM_PROMPT_STARTED, access.request(Capability.MESSAGING, explanationAcknowledged = true))
        assertEquals(listOf(Capability.MESSAGING), platform.requests)
        platform.rationale += Capability.MESSAGING
        assertEquals(CapabilityDisplayState.CAN_REQUEST, access.display(Capability.MESSAGING).state)

        platform.rationale.clear()
        assertEquals(CapabilityDisplayState.NEEDS_SETTINGS, access.display(Capability.MESSAGING).state)
        assertEquals(PermissionRequestResult.SETTINGS_REQUIRED, access.request(Capability.MESSAGING, explanationAcknowledged = true))
        assertEquals(1, platform.requests.size)
        assertTrue(platform.settingsOpens.isEmpty())

        access.openSettings(Capability.MESSAGING)
        assertEquals(listOf(Capability.MESSAGING), platform.settingsOpens)
    }

    @Test fun displayCopyIsNonTechnicalAndReadinessHasNoSideEffects() {
        val platform = FakePlatform()
        val access = CapabilityAccessController(platform, MemoryCapabilityAttemptStore())

        Capability.entries.forEach { capability ->
            val display = access.display(capability)
            assertFalse(display.reason.contains("android.permission"))
            assertFalse(display.reason.contains("ACCESS_"))
            assertFalse(display.reason.contains("CALL_PHONE"))
            assertFalse(display.reason.contains("SEND_SMS"))
        }
        val before = platform.requests.toList() to platform.settingsOpens.toList()
        val noContact = access.checkSosReadiness(eligibleContactCount = 0)
        assertFalse(noContact.hasEmergencyContact)
        assertEquals(before, platform.requests.toList() to platform.settingsOpens.toList())
    }

    private class FakePlatform(
        val granted: MutableSet<Capability> = mutableSetOf(),
        val rationale: MutableSet<Capability> = mutableSetOf()
    ) : CapabilityPlatform {
        val requests = mutableListOf<Capability>()
        val settingsOpens = mutableListOf<Capability>()
        override fun isGranted(capability: Capability) = capability in granted
        override fun shouldShowRationale(capability: Capability) = capability in rationale
        override fun request(capability: Capability) { requests += capability }
        override fun openSettings(capability: Capability) { settingsOpens += capability }
    }
}
