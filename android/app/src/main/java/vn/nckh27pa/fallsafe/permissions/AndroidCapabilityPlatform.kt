package vn.nckh27pa.fallsafe.permissions

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.Settings
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class SharedPreferencesCapabilityAttemptStore(context: Context) : CapabilityAttemptStore {
    private val preferences = context.getSharedPreferences("fallsafe_capability_attempts", Context.MODE_PRIVATE)
    override fun wasAttempted(capability: Capability) = preferences.getBoolean(capability.name, false)
    override fun markAttempted(capability: Capability) {
        preferences.edit().putBoolean(capability.name, true).apply()
    }
}

class SharedPreferencesPermissionSetupStore(context: Context) : PermissionSetupStore {
    private val preferences = context.getSharedPreferences("fallsafe_permission_setup", Context.MODE_PRIVATE)
    override var seen: Boolean
        get() = preferences.getBoolean("seen", false)
        set(value) { preferences.edit().putBoolean("seen", value).apply() }
}

class AndroidCapabilityPlatform(
    private val activity: Activity,
    private val launchRequest: (Capability) -> Unit
) : CapabilityPlatform {
    override fun isGranted(capability: Capability): Boolean = when (capability) {
        Capability.CALLING -> granted(Manifest.permission.CALL_PHONE)
        Capability.MESSAGING -> granted(Manifest.permission.SEND_SMS)
        Capability.LOCATION -> granted(Manifest.permission.ACCESS_FINE_LOCATION) || granted(Manifest.permission.ACCESS_COARSE_LOCATION)
    }

    override fun locationPrecision(): LocationPrecision = when {
        granted(Manifest.permission.ACCESS_FINE_LOCATION) -> LocationPrecision.PRECISE
        granted(Manifest.permission.ACCESS_COARSE_LOCATION) -> LocationPrecision.APPROXIMATE
        else -> LocationPrecision.NONE
    }

    override fun hasPhoneStateAccess() = granted(Manifest.permission.READ_PHONE_STATE)

    override fun shouldShowRationale(capability: Capability): Boolean = permissions(capability)
        .filterNot(::granted)
        .any { ActivityCompat.shouldShowRequestPermissionRationale(activity, it) }

    override fun request(capability: Capability) = launchRequest(capability)

    override fun openSettings(capability: Capability) {
        activity.startActivity(
            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                .setData(Uri.fromParts("package", activity.packageName, null))
        )
    }

    private fun granted(permission: String) =
        ContextCompat.checkSelfPermission(activity, permission) == PackageManager.PERMISSION_GRANTED

    private fun permissions(capability: Capability): List<String> = when (capability) {
        Capability.CALLING -> listOf(Manifest.permission.CALL_PHONE)
        Capability.MESSAGING -> listOf(Manifest.permission.SEND_SMS)
        Capability.LOCATION -> listOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
    }
}
