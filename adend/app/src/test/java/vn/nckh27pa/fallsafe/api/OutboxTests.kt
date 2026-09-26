package vn.nckh27pa.fallsafe.api

import core.*
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test

class MemorySyncStore : SyncStore {
    var json: String? = null
    override fun read() = json
    override fun write(value: String): Boolean { json = value; return true }
}
class OutboxTests {
    private val config = ApiConfig("http://localhost/", "phone", "user")
    @Test fun persistenceFifoReconstructionAndStableIdentity() = runBlocking {
        val store = MemorySyncStore()
        val outbox = SyncOutbox(store)
        val mapper = TransitionSync(outbox, config, "session-one") { 100L }
        val verifying = Snapshot(State.VERIFYING, 1, core.Response.UNKNOWN, Status.COUNTDOWN, 10000)
        mapper.observe(verifying); mapper.observe(verifying)
        mapper.observe(verifying.copy(state = State.MONITORING, response = core.Response.SAFE))
        val rebuilt = SyncOutbox(store)
        assertEquals(listOf(OperationKind.EVENT, OperationKind.CANCEL), rebuilt.pending().map { it.kind })
        val id = rebuilt.pending().first().event!!.eventId
        TransitionSync(rebuilt, config, "session-one") { 200L }.observe(verifying)
        assertEquals(2, rebuilt.pending().size)
        val sent = mutableListOf<SyncOperation>()
        rebuilt.flush(1000) { sent += it; ApiResult.Success(Unit) }
        assertEquals(listOf(OperationKind.EVENT, OperationKind.CANCEL), sent.map { it.kind })
        assertEquals(id, sent[1].action!!.eventId)
        assertTrue(SyncOutbox(store).pending().isEmpty())
        val next = SyncOutbox(store)
        TransitionSync(next, config, "session-two") { 300L }.observe(verifying)
        assertNotEquals(id, next.pending().single().event!!.eventId)
    }
    @Test fun retriesKeepHeadAndUseBoundedBackoff() = runBlocking {
        val outbox = SyncOutbox(MemorySyncStore(), jitter = { 1.0 })
        val mapper = TransitionSync(outbox, config, "s") { 1L }
        mapper.observe(Snapshot(State.AWAITING_HELP, 1, core.Response.NEED_HELP, Status.SENT, null))
        mapper.observe(Snapshot(State.MONITORING, 1, core.Response.NEED_HELP, Status.ACKNOWLEDGED, null))
        var calls = 0
        outbox.flush(0) { calls++; ApiResult.Failure(ErrorKind.OFFLINE) }
        outbox.flush(999) { calls++; ApiResult.Success(Unit) }
        assertEquals(1, calls)
        assertEquals(2, outbox.pending().size)
        repeat(10) { outbox.flush(1000000L * (it + 1)) { ApiResult.Failure(ErrorKind.TIMEOUT) } }
        assertTrue(outbox.pending().first().nextAttemptMs <= 10000000L + 30000)
    }
    @Test fun manualTimeoutResolveAndCancelMappedExactlyOnce() {
        val outbox = SyncOutbox(MemorySyncStore())
        val mapper = TransitionSync(outbox, config, "s") { 1L }
        val manual = Snapshot(State.AWAITING_HELP, 1, core.Response.NEED_HELP, Status.SENT, null)
        mapper.observe(manual); mapper.observe(manual)
        mapper.observe(manual.copy(state = State.MONITORING, status = Status.ACKNOWLEDGED))
        val timeout = manual.copy(eventId = 2, response = core.Response.NO_RESPONSE)
        mapper.observe(timeout); mapper.observe(timeout)
        val ops = outbox.pending()
        assertEquals(listOf(OperationKind.SOS, OperationKind.RESOLVE, OperationKind.SOS), ops.map { it.kind })
        assertEquals("MANUAL_APP_BUTTON", ops[0].action!!.triggerSource)
        assertEquals("COUNTDOWN_TIMEOUT", ops[2].action!!.triggerSource)
        assertEquals("NO_RESPONSE", ops[2].action!!.response)
    }
    @Test fun failedPersistenceDoesNotThrowAwayMemoryOrThrowIntoSafetyCore() {
        val store = object : SyncStore { override fun read(): String? = null; override fun write(value: String) = false }
        val outbox = SyncOutbox(store)
        TransitionSync(outbox, config, "s") { 1L }.observe(Snapshot(State.AWAITING_HELP, 1, core.Response.NEED_HELP, Status.SENT, null))
        assertEquals(1, outbox.pending().size)
        assertFalse(outbox.durable)
    }
}
