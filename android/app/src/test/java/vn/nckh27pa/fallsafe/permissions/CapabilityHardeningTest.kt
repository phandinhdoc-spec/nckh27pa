package vn.nckh27pa.fallsafe.permissions

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.DemoController

class CapabilityHardeningTest {
    @Test fun noPermissionReportsEveryCapabilityMissingAndRequiresExplanationFirst() {
        val platform = MutablePlatform()
        val access = CapabilityAccessController(platform, MemoryCapabilityAttemptStore())

        assertEquals(Capability.entries.toSet(), access.checkSosReadiness(1).missingCapabilities)
        Capability.entries.forEach {
            assertEquals(PermissionRequestResult.EXPLANATION_REQUIRED, access.request(it, false))
        }
        assertTrue(platform.requests.isEmpty())
    }

    @Test fun deniedOnceCanRequestAgainAndPermanentDenialOnlyOffersSettings() {
        val platform = MutablePlatform()
        val access = CapabilityAccessController(platform, MemoryCapabilityAttemptStore())

        assertEquals(PermissionRequestResult.SYSTEM_PROMPT_STARTED, access.request(Capability.LOCATION, true))
        platform.rationale += Capability.LOCATION
        assertEquals(CapabilityDisplayState.CAN_REQUEST, access.display(Capability.LOCATION).state)
        assertEquals(PermissionRequestResult.SYSTEM_PROMPT_STARTED, access.request(Capability.LOCATION, true))
        platform.rationale.clear()
        assertEquals(CapabilityDisplayState.NEEDS_SETTINGS, access.display(Capability.LOCATION).state)
        assertEquals(PermissionRequestResult.SETTINGS_REQUIRED, access.request(Capability.LOCATION, true))
        assertEquals(2, platform.requests.size)
        assertTrue(platform.settings.isEmpty())
        access.openSettings(Capability.LOCATION)
        assertEquals(listOf(Capability.LOCATION), platform.settings)
    }

    @Test fun refreshTruthAfterSettingsImmediatelyBecomesGranted() {
        val platform = MutablePlatform()
        val access = CapabilityAccessController(platform, MemoryCapabilityAttemptStore())
        access.request(Capability.MESSAGING, true)
        assertEquals(CapabilityDisplayState.NEEDS_SETTINGS, access.display(Capability.MESSAGING).state)

        platform.granted += Capability.MESSAGING

        assertEquals(CapabilityDisplayState.GRANTED, access.display(Capability.MESSAGING).state)
    }

    @Test fun approximateLocationIsWorkingAndExplainedWithoutPermissionConstants() {
        val platform = MutablePlatform(
            granted = mutableSetOf(Capability.LOCATION),
            precision = LocationPrecision.APPROXIMATE
        )
        val display = CapabilityAccessController(platform, MemoryCapabilityAttemptStore()).display(Capability.LOCATION)

        assertEquals(CapabilityDisplayState.GRANTED, display.state)
        assertEquals(LocationPrecision.APPROXIMATE, display.locationPrecision)
        assertTrue(display.note!!.contains("gần đúng", ignoreCase = true))
        assertFalse(display.note!!.contains("ACCESS_"))
    }

    @Test fun messagingGrantDoesNotDependOnPhoneStateAndShowsMultiSimDegradation() {
        val platform = MutablePlatform(
            granted = mutableSetOf(Capability.MESSAGING),
            phoneStateGranted = false
        )
        val display = CapabilityAccessController(platform, MemoryCapabilityAttemptStore()).display(Capability.MESSAGING)

        assertEquals(CapabilityDisplayState.GRANTED, display.state)
        assertTrue(display.note!!.contains("nhiều SIM"))
    }

    @Test fun controllerOffersOrderedPersistedFirstRunSetup() {
        val platform = MutablePlatform()
        val setupStore = MemoryPermissionSetupStore()
        val controller = DemoController(permissionSetupStore = setupStore)
        controller.bindCapabilityAccess(CapabilityAccessController(platform, MemoryCapabilityAttemptStore()))

        assertEquals(Capability.LOCATION, controller.nextPermissionSetupStep())
        platform.granted += Capability.LOCATION
        assertEquals(Capability.MESSAGING, controller.nextPermissionSetupStep())
        platform.granted += Capability.MESSAGING
        assertEquals(Capability.CALLING, controller.nextPermissionSetupStep())
        platform.granted += Capability.CALLING
        assertNull(controller.nextPermissionSetupStep())
        assertFalse(controller.permissionSetupSeen)
        controller.markPermissionSetupSeen()
        assertTrue(DemoController(permissionSetupStore = setupStore).permissionSetupSeen)
        assertFalse(controller.permissionSetupExplanation.contains("ACCESS_"))
    }

    private class MutablePlatform(
        val granted: MutableSet<Capability> = mutableSetOf(),
        val rationale: MutableSet<Capability> = mutableSetOf(),
        var precision: LocationPrecision = LocationPrecision.NONE,
        var phoneStateGranted: Boolean = true
    ) : CapabilityPlatform {
        val requests = mutableListOf<Capability>()
        val settings = mutableListOf<Capability>()
        override fun isGranted(capability: Capability) = capability in granted
        override fun shouldShowRationale(capability: Capability) = capability in rationale
        override fun locationPrecision() = precision
        override fun hasPhoneStateAccess() = phoneStateGranted
        override fun request(capability: Capability) { requests += capability }
        override fun openSettings(capability: Capability) { settings += capability }
    }
}
