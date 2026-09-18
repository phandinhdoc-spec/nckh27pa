package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test

class MonitoringForegroundPolicyTest {
    @Test fun locationTypeRequiresAtLeastOneForegroundLocationPermission() {
        assertFalse(MonitoringForegroundPolicy.includeLocationType(false, false))
        assertTrue(MonitoringForegroundPolicy.includeLocationType(true, false))
        assertTrue(MonitoringForegroundPolicy.includeLocationType(false, true))
    }
    @Test fun manualOrAutomaticStateRequestsOnlyOneRefreshPerEvent() {
        val gate=vn.nckh27pa.fallsafe.api.EventLocationRefreshGate()
        assertTrue(gate.shouldRequest(1))
        assertFalse(gate.shouldRequest(1))
        assertTrue(gate.shouldRequest(2))
    }
    @Test fun controllerCallsOnlyTheSelectedContactThroughInjectedFallback() {
        val contact=EmergencyContact("selected","Mai","","0901234567")
        val controller=DemoController(InMemoryContactRepository(listOf(contact)))
        var called:String?=null
        controller.simCallGateway=vn.nckh27pa.fallsafe.emergency.SimCallGateway { phone ->
            called=phone;vn.nckh27pa.fallsafe.emergency.SimCallResult(true,"Đã mở")
        }
        assertFalse(controller.callContactViaSim("missing").started)
        assertNull(called)
        assertTrue(controller.callContactViaSim("selected").started)
        assertEquals("0901234567",called)
        controller.close()
    }
    @Test fun simCallFailureIsReportedWithoutChangingTheSelectedContact() {
        val contact=EmergencyContact("selected","Mai","","0901234567")
        val controller=DemoController(InMemoryContactRepository(listOf(contact)))
        controller.simCallGateway=vn.nckh27pa.fallsafe.emergency.SimCallGateway {
            vn.nckh27pa.fallsafe.emergency.SimCallResult(false,"SIM không khả dụng")
        }
        val result=controller.callContactViaSim("selected")
        assertFalse(result.started)
        assertEquals("SIM không khả dụng",result.detail)
        controller.close()
    }
}
