package vn.nckh27pa.fallsafe.ai

import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.CancellationException
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.emergency.EmergencyBackendGateway
import vn.nckh27pa.fallsafe.emergency.EmergencyCoordinator
import vn.nckh27pa.fallsafe.emergency.EmergencySmsGateway
import vn.nckh27pa.fallsafe.emergency.MemoryEmergencyStore
import vn.nckh27pa.fallsafe.emergency.SmsDeliveryStatus
import vn.nckh27pa.fallsafe.emergency.SmsDispatchState

class OptionalAiAssistantTest {
    @Test fun disabledByDefaultAndCannotEnableBeforePersistedConsent() = runTest {
        val store = MemoryAiPreferenceStore()
        val provider = RecordingProvider(AiProviderResult.Success("ok"))
        val assistant = OptionalAiAssistant(store, provider)

        assertFalse(assistant.state().consentGranted)
        assertFalse(assistant.state().enabled)
        assertFalse(assistant.setEnabled(true))
        assertEquals(AiRequestResult.Disabled, assistant.requestText("Xin chào"))
        assertTrue(provider.prompts.isEmpty())

        assistant.setConsent(true)
        assertTrue(assistant.setEnabled(true))
        val restored = OptionalAiAssistant(store, RecordingProvider(AiProviderResult.Success("ok")))
        assertTrue(restored.state().consentGranted)
        assertTrue(restored.state().enabled)
        restored.setConsent(false)
        assertFalse(restored.state().enabled)
    }

    @Test fun enabledRequestsAreTextOnlyAndUnavailableReasonsStayExplicit() = runTest {
        for (reason in listOf(
            AiUnavailableReason.OFFLINE,
            AiUnavailableReason.BACKEND_UNAVAILABLE,
            AiUnavailableReason.NOT_CONFIGURED
        )) {
            val store = MemoryAiPreferenceStore(consentGranted = true, enabled = true)
            val provider = RecordingProvider(AiProviderResult.Unavailable(reason))
            val assistant = OptionalAiAssistant(store, provider)
            assertEquals(AiRequestResult.Unavailable(reason), assistant.requestText("Tóm tắt hướng dẫn an toàn"))
            assertEquals(listOf("Tóm tắt hướng dẫn an toàn"), provider.prompts)
        }
    }

    @Test fun aiFailureIsNeverConsultedByOrAllowedToBlockSos() = runTest {
        var aiCalls = 0
        val assistant = OptionalAiAssistant(
            MemoryAiPreferenceStore(consentGranted = true, enabled = true),
            TextAiProvider { aiCalls++; error("AI provider failed") }
        )
        assertEquals(
            AiRequestResult.Unavailable(AiUnavailableReason.BACKEND_UNAVAILABLE),
            assistant.requestText("optional")
        )
        val sent = mutableListOf<String>()
        val coordinator = EmergencyCoordinator(
            sms = object : EmergencySmsGateway {
                override fun send(request: vn.nckh27pa.fallsafe.emergency.SmsRequest): SmsDispatchState {
                    sent += request.contactId
                    return SmsDispatchState(request.eventId, request.contactId, SmsDeliveryStatus.QUEUED)
                }
            },
            backend = EmergencyBackendGateway { error("backend offline") },
            store = MemoryEmergencyStore(),
            nowMs = { 1_000 }
        )

        coordinator.dispatchManual(
            "event-ai-independent",
            listOf(EmergencyContact(id="c1", name="Mai", relationship="", phone="0901")),
            "Bà An",
            null
        )

        assertEquals(listOf("c1"), sent)
        assertEquals(1, aiCalls)
    }

    @Test fun coroutineCancellationIsNeverConvertedIntoProviderUnavailability() = runTest {
        val assistant = OptionalAiAssistant(
            MemoryAiPreferenceStore(consentGranted = true, enabled = true),
            TextAiProvider { throw CancellationException("cancelled") }
        )
        var propagated = false
        try {
            assistant.requestText("optional")
        } catch (_: CancellationException) {
            propagated = true
        }
        assertTrue(propagated)
    }

    private class RecordingProvider(private val result: AiProviderResult) : TextAiProvider {
        val prompts = mutableListOf<String>()
        override suspend fun requestText(prompt: String): AiProviderResult {
            prompts += prompt
            return result
        }
    }
}
