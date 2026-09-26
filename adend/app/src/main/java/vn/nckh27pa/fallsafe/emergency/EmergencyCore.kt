package vn.nckh27pa.fallsafe.emergency

import vn.nckh27pa.fallsafe.EmergencyContact
import vn.nckh27pa.fallsafe.ContactValidator
import vn.nckh27pa.fallsafe.permissions.CapabilitySnapshot
import vn.nckh27pa.fallsafe.permissions.CapabilitySnapshotProvider
import vn.nckh27pa.fallsafe.permissions.LocationPrecision
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.UUID

const val LOCATION_FRESH_MS = 120_000L

/**
 * Where a fix came from. PHONE/ESP32_GNSS keep their original meaning; the rest were added with the
 * best-available location pipeline. No satellite fix is required for any source to be usable.
 */
enum class LocationSource { PHONE, ESP32_GNSS, FUSED, GPS, NETWORK, CACHED, UNKNOWN }

/** Bounded budget for [LocationRepository.getBestAvailableLocation]; never blocks SOS longer than this. */
const val SOS_LOCATION_TIMEOUT_MS = 8_000L
enum class LocationFreshness { FRESH, STALE, UNAVAILABLE }
enum class LocationFailureCause { PERMISSION_DENIED, PROVIDER_DISABLED, TIMEOUT, NO_FIX, INVALID_FIX }

class LocationFix private constructor(
    val latitude: Double,
    val longitude: Double,
    val accuracyM: Float?,
    val fixTimeMs: Long,
    val source: LocationSource
) {
    /** Single source of truth for the shared location link (SMS, backend, UI). */
    val mapsUrl: String get() = "https://maps.google.com/?q=$latitude,$longitude"
    /** Single source of truth for the platform geo: URI used when opening a map app. */
    val geoUri: String get() = "geo:$latitude,$longitude?q=$latitude,$longitude"
    fun freshness(nowMs: Long): LocationFreshness =
        if (nowMs >= fixTimeMs && nowMs - fixTimeMs <= LOCATION_FRESH_MS) LocationFreshness.FRESH else LocationFreshness.STALE
    companion object {
        fun validated(latitude: Double, longitude: Double, accuracyM: Float?, fixTimeMs: Long, source: LocationSource): LocationFix? {
            if (!latitude.isFinite() || !longitude.isFinite() || latitude !in -90.0..90.0 || longitude !in -180.0..180.0) return null
            if (latitude == 0.0 && longitude == 0.0) return null
            if (fixTimeMs < 0 || (accuracyM != null && (!accuracyM.isFinite() || accuracyM < 0f))) return null
            return LocationFix(latitude, longitude, accuracyM, fixTimeMs, source)
        }
    }
}

/**
 * Outcome of a bounded best-available-location lookup. Always completes; never throws.
 * [cause] is null when the returned fix is fresh and current; otherwise it explains why the
 * fallback/absence happened while [fix] may still be usable.
 */
data class LocationLookup(
    val fix: LocationFix?,
    val cause: LocationFailureCause?,
    val fromCache: Boolean,
    val elapsedMs: Long
)

/**
 * Single entry point for SOS location. Implementations MUST combine cached/last-known, fused,
 * Wi-Fi/cell and GNSS sources and MUST NOT require a satellite fix to return a usable fix.
 */
interface LocationRepository {
    suspend fun getBestAvailableLocation(timeoutMs: Long = SOS_LOCATION_TIMEOUT_MS): LocationLookup
}

data class LocationState(
    val fix: LocationFix? = null,
    val freshness: LocationFreshness = LocationFreshness.UNAVAILABLE,
    val cause: LocationFailureCause? = LocationFailureCause.NO_FIX,
    val explanation: String = "Đang xác định vị trí...",
    val remediation: String? = null
)

data class ManualShareConfirmation(val token: String, val contactId: String, val preview: String)

