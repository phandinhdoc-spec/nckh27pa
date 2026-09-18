package vn.nckh27pa.fallsafe.location

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.net.Uri
import android.os.Build
import android.os.CancellationSignal
import android.os.Handler
import androidx.core.content.ContextCompat
import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.emergency.*
import java.util.UUID
import java.util.concurrent.Executor

class AndroidEmergencyLocationController(
    private val context: Context,
    private val contacts: () -> List<EmergencyContact>,
    private val sms: EmergencySmsGateway,
    private val onFix: (LocationFix) -> Unit = {},
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val displayName: () -> String = { DEFAULT_USER_DISPLAY_NAME }
) : EmergencyLocationController {
    @Volatile override var locationState: LocationState = LocationState(); private set
    private val manager=context.getSystemService(LocationManager::class.java)
    private val confirmations=mutableMapOf<String,Pair<EmergencyContact,LocationFix>>()
    private val executor=Executor { command -> android.os.Handler(context.mainLooper).post(command) }
    private val handler=Handler(context.mainLooper)
    private var generation=0L
    private var cancellationSignal:CancellationSignal?=null
    private var timeout:Runnable?=null
    private var legacyListener:LocationListener?=null

    override fun onVerifyingStarted() {
        generation++
        cancelActiveRequest()
        if(ContextCompat.checkSelfPermission(context,Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED &&
            ContextCompat.checkSelfPermission(context,Manifest.permission.ACCESS_COARSE_LOCATION)!=PackageManager.PERMISSION_GRANTED) {
            locationState=LocationState(cause=LocationFailureCause.PERMISSION_DENIED,explanation="Ứng dụng chưa được cấp quyền vị trí.",remediation="Mở Cài đặt và cấp quyền vị trí cho FallSafe.");return
        }
        val requestGeneration=generation
        val attempt=BoundedLocationResolutionAttempt(::lastKnownFix) { resolution ->
            if(requestGeneration!=generation)return@BoundedLocationResolutionAttempt
            cancelActiveRequest()
            publish(resolution)
        }
        val provider=try {
            when {
                manager.isProviderEnabled(LocationManager.GPS_PROVIDER)->LocationManager.GPS_PROVIDER
                manager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)->LocationManager.NETWORK_PROVIDER
                else->null
            }
        } catch (_:RuntimeException) { null }
        if(provider==null){attempt.fail(LocationFailureCause.PROVIDER_DISABLED);return}
        locationState=LocationState(cause=LocationFailureCause.NO_FIX,explanation="Đang chờ tín hiệu vị trí.",remediation="Di chuyển ra nơi thoáng; cảnh báo vẫn tiếp tục.")
        val timeoutTask=Runnable { if(requestGeneration==generation) attempt.fail(LocationFailureCause.TIMEOUT) }
        timeout=timeoutTask
        handler.postDelayed(timeoutTask,CURRENT_FIX_TIMEOUT_MS)
        try {
            if(Build.VERSION.SDK_INT>=30) {
                val signal=CancellationSignal();cancellationSignal=signal
                manager.getCurrentLocation(provider,signal,executor) { location -> acceptPhoneLocation(location,attempt) }
            } else {
                val listener=LocationListener { location -> acceptPhoneLocation(location,attempt) }
                legacyListener=listener
                @Suppress("DEPRECATION") manager.requestSingleUpdate(provider,listener,context.mainLooper)
            }
        } catch (_:SecurityException) {
            attempt.fail(LocationFailureCause.PERMISSION_DENIED)
        } catch (_:RuntimeException) {
            attempt.fail(LocationFailureCause.NO_FIX)
        }
    }
    private fun acceptPhoneLocation(location:Location?,attempt:BoundedLocationResolutionAttempt) {
        val fix=location?.let{LocationFix.validated(it.latitude,it.longitude,it.accuracy.takeIf{a->a.isFinite()&&a>=0},it.time,LocationSource.PHONE)}
        if(fix==null)attempt.fail(if(location==null)LocationFailureCause.NO_FIX else LocationFailureCause.INVALID_FIX)
        else attempt.current(fix)
    }
    private fun publish(resolution:LocationResolution) {
        val fix=resolution.fix
        if(fix!=null){
            val fallback=resolution.cause!=null
            locationState=LocationState(
                fix,fix.freshness(nowMs()),resolution.cause,
                if(fallback)"Đang dùng vị trí hợp lệ gần nhất; cảnh báo vẫn tiếp tục." else "Đã có vị trí mới.",
                if(fallback)remediation(resolution.cause!!) else null
            )
            onFix(fix)
        } else {
            locationState=LocationState(cause=resolution.cause,explanation=explanation(resolution.cause),remediation=remediation(resolution.cause))
        }
    }
    private fun lastKnownFix():LocationFix? = try {
        manager.allProviders.mapNotNull { provider ->
            try {
                manager.getLastKnownLocation(provider)?.let { location ->
                    LocationFix.validated(location.latitude,location.longitude,location.accuracy.takeIf{it.isFinite()&&it>=0},location.time,LocationSource.PHONE)
                }
            } catch (_:RuntimeException) { null }
        }.maxByOrNull { it.fixTimeMs }
    } catch (_:RuntimeException) { null }
    private fun cancelActiveRequest() {
        timeout?.let(handler::removeCallbacks);timeout=null
        cancellationSignal?.cancel();cancellationSignal=null
        legacyListener?.let { try{manager.removeUpdates(it)}catch(_:RuntimeException){} };legacyListener=null
    }
    private fun explanation(cause:LocationFailureCause?)=when(cause){
        LocationFailureCause.PERMISSION_DENIED->"Ứng dụng chưa được cấp quyền vị trí."
        LocationFailureCause.PROVIDER_DISABLED->"Dịch vụ vị trí đang tắt; cảnh báo vẫn tiếp tục."
        LocationFailureCause.TIMEOUT->"Không lấy được vị trí trong thời gian chờ; cảnh báo vẫn tiếp tục."
        LocationFailureCause.INVALID_FIX->"Thiết bị trả về vị trí không hợp lệ; cảnh báo vẫn tiếp tục."
        else->"Chưa nhận được vị trí; cảnh báo vẫn tiếp tục."
    }
    private fun remediation(cause:LocationFailureCause?)=when(cause){
        LocationFailureCause.PERMISSION_DENIED->"Cấp quyền vị trí sau khi xem giải thích trong ứng dụng."
        LocationFailureCause.PROVIDER_DISABLED->"Bật Vị trí/GPS trong cài đặt hệ thống."
        else->"Bật GPS và thử ở nơi thoáng."
    }
    private fun update(fix:LocationFix){locationState=LocationState(fix,fix.freshness(nowMs()),null,if(fix.freshness(nowMs())==LocationFreshness.FRESH)"Đã có vị trí mới." else "Vị trí đã cũ.",if(fix.freshness(nowMs())==LocationFreshness.STALE)"Chờ GPS cập nhật ở nơi thoáng." else null);onFix(fix)}
    override fun acceptEsp32Gnss(fix:LocationFix){if(fix.source==LocationSource.ESP32_GNSS)update(fix)}
    override fun openMyLocation():Boolean {
        val fix=locationState.fix?:return false
        val geo=Intent(Intent.ACTION_VIEW,Uri.parse("geo:${fix.latitude},${fix.longitude}?q=${fix.latitude},${fix.longitude}")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        return try{context.startActivity(geo);true}catch(_:ActivityNotFoundException){try{context.startActivity(Intent(Intent.ACTION_VIEW,Uri.parse(fix.mapsUrl)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));true}catch(_:ActivityNotFoundException){false}}
    }
    override fun requestManualShare(contactId:String):ManualShareConfirmation? {
        val contact=contacts().firstOrNull{it.id==contactId}?:return null;val fix=locationState.fix?:return null
        val token=UUID.randomUUID().toString();val preview=EmergencyMessageFormatter.manualLocation(displayName(),fix);confirmations[token]=contact to fix
        return ManualShareConfirmation(token,contactId,preview)
    }
    override fun confirmManualShare(token:String):SmsDispatchState {
        val pair=confirmations.remove(token)?:return SmsDispatchState("manual", "", SmsDeliveryStatus.FAILED,"Xác nhận không hợp lệ hoặc đã dùng")
        val event="manual-${UUID.randomUUID()}";return sms.send(SmsRequest(event,pair.first.id,pair.first.phone,EmergencyMessageFormatter.manualLocation(displayName(),pair.second)))
    }
    private companion object { const val CURRENT_FIX_TIMEOUT_MS=8_000L }
}
