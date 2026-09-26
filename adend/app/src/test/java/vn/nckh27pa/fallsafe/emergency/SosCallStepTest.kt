package vn.nckh27pa.fallsafe.emergency

import kotlinx.coroutines.delay
import kotlinx.coroutines.test.runTest
import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.location.*
import vn.nckh27pa.fallsafe.permissions.CapabilitySnapshot

class SosCallStepTest {
    @Test fun savedPrimaryNumberIsNormalizedAndPreferred() {
        val harness = Harness()
        val chosen = primary.copy(phone = "+84 901-234-567", isPrimary = true)
        harness.coordinator.dispatchManual("event", listOf(backup, chosen), "Bà An", null)
        assertEquals(listOf("+84901234567"), harness.numbers)
        assertTrue(harness.messages.any { it.phone == "+84901234567" })
    }

    @Test fun lateLocationSendsOnlyOneSupplementAcrossCoordinatorRecreation() {
        val messages = mutableListOf<SmsRequest>()
        val store = MemoryEmergencyStore()
        fun coordinator() = EmergencyCoordinator(
            sms = object : EmergencySmsGateway {
                override fun send(request: SmsRequest): SmsDispatchState {
                    messages += request
                    return SmsDispatchState(request.eventId, request.contactId, SmsDeliveryStatus.SENDING)
                }
            }, backend = EmergencyBackendGateway { VoiceDispatchStatus.UNAVAILABLE }, store = store
        )
        coordinator().dispatchManual("event", listOf(primary), "Bà An", null)
        val fix = LocationFix.validated(10.5, 106.5, 100f, 1000, LocationSource.NETWORK)!!
        repeat(3) { coordinator().updateLocation("event", fix) }
        assertEquals(2, messages.size)
        assertFalse(messages.first().message.contains("maps.google"))
        assertTrue(messages.last().supplement)
        assertTrue(messages.last().message.contains(fix.mapsUrl))
    }
    private val primary = EmergencyContact(id="c1", name="Mai", relationship="", phone="0901")
    private val backup = EmergencyContact(id="c2", name="An", relationship="", phone="0902")
    private class Harness(
        voice: VoiceDispatchStatus = VoiceDispatchStatus.UNAVAILABLE,
        calling: Boolean = true,
        messaging: Boolean = true,
        callState: CallDispatchState = CallDispatchState(CallStatus.STARTED),
        smsFails: Boolean = false,
        callThrows: Boolean = false
    ) {
        val messages = mutableListOf<SmsRequest>()
        val numbers = mutableListOf<String>()
        val coordinator = EmergencyCoordinator(
            sms = object : EmergencySmsGateway {
                override fun send(request: SmsRequest): SmsDispatchState {
                    messages += request
                    if (smsFails) error("SMS unavailable")
                    return SmsDispatchState(request.eventId, request.contactId, SmsDeliveryStatus.SENDING)
                }
            },
            backend = EmergencyBackendGateway { voice },
            call = EmergencyCallGateway { numbers += it; if (callThrows) error("dial failure"); callState },
            store = MemoryEmergencyStore(),
            capabilities = { CapabilitySnapshot(calling, messaging, false) }
        )
        fun report() = coordinator.report("event")!!
    }
    @Test fun callsFirstEligibleContactExactlyOnceEvenOnRepeatedDispatch() {
        val harness = Harness()
        val contacts = listOf(primary.copy(receiveSos=false), primary.copy(phone=" "), backup, primary)
        repeat(2) { harness.coordinator.dispatchManual("event", contacts, "Bà An", null) }
        assertEquals(listOf(backup.phone), harness.numbers)
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
        assertEquals("Đã gọi SIM tới An.", harness.report().steps.first { it.step == SosStep.SIM_CALL }.detail)
        assertEquals(SosStep.entries.toList(), harness.report().steps.map { it.step })
    }
    @Test fun backendStartedDoesNotSuppressHandsetCall() {
        val harness = Harness(voice = VoiceDispatchStatus.STARTED)
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        assertEquals(listOf(primary.phone), harness.numbers)
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
    }
    @Test fun backendNotStartedAttemptsHandsetCall() {
        for (voice in listOf(VoiceDispatchStatus.CONFIGURED, VoiceDispatchStatus.UNAVAILABLE, VoiceDispatchStatus.DISABLED)) {
            val harness = Harness(voice = voice)
            harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
            assertEquals(voice.name, listOf(primary.phone), harness.numbers)
            assertEquals(voice.name, SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
        }
    }
    @Test fun callPermissionDeniedStillSubmitsSmsWithoutWaitingForDelivery() {
        val harness = Harness(calling = false)
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        assertEquals(SosStepStatus.PERMISSION_MISSING, harness.report().statusOf(SosStep.SIM_CALL))
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
        assertEquals(1, harness.messages.size); assertTrue(harness.numbers.isEmpty())
    }
    @Test fun missingLocationBlocksNeitherSmsNorSimCall() {
        val harness = Harness()
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
        assertEquals(listOf(primary.phone), harness.numbers)
        val message = harness.messages.single().message
        assertTrue(message.contains("Hiện chưa xác định được vị trí chính xác. Vui lòng gọi lại hoặc kiểm tra ứng dụng theo dõi."))
        assertFalse(message.contains("maps.google")); assertFalse(message.contains("Tọa độ:"))
    }
    @Test fun gpsUnavailableNetworkFixProducesMapsSmsAndSimCall() = runTest {
        val source = object : PlatformLocationSource {
            override fun permission() = LocationPermission.APPROXIMATE
            override fun enabledProviders() = setOf(LocationProviderKind.NETWORK)
            override suspend fun lastKnown(): LocationFix? = null
            override suspend fun current(kind: LocationProviderKind, timeoutMs: Long) = LocationFix.validated(10.5,106.5,150f,1000,LocationSource.UNKNOWN)
        }
        val lookup = BestAvailableLocationRepository(source) { 1000 }.getBestAvailableLocation()
        val harness = Harness()
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", lookup.fix)
        assertEquals(LocationSource.NETWORK, lookup.fix!!.source)
        assertTrue(harness.messages.single().message.contains("https://maps.google.com/?q=10.5,106.5"))
        assertTrue(harness.messages.single().message.contains("Nguồn vị trí: điện thoại"))
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
    }
    @Test fun noEligibleRecipientReportsUnavailableWithoutDialing() {
        val harness = Harness()
        harness.coordinator.dispatchManual("event", listOf(primary.copy(receiveSos=false)), "Bà An", null)
        assertTrue(harness.numbers.isEmpty())
        assertEquals(SosStepStatus.UNAVAILABLE, harness.report().statusOf(SosStep.SIM_CALL))
        assertEquals("Chưa có người thân để gọi.", harness.report().steps.first { it.step == SosStep.SIM_CALL }.detail)
    }
    @Test fun everyGatewayFailureMapsDetailWithoutRetry() {
        for ((status, expected) in listOf(CallStatus.PERMISSION_MISSING to SosStepStatus.PERMISSION_MISSING,
            CallStatus.UNAVAILABLE to SosStepStatus.UNAVAILABLE, CallStatus.FAILED to SosStepStatus.FAILED)) {
            val harness = Harness(callState = CallDispatchState(status, "Chi tiết từ thiết bị"))
            harness.coordinator.dispatchManual("event", listOf(primary, backup), "Bà An", null)
            assertEquals(listOf(primary.phone), harness.numbers)
            assertEquals(expected, harness.report().statusOf(SosStep.SIM_CALL))
            assertEquals("Chi tiết từ thiết bị", harness.report().steps.first { it.step == SosStep.SIM_CALL }.detail)
            assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
        }
    }
    @Test fun smsFailureStillDialsAndCallExceptionIsReported() {
        val harness = Harness(smsFails=true, callThrows=true)
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        assertEquals(listOf(primary.phone), harness.numbers)
        assertEquals(SosStepStatus.FAILED, harness.report().statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.FAILED, harness.report().statusOf(SosStep.SIM_CALL))
        assertNotNull(harness.report().steps.first { it.step == SosStep.SIM_CALL }.detail)
    }
    @Test fun smsPermissionDeniedStillCallsSim() {
        val harness = Harness(messaging=false)
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        assertTrue(harness.messages.isEmpty()); assertEquals(listOf(primary.phone), harness.numbers)
        assertEquals(SosStepStatus.PERMISSION_MISSING, harness.report().statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
    }
    @Test fun deniedLocationPermissionStillQueuesSmsAndAttemptsSimCall() = runTest {
        val source = object : PlatformLocationSource {
            override fun permission() = LocationPermission.DENIED
            override fun enabledProviders() = emptySet<LocationProviderKind>()
            override suspend fun lastKnown(): LocationFix? = null
            override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? = null
        }
        val lookup = BestAvailableLocationRepository(source) { 0 }.getBestAvailableLocation()
        assertEquals(LocationFailureCause.PERMISSION_DENIED, lookup.cause)
        val harness = Harness()
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", lookup.fix)
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
        assertEquals(listOf(primary.phone), harness.numbers)
    }
    @Test fun locationTimeoutOrUnavailableStillQueuesSmsAndAttemptsSimCallWithoutFabricatedCoordinates() = runTest {
        for (source in listOf<PlatformLocationSource>(timedOutSource(), unavailableSource())) {
            val lookup = BestAvailableLocationRepository(source) { testScheduler.currentTime }.getBestAvailableLocation(100)
            assertNull(lookup.fix)
            val harness = Harness()
            harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", lookup.fix)
            assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
            assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SIM_CALL))
            assertEquals(listOf(primary.phone), harness.numbers)
            val message = harness.messages.single().message
            assertFalse(message.contains("maps.google"))
            assertFalse(message.contains("Tọa độ:"))
            assertFalse(message.contains("0.0,0.0"))
        }
    }
    @Test fun noSupplementWhenFirstSmsAlreadyCarriedAFix() {
        val harness = Harness()
        val fix = LocationFix.validated(10.5, 106.5, 8f, 1000, LocationSource.PHONE)!!
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", fix)
        harness.coordinator.updateLocation("event", LocationFix.validated(10.6, 106.6, 8f, 2000, LocationSource.PHONE)!!)
        harness.coordinator.updateLocation("event", LocationFix.validated(10.7, 106.7, 8f, 3000, LocationSource.PHONE)!!)
        assertEquals(1, harness.messages.size)
        assertFalse(harness.messages.single().supplement)
        assertTrue(harness.messages.single().message.contains(fix.mapsUrl))
    }
    @Test fun supplementIsStillExactlyOneWhenFirstSmsHadNoFixAndSeveralFixesArrive() {
        val harness = Harness()
        harness.coordinator.dispatchManual("event", listOf(primary), "Bà An", null)
        harness.coordinator.updateLocation("event", LocationFix.validated(10.5, 106.5, 8f, 2000, LocationSource.PHONE)!!)
        harness.coordinator.updateLocation("event", LocationFix.validated(10.6, 106.6, 8f, 3000, LocationSource.PHONE)!!)
        harness.coordinator.updateLocation("event", LocationFix.validated(10.7, 106.7, 8f, 4000, LocationSource.PHONE)!!)
        assertEquals(2, harness.messages.size)
        assertFalse(harness.messages.first().supplement)
        assertTrue(harness.messages.last().supplement)
        assertTrue(harness.messages.last().message.contains("maps.google"))
    }
    @Test fun blankPhoneContactIsSkippedWithoutStoppingOtherContacts() {
        val harness = Harness()
        val blank = primary.copy(id = "blank", name = "Trống", phone = "   ")
        val valid = backup.copy(id = "valid", phone = "0902")
        harness.coordinator.dispatchManual("event", listOf(blank, valid), "Bà An", null)
        assertEquals(listOf("0902"), harness.messages.map { it.phone })
        assertEquals(1, harness.messages.size)
        assertEquals(listOf("0902"), harness.numbers)
        assertEquals(SosStepStatus.SUCCESS, harness.report().statusOf(SosStep.SMS))
        assertTrue(harness.report().steps.first { it.step == SosStep.SMS }.detail!!.contains("bỏ qua"))
    }
    private fun timedOutSource() = object : PlatformLocationSource {
        override fun permission() = LocationPermission.PRECISE
        override fun enabledProviders() = setOf(LocationProviderKind.FUSED)
        override suspend fun lastKnown(): LocationFix? = null
        override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? {
            delay(20_000)
            return null
        }
    }
    private fun unavailableSource() = object : PlatformLocationSource {
        override fun permission() = LocationPermission.PRECISE
        override fun enabledProviders() = emptySet<LocationProviderKind>()
        override suspend fun lastKnown(): LocationFix? = null
        override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? = null
    }
}
