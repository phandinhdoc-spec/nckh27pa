package core

fun main() {
    println("CORE ONLY: fake monotonic clock and fake recipient; no sensors/UI/real transport")
    val clock = object : MonotonicClock {
        var time = 0L
        override fun nowMs() = time
    }
    lateinit var core: AlertCore
    core = AlertCore(clock, AlertSink { alert ->
        println("${core.snapshot().state}: fake recipient accepted event ${alert.eventId}, ${alert.response}; ${alert.locationMessage}")
    })
    println(core.snapshot().state)
    core.suspected(); println(core.snapshot().state)
    core.evidenceConfirmed(); println("${core.snapshot().state}: ${core.snapshot().remainingMs} ms")
    clock.time = 10_000; core.tick(); println("${core.snapshot().state}: ${core.snapshot().status}")
}
