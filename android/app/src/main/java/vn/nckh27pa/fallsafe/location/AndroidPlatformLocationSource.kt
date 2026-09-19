package vn.nckh27pa.fallsafe.location

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.CancellationSignal
import android.util.Log
import androidx.core.content.ContextCompat
import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.android.gms.location.LocationServices
import com.google.android.gms.location.Priority
import com.google.android.gms.tasks.CancellationTokenSource
import com.google.android.gms.tasks.Task
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull
import vn.nckh27pa.fallsafe.emergency.LocationFix
import vn.nckh27pa.fallsafe.emergency.LocationSource
import kotlin.coroutines.resume

class AndroidPlatformLocationSource(private val context: Context) : PlatformLocationSource {
    private val manager = context.getSystemService(LocationManager::class.java)
    private val fused by lazy { LocationServices.getFusedLocationProviderClient(context) }
    private val playServices: Boolean get() {
        val status = GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(context)
        Log.i(TAG, "playServicesAvailability=$status")
        return status == ConnectionResult.SUCCESS
    }
    override fun permission() = when {
        granted(Manifest.permission.ACCESS_FINE_LOCATION) -> LocationPermission.PRECISE
        granted(Manifest.permission.ACCESS_COARSE_LOCATION) -> LocationPermission.APPROXIMATE
        else -> LocationPermission.DENIED
    }
    private fun granted(permission: String) = ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
    private fun enabled(name: String) = try { manager.isProviderEnabled(name) } catch (_: RuntimeException) { false }
    private fun systemLocationEnabled() = try {
        if (Build.VERSION.SDK_INT >= 28) manager.isLocationEnabled
        else enabled(LocationManager.GPS_PROVIDER) || enabled(LocationManager.NETWORK_PROVIDER)
    } catch (_: RuntimeException) { false }
    override fun enabledProviders(): Set<LocationProviderKind> = try {
        LocationProviderAvailability.enabled(
            playServicesAvailable = playServices,
            systemLocationEnabled = systemLocationEnabled(),
            platformFusedEnabled = manager.allProviders.contains(platformFused) && enabled(platformFused),
            precisePermission = permission() == LocationPermission.PRECISE,
            gpsEnabled = enabled(LocationManager.GPS_PROVIDER),
            networkEnabled = enabled(LocationManager.NETWORK_PROVIDER)
        )
    } catch (_: SecurityException) {
        Log.w(TAG, "enabledProviders cause=permission_denied")
        emptySet()
    } catch (_: RuntimeException) {
        Log.w(TAG, "enabledProviders cause=unavailable")
        emptySet()
    }
    override suspend fun lastKnown(): LocationFix? = try {
        val candidates = mutableListOf<LocationFix>()
        // Bound the optional Play Services cache query so platform cache/current work keeps its budget.
        // lastLocation has no cancellation token; late task completion is ignored by the continuation guard.
        if (playServices) withTimeoutOrNull(500) { try { fused.lastLocation.awaitLocation() } catch (_: RuntimeException) { null } }
            ?.fix(LocationSource.CACHED)?.let(candidates::add)
        for (provider in manager.allProviders) {
            if (provider == LocationManager.GPS_PROVIDER && permission() != LocationPermission.PRECISE) continue
            try { manager.getLastKnownLocation(provider)?.fix(LocationSource.CACHED)?.let(candidates::add) }
            catch (_: RuntimeException) { Log.w(TAG, "cached source=$provider cause=unavailable") }
        }
        candidates.maxByOrNull { it.fixTimeMs }
    } catch (_: SecurityException) {
        Log.w(TAG, "cached cause=permission_denied")
        null
    } catch (_: RuntimeException) {
        Log.w(TAG, "cached cause=unavailable")
        null
    }
    override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? {
        val raw = withTimeoutOrNull(timeoutMs) {
            try {
                if (playServices && kind != LocationProviderKind.GPS) {
                    val token = CancellationTokenSource()
                    try {
                        fused.getCurrentLocation(if (kind == LocationProviderKind.FUSED) Priority.PRIORITY_HIGH_ACCURACY
                            else Priority.PRIORITY_BALANCED_POWER_ACCURACY, token.token).awaitLocation()
                    } finally { token.cancel() }
                } else platformCurrent(when (kind) {
                    LocationProviderKind.FUSED -> platformFused
                    LocationProviderKind.GPS -> LocationManager.GPS_PROVIDER
                    LocationProviderKind.NETWORK -> LocationManager.NETWORK_PROVIDER
                })
            } catch (_: RuntimeException) { null }
        }
        val fix = raw?.fix(LocationSource.valueOf(kind.name))
        Log.i(TAG, "source=$kind accuracy=${fix?.accuracyM} age=${fix?.let { System.currentTimeMillis() - it.fixTimeMs }} cause=${if (fix == null) "NO_FIX_OR_TIMEOUT" else "none"}")
        return fix
    }
    private suspend fun Task<Location>.awaitLocation(): Location? = suspendCancellableCoroutine { continuation ->
        addOnSuccessListener { if (continuation.isActive) continuation.resume(it) }
        addOnFailureListener { if (continuation.isActive) continuation.resume(null) }
        addOnCanceledListener { if (continuation.isActive) continuation.resume(null) }
    }
    private suspend fun platformCurrent(provider: String): Location? = suspendCancellableCoroutine { continuation ->
        if (Build.VERSION.SDK_INT >= 30) {
            val signal = CancellationSignal()
            continuation.invokeOnCancellation { signal.cancel() }
            manager.getCurrentLocation(provider, signal, ContextCompat.getMainExecutor(context)) {
                if (continuation.isActive) continuation.resume(it)
            }
        } else {
            val listener = object : LocationListener {
                override fun onLocationChanged(location: Location) {
                    manager.removeUpdates(this)
                    if (continuation.isActive) continuation.resume(location)
                }
            }
            continuation.invokeOnCancellation { try { manager.removeUpdates(listener) } catch (_: RuntimeException) { } }
            @Suppress("DEPRECATION")
            manager.requestSingleUpdate(provider, listener, context.mainLooper)
        }
    }
    private fun Location.fix(source: LocationSource) = LocationFix.validated(latitude, longitude,
        if (hasAccuracy()) accuracy else null, time, source)
    private val platformFused get() = if (Build.VERSION.SDK_INT >= 31) LocationManager.FUSED_PROVIDER else "fused"
    private companion object { const val TAG = "FallSafe/Location" }
}
