package core

/** Nondecreasing elapsed milliseconds, never civil/wall time. */
fun interface MonotonicClock { fun nowMs(): Long }

/** Synchronous acceptance: return means SENT; throw Exception means FAILED. */
fun interface AlertSink { fun send(alert: Alert) }

enum class State { MONITORING, SUSPECTED, VERIFYING, ALERTING, AWAITING_HELP }
enum class Response { UNKNOWN, SAFE, NEED_HELP, NO_RESPONSE }
enum class Status { NOT_REQUIRED, COUNTDOWN, SENDING, SENT, FAILED, ACKNOWLEDGED }
data class Alert(
    val eventId: Long,
    val response: Response,
    val locationMessage: String = "chưa xác định được vị trí"
)
data class Snapshot(
    val state: State,
    val eventId: Long,
    val response: Response,
    val status: Status,
    val remainingMs: Long?
)
data class RecordedEvent(val eventId: Long, val state: State, val status: Status)

/**
 * Confined to the creating thread, including reads and sink callbacks.
 * Host must call tick at the deadline; this core does not schedule background work.
 * Calls reentering from the synchronous sink cannot finish or resend the active event.
 */
class AlertCore(
    private val clock: MonotonicClock,
    private val sink: AlertSink,
    private val timeoutMs: Long = 10_000,
    private val logCapacity: Int = 32
) {
    init {
        require(timeoutMs > 0) { "timeoutMs must be positive" }
        require(logCapacity > 0) { "logCapacity must be positive" }
    }

    private val owner = Thread.currentThread()
    private var state = State.MONITORING
    private var eventId = 0L
    private var response = Response.UNKNOWN
    private var status = Status.NOT_REQUIRED
    private var started = 0L
    private var sending = false
    private val log = ArrayDeque<RecordedEvent>()

    private fun owned() {
        check(Thread.currentThread() === owner) { "Call on owner thread" }
    }

    /** Display-only read; never sends an alert. */
    fun snapshot(): Snapshot {
        owned()
        val remaining = if (state == State.VERIFYING)
            (timeoutMs - (clock.nowMs() - started)).coerceAtLeast(0) else null
        return Snapshot(state, eventId, response, status, remaining)
    }

    fun events(): List<RecordedEvent> {
        owned()
        return log.toList()
    }

    private fun record() {
        log.addLast(RecordedEvent(eventId, state, status))
        while (log.size > logCapacity) log.removeFirst()
    }

    fun suspected() {
        owned()
        if (state == State.MONITORING) {
            eventId++
            response = Response.UNKNOWN
            status = Status.NOT_REQUIRED
            state = State.SUSPECTED
            record()
        }
    }

    fun evidenceConfirmed() {
        owned()
        if (state == State.SUSPECTED) {
            started = clock.nowMs()
            state = State.VERIFYING
            status = Status.COUNTDOWN
            record()
        }
    }

    fun riskCleared() {
        owned()
        if (state == State.SUSPECTED) {
            state = State.MONITORING
            record()
        }
    }

    private fun dispatch(reason: Response) {
        if (state == State.ALERTING || state == State.AWAITING_HELP) return
        if (state == State.MONITORING) eventId++
        response = reason
        state = State.ALERTING
        status = Status.SENDING
        record()
        // Set both state and guard before invoking external code.
        sending = true
        try {
            sink.send(Alert(eventId, reason))
            status = Status.SENT
            state = State.AWAITING_HELP
        } catch (_: Exception) {
            // Never retain transport messages, which may contain private data.
            status = Status.FAILED
        } finally {
            sending = false
        }
        record()
    }

    fun tick() {
        owned()
        if (state == State.VERIFYING && clock.nowMs() - started >= timeoutMs)
            dispatch(Response.NO_RESPONSE)
    }

    fun safe() {
        owned()
        tick()
        if (state == State.VERIFYING || state == State.SUSPECTED) {
            response = Response.SAFE
            status = Status.NOT_REQUIRED
            state = State.MONITORING
            record()
        }
    }

    fun needHelp() {
        owned()
        dispatch(Response.NEED_HELP)
    }

    fun sos() {
        owned()
        dispatch(Response.NEED_HELP)
    }

    fun complete() {
        owned()
        if (!sending && (state == State.AWAITING_HELP || state == State.ALERTING)) {
            state = State.MONITORING
            status = Status.ACKNOWLEDGED
            record()
        }
    }
}
