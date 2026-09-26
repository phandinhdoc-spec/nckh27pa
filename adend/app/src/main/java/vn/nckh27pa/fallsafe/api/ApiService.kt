package vn.nckh27pa.fallsafe.api

import com.google.gson.GsonBuilder
import kotlinx.coroutines.CancellationException
import okhttp3.OkHttpClient
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import retrofit2.http.*
import vn.nckh27pa.fallsafe.BuildConfig
import vn.nckh27pa.fallsafe.EmergencyContact
import java.io.Closeable
import java.io.IOException
import java.io.InterruptedIOException
import java.util.concurrent.TimeUnit
import vn.nckh27pa.fallsafe.device.DeviceDetails
import vn.nckh27pa.fallsafe.device.DeviceValueSource

data class ApiConfig(val baseUrl: String, val deviceId: String, val userId: String, val espDeviceId: String? = deviceId.takeUnless { it.startsWith("PHONE-",ignoreCase=true)||it.equals("phone",ignoreCase=true) }) {
    companion object {
        fun normalize(url: String): String = url.trim().trimEnd('/') + "/"
        fun generated() = ApiConfig(BuildConfig.API_BASE_URL, BuildConfig.FALLSAFE_DEVICE_ID, BuildConfig.FALLSAFE_USER_ID, BuildConfig.FALLSAFE_ESP32_DEVICE_ID.trim().ifEmpty { null })
    }
}
sealed interface ApiResult<out T> {
    data object Loading : ApiResult<Nothing>
    data class Success<T>(val value: T) : ApiResult<T>
    data class Failure(val kind: ErrorKind, val code: String? = null) : ApiResult<Nothing>
}
enum class ErrorKind { OFFLINE, TIMEOUT, VALIDATION, CONFLICT, BACKEND, MALFORMED, STORAGE }
data class ApiError(val code: String?, val message: String?)
data class Envelope<T>(val success: Boolean?, val data: T?, val error: ApiError?, val timestamp: Long?)
data class ContactDto(val id: String?, val userId: String?, val name: String?, val relationship: String?, val phone: String?, val receiveSos: Boolean?, val isPrimary: Boolean?, val callPriority: Int? = null) {
    fun domain(user: String): EmergencyContact? = if (userId == user && !id.isNullOrBlank() &&
        !name.isNullOrBlank() && relationship != null && phone != null && receiveSos != null && isPrimary != null)
        EmergencyContact(id, name, relationship, phone, receiveSos, isPrimary, callPriority ?: Int.MAX_VALUE) else null
    companion object {
        fun from(c: EmergencyContact, user: String) = ContactDto(c.id, user, c.name, c.relationship, c.phone, c.receiveSos, c.isPrimary, c.callPriority)
    }
}
data class ContactsDto(val userId: String?, val contacts: List<ContactDto>?)
data class ContactReply(val contact: ContactDto?)
data class DeleteReply(val deletedId: String?, val remainingCount: Int?)
data class AlertReply(val eventId: String?, val alertState: String?, val dispatchStatus: String?)
data class ActiveAlert(val activeEventId: String?, val alertState: String?, val caregiverAcknowledged: Boolean?)
data class Receipt(val deviceId: String?, val stored: Boolean?, val recorded: Boolean?)
data class EventRequest(val eventId: String, val deviceId: String, val userId: String,
    val sequenceNumber: Long, val timestampMs: Long, val eventType: String = "IMPACT_DETECTED",
    val severity: String = "CRITICAL", val alertState: String = "VERIFYING", val sensorSource: String = "PHONE",
    val peakAccelerationMs2: Float? = null, val orientationChangeDeg: Float? = null,
    val altitudeDeltaM: Float? = null, val sosButtonPressed: Boolean = false,
    val confidencePercent: Int = 0, val response: String = "UNKNOWN", val location: Any? = null,
    val displayName:String?=null)
data class ActionRequest(val eventId: String, val deviceId: String, val userId: String, val timestampMs: Long,
    val triggerSource: String? = null, val response: String? = null, val reason: String? = null,
    val resolvedBy: String? = null, val location: Any? = null, val displayName:String?=null)
data class TransportStatusRequest(val contactId:String,val channel:String="SMS",val status:String,val timestampMs:Long,val detail:String?=null)
data class TransportStatusReply(val eventId:String?,val contactId:String?,val channel:String?,val status:String?,val detail:String?,val timestampMs:Long?)
data class AiTextRequest(val text:String)
data class AiTextReply(val status:String?,val text:String?)
data class LocationPayload(val latitude:Double,val longitude:Double,val accuracyM:Float?,val timestampMs:Long,val locationMessage:String="chưa xác định được địa chỉ")
data class SensorRequest(val deviceId: String, val sequenceNumber: Long, val timestampMs: Long,
    val accelXMs2: Float, val accelYMs2: Float, val accelZMs2: Float,
    val gyroXDps: Float?, val gyroYDps: Float?, val gyroZDps: Float?, val pressurePa: Float?,
    val altitudeDeltaM: Float?, val batteryPercent: Int, val batteryVoltageMv: Int?, val isCharging: Boolean,
    val sensorQuality: Int, val motionState: String, val sensorSource: String = "PHONE",
    val temperatureC: Float? = null, val sosButtonPressed: Boolean = false, val location: Any? = null)
