package vn.nckh27pa.fallsafe.emergency

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import androidx.core.content.ContextCompat

data class SimCallResult(val started:Boolean,val detail:String)
fun interface SimCallGateway { fun call(phone:String):SimCallResult }

/** Opens a normal user-visible SIM call. It never claims or attempts automated speech. */
class ManualSimCallFallback(private val context:Context):SimCallGateway {
    override fun call(phone:String):SimCallResult {
        if(!context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CALLING))return SimCallResult(false,"Thiết bị không hỗ trợ cuộc gọi SIM")
        if(ContextCompat.checkSelfPermission(context,Manifest.permission.CALL_PHONE)!=PackageManager.PERMISSION_GRANTED)return SimCallResult(false,EmergencyFailureMessages.callPermissionMissing)
        return try{context.startActivity(Intent(Intent.ACTION_CALL,Uri.parse("tel:${Uri.encode(phone)}")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));SimCallResult(true,"Đã mở cuộc gọi SIM thường; không có giọng nói tự động")}
        catch(_:ActivityNotFoundException){SimCallResult(false,"Không có ứng dụng gọi điện")}
        catch(_:SecurityException){SimCallResult(false,"Hệ thống từ chối quyền gọi điện")}
    }
}
