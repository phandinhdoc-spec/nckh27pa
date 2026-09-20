package vn.nckh27pa.fallsafe.location

import android.content.ActivityNotFoundException
import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import android.content.Intent
import android.net.Uri
import android.util.Log
import kotlinx.coroutines.*
import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.emergency.*
import java.util.UUID

class AndroidEmergencyLocationController(
    private val context: Context,
    private val contacts: () -> List<EmergencyContact>,
    private val sms: EmergencySmsGateway,
    private val onFix: (LocationFix) -> Unit = {},
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val displayName: () -> String = { DEFAULT_USER_DISPLAY_NAME },
    private val source: PlatformLocationSource = AndroidPlatformLocationSource(context)
) : EmergencyLocationController {
    override var locationState: LocationState by mutableStateOf(LocationState()); private set
    @Volatile override var lastMapOpenReason:String?=null; private set

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val confirmations=mutableMapOf<String,Pair<EmergencyContact,LocationFix>>()
    private var generation=0L
    private var activeRequest: Job? = null
    private val policy = LocationRecoveryPolicy(nowMs)
    private val recovery = LocationRecoveryEngine(source, policy, nowMs)

    override fun refreshPermissionTruth() = recheck(LocationSignal.FOREGROUND_RESUMED)
    fun onSystemLocationChanged() = recheck(LocationSignal.SYSTEM_LOCATION_CHANGED)
    fun close() {
        activeRequest?.cancel()
        recovery.reset()
    }
    private fun recheck(signal: LocationSignal) {
        val current = locationState
        val evaluation = recovery.evaluate(
            signal,
            requestInFlight = activeRequest?.isActive == true,
            hasFix = current.fix != null,
            fixIsFresh = current.freshness == LocationFreshness.FRESH
        )
        Log.i(TAG, "recovery signal=$signal availability=${evaluation.availability} changed=${evaluation.availabilityChanged} decision=${evaluation.decision}")
        when (evaluation.decision) {
            LocationRecoveryDecision.PermissionDenied -> locationState = LocationState(
                cause = LocationFailureCause.PERMISSION_DENIED,
                explanation = explanation(LocationFailureCause.PERMISSION_DENIED),
                remediation = remediation(LocationFailureCause.PERMISSION_DENIED)
            )
            LocationRecoveryDecision.ProviderDisabled -> locationState = LocationState(
                cause = LocationFailureCause.PROVIDER_DISABLED,
                explanation = explanation(LocationFailureCause.PROVIDER_DISABLED),
                remediation = remediation(LocationFailureCause.PROVIDER_DISABLED)
            )
            LocationRecoveryDecision.Acquire -> launchLookup()
            LocationRecoveryDecision.None -> {}
        }
    }

    override fun onVerifyingStarted() {
        policy.markAttempt()
        locationState = LocationState()
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "checking", requestStarted = true, result = "STARTED", exception = "none")
        launchLookup()
    }
    private fun launchLookup() {
        val requestGeneration = ++generation
        activeRequest?.cancel()
        activeRequest = scope.launch {
            var publishedFix: LocationFix? = null
            val lookup = recovery.lookup(onCached = { cached ->
                if (requestGeneration == generation && isActive) {
                    publishedFix = cached.fix
                    publish(cached)
                }
            })
            if (requestGeneration != generation || !isActive) return@launch
            if (lookup.fix !== publishedFix || publishedFix == null) publish(lookup)
        }
    }
    private fun publish(lookup: LocationLookup) {
        val fix = lookup.fix
        val cause = resolveDisplayedCause(lookup.cause, source.permission() != LocationPermission.DENIED)
        // TEMPORARY DIAGNOSTIC
        if (fix == null) {
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("LOCATION", "fix_null_cause=${cause?.name}")
        }
        vn.nckh27pa.fallsafe.AndroidTrace.logLocation(entered = true, providerEnabled = "published", requestStarted = true, result = fix?.let { "FIX_${it.source.name}" } ?: (cause?.name ?: "NO_FIX"), exception = "none")
        locationState = if (fix != null) LocationState(
            fix, fix.freshness(nowMs()), cause,
            if (lookup.fromCache) "Đang dùng vị trí hợp lệ gần nhất; cảnh báo vẫn tiếp tục." else "Đã có vị trí mới.",
            if (cause != null) remediation(cause) else null
        ) else LocationState(cause = cause, explanation = explanation(cause), remediation = remediation(cause))
        Log.i("FallSafe/Location", "source=${fix?.source} accuracy=${fix?.accuracyM} age=${fix?.let { nowMs() - it.fixTimeMs }} cause=$cause")
        fix?.let(onFix)
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
        else->"Ra nơi thoáng hơn có thể tăng độ chính xác; cảnh báo vẫn được gửi."
    }
    private fun update(fix:LocationFix){locationState=LocationState(fix,fix.freshness(nowMs()),null,if(fix.freshness(nowMs())==LocationFreshness.FRESH)"Đã có vị trí mới." else "Vị trí đã cũ.",if(fix.freshness(nowMs())==LocationFreshness.STALE)"Ra nơi thoáng hơn có thể tăng độ chính xác; cảnh báo vẫn được gửi." else null);onFix(fix)}
    override fun acceptEsp32Gnss(fix:LocationFix){if(fix.source==LocationSource.ESP32_GNSS)update(fix)}
    override fun openMyLocation():Boolean {
        val fix=locationState.fix?:run{lastMapOpenReason="Chưa có vị trí để mở bản đồ.";return false}
        var failure:String?=null
        val geoUri=Uri.parse(fix.geoUri)
        val resolver=MapLaunchResolver(
            launcher=MapTargetLauncher { target ->
                val intent=when(target) {
                    MapTarget.GOOGLE_MAPS->Intent(Intent.ACTION_VIEW,geoUri).setPackage(GOOGLE_MAPS_PACKAGE)
                    MapTarget.GENERIC_MAPS->Intent(Intent.ACTION_VIEW,geoUri)
                    MapTarget.BROWSER->Intent(Intent.ACTION_VIEW,Uri.parse(fix.mapsUrl))
                }.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                try{context.startActivity(intent);true}
                catch(_:ActivityNotFoundException){failure="Không có ứng dụng phù hợp để mở bản đồ.";false}
                catch(_:SecurityException){failure="Hệ thống từ chối mở bản đồ.";false}
                catch(_:RuntimeException){failure="Không thể mở bản đồ lúc này.";false}
            },
            failureReason={failure}
        )
        val result=resolver.open(fix.latitude,fix.longitude,fix.mapsUrl)
        lastMapOpenReason=result.reason
        return result.opened
    }
    override fun requestManualShare(contactId:String):ManualShareConfirmation? {
        val contact=contacts().firstOrNull{it.id==contactId}?:return null;val fix=locationState.fix?:return null
        val token=UUID.randomUUID().toString();val preview=EmergencyMessageFormatter.manualLocation(displayName(),fix);confirmations[token]=contact to fix
        return ManualShareConfirmation(token,contactId,preview)
    }
    override fun confirmManualShare(token:String):SmsDispatchState {
        val pair=confirmations.remove(token)?:return SmsDispatchState("manual", "", SmsDeliveryStatus.FAILED,"Xác nhận không hợp lệ hoặc đã dùng")
        val event="manual-${UUID.randomUUID()}"
        return try{sms.send(SmsRequest(event,pair.first.id,pair.first.phone,EmergencyMessageFormatter.manualLocation(displayName(),pair.second)))}
        catch(_:SecurityException){SmsDispatchState(event,pair.first.id,SmsDeliveryStatus.FAILED,EmergencyFailureMessages.messagingPermissionMissing)}
        catch(_:RuntimeException){SmsDispatchState(event,pair.first.id,SmsDeliveryStatus.FAILED,"Không thể gửi vị trí lúc này.")}
    }
    private companion object {
        const val GOOGLE_MAPS_PACKAGE="com.google.android.apps.maps"
        const val TAG = "FallSafe/Location"
    }
}
