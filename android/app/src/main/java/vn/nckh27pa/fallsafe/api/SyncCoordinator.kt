package vn.nckh27pa.fallsafe.api

import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import core.State
import kotlinx.coroutines.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import vn.nckh27pa.fallsafe.DemoController
import vn.nckh27pa.fallsafe.PhoneSensorPacket
import java.io.Closeable
import java.util.UUID

/** Application lifetime, main-thread confined. A single owned job does bounded HTTP work;
 * core callbacks only persist intent. Process lifecycle starts/stops idle polling, while
 * foreground-service monitoring or an active emergency keeps synchronization alive. */
class SyncCoordinator(
    private val controller: DemoController,
    private val repository: ApiRepository,
    private val config: ApiConfig,
    private val outbox: SyncOutbox,
    dispatcher: CoroutineDispatcher = Dispatchers.Main.immediate,
    private val wallMs: () -> Long = System::currentTimeMillis,
    private val elapsedMs: () -> Long = { android.os.SystemClock.elapsedRealtime() },
    private val battery: () -> BatteryReading? = { null },
    private val heartbeat: (BatteryReading, PhoneSensorPacket?) -> HeartbeatRequest? = { _, _ -> null }
) : DefaultLifecycleObserver, Closeable {
    private val scope = CoroutineScope(SupervisorJob() + dispatcher)
    private val transitions = TransitionSync(outbox, config)
    private val telemetry = PhoneTelemetry(config, outbox)
    private val cycleMutex = Mutex()
    private var job: Job? = null
    private var started = false
    private var visible = false
    private var closed = false
    private var latest: PhoneSensorPacket? = null
    private var contactsRequested = true
    private var contactRevision = 0L
    private var nextContacts = 0L
    private var nextPoll = 0L
    private var nextHeartbeat = 0L
    private var nextSensor = 0L
    private var lastLocalEvent = 0L

    init {
        controller.onSyncStateChanged = {
            if (!controller.replaying && controller.source.startsWith("PHONE_ONLY")) {
                transitions.observe(controller.snapshot)
                if (lastLocalEvent != controller.snapshot.eventId) {
                    lastLocalEvent = controller.snapshot.eventId
                    controller.caregiverAcknowledged = false
                    controller.sosDeliveryMessage = "SOS đã lưu trên điện thoại; chưa xác nhận gửi ra ngoài."
                    nextPoll = 0
                }
                if (!outbox.durable) report(ApiResult.Failure(ErrorKind.STORAGE))
            }
            if (started) ensureRunning()
        }
        controller.onPhoneReading = { latest = it }
        controller.onSyncResume = { resume() }
        controller.onContactMutation = { kind, contact, id ->
            contactRevision++
            outbox.enqueue(SyncOperation(UUID.randomUUID().toString(), kind, userId = config.userId, contact = contact, contactId = id))
            contactsRequested = true
            if (!outbox.durable) report(ApiResult.Failure(ErrorKind.STORAGE))
            if (started) ensureRunning()
        }
    }
    fun remoteEventId(): String = transitions.eventId(controller.snapshot.eventId)
    override fun onStart(owner: LifecycleOwner) { visible = true; resume() }
    override fun onStop(owner: LifecycleOwner) { visible = false }
    fun start() { started = true; ensureRunning() }
    fun resume() { contactsRequested = true; nextContacts = 0; start() }
    private fun keepRunning() = visible || controller.foreground || controller.backgroundMonitoring ||
        controller.snapshot.state != State.MONITORING
    private fun ensureRunning() {
        if (closed || job?.isActive == true) return
        job = scope.launch {
            do {
                syncOnce()
                if (!keepRunning()) break
                delay(1000)
            } while (isActive)
        }
    }
    suspend fun syncOnce() = cycleMutex.withLock {
        if (closed) return@withLock
        val now = wallMs()
        val hadPending = outbox.pending().isNotEmpty()
        if (hadPending) controller.apiState = ApiResult.Loading
        val flush = outbox.flush(now, ::send)
        if (hadPending || flush is ApiResult.Failure) report(flush)
        if (hadPending && outbox.pending().none { it.userId != null }) contactsRequested = true

        if (contactsRequested && now >= nextContacts && outbox.pending().none { it.userId != null }) {
            nextContacts = now + 30000
            val revision = contactRevision
            val result = repository.contacts(config.userId)
            if (result is ApiResult.Success && revision == contactRevision) {
                controller.contactRepository.saveContacts(result.value)
                controller.reloadContactsFromRepo()
                contactsRequested = false
            }
            // Do not conceal a rejected mutation with a successful subsequent read.
            if (flush !is ApiResult.Failure) report(result)
        }
        if (controller.snapshot.state in listOf(State.VERIFYING, State.ALERTING, State.AWAITING_HELP) &&
            controller.source.startsWith("PHONE_ONLY") && now >= nextPoll) {
            nextPoll = now + 3000
            val id = remoteEventId()
            val active = repository.active(config)
            if (active is ApiResult.Success && active.value.activeEventId == id && remoteEventId() == id &&
                controller.snapshot.state == State.AWAITING_HELP) {
                controller.caregiverAcknowledged = active.value.caregiverAcknowledged == true
            }
        }
        // Safety outbox always drains before best-effort telemetry. Latest sample only,
        // bounded to one per second; stale/offline samples never accumulate unbounded work.
        if (outbox.pending().isEmpty() && (visible || controller.foreground || controller.backgroundMonitoring)) {
            val b = battery()
            val elapsed = elapsedMs()
            val fresh = latest?.takeIf { elapsed - it.timestampNs / 1_000_000 in 0..500 }
            if (now >= nextHeartbeat && b != null) {
                nextHeartbeat = now + 60000
                heartbeat(b, fresh)?.let { repository.heartbeat(config.deviceId, it) }
            }
            if (now >= nextSensor && fresh != null) {
                telemetry.sample(fresh, b, elapsed)?.let {
                    val result = repository.sensor(it)
                    nextSensor = now + if (result is ApiResult.Failure) 30000 else 1000
                }
            }
        }
    }
    private suspend fun send(op: SyncOperation): ApiResult<*> {
        val result = when (op.kind) {
            OperationKind.EVENT -> repository.event(op.event!!)
            OperationKind.CANCEL, OperationKind.SOS, OperationKind.RESOLVE -> repository.action(op.kind, op.action!!)
            OperationKind.CONTACT_ADD -> repository.addContact(op.userId!!, op.contact!!)
            OperationKind.CONTACT_UPDATE -> repository.updateContact(op.userId!!, op.contact!!)
            OperationKind.CONTACT_DELETE -> repository.deleteContact(op.userId!!, op.contactId!!)
        }
        if (op.kind == OperationKind.SOS && result is ApiResult.Success && op.action?.eventId == remoteEventId()) {
            controller.sosDeliveryMessage = "Máy chủ đã ghi nhận SOS; chưa xác nhận gửi ra ngoài."
        }
        return result
    }
    private fun report(result: ApiResult<*>) {
        controller.apiState = result
        controller.syncStatus = when (result) {
            ApiResult.Loading -> "Đang đồng bộ máy chủ"
            is ApiResult.Success -> "Đã đồng bộ máy chủ • không xác nhận gửi SMS/cuộc gọi"
            is ApiResult.Failure -> when(result.kind) {
                ErrorKind.OFFLINE, ErrorKind.TIMEOUT -> "Chưa kết nối máy chủ • đã giữ thao tác để thử lại"
                ErrorKind.STORAGE -> "Không lưu được hàng đợi • cảnh báo tại chỗ vẫn hoạt động"
                ErrorKind.VALIDATION, ErrorKind.CONFLICT -> "Máy chủ từ chối thao tác: ${result.code ?: result.kind}"
                else -> "Lỗi phản hồi máy chủ • đang giữ thao tác để thử lại"
            }
        }
        if (outbox.rejected().isNotEmpty()) controller.syncStatus += " • ${outbox.rejected().size} thao tác bị từ chối, cần kiểm tra"
    }
    override fun close() {
        closed = true; scope.cancel()
        controller.onSyncStateChanged = null; controller.onPhoneReading = null
        controller.onContactMutation = null; controller.onSyncResume = null
    }
}
