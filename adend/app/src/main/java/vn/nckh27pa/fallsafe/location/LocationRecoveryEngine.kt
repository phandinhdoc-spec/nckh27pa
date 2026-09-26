package vn.nckh27pa.fallsafe.location

import vn.nckh27pa.fallsafe.emergency.LocationLookup
import vn.nckh27pa.fallsafe.emergency.SOS_LOCATION_TIMEOUT_MS

/**
 * Pure (no Android/Compose/Main-dispatcher) recovery orchestrator: samples the current location
 * availability, tracks whether it changed since the previous signal, and runs the bounded
 * best-available lookup only when the policy decides to acquire. The caller (the controller) owns
 * the coroutine that hosts the lookup and its single in-flight source of truth.
 */
class LocationRecoveryEngine(
    private val source: PlatformLocationSource,
    private val policy: LocationRecoveryPolicy,
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val timeoutMs: Long = SOS_LOCATION_TIMEOUT_MS
) {
    data class Evaluation(
        val decision: LocationRecoveryDecision,
        val availability: LocationAvailability,
        val availabilityChanged: Boolean
    )

    private var lastAvailability: LocationAvailability? = null

    fun availability(): LocationAvailability =
        LocationAvailability(source.permission(), source.enabledProviders())

    fun evaluate(
        signal: LocationSignal,
        requestInFlight: Boolean,
        hasFix: Boolean,
        fixIsFresh: Boolean
    ): Evaluation {
        val availability = availability()
        val changed = lastAvailability != availability
        lastAvailability = availability
        val decision = policy.decide(
            LocationRecoveryInput(signal, availability, changed, requestInFlight, hasFix, fixIsFresh)
        )
        return Evaluation(decision, availability, changed)
    }

    suspend fun lookup(onCached: ((LocationLookup) -> Unit)? = null): LocationLookup =
        BestAvailableLocationRepository(source, onCached = onCached, nowMs = nowMs)
            .getBestAvailableLocation(timeoutMs)

    fun reset() {
        lastAvailability = null
        policy.reset()
    }
}
