package vn.nckh27pa.fallsafe.location

import android.content.Context
import android.database.ContentObserver
import android.os.Handler
import android.os.Looper
import android.provider.Settings

/**
 * Best-effort ContentObserver that fires [onChange] on the main looper when the system location
 * setting changes. Deterministic recovery is the resume re-check; this is only an enhancement for
 * Quick Settings toggles that do not resume the activity.
 */
class SystemLocationAvailabilityObserver(
    context: Context,
    private val onChange: () -> Unit
) {
    private val contentResolver = context.contentResolver
    private val observer = object : ContentObserver(Handler(Looper.getMainLooper())) {
        override fun onChange(selfChange: Boolean) { onChange() }
    }
    @Volatile private var registered = false

    fun register(): Boolean {
        if (registered) return false
        var any = false
        try {
            contentResolver.registerContentObserver(
                Settings.Secure.getUriFor(Settings.Secure.LOCATION_PROVIDERS_ALLOWED), false, observer
            )
            any = true
        } catch (_: Exception) { }
        try {
            contentResolver.registerContentObserver(
                Settings.Secure.getUriFor("location_mode"), false, observer
            )
            any = true
        } catch (_: Exception) { }
        registered = any
        return any
    }

    fun close() {
        if (!registered) return
        try { contentResolver.unregisterContentObserver(observer) } catch (_: Exception) { }
        registered = false
    }
}
