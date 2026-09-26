package vn.nckh27pa.fallsafe

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.util.Log
import androidx.core.content.ContextCompat

// TEMPORARY DIAGNOSTIC HELPER — Remove after diagnosing real-device behavior
object AndroidTrace {
    const val TAG = "FallSafe/AndroidTrace"

    fun log(msg: String) {
        Log.i(TAG, msg)
    }

    fun maskPhone(phone: String?): String {
        if (phone.isNullOrBlank()) return "empty"
        val trimmed = phone.trim()
        if (trimmed.length <= 3) return "***(len=${trimmed.length})"
        return "***${trimmed.takeLast(3)}(len=${trimmed.length})"
    }

    fun logPermission(context: Context) {
        val fine = ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        val coarse = ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED
        val sendSms = ContextCompat.checkSelfPermission(context, Manifest.permission.SEND_SMS) == PackageManager.PERMISSION_GRANTED
        val callPhone = ContextCompat.checkSelfPermission(context, Manifest.permission.CALL_PHONE) == PackageManager.PERMISSION_GRANTED
        val readPhoneState = ContextCompat.checkSelfPermission(context, Manifest.permission.READ_PHONE_STATE) == PackageManager.PERMISSION_GRANTED
        Log.i(TAG, "PERMISSION FINE=$fine COARSE=$coarse SEND_SMS=$sendSms CALL_PHONE=$callPhone READ_PHONE_STATE=$readPhoneState")
    }

    fun logCapabilities(
        locationCapability: String,
        smsCapability: String,
        callCapability: String,
        snapshotCalling: Boolean,
        snapshotMessaging: Boolean,
        snapshotLocation: Boolean,
        locationPrecision: String
    ) {
        Log.i(TAG, "CAPABILITIES locationCapability=$locationCapability smsCapability=$smsCapability callCapability=$callCapability snapshotCalling=$snapshotCalling snapshotMessaging=$snapshotMessaging snapshotLocation=$snapshotLocation locationPrecision=$locationPrecision")
    }

    fun logLocation(
        entered: Boolean,
        providerEnabled: String,
        requestStarted: Boolean,
        result: String,
        exception: String
    ) {
        Log.i(TAG, "LOCATION entered=$entered providerEnabled=$providerEnabled requestStarted=$requestStarted result=$result exception=$exception")
    }

    fun logSms(
        entered: Boolean,
        contactExists: Boolean,
        phonePresent: Boolean,
        permission: Boolean,
        sendTextMessageReached: Boolean,
        exception: String
    ) {
        Log.i(TAG, "SMS entered=$entered contactExists=$contactExists phonePresent=$phonePresent permission=$permission sendTextMessageReached=$sendTextMessageReached exception=$exception")
    }

    fun logCall(
        entered: Boolean,
        contactExists: Boolean,
        phonePresent: Boolean,
        permission: Boolean,
        intentCreated: Boolean,
        startActivityReached: Boolean,
        startActivityReturned: Boolean,
        exception: String
    ) {
        Log.i(TAG, "CALL entered=$entered contactExists=$contactExists phonePresent=$phonePresent permission=$permission intentCreated=$intentCreated startActivityReached=$startActivityReached startActivityReturned=$startActivityReturned exception=$exception")
    }

    fun logBlocked(component: String, reason: String) {
        Log.i(TAG, "BLOCKED component=$component reason=$reason")
    }
}
