package core

class FakeClock(var time: Long = 0) : MonotonicClock { override fun nowMs() = time }
fun main() {
    var passed = 0
    fun test(name: String, body: () -> Unit) { body(); println("PASS $name"); passed++ }
    test("suspected, clear, confirmed and duplicate callbacks") {
        val clock = FakeClock(); val c = AlertCore(clock, AlertSink {})
        c.evidenceConfirmed(); check(c.snapshot().state == State.MONITORING)
        c.suspected(); check(c.snapshot().state == State.SUSPECTED)
        c.riskCleared(); check(c.snapshot().state == State.MONITORING)
        c.suspected(); c.evidenceConfirmed(); check(c.snapshot().remainingMs == 10_000L)
        clock.time = 9_999; c.suspected(); c.evidenceConfirmed(); c.riskCleared()
        check(c.snapshot().state == State.VERIFYING); check(c.snapshot().remainingMs == 1L)
    }
    test("deadline 10s, no response, absent location, one send") {
        val clock = FakeClock(500); val sent = mutableListOf<Alert>()
        val c = AlertCore(clock, AlertSink { sent += it }); c.suspected(); c.evidenceConfirmed()
        clock.time = 10_499; c.tick(); check(sent.isEmpty())
        clock.time = 10_500; c.tick(); c.tick(); c.evidenceConfirmed(); c.suspected()
        check(sent.size == 1); check(sent.single().response == Response.NO_RESPONSE)
        check(sent.single().locationMessage == "chưa xác định được vị trí")
        check(c.snapshot().state == State.AWAITING_HELP); check(c.snapshot().status == Status.SENT)
        check(c.events().map { it.state }.contains(State.ALERTING))
    }
    test("SAFE before deadline cancels; at and after deadline preserves alert") {
        for (time in listOf(9_999L, 10_000L, 10_001L)) {
            val clock = FakeClock(); val sent = mutableListOf<Alert>(); val c = AlertCore(clock, AlertSink { sent += it })
            c.suspected(); c.evidenceConfirmed(); clock.time = time; c.safe()
            if (time < 10_000) {
                check(c.snapshot().state == State.MONITORING); check(c.snapshot().response == Response.SAFE)
                clock.time = 20_000; c.tick(); check(sent.isEmpty())
            } else {
                check(sent.size == 1); c.safe(); check(c.snapshot().status == Status.SENT)
                check(c.snapshot().response == Response.NO_RESPONSE)
            }
        }
    }
    test("immediate NEED_HELP and SOS, dedup, completion permits new event") {
        for (action in listOf("help", "sos")) for (initial in 0..2) {
            val sent = mutableListOf<Alert>(); val c = AlertCore(FakeClock(), AlertSink { sent += it })
            if (initial >= 1) c.suspected(); if (initial == 2) c.evidenceConfirmed()
            c.complete(); if (action == "help") c.needHelp() else c.sos()
            check(sent.size == 1); check(sent.single().response == Response.NEED_HELP)
            c.sos(); c.needHelp(); c.safe(); check(sent.size == 1)
            c.complete(); check(c.snapshot().state == State.MONITORING)
            check(c.snapshot().status == Status.ACKNOWLEDGED)
            c.sos(); check(sent.size == 2); check(sent[0].eventId != sent[1].eventId)
        }
    }
    test("sink failure stays FAILED; reentrant calls cannot send or complete") {
        var calls = 0; lateinit var c: AlertCore
        val during = mutableListOf<Snapshot>()
        c = AlertCore(FakeClock(), AlertSink {
            calls++; during += c.snapshot()
            c.complete(); c.sos(); c.needHelp(); c.safe(); c.tick(); c.suspected(); c.evidenceConfirmed()
            throw IllegalStateException("private transport details")
        })
        c.sos(); check(calls == 1); check(c.snapshot().status == Status.FAILED)
        check(during.single().state == State.ALERTING)
        check(during.single().status == Status.SENDING)
        check(c.snapshot().state == State.ALERTING); c.sos(); check(calls == 1)
        check(!c.events().toString().contains("private")); c.complete(); c.sos(); check(calls == 2)
    }
    test("invalid timeout and log capacity rejected") {
        for (timeout in listOf(0L, -1L)) check(runCatching { AlertCore(FakeClock(), AlertSink {}, timeout) }.exceptionOrNull() is IllegalArgumentException)
        for (capacity in listOf(0, -1)) check(runCatching { AlertCore(FakeClock(), AlertSink {}, logCapacity = capacity) }.exceptionOrNull() is IllegalArgumentException)
    }
    test("bounded sink and state history retain latest records; copies isolated") {
        val sink = RecordingSink(2); val c = AlertCore(FakeClock(), sink, logCapacity = 3)
        repeat(20) { c.sos(); c.complete() }
        check(sink.alerts().map { it.eventId } == listOf(19L, 20L))
        check(c.events().size == 3); check(c.events().last().status == Status.ACKNOWLEDGED)
        val copy = c.events().toMutableList(); copy.clear(); check(c.events().size == 3)
        val alerts = sink.alerts().toMutableList(); alerts.clear(); check(sink.alerts().size == 2)
        check(runCatching { RecordingSink(0) }.exceptionOrNull() is IllegalArgumentException)
        check(runCatching { RecordingSink(-1) }.exceptionOrNull() is IllegalArgumentException)
    }
    test("wall time jumps do not advance injected monotonic deadline") {
        val clock = DualClock(); val sink = RecordingSink(); val c = AlertCore(clock, sink)
        c.suspected(); c.evidenceConfirmed()
        clock.wall = Long.MAX_VALUE; c.tick(); check(c.snapshot().remainingMs == 10_000L)
        clock.wall = Long.MIN_VALUE; clock.monotonic = 9_999; c.tick(); check(sink.alerts().isEmpty())
        clock.monotonic = 10_000; c.tick(); check(sink.alerts().size == 1)
    }
    test("all public core and recording sink access is owner-thread only") {
        val sink = RecordingSink(); val c = AlertCore(FakeClock(), sink)
        val actions = listOf<() -> Unit>({ c.suspected() }, { c.evidenceConfirmed() },
            { c.riskCleared() }, { c.tick() }, { c.safe() }, { c.needHelp() }, { c.sos() },
            { c.complete() }, { c.snapshot() }, { c.events() }, { sink.alerts() },
            { sink.send(Alert(1, Response.NEED_HELP)) })
        var failures = 0
        val t = Thread { for (action in actions) if (runCatching(action).exceptionOrNull() is IllegalStateException) failures++ }
        t.start(); t.join(); check(failures == actions.size)
        check(c.snapshot().state == State.MONITORING); check(sink.alerts().isEmpty())
    }
    // CORE-002: regression tests added after implementation, not test-first evidence.
    test("successful reentrant sink preserves sending event and completes after return") {
        val sent = mutableListOf<Alert>(); val during = mutableListOf<Snapshot>()
        lateinit var c: AlertCore
        c = AlertCore(FakeClock(), AlertSink { alert ->
            sent += alert
            during += c.snapshot()
            c.complete(); c.sos(); c.needHelp(); c.safe(); c.tick()
            c.suspected(); c.evidenceConfirmed(); c.riskCleared()
            during += c.snapshot()
        })
        c.sos()
        check(sent.size == 1)
        val first = sent.single()
        check(during == List(2) {
            Snapshot(State.ALERTING, first.eventId, Response.NEED_HELP, Status.SENDING, null)
        })
        check(c.snapshot() == Snapshot(State.AWAITING_HELP, first.eventId, Response.NEED_HELP, Status.SENT, null))
        check(c.events().map { it.status } == listOf(Status.SENDING, Status.SENT))
        c.sos(); c.needHelp(); c.tick(); check(sent.size == 1)
        c.complete()
        check(c.snapshot().state == State.MONITORING); check(c.snapshot().status == Status.ACKNOWLEDGED)
        c.sos(); check(sent.size == 2); check(sent.last().eventId != first.eventId)
        check(c.snapshot().state == State.AWAITING_HELP); check(c.snapshot().status == Status.SENT)
    }
    test("deadline at Long.MAX_VALUE preserves remaining time and SAFE boundary") {
        for (safeBefore in listOf(true, false)) {
            val clock = FakeClock(Long.MAX_VALUE - 10_000); val sink = RecordingSink()
            val c = AlertCore(clock, sink); c.suspected(); c.evidenceConfirmed()
            check(c.snapshot().remainingMs == 10_000L)
            clock.time = Long.MAX_VALUE - 1; c.tick()
            check(c.snapshot().remainingMs == 1L); check(sink.alerts().isEmpty())
            if (safeBefore) c.safe()
            clock.time = Long.MAX_VALUE
            if (safeBefore) {
                c.tick(); check(sink.alerts().isEmpty())
                check(c.snapshot().state == State.MONITORING); check(c.snapshot().response == Response.SAFE)
            } else {
                check(c.snapshot().remainingMs == 0L); check(sink.alerts().isEmpty())
                c.safe(); c.tick(); c.sos()
                check(sink.alerts().single().response == Response.NO_RESPONSE)
                check(c.snapshot().state == State.AWAITING_HELP); check(c.snapshot().status == Status.SENT)
            }
        }
    }
    test("Long.MAX_VALUE timeout uses valid elapsed time without deadline overflow") {
        for (start in listOf(0L, 1L)) {
            val clock = FakeClock(start); val sink = RecordingSink()
            val c = AlertCore(clock, sink, timeoutMs = Long.MAX_VALUE)
            c.suspected(); c.evidenceConfirmed()
            check(c.snapshot().remainingMs == Long.MAX_VALUE)
            clock.time = Long.MAX_VALUE - 1; c.tick()
            check(c.snapshot().remainingMs == start + 1); check(sink.alerts().isEmpty())
            clock.time = Long.MAX_VALUE
            check(c.snapshot().remainingMs == start); check(sink.alerts().isEmpty())
            c.tick(); c.tick()
            if (start == 0L) {
                check(sink.alerts().single().response == Response.NO_RESPONSE)
                check(c.snapshot().state == State.AWAITING_HELP); check(c.snapshot().status == Status.SENT)
            } else {
                // Deadline is outside the valid clock domain; do not wrap the clock.
                check(sink.alerts().isEmpty()); check(c.snapshot().state == State.VERIFYING)
                check(c.snapshot().status == Status.COUNTDOWN); check(c.snapshot().remainingMs == 1L)
            }
        }
    }
    println("PASS $passed tests")
}

// Test-only independent time domains: core receives only monotonic time.
class DualClock(var monotonic: Long = 0, var wall: Long = 0) : MonotonicClock {
    override fun nowMs() = monotonic
}
