package vn.nckh27pa.fallsafe.ai

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Test
import vn.nckh27pa.fallsafe.api.AiTextReply
import vn.nckh27pa.fallsafe.api.ApiResult
import vn.nckh27pa.fallsafe.api.ErrorKind

class BackendTextAiProviderTest {
    @Test fun mapsSuccessAndSendsOnlyTheExplicitText() = runTest {
        val prompts = mutableListOf<String>()
        val provider = BackendTextAiProvider { text ->
            prompts += text
            ApiResult.Success(AiTextReply("COMPLETED", "Kết quả"))
        }

        assertEquals(AiProviderResult.Success("Kết quả"), provider.requestText("Chỉ văn bản này"))
        assertEquals(listOf("Chỉ văn bản này"), prompts)
    }

    @Test fun mapsOfflineNotConfiguredTimeoutAndBackendUnavailable() = runTest {
        val cases: List<Pair<ApiResult<AiTextReply>, AiUnavailableReason>> = listOf(
            ApiResult.Failure(ErrorKind.OFFLINE) to AiUnavailableReason.OFFLINE,
            ApiResult.Failure(ErrorKind.BACKEND, "NOT_CONFIGURED") to AiUnavailableReason.NOT_CONFIGURED,
            ApiResult.Failure(ErrorKind.TIMEOUT) to AiUnavailableReason.BACKEND_UNAVAILABLE,
            ApiResult.Failure(ErrorKind.BACKEND, "SERVICE_UNAVAILABLE") to AiUnavailableReason.BACKEND_UNAVAILABLE
        )
        for ((api, expected) in cases) {
            val provider = BackendTextAiProvider { api }
            assertEquals(AiProviderResult.Unavailable(expected), provider.requestText("text"))
        }
    }
}