interface EmergencyLocationController {
    val locationState: LocationState
    val lastMapOpenReason: String? get() = null
    fun refreshPermissionTruth()
    fun onVerifyingStarted()
    fun acceptEsp32Gnss(fix: LocationFix)
    fun openMyLocation(): Boolean
    fun requestManualShare(contactId: String): ManualShareConfirmation?
    fun confirmManualShare(token: String): SmsDispatchState
}

enum class SmsDeliveryStatus { QUEUED, SENDING, SENT, DELIVERED, FAILED }
data class SmsRequest(val eventId: String, val contactId: String, val phone: String, val message: String, val subscriptionId: Int? = null, val supplement: Boolean = false)
data class SmsDispatchState(val eventId: String, val contactId: String, val status: SmsDeliveryStatus, val detail: String? = null)
interface EmergencySmsGateway { fun send(request: SmsRequest): SmsDispatchState }
enum class VoiceDispatchStatus { CONFIGURED, STARTED, UNAVAILABLE, DISABLED }
fun interface EmergencyBackendGateway { fun startVoice(eventId: String): VoiceDispatchStatus }

enum class CallStatus { STARTED, PERMISSION_MISSING, UNAVAILABLE, FAILED }
data class CallDispatchState(val status: CallStatus, val detail: String? = null)

/** Direct handset call path (ACTION_CALL). Independent of the backend voice adapter. */
fun interface EmergencyCallGateway { fun call(phone: String): CallDispatchState }

enum class SosStep(val vietnameseLabel: String) {
    LOCATION("Vị trí"),
    SMS("Tin nhắn"),
    VOICE_CALL("Cuộc gọi trợ giúp"),
    SIM_CALL("Cuộc gọi SIM"),
    MAP_LINK("Liên kết bản đồ")
}
enum class SosStepStatus { SUCCESS, PARTIAL, PERMISSION_MISSING, UNAVAILABLE, FAILED, SKIPPED }
data class SosStepResult(val step: SosStep, val status: SosStepStatus, val detail: String? = null)
data class SosDispatchReport(val eventId: String, val steps: List<SosStepResult>) {
    fun statusOf(step: SosStep): SosStepStatus? = steps.firstOrNull { it.step == step }?.status
    val failedSteps: List<SosStepResult>
        get() = steps.filter { it.status !in setOf(SosStepStatus.SUCCESS, SosStepStatus.SKIPPED) }
    val allSucceeded: Boolean
        get() = SosStep.entries.all { statusOf(it) in setOf(SosStepStatus.SUCCESS, SosStepStatus.SKIPPED) }
    fun labelOf(step: SosStep): String = step.vietnameseLabel
}

object EmergencyFailureMessages {
    const val callPermissionMissing = "Chưa cho phép gọi điện; không thể tự động gọi. Hãy bật quyền trong phần thiết lập."
    const val messagingPermissionMissing = "Chưa cho phép gửi tin nhắn; hãy bật quyền này trong phần thiết lập."
    const val simAccessMissing = "Chưa cho phép kiểm tra SIM; hãy bật quyền điện thoại trong phần thiết lập."
}

