package vn.nckh27pa.fallsafe.api

import android.content.Context
import com.google.gson.Gson
import core.Snapshot
import core.State
import core.Status
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import vn.nckh27pa.fallsafe.EmergencyContact
import java.util.UUID

interface SyncStore { fun read(): String?; fun write(value: String): Boolean }
class PreferencesSyncStore(context: Context, scope: String) : SyncStore {
    private val prefs = context.getSharedPreferences("fallsafe_sync_$scope", Context.MODE_PRIVATE)
    override fun read() = prefs.getString("outbox", null)
    // Commit before acknowledging a queued safety transition. No HTTP runs here.
    override fun write(value: String) = prefs.edit().putString("outbox", value).commit()
}
data class SyncOperation(val identity: String, val kind: OperationKind,
    val event: EventRequest? = null, val action: ActionRequest? = null,
    val userId: String? = null, val contact: EmergencyContact? = null, val contactId: String? = null,
    val transportStatus:TransportStatusRequest?=null,
    var attempts: Int = 0, var nextAttemptMs: Long = 0)
private data class OutboxState(
    val pending: MutableList<SyncOperation> = mutableListOf(),
    val completed: MutableSet<String> = mutableSetOf(),
    val rejected: MutableList<SyncOperation> = mutableListOf(),
    var sequence: Long = 0)

/** Owner-thread confined; the mutex also excludes overlapping suspending flushes. */
class SyncOutbox(private val store: SyncStore, private val jitter: () -> Double = { 0.8 + Math.random() * 0.4 }) {
    private val gson = Gson()
    var durable = true; private set
    private var corrupted = false
    private val state = try { store.read()?.let { gson.fromJson(it, OutboxState::class.java) } ?: OutboxState() }
        catch (_: Exception) { corrupted = true; durable = false; OutboxState() }
    private val mutex = Mutex()
    fun pending(): List<SyncOperation> = state.pending.toList()
    fun rejected(): List<SyncOperation> = state.rejected.toList()
    private fun persist() {
        // Do not overwrite unreadable durable data; report the failure for recovery.
        durable = !corrupted && try { store.write(gson.toJson(state)) } catch (_: Exception) { false }
    }
    fun enqueue(operation: SyncOperation) {
        if (operation.identity in state.completed || state.pending.any { it.identity == operation.identity } ||
            state.rejected.any { it.identity == operation.identity }) return
        state.pending += operation
        persist()
    }
    fun nextSequence(): Long { state.sequence++; persist(); return state.sequence }
    suspend fun flush(nowMs: Long, send: suspend (SyncOperation) -> ApiResult<*>): ApiResult<*> = mutex.withLock {
        persist()
        if (!durable) return@withLock ApiResult.Failure(ErrorKind.STORAGE)
        var last: ApiResult<*> = ApiResult.Success(Unit)
        while (state.pending.isNotEmpty()) {
            val head = state.pending.first()
            if (nowMs < head.nextAttemptMs) return@withLock last
            last = send(head)
            when (last) {
                is ApiResult.Success -> { state.pending.removeAt(0); state.completed += head.identity }
                is ApiResult.Failure -> {
                    val failure = last
                    val replayDelete = head.kind == OperationKind.CONTACT_DELETE && failure.code == "RESOURCE_NOT_FOUND"
                    if (replayDelete) { state.pending.removeAt(0); state.completed += head.identity }
                    else if (failure.kind in listOf(ErrorKind.VALIDATION, ErrorKind.CONFLICT) || failure.code == "RESOURCE_NOT_FOUND") {
                        state.pending.removeAt(0); state.rejected += head
                        persist()
                        return@withLock failure // Retain rejected operation for diagnosis, never retry invalid mutations.
                    } else {
                        head.attempts = (head.attempts + 1).coerceAtMost(30)
                        val base = (1000L shl (head.attempts - 1).coerceAtMost(5)).coerceAtMost(30000)
                        head.nextAttemptMs = nowMs + (base * jitter()).toLong().coerceIn(800, 30000)
                        persist(); return@withLock failure
                    }
                }
                ApiResult.Loading -> return@withLock last
            }
            persist()
            if (!durable) return@withLock ApiResult.Failure(ErrorKind.STORAGE)
        }
        last
    }
}

/** UUID from process-session + numeric core ID is stable across all persisted retries.
 * A new core uses a new session namespace, so numeric IDs may safely restart at 1. */
class TransitionSync(private val outbox: SyncOutbox, private val config: ApiConfig,
    private val sessionId: String = UUID.randomUUID().toString(),
    private val displayName:()->String={"Người dùng FallSafe"},
    private val identity: vn.nckh27pa.fallsafe.emergency.EventIdentityStore = vn.nckh27pa.fallsafe.emergency.SessionEventIdentityStore(sessionId),
    private val wallMs: () -> Long = System::currentTimeMillis) {
    fun eventId(localId: Long): String = identity.id(localId)
    fun clearEventId(localId: Long) = identity.clear(localId)
    fun observe(s: Snapshot) {
        if (s.eventId <= 0) return
        val kind = when {
            s.state == State.VERIFYING -> OperationKind.EVENT
            s.state == State.MONITORING && s.response == core.Response.SAFE -> OperationKind.CANCEL
            s.state == State.MONITORING && s.status == Status.ACKNOWLEDGED -> OperationKind.RESOLVE
            s.state in listOf(State.ALERTING, State.AWAITING_HELP) -> OperationKind.SOS
            else -> return
        }
        val id = eventId(s.eventId)
        val now = wallMs()
        val event = if (kind == OperationKind.EVENT) EventRequest(id, config.deviceId, config.userId, s.eventId, now,displayName=displayName()) else null
        val action = if (event == null) ActionRequest(id, config.deviceId, config.userId, now,
            triggerSource = if (kind == OperationKind.SOS) { if (s.response == core.Response.NO_RESPONSE) "COUNTDOWN_TIMEOUT" else "MANUAL_APP_BUTTON" } else null,
            response = if (kind == OperationKind.SOS) s.response.name else null,
            reason = if (kind == OperationKind.CANCEL) "SAFE" else null,
            resolvedBy = if (kind == OperationKind.RESOLVE) config.userId else null,
            displayName = if(kind==OperationKind.SOS)displayName() else null) else null
        outbox.enqueue(SyncOperation("$id:$kind", kind, event, action))
    }
}
