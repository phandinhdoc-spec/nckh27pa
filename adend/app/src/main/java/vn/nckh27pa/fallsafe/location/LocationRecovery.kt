package vn.nckh27pa.fallsafe.location

enum class LocationSignal { FOREGROUND_RESUMED, SYSTEM_LOCATION_CHANGED, EMERGENCY_STARTED }

data class LocationAvailability(val permission: LocationPermission, val providers: Set<LocationProviderKind>) {
    val usable: Boolean get() = permission != LocationPermission.DENIED && providers.isNotEmpty()
}

sealed interface LocationRecoveryDecision {
    data object None : LocationRecoveryDecision
    data object PermissionDenied : LocationRecoveryDecision
    data object ProviderDisabled : LocationRecoveryDecision
    data object Acquire : LocationRecoveryDecision
}

data class LocationRecoveryInput(
    val signal: LocationSignal,
    val availability: LocationAvailability,
    val availabilityChanged: Boolean,
    val requestInFlight: Boolean,
    val hasFix: Boolean,
    val fixIsFresh: Boolean
)

class LocationRecoveryPolicy(
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val cooldownMs: Long = RECOVERY_COOLDOWN_MS
) {
    private var lastAttempt: Long? = null

    fun decide(input: LocationRecoveryInput): LocationRecoveryDecision {
        if (input.requestInFlight) return LocationRecoveryDecision.None
        if (input.availability.permission == LocationPermission.DENIED)
            return if (input.hasFix) LocationRecoveryDecision.None else LocationRecoveryDecision.PermissionDenied
        if (input.availability.providers.isEmpty())
            return if (input.hasFix) LocationRecoveryDecision.None else LocationRecoveryDecision.ProviderDisabled
        val wantsAttempt = input.availabilityChanged || !input.hasFix || !input.fixIsFresh
        if (!wantsAttempt) return LocationRecoveryDecision.None
        if (!input.availabilityChanged) {
            val last = lastAttempt
            if (last == null) return LocationRecoveryDecision.None
            if (nowMs() - last < cooldownMs) return LocationRecoveryDecision.None
        }
        lastAttempt = nowMs()
        return LocationRecoveryDecision.Acquire
    }

    fun markAttempt() { lastAttempt = nowMs() }

    fun reset() { lastAttempt = null }

    companion object { const val RECOVERY_COOLDOWN_MS = 15_000L }
}