object EmergencyMessageFormatter {
    private fun sourceLabel(source: LocationSource) = when (source) {
        LocationSource.ESP32_GNSS -> "ESP32 GNSS"
        LocationSource.UNKNOWN -> "không xác định"
        else -> "điện thoại"
    }
    private fun time(value: Long): String = SimpleDateFormat("yyyy-MM-dd HH:mm:ss 'UTC'", Locale.ROOT).apply { timeZone=TimeZone.getTimeZone("UTC") }.format(Date(value))
    fun emergency(displayName: String?, eventTimeMs: Long, fix: LocationFix?, nowMs: Long): String {
        val name=displayName?.trim().takeUnless { it.isNullOrEmpty() } ?: "Người dùng FallSafe"
        val header="CẢNH BÁO SOS: Tôi có thể đã bị ngã và cần hỗ trợ. Người cần trợ giúp: $name. Thời điểm sự kiện: ${time(eventTimeMs)}."
        if (fix==null) return "$header Hiện chưa xác định được vị trí chính xác. Vui lòng gọi lại hoặc kiểm tra ứng dụng theo dõi."
        val source=sourceLabel(fix.source)
        val fixDescription=if(fix.freshness(nowMs)==LocationFreshness.STALE)
            " Vị trí gần nhất, cập nhật lúc ${time(fix.fixTimeMs)}." else " Thời điểm fix: ${time(fix.fixTimeMs)}."
        val accuracy=fix.accuracyM?.let { " Độ chính xác: ${it.toInt()} m." } ?: ""
        return "$header Tọa độ: ${fix.latitude},${fix.longitude}. ${fix.mapsUrl}.$fixDescription$accuracy Nguồn vị trí: $source."
    }
    fun locationSupplement(displayName: String?, fix: LocationFix, nowMs: Long): String {
        val source=sourceLabel(fix.source)
        val accuracy=fix.accuracyM?.let { " Độ chính xác: ${it.toInt()} m." } ?: ""
        return "Bổ sung vị trí cho cảnh báo của ${displayName?.trim().takeUnless { it.isNullOrEmpty() } ?: "Người dùng FallSafe"}: ${fix.latitude},${fix.longitude}. ${fix.mapsUrl}. Thời điểm fix: ${time(fix.fixTimeMs)}.$accuracy Nguồn vị trí: $source."
    }
    fun manualLocation(displayName: String?, fix: LocationFix): String =
        "${displayName?.trim().takeUnless { it.isNullOrEmpty() } ?: "Người dùng FallSafe"} chia sẻ vị trí hiện tại: ${fix.latitude},${fix.longitude}. ${fix.mapsUrl}"
}

data class SmsAggregationResult(val status: SmsDeliveryStatus, val detail: String? = null)

/** Separates network submission (sentIntent) from handset delivery reports. */
class SmsPartAggregation(private val total: Int) {
    init { require(total > 0) }
    private val sent = mutableSetOf<Int>()
    private val sentFailed = mutableSetOf<Int>()
    private val delivered = mutableSetOf<Int>()
    private val deliveryFailed = mutableSetOf<Int>()
    @Synchronized fun recordSent(index: Int, successful: Boolean): SmsAggregationResult {
        if (index !in 0 until total) return result()
        if (successful) sent += index else sentFailed += index
        return result()
    }
    @Synchronized fun recordDelivery(index: Int, successful: Boolean): SmsAggregationResult {
        if (index !in 0 until total) return result()
        if (successful) { delivered += index; deliveryFailed -= index }
        else if (index !in delivered) deliveryFailed += index
        return result()
    }
    @Synchronized fun result(): SmsAggregationResult = when {
        sentFailed.isNotEmpty() -> SmsAggregationResult(SmsDeliveryStatus.FAILED,"Nhà mạng hoặc thiết bị báo gửi thất bại")
        delivered.size == total -> SmsAggregationResult(SmsDeliveryStatus.DELIVERED)
        sent.size == total -> SmsAggregationResult(SmsDeliveryStatus.SENT,
            if(deliveryFailed.isNotEmpty())"Đã gửi tới mạng; báo cáo giao SMS thất bại hoặc không được hỗ trợ" else null)
        else -> SmsAggregationResult(SmsDeliveryStatus.SENDING,
            if(deliveryFailed.isNotEmpty())"Báo cáo giao SMS thất bại hoặc không được hỗ trợ" else null)
    }
}

/** Selection policy deliberately falls back to Android's default SMS subscription during an SOS. */
object SmsSubscriptionChoice {
    fun resolve(requestedId: Int?, defaultId: Int?): Int? = requestedId ?: defaultId
}

