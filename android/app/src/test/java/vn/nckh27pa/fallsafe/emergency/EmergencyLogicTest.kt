package vn.nckh27pa.fallsafe.emergency

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.EmergencyContact

class EmergencyLogicTest {
    @Test fun permissionFailureCopyNeverExposesAndroidConstantNames() {
        val copy = listOf(
            EmergencyFailureMessages.callPermissionMissing,
            EmergencyFailureMessages.messagingPermissionMissing,
            EmergencyFailureMessages.simAccessMissing
        ).joinToString(" ")
        assertFalse(copy.contains("CALL_PHONE"))
        assertFalse(copy.contains("SEND_SMS"))
        assertFalse(copy.contains("READ_PHONE_STATE"))
        assertFalse(copy.contains("android.permission"))
    }

    @Test fun locationValidationAndFreshnessRejectFalseCoordinates() {
        assertNull(LocationFix.validated(0.0, 0.0, 5f, 1000, LocationSource.PHONE))
        assertNull(LocationFix.validated(Double.NaN, 106.0, 5f, 1000, LocationSource.PHONE))
        assertNull(LocationFix.validated(91.0, 106.0, 5f, 1000, LocationSource.ESP32_GNSS))
        val fix = LocationFix.validated(10.5, 106.5, 8f, 1000, LocationSource.PHONE)!!
        assertEquals(LocationFreshness.FRESH, fix.freshness(121000))
        assertEquals(LocationFreshness.STALE, fix.freshness(121001))
        assertEquals("https://www.google.com/maps/search/?api=1&query=10.5,106.5", fix.mapsUrl)
    }

    @Test fun emergencyAndManualMessagesAreTruthfulAndDistinct() {
        val fix = LocationFix.validated(10.5, 106.5, 8f, 1000, LocationSource.PHONE)!!
        val emergency = EmergencyMessageFormatter.emergency("Người dùng FallSafe", 2000, fix, 200000)
        assertTrue(emergency.startsWith("CẢNH BÁO SOS: Tôi có thể đã bị ngã."))
        assertTrue(emergency.contains("Thời điểm sự kiện:"))
        assertTrue(emergency.contains("Tọa độ: 10.5,106.5"))
        assertTrue(emergency.contains(fix.mapsUrl))
        assertTrue(emergency.contains("Vị trí gần nhất, cập nhật lúc"))
        assertTrue(emergency.contains("Nguồn vị trí: điện thoại"))
        assertTrue(emergency.contains("8 m"))
        val manual = EmergencyMessageFormatter.manualLocation("Người dùng FallSafe", fix)
        assertFalse(manual.contains("ngã", ignoreCase = true))
        assertFalse(manual.contains("SOS", ignoreCase = true))
    }

    @Test fun emergencyWithoutAFixUsesTheRequiredTruthfulFallback() {
        val emergency = EmergencyMessageFormatter.emergency("Bà An", 2_000, null, 2_000)
        assertTrue(emergency.startsWith("CẢNH BÁO SOS: Tôi có thể đã bị ngã."))
        assertTrue(emergency.contains("Thời điểm sự kiện:"))
        assertTrue(emergency.contains("Chưa xác định được vị trí."))
        assertFalse(emergency.contains("0.0,0.0"))
    }

    @Test fun espSourceAndLateSupplementAreExplicitWithoutInventingANewEventTime() {
        val fix = LocationFix.validated(10.5, 106.5, null, 1_000, LocationSource.ESP32_GNSS)!!
        val emergency = EmergencyMessageFormatter.emergency("Bà An", 2_000, fix, 2_000)
        assertTrue(emergency.contains("Nguồn vị trí: ESP32 GNSS"))
        val supplement = EmergencyMessageFormatter.locationSupplement("Bà An", fix, 999_999)
        assertTrue(supplement.startsWith("Bổ sung vị trí"))
        assertFalse(supplement.contains("Thời điểm sự kiện"))
        assertTrue(supplement.contains("Thời điểm fix"))
    }

    @Test fun deliveryFailureAfterAllPartsSentPreservesSentSubmission() {
        val aggregate = SmsPartAggregation(2)
        assertEquals(SmsDeliveryStatus.SENDING, aggregate.recordSent(0, true).status)
        assertEquals(SmsDeliveryStatus.SENT, aggregate.recordSent(1, true).status)
        val deliveryFailure = aggregate.recordDelivery(0, false)
        assertEquals(SmsDeliveryStatus.SENT, deliveryFailure.status)
        assertTrue(deliveryFailure.detail!!.contains("giao", ignoreCase = true))
        assertEquals(SmsDeliveryStatus.SENT, aggregate.recordDelivery(0, true).status)
        assertEquals(SmsDeliveryStatus.DELIVERED, aggregate.recordDelivery(1, true).status)
    }

