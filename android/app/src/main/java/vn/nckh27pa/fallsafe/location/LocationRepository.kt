package vn.nckh27pa.fallsafe.location

import kotlinx.coroutines.withTimeoutOrNull
import vn.nckh27pa.fallsafe.emergency.*

enum class LocationProviderKind { FUSED, GPS, NETWORK }
enum class LocationPermission { PRECISE, APPROXIMATE, DENIED }

/** Pure provider policy so a Play-services client is never mistaken for an enabled system location service. */
object LocationProviderAvailability {
    fun enabled(
        playServicesAvailable: Boolean,
        systemLocationEnabled: Boolean,
        platformFusedEnabled: Boolean,
        precisePermission: Boolean,
        gpsEnabled: Boolean,
        networkEnabled: Boolean
    ): Set<LocationProviderKind> = buildSet {
        if (systemLocationEnabled && (playServicesAvailable || platformFusedEnabled)) add(LocationProviderKind.FUSED)
        if (precisePermission && gpsEnabled) add(LocationProviderKind.GPS)
        if (networkEnabled) add(LocationProviderKind.NETWORK)
    }
}

interface PlatformLocationSource {
    fun permission(): LocationPermission
    fun enabledProviders(): Set<LocationProviderKind>
    suspend fun lastKnown(): LocationFix?
    suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix?
}

class BestAvailableLocationRepository(
    private val source: PlatformLocationSource,
    private val onCached: ((LocationLookup) -> Unit)? = null,
    private val nowMs: () -> Long = System::currentTimeMillis
) : LocationRepository {
    override suspend fun getBestAvailableLocation(timeoutMs: Long): LocationLookup {
        val started = nowMs()
        val budget = timeoutMs.coerceAtLeast(0)
        fun elapsed() = (nowMs() - started).coerceIn(0, budget)
        fun result(fix: LocationFix? = null, cause: LocationFailureCause? = null, cached: Boolean = false) =
            LocationLookup(fix, cause, cached, elapsed())
        var stale: LocationFix? = null
        return try {
            withTimeoutOrNull(budget) {
                if (source.permission() == LocationPermission.DENIED)
                    return@withTimeoutOrNull result(cause = LocationFailureCause.PERMISSION_DENIED)
                val enabled = try { source.enabledProviders() } catch (_: Exception) { emptySet() }
                if (enabled.isEmpty()) return@withTimeoutOrNull result(cause = LocationFailureCause.PROVIDER_DISABLED)
                stale = try { source.lastKnown()?.validated() } catch (_: Exception) { null }
                stale?.takeIf { it.freshness(nowMs()) == LocationFreshness.FRESH }?.let {
                    val cached = result(it, cached = true)
                    if (onCached == null) return@withTimeoutOrNull cached
                    onCached.invoke(cached)
                }
                var timedOut = false
                for (kind in LocationProviderKind.entries.filter { it in enabled }) {
                    val remaining = budget - elapsed()
                    if (remaining <= 0) { timedOut = true; break }
                    val passBudget = when (kind) {
                        LocationProviderKind.FUSED, LocationProviderKind.GPS -> (remaining / 2).coerceAtLeast(1)
                        LocationProviderKind.NETWORK -> remaining
                    }
                    var completed = false
                    val fix = try {
                        withTimeoutOrNull(passBudget) {
                            source.current(kind, passBudget).also { completed = true }
                        }
                    } catch (_: Exception) { null }
                    if (!completed) timedOut = true
                    fix?.validated(LocationSource.valueOf(kind.name))?.let {
                        return@withTimeoutOrNull result(it)
                    }
                }
                stale?.let { result(it, LocationFailureCause.TIMEOUT, true) }
                    ?: result(cause = if (timedOut) LocationFailureCause.TIMEOUT else LocationFailureCause.NO_FIX)
            } ?: result(stale, LocationFailureCause.TIMEOUT, stale != null)
        // Android adapters return safe failures; these catches are a second defence for other ports.
        } catch (_: SecurityException) {
            result(cause = LocationFailureCause.PERMISSION_DENIED)
        } catch (_: Exception) {
            result(stale, if (stale != null) LocationFailureCause.TIMEOUT else LocationFailureCause.NO_FIX, stale != null)
        }
    }
    private fun LocationFix.validated(source: LocationSource = this.source) =
        LocationFix.validated(latitude, longitude, accuracyM, fixTimeMs, source)
}