object SosDiagnostic {
    fun line(eventId: String, result: SosStepResult, elapsedMs: Long, capabilities: CapabilitySnapshot, fix: LocationFix?): String =
        "eventId=$eventId step=${result.step} status=${result.status} elapsedMs=$elapsedMs " +
            "source=${fix?.source ?: "none"} accuracyM=${fix?.accuracyM ?: "none"} " +
            "calling=${capabilities.calling} messaging=${capabilities.messaging} location=${capabilities.location}" +
            (result.detail?.let { " detail=$it" } ?: "")
}

data class EmergencyRecord(
    val eventId: String,
    var cancelled: Boolean = false,
    var dispatched: Boolean = false,
    var dispatchedWithFix: Boolean = false,
    var voiceStarted: Boolean = false,
    var fix: LocationFix? = null,
    var displayName: String = "Người dùng FallSafe",
    var eventTimeMs: Long = 0,
    var contacts: List<EmergencyContact> = emptyList(),
    val smsSentContacts: MutableSet<String> = mutableSetOf(),
    val supplementSentContacts: MutableSet<String> = mutableSetOf(),
    var dispatchReport: SosDispatchReport? = null
)
interface EmergencyStore { fun get(eventId: String): EmergencyRecord?; fun save(record: EmergencyRecord) }
class MemoryEmergencyStore : EmergencyStore {
    private val records=mutableMapOf<String,EmergencyRecord>()
    override fun get(eventId:String)=records[eventId]
    override fun save(record:EmergencyRecord){records[record.eventId]=record}
}

