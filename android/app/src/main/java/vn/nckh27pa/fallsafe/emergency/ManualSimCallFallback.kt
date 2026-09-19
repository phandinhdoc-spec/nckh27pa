package vn.nckh27pa.fallsafe.emergency

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import androidx.core.content.ContextCompat
import vn.nckh27pa.fallsafe.permissions.resolveTelephonySupport

data class SimCallResult(val started:Boolean,val detail:String)
fun interface SimCallGateway { fun call(phone:String):SimCallResult }

/** Opens a normal user-visible SIM call. It never claims or attempts automated speech. */
class ManualSimCallFallback(private val context:Context):SimCallGateway {
    override fun call(phone:String):SimCallResult {
        if(!resolveTelephonySupport(
                hasBaseTelephony = context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY),
                hasGranularFeature = context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CALLING)
            )) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "FEATURE_TELEPHONY_CALLING=false")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = ContextCompat.checkSelfPermission(context, Manifest.permission.CALL_PHONE) == PackageManager.PERMISSION_GRANTED, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
            return SimCallResult(false,"Thiết bị không hỗ trợ cuộc gọi SIM")
        }
        if(ContextCompat.checkSelfPermission(context,Manifest.permission.CALL_PHONE)!=PackageManager.PERMISSION_GRANTED) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "checkSelfPermission(CALL_PHONE)!=PERMISSION_GRANTED")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = false, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
            return SimCallResult(false,EmergencyFailureMessages.callPermissionMissing)
        }
        return try{
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logPermission(context)
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = "none")
            context.startActivity(Intent(Intent.ACTION_CALL,Uri.parse("tel:${Uri.encode(phone)}")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = true, exception = "none")
            SimCallResult(true,"Đã mở cuộc gọi SIM thường; không có giọng nói tự động")
        }
        catch(e:ActivityNotFoundException){
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "ActivityNotFoundException: ${e.message}")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = e.javaClass.simpleName)
            SimCallResult(false,"Không có ứng dụng gọi điện")
        }
        catch(e:SecurityException){
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "SecurityException: ${e.message}")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = e.javaClass.simpleName)
            SimCallResult(false,"Hệ thống từ chối quyền gọi điện")
        }
    }
}
