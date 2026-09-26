package vn.nckh27pa.fallsafe.emergency

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.permissions.CapabilitySnapshot
import vn.nckh27pa.fallsafe.permissions.LocationPrecision

class SosDispatchReportTest {
    private val contact = EmergencyContact(id="c1", name="Mai", relationship="", phone="0901")
    private val fix = LocationFix.validated(10.5, 106.5, 9f, 1_000, LocationSource.PHONE)!!

    @Test fun allGrantedProducesSuccessfulIndependentStepsAndPersistsReport() {
        val store = MemoryEmergencyStore()
        val sms = RecordingSms()
        val coordinator = coordinator(store, sms, VoiceDispatchStatus.STARTED, allGranted())

        coordinator.dispatchManual("all", listOf(contact), "Bà An", fix)

        val report = store.get("all")!!.dispatchReport!!
        assertEquals(SosStep.entries.toList(), report.steps.map { it.step })
        SosStep.entries.filter { it != SosStep.SIM_CALL }.forEach { assertEquals(SosStepStatus.SUCCESS, report.statusOf(it)) }
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.SIM_CALL))
        assertTrue(report.allSucceeded)
        assertTrue(report.failedSteps.isEmpty())
        assertEquals(report, coordinator.report("all"))
        assertTrue(report.labelOf(SosStep.VOICE_CALL).contains("gọi", ignoreCase = true))
    }

    @Test fun locationOnlyLeavesLocationUnaffectedAndReportsBothMissingTransports() {
        val coordinator = coordinator(
            MemoryEmergencyStore(), RecordingSms(), VoiceDispatchStatus.UNAVAILABLE,
            CapabilitySnapshot(calling = false, messaging = false, location = true, locationPrecision = LocationPrecision.APPROXIMATE)
        )

        coordinator.dispatchManual("location-only", listOf(contact), "Bà An", fix)
        val report = coordinator.report("location-only")!!

        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.LOCATION))
        assertEquals(SosStepStatus.PERMISSION_MISSING, report.statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.PERMISSION_MISSING, report.statusOf(SosStep.VOICE_CALL))
    }

    @Test fun missingCallPermissionDoesNotBlockSmsAndUsesCannotAutoCallCopy() {
        val sms = RecordingSms()
        val coordinator = coordinator(
            MemoryEmergencyStore(), sms, VoiceDispatchStatus.UNAVAILABLE,
            CapabilitySnapshot(calling = false, messaging = true, location = true, locationPrecision = LocationPrecision.PRECISE)
        )

        coordinator.dispatchManual("no-call", listOf(contact), "Bà An", fix)
        val report = coordinator.report("no-call")!!

        assertEquals(1, sms.requests.size)
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.PERMISSION_MISSING, report.statusOf(SosStep.VOICE_CALL))
        assertTrue(report.steps.first { it.step == SosStep.VOICE_CALL }.detail!!.contains("không thể tự động gọi"))
    }

    @Test fun missingSmsPermissionDoesNotAffectManualSimCallOrOtherReportSteps() {
        val manual = SimCallGateway { SimCallResult(true, "Đã mở cuộc gọi SIM") }
        val coordinator = coordinator(
            MemoryEmergencyStore(), RecordingSms(), VoiceDispatchStatus.UNAVAILABLE,
            CapabilitySnapshot(calling = true, messaging = false, location = true, locationPrecision = LocationPrecision.PRECISE)
        )

        coordinator.dispatchManual("no-sms", listOf(contact), "Bà An", fix)
        val report = coordinator.report("no-sms")!!

        assertTrue(manual.call(contact.phone).started)
        assertEquals(SosStepStatus.PERMISSION_MISSING, report.statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.UNAVAILABLE, report.statusOf(SosStep.VOICE_CALL))
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.LOCATION))
    }

    @Test fun disabledLocationProviderIsUnavailableButStillDispatchesSmsAndVoiceWithTruthfulText() {
        val sms = RecordingSms()
        val coordinator = coordinator(MemoryEmergencyStore(), sms, VoiceDispatchStatus.STARTED, allGranted())

        coordinator.dispatchManual("no-fix", listOf(contact), "Bà An", null)
        val report = coordinator.report("no-fix")!!

        assertEquals(SosStepStatus.UNAVAILABLE, report.statusOf(SosStep.LOCATION))
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.VOICE_CALL))
        assertEquals(SosStepStatus.SKIPPED, report.statusOf(SosStep.MAP_LINK))
        assertTrue(sms.requests.single().message.contains("Hiện chưa xác định được vị trí chính xác."))
        assertFalse(sms.requests.single().message.contains("0.0,0.0"))
    }

    @Test fun throwingSmsFailsItsStepWithoutBlockingLocationOrVoice() {
        val coordinator = coordinator(
            MemoryEmergencyStore(),
            object : EmergencySmsGateway { override fun send(request: SmsRequest): SmsDispatchState = error("SIM lỗi") },
            VoiceDispatchStatus.STARTED,
            allGranted()
        )

        coordinator.dispatchManual("sms-fail", listOf(contact), "Bà An", fix)
        val report = coordinator.report("sms-fail")!!

        assertEquals(SosStepStatus.FAILED, report.statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.LOCATION))
        assertEquals(SosStepStatus.SUCCESS, report.statusOf(SosStep.VOICE_CALL))
        assertEquals(SosStepStatus.FAILED, report.statusOf(SosStep.MAP_LINK))
    }

    @Test fun manualSimFailureIsFactualAndDoesNotMutateSosReport() {
        val store = MemoryEmergencyStore()
        val coordinator = coordinator(store, RecordingSms(), VoiceDispatchStatus.STARTED, allGranted())
        coordinator.dispatchManual("sim-failure", listOf(contact), "Bà An", fix)
        val before = coordinator.report("sim-failure")
        val gateway = SimCallGateway { SimCallResult(false, "Không có ứng dụng gọi điện") }

        val call = gateway.call(contact.phone)

        assertFalse(call.started)
        assertEquals("Không có ứng dụng gọi điện", call.detail)
        assertEquals(before, coordinator.report("sim-failure"))
    }

    private fun allGranted() = CapabilitySnapshot(true, true, true, LocationPrecision.PRECISE)

    private fun coordinator(
        store: EmergencyStore,
        sms: EmergencySmsGateway,
        voice: VoiceDispatchStatus,
        snapshot: CapabilitySnapshot
    ) = EmergencyCoordinator(
        sms = sms,
        backend = EmergencyBackendGateway { voice },
        call = EmergencyCallGateway { CallDispatchState(CallStatus.STARTED) },
        store = store,
        nowMs = { 2_000 },
        capabilities = { snapshot }
    )

    private class RecordingSms : EmergencySmsGateway {
        val requests = mutableListOf<SmsRequest>()
        override fun send(request: SmsRequest): SmsDispatchState {
            requests += request
            return SmsDispatchState(request.eventId, request.contactId, SmsDeliveryStatus.SENT)
        }
    }
}
