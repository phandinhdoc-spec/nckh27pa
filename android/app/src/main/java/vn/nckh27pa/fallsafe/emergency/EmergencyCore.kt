package vn.nckh27pa.fallsafe.emergency

import vn.nckh27pa.fallsafe.EmergencyContact
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.UUID

const val LOCATION_FRESH_MS = 120_000L

enum class LocationSource { PHONE, ESP32_GNSS }
enum class LocationFreshness { FRESH, STALE, UNAVAILABLE }
enum class LocationFailureCause { PERMISSION_DENIED, PROVIDER_DISABLED, TIMEOUT, NO_FIX, INVALID_FIX }

class LocationFix private constructor(
    val latitude: Double,
    val longitude: Double,
    val accuracyM: Float?,
    val fixTimeMs: Long,
    val source: LocationSource
) {
    val mapsUrl: String get() = "https://www.google.com/maps/search/?api=1&query=$latitude,$longitude"
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

data class LocationState(
    val fix: LocationFix? = null,
    val freshness: LocationFreshness = LocationFreshness.UNAVAILABLE,
    val cause: LocationFailureCause? = LocationFailureCause.NO_FIX,
    val explanation: String = "Chưa có vị trí GPS.",
    val remediation: String? = "Di chuyển ra nơi thoáng và thử lại."
)

data class LocationResolution(val fix: LocationFix?, val cause: LocationFailureCause?)

/** Completes once. Failures use a validated last-known fix when one is available. */
class BoundedLocationResolutionAttempt(
    private val lastKnown: () -> LocationFix?,
    private val onComplete: (LocationResolution) -> Unit
) {
    private var completed = false
    @Synchronized fun current(fix: LocationFix) = complete(LocationResolution(fix, null))
    @Synchronized fun fail(cause: LocationFailureCause) = complete(LocationResolution(lastKnown(), cause))
    private fun complete(resolution: LocationResolution) {
        if (completed) return
        completed = true
        onComplete(resolution)
    }
}

data class ManualShareConfirmation(val token: String, val contactId: String, val preview: String)

interface EmergencyLocationController {
    val locationState: LocationState
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
fun interface EmergencyBackendGateway { fun startVoice(eventId: String) }

object EmergencyFailureMessages {
    const val callPermissionMissing = "Chưa cho phép gọi điện; hãy bật quyền này trong phần thiết lập."
    const val messagingPermissionMissing = "Chưa cho phép gửi tin nhắn; hãy bật quyền này trong phần thiết lập."
    const val simAccessMissing = "Chưa cho phép kiểm tra SIM; hãy bật quyền điện thoại trong phần thiết lập."
}

object EmergencyMessageFormatter {
    private fun time(value: Long): String = SimpleDateFormat("yyyy-MM-dd HH:mm:ss 'UTC'", Locale.ROOT).apply { timeZone=TimeZone.getTimeZone("UTC") }.format(Date(value))
    fun emergency(displayName: String?, eventTimeMs: Long, fix: LocationFix?, nowMs: Long): String {
        val name=displayName?.trim().takeUnless { it.isNullOrEmpty() } ?: "Người dùng FallSafe"
        val header="CẢNH BÁO SOS: Tôi có thể đã bị ngã. Người cần trợ giúp: $name. Thời điểm sự kiện: ${time(eventTimeMs)}."
        if (fix==null) return "$header Chưa xác định được vị trí."
        val source=when(fix.source){LocationSource.PHONE->"điện thoại";LocationSource.ESP32_GNSS->"ESP32 GNSS"}
        val fixDescription=if(fix.freshness(nowMs)==LocationFreshness.STALE)
            " Vị trí gần nhất, cập nhật lúc ${time(fix.fixTimeMs)}." else " Thời điểm fix: ${time(fix.fixTimeMs)}."
        val accuracy=fix.accuracyM?.let { " Độ chính xác: ${it.toInt()} m." } ?: ""
        return "$header Tọa độ: ${fix.latitude},${fix.longitude}. ${fix.mapsUrl}.$fixDescription$accuracy Nguồn vị trí: $source."
    }
    fun locationSupplement(displayName: String?, fix: LocationFix, nowMs: Long): String {
        val source=if(fix.source==LocationSource.PHONE)"điện thoại" else "ESP32 GNSS"
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
        require(index in 0 until total)
        if (successful) sent += index else sentFailed += index
        return result()
    }
    @Synchronized fun recordDelivery(index: Int, successful: Boolean): SmsAggregationResult {
        require(index in 0 until total)
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

data class EmergencyRecord(
    val eventId: String,
    var cancelled: Boolean = false,
    var dispatched: Boolean = false,
    var voiceStarted: Boolean = false,
    var fix: LocationFix? = null,
    var displayName: String = "Người dùng FallSafe",
    var eventTimeMs: Long = 0,
    var contacts: List<EmergencyContact> = emptyList(),
    val smsSentContacts: MutableSet<String> = mutableSetOf(),
    val supplementSentContacts: MutableSet<String> = mutableSetOf()
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
    private val store: EmergencyStore,
    private val nowMs: () -> Long = System::currentTimeMillis
) {
    fun newEventId(): String = UUID.randomUUID().toString()
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
        val record=store.get(eventId)?:EmergencyRecord(eventId,eventTimeMs=nowMs())
        if(record.cancelled)return
        record.fix=fix?:record.fix;record.contacts=contacts.filter{it.receiveSos};record.displayName=displayName.ifBlank{"Người dùng FallSafe"}
        if(!record.voiceStarted){
            record.voiceStarted=true
            store.save(record)
            try { backend.startVoice(eventId) } catch (_: Exception) { }
        }
        if(!record.dispatched){
            record.dispatched=true;store.save(record)
            for(contact in record.contacts) if(record.smsSentContacts.add(contact.id)) {
                store.save(record)
                try {
                    sms.send(SmsRequest(eventId,contact.id,contact.phone,EmergencyMessageFormatter.emergency(record.displayName,record.eventTimeMs,record.fix,nowMs())))
                } catch (_: Exception) { }
            }
        }
        store.save(record)
    }
    @Synchronized fun updateLocation(eventId:String,fix:LocationFix) {
        val record=store.get(eventId)?:return
        if(record.cancelled)return
        record.fix=fix
        if(record.dispatched) for(contact in record.contacts) if(record.supplementSentContacts.add(contact.id)) {
            store.save(record)
            try {
                sms.send(SmsRequest(eventId,contact.id,contact.phone,EmergencyMessageFormatter.locationSupplement(record.displayName,fix,nowMs()),supplement=true))
            } catch (_: Exception) { }
        }
        store.save(record)
    }
}