class EmergencyCoordinator(
    private val sms: EmergencySmsGateway,
    private val backend: EmergencyBackendGateway,
    private val call: EmergencyCallGateway = EmergencyCallGateway { CallDispatchState(CallStatus.UNAVAILABLE, "Chưa cấu hình cuộc gọi SIM") },
    private val store: EmergencyStore,
    private val nowMs: () -> Long = System::currentTimeMillis,
    private val capabilities: CapabilitySnapshotProvider = CapabilitySnapshotProvider {
        CapabilitySnapshot(true, true, true, LocationPrecision.PRECISE)
    },
    private val logStatus: (tag: String, contactId: String?, status: String) -> Unit = { _, _, _ -> }
) {
    constructor(
        sms: EmergencySmsGateway,
        backend: EmergencyBackendGateway,
        store: EmergencyStore,
        nowMs: () -> Long = System::currentTimeMillis,
        capabilities: CapabilitySnapshotProvider = CapabilitySnapshotProvider {
            CapabilitySnapshot(true, true, true, LocationPrecision.PRECISE)
        }
    ) : this(sms, backend, EmergencyCallGateway { CallDispatchState(CallStatus.UNAVAILABLE, "Chưa cấu hình cuộc gọi SIM") }, store, nowMs, capabilities)

    @Volatile var latestReport: SosDispatchReport? = null; private set
    fun newEventId(): String = UUID.randomUUID().toString()
    fun report(eventId: String): SosDispatchReport? = store.get(eventId)?.dispatchReport?.also { latestReport = it }
    @Synchronized fun beginVerifying(eventId: String = newEventId()): String {
        if(store.get(eventId)==null) store.save(EmergencyRecord(eventId,eventTimeMs=nowMs()))
        return eventId
    }
    @Synchronized fun cancel(eventId:String) { store.get(eventId)?.takeIf{!it.dispatched}?.let{it.cancelled=true;store.save(it)} }
    @Synchronized fun timeout(eventId:String,contacts:List<EmergencyContact>,displayName:String,fix:LocationFix?=null)=dispatch(eventId,contacts,displayName,fix)
    @Synchronized fun dispatchManual(eventId:String=newEventId(),contacts:List<EmergencyContact>,displayName:String,fix:LocationFix?):String {
        beginVerifying(eventId);dispatch(eventId,contacts,displayName,fix);return eventId
    }
    private fun dispatch(eventId:String,contacts:List<EmergencyContact>,displayName:String,fix:LocationFix?) {
        val dispatchStarted = nowMs()
        val record=store.get(eventId)?:EmergencyRecord(eventId,eventTimeMs=nowMs())
        if(record.cancelled)return
        if(record.dispatched){record.dispatchReport?.let{latestReport=it};return}
        record.fix=fix?:record.fix
        record.dispatchedWithFix=record.fix!=null
        record.contacts=contacts.filter{it.receiveSos}.map { it.copy(phone = ContactValidator.normalize(it.phone)) }
            .sortedWith(compareByDescending<EmergencyContact> { it.isPrimary }.thenBy { it.callPriority })
        record.displayName=displayName.ifBlank{"Người dùng FallSafe"}
        record.dispatched=true;store.save(record)
        val permissionFacts=try{capabilities.snapshot()}catch(_:Exception){CapabilitySnapshot(false,false,false)}
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.log("SOS_DISPATCH_INPUTS eventId=$eventId calling=${permissionFacts.calling} messaging=${permissionFacts.messaging} location=${permissionFacts.location} precision=${permissionFacts.locationPrecision} fixPresent=${record.fix != null} contactsCount=${record.contacts.size}")
        val steps=mutableListOf<SosStepResult>()
        steps += if(record.fix!=null) SosStepResult(SosStep.LOCATION,SosStepStatus.SUCCESS,"Đã xác định được vị trí.")
            else SosStepResult(SosStep.LOCATION,SosStepStatus.UNAVAILABLE,"Chưa xác định được vị trí.")

        val smsOutcome=dispatchSms(record,permissionFacts)
        // Handset rescue must not depend on the backend accepting a voice request.
        val simResult=dispatchSimCall(record, permissionFacts)
        val voiceResult=if(!record.voiceStarted){
            record.voiceStarted=true;store.save(record)
            val status=try{backend.startVoice(eventId)}catch(_:Exception){VoiceDispatchStatus.UNAVAILABLE}
            when {
                status==VoiceDispatchStatus.STARTED -> SosStepResult(SosStep.VOICE_CALL,SosStepStatus.SUCCESS,"Yêu cầu gọi trợ giúp đã được tiếp nhận.")
                !permissionFacts.calling -> SosStepResult(SosStep.VOICE_CALL,SosStepStatus.PERMISSION_MISSING,EmergencyFailureMessages.callPermissionMissing)
                status==VoiceDispatchStatus.DISABLED -> SosStepResult(SosStep.VOICE_CALL,SosStepStatus.UNAVAILABLE,"Tính năng gọi qua máy chủ đang tắt; vẫn có thể gọi SIM thủ công.")
                status==VoiceDispatchStatus.CONFIGURED -> SosStepResult(SosStep.VOICE_CALL,SosStepStatus.UNAVAILABLE,"Dịch vụ gọi đã được cấu hình nhưng chưa tiếp nhận yêu cầu.")
                else -> SosStepResult(SosStep.VOICE_CALL,SosStepStatus.UNAVAILABLE,"Dịch vụ gọi tự động chưa được cấu hình; vẫn có thể gọi SIM thủ công.")
            }
        } else record.dispatchReport?.steps?.firstOrNull{it.step==SosStep.VOICE_CALL}
            ?:SosStepResult(SosStep.VOICE_CALL,SosStepStatus.UNAVAILABLE,"Chưa có kết quả cuộc gọi trợ giúp.")

        steps += smsOutcome.result
        steps += voiceResult
        logStatus("FallSafe/CALL", null, "VOICE_CALL:${voiceResult.status}")
        steps += simResult
        steps += when {
            record.fix==null -> SosStepResult(SosStep.MAP_LINK,SosStepStatus.SKIPPED,"Không có vị trí để tạo liên kết bản đồ.")
            smsOutcome.mapLinkSubmitted -> SosStepResult(SosStep.MAP_LINK,SosStepStatus.SUCCESS,"Tin nhắn có liên kết bản đồ.")
            else -> SosStepResult(SosStep.MAP_LINK,SosStepStatus.FAILED,"Chưa gửi được liên kết bản đồ cho người thân.")
        }
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.log("SOS_DISPATCH_RESULTS eventId=$eventId steps=${steps.joinToString { "${it.step.name}=${it.status.name}" }}")
        record.dispatchReport=SosDispatchReport(eventId,steps).also{latestReport=it}
        steps.forEach { logStatus("FallSafe/SOS", null, SosDiagnostic.line(eventId, it, (nowMs() - dispatchStarted).coerceAtLeast(0), permissionFacts, record.fix)) }
        store.save(record)
    }

    private fun dispatchSimCall(
        record: EmergencyRecord,
        permissionFacts: CapabilitySnapshot
    ): SosStepResult {
        val primary = record.contacts.firstOrNull { it.receiveSos && it.phone.isNotBlank() }
        fun result(status: SosStepStatus, detail: String) = SosStepResult(SosStep.SIM_CALL, status, detail).also {
            logStatus("FallSafe/CALL", primary?.id, "SIM_CALL:$status")
        }
        if (!permissionFacts.calling) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "permissionFacts.calling=false")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = primary != null, phonePresent = primary?.phone?.isNotBlank() == true, permission = false, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
            return result(SosStepStatus.PERMISSION_MISSING, EmergencyFailureMessages.callPermissionMissing)
        }
        if (primary == null) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "primary_contact_null")
            vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = false, phonePresent = false, permission = permissionFacts.calling, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
            return result(SosStepStatus.UNAVAILABLE, "Chưa có người thân để gọi.")
        }
        val state = try { call.call(primary.phone) }
            catch (_: SecurityException) { CallDispatchState(CallStatus.PERMISSION_MISSING, EmergencyFailureMessages.callPermissionMissing) }
            catch (_: Exception) { CallDispatchState(CallStatus.FAILED, "Không thể thực hiện cuộc gọi SIM.") }
        return when (state.status) {
            CallStatus.STARTED -> result(SosStepStatus.SUCCESS, "Đã gọi SIM tới ${primary.name}.")
            CallStatus.PERMISSION_MISSING -> result(SosStepStatus.PERMISSION_MISSING, state.detail ?: EmergencyFailureMessages.callPermissionMissing)
            CallStatus.UNAVAILABLE -> result(SosStepStatus.UNAVAILABLE, state.detail ?: "Cuộc gọi SIM không khả dụng.")
            CallStatus.FAILED -> result(SosStepStatus.FAILED, state.detail ?: "Không thể thực hiện cuộc gọi SIM.")
        }
    }

    private data class SmsStepOutcome(val result:SosStepResult,val mapLinkSubmitted:Boolean)
    private fun dispatchSms(record:EmergencyRecord,permissionFacts:CapabilitySnapshot):SmsStepOutcome {
        if(!permissionFacts.messaging) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("SMS", "permissionFacts.messaging=false")
            vn.nckh27pa.fallsafe.AndroidTrace.logSms(entered = true, contactExists = record.contacts.isNotEmpty(), phonePresent = record.contacts.any { it.phone.isNotBlank() }, permission = false, sendTextMessageReached = false, exception = "none")
            return SmsStepOutcome(SosStepResult(SosStep.SMS,SosStepStatus.PERMISSION_MISSING,EmergencyFailureMessages.messagingPermissionMissing),false)
        }
        if(record.contacts.isEmpty()) {
            // TEMPORARY DIAGNOSTIC
            vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("SMS", "contacts_empty")
            vn.nckh27pa.fallsafe.AndroidTrace.logSms(entered = true, contactExists = false, phonePresent = false, permission = permissionFacts.messaging, sendTextMessageReached = false, exception = "none")
            return SmsStepOutcome(SosStepResult(SosStep.SMS,SosStepStatus.UNAVAILABLE,"Chưa có người thân nhận cảnh báo."),false)
        }
        var succeeded=0;var failed=0;var skipped=0;val details=mutableListOf<String>()
        for(contact in record.contacts) if(record.smsSentContacts.add(contact.id)) {
            if(contact.phone.isBlank()){
                // TEMPORARY DIAGNOSTIC
                vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("SMS", "phone_blank_contact=${contact.id}")
                skipped++;details += "Số điện thoại của ${contact.name} để trống nên chưa gửi.";continue
            }
            store.save(record)
            val state=try {
                sms.send(SmsRequest(record.eventId,contact.id,contact.phone,EmergencyMessageFormatter.emergency(record.displayName,record.eventTimeMs,record.fix,nowMs())))
            } catch (_:Exception) {
                SmsDispatchState(record.eventId,contact.id,SmsDeliveryStatus.FAILED,"Không thể gửi SMS trên thiết bị.")
            }
            logStatus("FallSafe/SMS", contact.id, state.status.name)
            state.detail?.let(details::add)
            if(state.status==SmsDeliveryStatus.FAILED){failed++}else succeeded++
        }
        val result=when {
            failed==0 && succeeded==record.contacts.size -> SosStepResult(SosStep.SMS,SosStepStatus.SUCCESS,
                "Đã chuyển toàn bộ tin nhắn cho thiết bị gửi." + details.firstOrNull()?.let { " $it" }.orEmpty())
            failed==0 && succeeded>0 -> SosStepResult(SosStep.SMS,SosStepStatus.SUCCESS,
                "Đã gửi tin nhắn cho $succeeded người; bỏ qua $skipped người chưa có số điện thoại.")
            succeeded>0 -> SosStepResult(SosStep.SMS,SosStepStatus.PARTIAL,details.firstOrNull()?:"Một số tin nhắn chưa gửi được.")
            skipped>0 -> SosStepResult(SosStep.SMS,SosStepStatus.FAILED,"Không thể gửi tin nhắn: chưa có số điện thoại hợp lệ.")
            else -> SosStepResult(SosStep.SMS,SosStepStatus.FAILED,details.firstOrNull()?:"Không thể gửi tin nhắn.")
        }
        return SmsStepOutcome(result,record.fix!=null&&succeeded>0)
    }
    @Synchronized fun updateLocation(eventId:String,fix:LocationFix) {
        val record=store.get(eventId)?:return
        if(record.cancelled)return
        record.fix=fix
        val canMessage=try{capabilities.snapshot().messaging}catch(_:Exception){false}
        var supplementSent=false
        if(record.dispatched&&!record.dispatchedWithFix&&canMessage) for(contact in record.contacts) if(contact.phone.isNotBlank()&&record.supplementSentContacts.add(contact.id)) {
            store.save(record)
            try {
                val state=sms.send(SmsRequest(eventId,contact.id,contact.phone,EmergencyMessageFormatter.locationSupplement(record.displayName,fix,nowMs()),supplement=true))
                if(state.status!=SmsDeliveryStatus.FAILED)supplementSent=true
            } catch (_: Exception) { }
        }
        record.dispatchReport?.let { old ->
            val revised=old.steps.map { step -> when(step.step) {
                SosStep.LOCATION -> SosStepResult(SosStep.LOCATION,SosStepStatus.SUCCESS,"Đã xác định được vị trí.")
                SosStep.MAP_LINK -> if(supplementSent)SosStepResult(SosStep.MAP_LINK,SosStepStatus.SUCCESS,"Tin nhắn bổ sung có liên kết bản đồ.") else step
                else -> step
            }}
            record.dispatchReport=SosDispatchReport(eventId,revised).also{latestReport=it}
        }
        store.save(record)
    }
}
