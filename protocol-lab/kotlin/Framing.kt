package lab

/** Host proposal only. Payload is opaque bytes; caller supplies monotonic milliseconds. */
object Framing {
    private fun put(b: ByteArray, p: Int, n: Long, size: Int) {
        repeat(size) { b[p + it] = (n ushr (8 * it)).toByte() }
    }
    fun encode(mtu: Int, kind: Int, id: Long, payload: ByteArray): List<ByteArray>? {
        if (mtu < 23 || kind !in 1..5 || id !in 1..0xffffffffL || payload.size !in 1..1024) return null
        val chunk = mtu - 19
        val count = (payload.size - 1) / chunk + 1
        return List(count) { i ->
            val offset = i * chunk
            val length = minOf(chunk, payload.size - offset)
            ByteArray(16 + length).also {
                it[0] = 0x46; it[1] = 0x53; it[2] = 1; it[3] = kind.toByte()
                put(it, 4, id, 4); put(it, 8, i.toLong(), 2); put(it, 10, count.toLong(), 2)
                put(it, 12, payload.size.toLong(), 2); put(it, 14, offset.toLong(), 2)
                payload.copyInto(it, 16, offset, offset + length)
            }
        }
    }
}

class Reassembler {
    private class Slot {
        var used = false
        var kind = 0
        var id = 0L
        var start = 0L
        var count = 0
        var total = 0
        var received = 0
        val data = ByteArray(1024)
        val offsets = IntArray(256)
        val lengths = IntArray(256) // zero means unseen
        fun clear() { used = false; lengths.fill(0); received = 0 }
    }
    private val slots = Array(2) { Slot() }
    var generation = 0L; private set
    private var lastNow = 0L
    val active: Int get() = slots.count { it.used }
    data class Result(val status: String, val payload: ByteArray? = null)
    private fun advance(now: Long): Boolean {
        if (now < lastNow) return false
        lastNow = now
        slots.forEach { if (it.used && now - it.start >= 2000) it.clear() }
        return true
    }
    fun tick(now: Long) = Result(if (advance(now)) "TICK" else "CLOCK")
    fun disconnect(): Result {
        check(generation < Long.MAX_VALUE) // fail closed instead of generation wrap
        slots.forEach { it.clear() }; generation++
        return Result("DISCONNECTED")
    }
    fun receive(now: Long, session: Long, characteristicKind: Int, frame: ByteArray): Result {
        if (!advance(now)) return Result("CLOCK")
        if (session != generation) return Result("STALE")
        if (frame.size !in 17..1040) return Result("INVALID")
        fun u(p: Int, size: Int): Long {
            var n = 0L
            repeat(size) { n = n or ((frame[p + it].toLong() and 255) shl (8 * it)) }
            return n
        }
        val kind = u(3,1).toInt(); val id = u(4,4)
        val index = u(8,2).toInt(); val count = u(10,2).toInt()
        val total = u(12,2).toInt(); val offset = u(14,2).toInt(); val length = frame.size - 16
        if (u(0,2) != 0x5346L || u(2,1) != 1L || kind !in 1..5 || kind != characteristicKind ||
            id == 0L || total !in 1..1024 || count !in 1..minOf(256,total) || index >= count || offset + length > total)
            return Result("INVALID")
        var s = slots.firstOrNull { it.used && it.kind == kind && it.id == id }
        if (s == null) {
            s = slots.firstOrNull { !it.used } ?: return Result("CAPACITY")
            s.clear(); s.used = true; s.kind = kind; s.id = id; s.start = now; s.count = count; s.total = total
        }
        fun conflict(): Result { s.clear(); return Result("CONFLICT") }
        if (s.count != count || s.total != total) return conflict()
        if (s.lengths[index] != 0) {
            if (s.offsets[index] != offset || s.lengths[index] != length) return conflict()
            repeat(length) { if (s.data[offset + it] != frame[16 + it]) return conflict() }
            return Result("DUPLICATE")
        }
        repeat(count) { i ->
            if (s.lengths[i] != 0 && offset < s.offsets[i] + s.lengths[i] && s.offsets[i] < offset + length) return conflict()
        }
        frame.copyInto(s.data, offset, 16)
        s.offsets[index] = offset; s.lengths[index] = length; s.received++
        if (s.received != count) return Result("PENDING")
        var end = 0
        repeat(count) { if (s.offsets[it] != end) return conflict(); end += s.lengths[it] }
        if (end != total) return conflict()
        val payload = s.data.copyOf(total)
        s.clear()
        return Result("COMPLETE", payload)
    }
}