    @Test fun sentIntentFailureIsTheOnlyPartFailureThatMarksSmsFailed() {
        val aggregate = SmsPartAggregation(2)
        aggregate.recordSent(0, true)
        assertEquals(SmsDeliveryStatus.FAILED, aggregate.recordSent(1, false).status)
    }

    @Test fun cancelledGenerationIgnoresLateLocationAndDispatchIsIdempotent() {
        val sms = RecordingSmsGateway()
        val backend = RecordingBackendGateway()
        val store = MemoryEmergencyStore()
        val coordinator = EmergencyCoordinator(sms, backend, store, { 1000 })
        val contact = EmergencyContact(id="c1", name="Mai", relationship="", phone="0901234567")
        val event = coordinator.beginVerifying("event-1")
        coordinator.cancel(event)
        coordinator.updateLocation(event, LocationFix.validated(10.5,106.5,5f,1000,LocationSource.PHONE)!!)
        coordinator.timeout(event, listOf(contact), "Người dùng FallSafe")
        assertEquals(0, sms.sent.size);assertEquals(0, backend.events.size)

        coordinator.dispatchManual("event-2", listOf(contact), "Người dùng FallSafe", null)
        coordinator.dispatchManual("event-2", listOf(contact), "Người dùng FallSafe", null)
        assertEquals(1, sms.sent.size);assertEquals(1, backend.events.size)
        assertTrue(sms.sent.single().message.contains("Chưa xác định được vị trí."))
        coordinator.updateLocation("event-2", LocationFix.validated(10.5,106.5,5f,1100,LocationSource.PHONE)!!)
        coordinator.updateLocation("event-2", LocationFix.validated(10.6,106.6,5f,1200,LocationSource.PHONE)!!)
        assertEquals(2, sms.sent.size)
    }

    @Test fun backendAndOneSmsFailureNeverBlockRemainingLocalSosDispatch() {
        val attempted = mutableListOf<String>()
        val sms = object : EmergencySmsGateway {
            override fun send(request: SmsRequest): SmsDispatchState {
                attempted += request.contactId
                if (request.contactId == "c1") error("SIM failure")
                return SmsDispatchState(request.eventId, request.contactId, SmsDeliveryStatus.QUEUED)
            }
        }
        val coordinator = EmergencyCoordinator(
            sms = sms,
            backend = EmergencyBackendGateway { error("offline") },
            store = MemoryEmergencyStore(),
            nowMs = { 1_000 }
        )
        val contacts = listOf(
            EmergencyContact(id="c1", name="Primary", relationship="", phone="0901", isPrimary=true, callPriority=0),
            EmergencyContact(id="c2", name="Backup", relationship="", phone="0902", callPriority=1),
            EmergencyContact(id="off", name="Disabled", relationship="", phone="0903", receiveSos=false, callPriority=2)
        )

        coordinator.dispatchManual("event-failure", contacts, "Bà An", null)

        assertEquals(listOf("c1", "c2"), attempted)
    }

    @Test fun noEligibleContactIsSafeAndStillAttemptsIndependentBackendVoice() {
        val sms = RecordingSmsGateway()
        val backend = RecordingBackendGateway()
        EmergencyCoordinator(sms, backend, MemoryEmergencyStore(), { 1_000 })
            .dispatchManual("event-empty", emptyList(), "Bà An", null)
        assertTrue(sms.sent.isEmpty())
        assertEquals(listOf("event-empty"), backend.events)
    }

    private class RecordingSmsGateway : EmergencySmsGateway {
        val sent=mutableListOf<SmsRequest>()
        override fun send(request: SmsRequest): SmsDispatchState { sent+=request;return SmsDispatchState(request.eventId,request.contactId,SmsDeliveryStatus.QUEUED) }
    }
    private class RecordingBackendGateway : EmergencyBackendGateway {
        val events=mutableListOf<String>()
        override fun startVoice(eventId: String):VoiceDispatchStatus { events+=eventId;return VoiceDispatchStatus.STARTED }
    }
}
