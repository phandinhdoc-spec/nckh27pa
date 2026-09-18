package vn.nckh27pa.fallsafe.ai

import android.content.Context
import kotlinx.coroutines.CancellationException

data class AiState(val consentGranted: Boolean, val enabled: Boolean)

enum class AiUnavailableReason { OFFLINE, BACKEND_UNAVAILABLE, NOT_CONFIGURED }

sealed interface AiProviderResult {
    data class Success(val text: String) : AiProviderResult
    data class Unavailable(val reason: AiUnavailableReason) : AiProviderResult
}

sealed interface AiRequestResult {
    data object Disabled : AiRequestResult
    data class Success(val text: String) : AiRequestResult
    data class Unavailable(val reason: AiUnavailableReason) : AiRequestResult
}

/** Deliberately accepts text only: SOS records, contacts, calls, SMS, and location are not inputs. */
fun interface TextAiProvider { suspend fun requestText(prompt: String): AiProviderResult }

interface AiPreferenceStore {
    fun state(): AiState
    fun save(state: AiState)
}

class MemoryAiPreferenceStore(
    consentGranted: Boolean = false,
    enabled: Boolean = false
) : AiPreferenceStore {
    private var value = AiState(consentGranted, enabled && consentGranted)
    override fun state() = value
    override fun save(state: AiState) { value = state.copy(enabled = state.enabled && state.consentGranted) }
}

class SharedPreferencesAiPreferenceStore(context: Context) : AiPreferenceStore {
    private val preferences = context.getSharedPreferences("fallsafe_optional_ai", Context.MODE_PRIVATE)
    override fun state(): AiState {
        val consent = preferences.getBoolean("consent_granted", false)
        return AiState(consent, consent && preferences.getBoolean("enabled", false))
    }
    override fun save(state: AiState) {
        preferences.edit()
            .putBoolean("consent_granted", state.consentGranted)
            .putBoolean("enabled", state.enabled && state.consentGranted)
            .apply()
    }
}

class OptionalAiAssistant(
    private val preferences: AiPreferenceStore,
    private val provider: TextAiProvider
) {
    fun state(): AiState = preferences.state()
    fun setConsent(granted: Boolean) {
        val previous = state()
        preferences.save(AiState(granted, previous.enabled && granted))
    }
    fun setEnabled(enabled: Boolean): Boolean {
        val current = state()
        if (enabled && !current.consentGranted) return false
        preferences.save(current.copy(enabled = enabled))
        return true
    }
    suspend fun requestText(prompt: String): AiRequestResult {
        if (!state().enabled) return AiRequestResult.Disabled
        val providerResult = try {
            provider.requestText(prompt)
        } catch (error: CancellationException) {
            throw error
        } catch (_: Exception) {
            AiProviderResult.Unavailable(AiUnavailableReason.BACKEND_UNAVAILABLE)
        }
        return when (val result = providerResult) {
            is AiProviderResult.Success -> AiRequestResult.Success(result.text)
            is AiProviderResult.Unavailable -> AiRequestResult.Unavailable(result.reason)
        }
    }
}

class UnavailableTextAiProvider(
    private val reason: AiUnavailableReason = AiUnavailableReason.NOT_CONFIGURED
) : TextAiProvider {
    override suspend fun requestText(prompt: String): AiProviderResult = AiProviderResult.Unavailable(reason)
}

class BackendTextAiProvider(
    private val request: suspend (String) -> vn.nckh27pa.fallsafe.api.ApiResult<vn.nckh27pa.fallsafe.api.AiTextReply>
) : TextAiProvider {
    constructor(repository: vn.nckh27pa.fallsafe.api.ApiRepository) : this(repository::aiText)
    override suspend fun requestText(prompt: String): AiProviderResult = when (val result = request(prompt)) {
        is vn.nckh27pa.fallsafe.api.ApiResult.Success -> result.value.text
            ?.takeIf { result.value.status == "COMPLETED" && it.isNotBlank() }
            ?.let { AiProviderResult.Success(it) }
            ?: AiProviderResult.Unavailable(AiUnavailableReason.BACKEND_UNAVAILABLE)
        is vn.nckh27pa.fallsafe.api.ApiResult.Failure -> AiProviderResult.Unavailable(when {
            result.kind == vn.nckh27pa.fallsafe.api.ErrorKind.OFFLINE -> AiUnavailableReason.OFFLINE
            result.code == "NOT_CONFIGURED" -> AiUnavailableReason.NOT_CONFIGURED
            else -> AiUnavailableReason.BACKEND_UNAVAILABLE
        })
        vn.nckh27pa.fallsafe.api.ApiResult.Loading -> AiProviderResult.Unavailable(AiUnavailableReason.BACKEND_UNAVAILABLE)
    }
}