data class HeartbeatRequest(val firmwareVersion: String, val timestampMs: Long, val uptimeSeconds: Long,
    val batteryPercent: Int, val batteryVoltageMv: Int?, val isCharging: Boolean,
    val imuStatus: String, val barometerStatus: String, val deviceType: String = "PHONE",
    val gnssStatus: String = "UNAVAILABLE", val bufferUsagePercent: Int? = null, val lastErrorCode: String? = null)
data class DeviceStatusDto(val deviceId:String?,val deviceType:String?,val firmwareVersion:String?,val batteryPercent:Int?,val isConnected:Boolean?,val gnssStatus:String?,val lastHeartbeatMs:Long?) {
    fun espDetails(expectedId:String):DeviceDetails? = if(deviceId==expectedId&&deviceType=="ESP32"&&isConnected!=null&&
        (batteryPercent==null||batteryPercent in 0..100)) DeviceDetails(deviceId,isConnected,batteryPercent,firmwareVersion,gnssStatus,DeviceValueSource.BACKEND_HEARTBEAT,lastHeartbeatMs,
            if(isConnected)"ESP32 đã phản hồi máy chủ" else "ESP32 mất kết nối theo heartbeat") else null
}

interface ApiService {
    @POST("ai/text") suspend fun aiText(@Body request:AiTextRequest):Response<Envelope<AiTextReply>>
    @GET("devices/{deviceId}/status") suspend fun deviceStatus(@Path("deviceId") device: String): Response<Envelope<DeviceStatusDto>>
    @GET("users/{userId}/contacts") suspend fun contacts(@Path("userId") user: String): Response<Envelope<ContactsDto>>
    @POST("users/{userId}/contacts") suspend fun addContact(@Path("userId") user: String, @Body contact: ContactDto): Response<Envelope<ContactReply>>
    @PUT("users/{userId}/contacts/{id}") suspend fun updateContact(@Path("userId") user: String, @Path("id") id: String, @Body contact: ContactDto): Response<Envelope<ContactReply>>
    @DELETE("users/{userId}/contacts/{id}") suspend fun deleteContact(@Path("userId") user: String, @Path("id") id: String): Response<Envelope<DeleteReply>>
    @POST("events") suspend fun event(@Body event: EventRequest): Response<Envelope<AlertReply>>
    @POST("alerts/cancel") suspend fun cancel(@Body action: ActionRequest): Response<Envelope<AlertReply>>
    @POST("alerts/sos") suspend fun sos(@Body action: ActionRequest): Response<Envelope<AlertReply>>
    @POST("alerts/resolve") suspend fun resolve(@Body action: ActionRequest): Response<Envelope<AlertReply>>
    @GET("alerts/active") suspend fun active(@Query("deviceId") device: String, @Query("userId") user: String): Response<Envelope<ActiveAlert>>
    @POST("sensors/ingest") suspend fun sensor(@Body sensor: SensorRequest): Response<Envelope<Receipt>>
    @POST("devices/{deviceId}/heartbeat") suspend fun heartbeat(@Path("deviceId") device: String, @Body status: HeartbeatRequest): Response<Envelope<Receipt>>
    @POST("alerts/{eventId}/transport-status") suspend fun transportStatus(@Path("eventId") eventId:String,@Body status:TransportStatusRequest):Response<Envelope<TransportStatusReply>>
}
class ApiClient(config: ApiConfig, timeoutMs: Long = 8_000) : Closeable {
    val http = OkHttpClient.Builder().connectTimeout(timeoutMs, TimeUnit.MILLISECONDS)
        .readTimeout(timeoutMs, TimeUnit.MILLISECONDS).writeTimeout(timeoutMs, TimeUnit.MILLISECONDS)
        .callTimeout(timeoutMs, TimeUnit.MILLISECONDS).retryOnConnectionFailure(false)
        .addInterceptor { chain -> chain.proceed(chain.request().newBuilder()
            .header("X-Device-Id", config.deviceId).header("X-User-Id", config.userId).build()) }.build()
    val service: ApiService = Retrofit.Builder().baseUrl(ApiConfig.normalize(config.baseUrl)).client(http)
        .addConverterFactory(GsonConverterFactory.create(GsonBuilder().serializeNulls().create()))
        .build().create(ApiService::class.java)
    override fun close() { http.dispatcher.cancelAll(); http.connectionPool.evictAll(); http.dispatcher.executorService.shutdown() }
}
