"""Historical continuation: execute only after stages 01–06."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
def run(name):
    result = subprocess.run([str(root/'run-tests.sh')], capture_output=True, text=True)
    (root/f'evidence/{name}.log').write_text(result.stdout+result.stderr+f'\nEXIT {result.returncode}\n')
    print(name, result.returncode, flush=True)
    return result.returncode
p = root/'tests/CoreTests.kt'
p.write_text(p.read_text().replace('    // NEXT', '''    test("bounded sink and state history retain latest records; copies isolated") {
        val sink = RecordingSink(2); val c = AlertCore(FakeClock(), sink, logCapacity = 3)
        repeat(20) { c.sos(); c.complete() }
        check(sink.alerts().map { it.eventId } == listOf(19L, 20L))
        check(c.events().size == 3); check(c.events().last().status == Status.ACKNOWLEDGED)
        val copy = c.events().toMutableList(); copy.clear(); check(c.events().size == 3)
        val alerts = sink.alerts().toMutableList(); alerts.clear(); check(sink.alerts().size == 2)
        check(runCatching { RecordingSink(0) }.exceptionOrNull() is IllegalArgumentException)
        check(runCatching { RecordingSink(-1) }.exceptionOrNull() is IllegalArgumentException)
    }
    // NEXT'''))
assert run('07-red') != 0
(root/'src/RecordingSink.kt').write_text('''package core

/** In-memory fake recipient; use only on its creating thread. No contact/location data. */
class RecordingSink(private val capacity: Int = 32) : AlertSink {
    init { require(capacity > 0) { "capacity must be positive" } }
    private val owner = Thread.currentThread()
    private val recorded = ArrayDeque<Alert>()
    private fun owned() { check(Thread.currentThread() === owner) { "Call on owner thread" } }
    override fun send(alert: Alert) {
        owned(); recorded.addLast(alert)
        while (recorded.size > capacity) recorded.removeFirst()
    }
    fun alerts(): List<Alert> { owned(); return recorded.toList() }
}
''')
assert run('07-green') == 0
p.write_text(p.read_text().replace('    // NEXT', '''    test("wall time jumps do not advance injected monotonic deadline") {
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
    // NEXT'''))
p.write_text(p.read_text()+'''\n// Test-only independent time domains: core receives only monotonic time.
class DualClock(var monotonic: Long = 0, var wall: Long = 0) : MonotonicClock {
    override fun nowMs() = monotonic
}
''')
assert run('08-contract-checks') == 0
