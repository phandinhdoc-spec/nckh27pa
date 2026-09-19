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

/**
 * A displayed location failure may only claim [LocationFailureCause.PERMISSION_DENIED] when the
 * platform really reports missing location permission. Any other failure keeps its honest cause so
 * a granted permission can never be shown as denied.
 */
internal fun resolveDisplayedCause(cause: LocationFailureCause?, permissionGranted: Boolean): LocationFailureCause? =
    if (cause == LocationFailureCause.PERMISSION_DENIED && permissionGranted) LocationFailureCause.NO_FIX else cause

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
        fun reportsPermissionDenied() = try { source.permission() == LocationPermission.DENIED } catch (_: Exception) { false }
        var stale: LocationFix? = null
        return try {
            withTimeoutOrNull(budget) {
                if (source.permission() == LocationPermission.DENIED) {
                    // TEMPORARY DIAGNOSTIC
                    vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "permission_denied")
                    vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "none", requestStarted = false, result = "PERMISSION_DENIED", exception = "none")
                    return@withTimeoutOrNull result(cause = LocationFailureCause.PERMISSION_DENIED)
                }
                val enabled = try { source.enabledProviders() } catch (_: Exception) { emptySet() }
                if (enabled.isEmpty()) {
                    // TEMPORARY DIAGNOSTIC
                    vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "provider_disabled")
                    vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "empty", requestStarted = false, result = "PROVIDER_DISABLED", exception = "none")
                    return@withTimeoutOrNull result(cause = LocationFailureCause.PROVIDER_DISABLED)
                }
                // TEMPORARY DIAGNOSTIC
                vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = enabled.joinToString(), requestStarted = true, result = "SEARCHING", exception = "none")
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
                        // TEMPORARY DIAGNOSTIC
                        vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = enabled.joinToString(), requestStarted = true, result = "SUCCESS_$kind", exception = "none")
                        return@withTimeoutOrNull result(it)
                    }
                }
                val finalCause = if (timedOut) LocationFailureCause.TIMEOUT else LocationFailureCause.NO_FIX
                // TEMPORARY DIAGNOSTIC
                vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "no_fix_obtained cause=$finalCause")
                vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = enabled.joinToString(), requestStarted = true, result = finalCause.name, exception = "none")
                stale?.let { result(it, LocationFailureCause.TIMEOUT, true) }
                    ?: result(cause = finalCause)
            } ?: run {
                // TEMPORARY DIAGNOSTIC
                vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "overall_timeout")
                vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "unknown", requestStarted = true, result = "TIMEOUT", exception = "none")
                result(stale, LocationFailureCause.TIMEOUT, stale != null)
            }
        // Android adapters return safe failures; these catches are a second defence for other ports.
        } catch (e: SecurityException) {
            // A stray SecurityException is only a denial when the platform still reports DENIED.
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "SecurityException: ${e.message}")
            vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "unknown", requestStarted = true, result = "EXCEPTION", exception = e.javaClass.simpleName)
            if (reportsPermissionDenied()) result(cause = LocationFailureCause.PERMISSION_DENIED)
            else result(stale, if (stale != null) LocationFailureCause.TIMEOUT else LocationFailureCause.NO_FIX, stale != null)
        } catch (e: Exception) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "Exception: ${e.message}")
            vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "unknown", requestStarted = true, result = "EXCEPTION", exception = e.javaClass.simpleName)
            result(stale, if (stale != null) LocationFailureCause.TIMEOUT else LocationFailureCause.NO_FIX, stale != null)
        }
    }
    private fun LocationFix.validated(source: LocationSource = this.source) =
        LocationFix.validated(latitude, longitude, accuracyM, fixTimeMs, source)
}
