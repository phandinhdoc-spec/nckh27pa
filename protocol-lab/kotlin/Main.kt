import lab.Framing
import lab.Reassembler

private fun unhex(s: String): ByteArray {
    if (s == "-") return byteArrayOf()
    require(s.length % 2 == 0)
    return ByteArray(s.length / 2) { s.substring(it*2,it*2+2).toInt(16).toByte() }
}
private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it.toInt() and 255) }
fun main() {
    val r = Reassembler()
    generateSequence(::readLine).forEach { line ->
        val p = line.split(' ')
        if (p[0] == "E") {
            val frames = Framing.encode(p[1].toInt(),p[2].toInt(),p[3].toLong(),unhex(p[4]))
            println(frames?.let { "FRAMES " + it.joinToString(",", transform=::hex) } ?: "INVALID")
        } else {
            val result = when (p[0]) {
                "R" -> r.receive(p[1].toLong(),p[2].toLong(),p[3].toInt(),unhex(p[4]))
                "T" -> r.tick(p[1].toLong())
                "D" -> r.disconnect()
                else -> error("Unknown host harness command")
            }
            println(result.status + (result.payload?.let { " " + hex(it) } ?: "") + " ${r.active} ${r.generation}")
        }
    }
}
