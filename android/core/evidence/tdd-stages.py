"""Historical staged TDD driver, run once after stage 01. Not the final test runner."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
def stage(n, test, old, new):
    path = root/'tests/CoreTests.kt'
    path.write_text(path.read_text().replace('    // NEXT', test+'\n    // NEXT'))
    def run(color):
        result = subprocess.run([str(root/'run-tests.sh')], capture_output=True, text=True)
        (root/f'evidence/{n}-{color}.log').write_text(result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
        print(f'{n} {color}: exit {result.returncode}', flush=True)
        return result.returncode
    assert run('red') != 0
    path = root/'src/Core.kt'; source = path.read_text(); assert old in source
    path.write_text(source.replace(old,new))
    assert run('green') == 0
stage('02', '''    test("deadline 10s, no response, absent location, one send") {
        val clock = FakeClock(500); val sent = mutableListOf<Alert>()
        val c = AlertCore(clock, AlertSink { sent += it }); c.suspected(); c.evidenceConfirmed()
        clock.time = 10_499; c.tick(); check(sent.isEmpty())
        clock.time = 10_500; c.tick(); c.tick(); c.evidenceConfirmed(); c.suspected()
        check(sent.size == 1); check(sent.single().response == Response.NO_RESPONSE)
        check(sent.single().locationMessage == "chưa xác định được vị trí")
        check(c.snapshot().state == State.AWAITING_HELP); check(c.snapshot().status == Status.SENT)
        check(c.events().map { it.state }.contains(State.ALERTING))
    }''', 'fun tick() { owned() }', '''private fun dispatch(reason: Response) {
        if (state == State.ALERTING || state == State.AWAITING_HELP) return
        if (state == State.MONITORING) eventId++
        response = reason; state = State.ALERTING; status = Status.SENDING; record()
        sink.send(Alert(eventId, reason))
        status = Status.SENT; state = State.AWAITING_HELP; record()
    }
    fun tick() { owned(); if (state == State.VERIFYING && clock.nowMs() - started >= timeoutMs) dispatch(Response.NO_RESPONSE) }''')
stage('03', '''    test("SAFE before deadline cancels; at and after deadline preserves alert") {
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
    }''', 'fun safe() { owned() }', '''fun safe() { owned(); tick(); if (state == State.VERIFYING || state == State.SUSPECTED) {
        response = Response.SAFE; status = Status.NOT_REQUIRED; state = State.MONITORING; record()
    } }''')
stage('04', '''    test("immediate NEED_HELP and SOS, dedup, completion permits new event") {
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
    }''', '''fun needHelp() { owned() }
    fun sos() { owned() }
    fun complete() { owned() }''', '''fun needHelp() { owned(); dispatch(Response.NEED_HELP) }
    fun sos() { owned(); dispatch(Response.NEED_HELP) }
    fun complete() { owned(); if (!sending && (state == State.AWAITING_HELP || state == State.ALERTING)) {
        state = State.MONITORING; status = Status.ACKNOWLEDGED; record()
    } }''')
stage('05', '''    test("sink failure stays FAILED; reentrant calls cannot send or complete") {
        var calls = 0; lateinit var c: AlertCore
        c = AlertCore(FakeClock(), AlertSink {
            calls++; check(c.snapshot().state == State.ALERTING); check(c.snapshot().status == Status.SENDING)
            c.complete(); c.sos(); c.needHelp(); c.safe(); c.tick(); c.suspected(); c.evidenceConfirmed()
            throw IllegalStateException("private transport details")
        })
        c.sos(); check(calls == 1); check(c.snapshot().status == Status.FAILED)
        check(c.snapshot().state == State.ALERTING); c.sos(); check(calls == 1)
        check(!c.events().toString().contains("private")); c.complete(); c.sos(); check(calls == 2)
    }''', '''sink.send(Alert(eventId, reason))
        status = Status.SENT; state = State.AWAITING_HELP; record()''', '''sending = true
        try {
            sink.send(Alert(eventId, reason))
            status = Status.SENT; state = State.AWAITING_HELP
        } catch (_: Exception) {
            status = Status.FAILED
        } finally { sending = false }
        record()''')
stage('06', '''    test("invalid timeout and log capacity rejected") {
        for (timeout in listOf(0L, -1L)) check(runCatching { AlertCore(FakeClock(), AlertSink {}, timeout) }.exceptionOrNull() is IllegalArgumentException)
        for (capacity in listOf(0, -1)) check(runCatching { AlertCore(FakeClock(), AlertSink {}, logCapacity = capacity) }.exceptionOrNull() is IllegalArgumentException)
    }''', 'private val owner = Thread.currentThread()', '''init { require(timeoutMs > 0) { "timeoutMs must be positive" }; require(logCapacity > 0) { "logCapacity must be positive" } }
    private val owner = Thread.currentThread()''')
