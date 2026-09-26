package core

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
