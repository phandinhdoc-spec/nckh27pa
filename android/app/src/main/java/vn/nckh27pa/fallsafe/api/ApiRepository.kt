package vn.nckh27pa.fallsafe.api

import com.google.gson.Gson
import com.google.gson.JsonParseException
import kotlinx.coroutines.CancellationException
import retrofit2.Response
import java.io.IOException
import java.io.InterruptedIOException
import vn.nckh27pa.fallsafe.EmergencyContact

/** All HTTP and parsing failures become values; cancellation always propagates. */
class ApiRepository(private val api: ApiService) {
    private suspend fun <T> request(valid: (T) -> Boolean, call: suspend () -> Response<Envelope<T>>): ApiResult<T> = try {
        val response = call()
        if (!response.isSuccessful) {
            val error = response.errorBody()?.use { Gson().fromJson(it.string(), Envelope::class.java) }
            if (error?.success != false || error.error?.code == null) ApiResult.Failure(ErrorKind.MALFORMED)
            else ApiResult.Failure(when(response.code()) {
                400 -> ErrorKind.VALIDATION
                409 -> ErrorKind.CONFLICT
                else -> ErrorKind.BACKEND
            }, error.error.code)
        } else {
            val envelope = response.body()
            val data = envelope?.data
            if (envelope?.success != true || envelope.timestamp == null || data == null || !valid(data))
                ApiResult.Failure(ErrorKind.MALFORMED)
            else ApiResult.Success(data)
        }
    } catch (e: CancellationException) { throw e
    } catch (_: JsonParseException) { ApiResult.Failure(ErrorKind.MALFORMED)
    } catch (_: com.google.gson.stream.MalformedJsonException) { ApiResult.Failure(ErrorKind.MALFORMED)
    } catch (_: java.io.EOFException) { ApiResult.Failure(ErrorKind.MALFORMED)
    } catch (_: InterruptedIOException) { ApiResult.Failure(ErrorKind.TIMEOUT)
    } catch (_: IOException) { ApiResult.Failure(ErrorKind.OFFLINE)
    } catch (_: RuntimeException) { ApiResult.Failure(ErrorKind.MALFORMED) }

    suspend fun contacts(user: String): ApiResult<List<EmergencyContact>> {
        return when (val result = request({ d: ContactsDto -> d.userId == user && d.contacts != null && d.contacts.all { it.domain(user) != null } }) { api.contacts(user) }) {
            is ApiResult.Success -> ApiResult.Success(result.value.contacts!!.map { it.domain(user)!! })
            is ApiResult.Failure -> result
            ApiResult.Loading -> ApiResult.Loading
        }
    }
    suspend fun addContact(user: String, c: EmergencyContact) = request({ d: ContactReply -> d.contact?.domain(user)?.id == c.id }) { api.addContact(user, ContactDto.from(c, user)) }
    suspend fun updateContact(user: String, c: EmergencyContact) = request({ d: ContactReply -> d.contact?.domain(user)?.id == c.id }) { api.updateContact(user, c.id, ContactDto.from(c, user)) }
    suspend fun deleteContact(user: String, id: String) = request({ d: DeleteReply -> d.deletedId == id && d.remainingCount != null }) { api.deleteContact(user, id) }
    suspend fun event(e: EventRequest) = request({ d: AlertReply -> d.eventId == e.eventId && d.alertState != null }) { api.event(e) }
    suspend fun action(kind: OperationKind, a: ActionRequest) = request({ d: AlertReply -> d.eventId == a.eventId && d.alertState != null }) {
        when(kind) { OperationKind.CANCEL -> api.cancel(a); OperationKind.SOS -> api.sos(a); OperationKind.RESOLVE -> api.resolve(a); else -> error("Not an alert action") }
    }
    suspend fun active(config: ApiConfig) = request({ d: ActiveAlert -> d.alertState != null && d.caregiverAcknowledged != null }) { api.active(config.deviceId, config.userId) }
    suspend fun sensor(s: SensorRequest) = request({ d: Receipt -> d.deviceId == s.deviceId && d.stored == true }) { api.sensor(s) }
    suspend fun heartbeat(device: String, h: HeartbeatRequest) = request({ d: Receipt -> d.deviceId == device && d.recorded == true }) { api.heartbeat(device, h) }
}
enum class OperationKind { EVENT, CANCEL, SOS, RESOLVE, CONTACT_ADD, CONTACT_UPDATE, CONTACT_DELETE }
