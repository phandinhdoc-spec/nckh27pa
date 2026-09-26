package vn.nckh27pa.fallsafe.protocol

import com.google.gson.Strictness
import com.google.gson.stream.JsonReader
import com.google.gson.stream.JsonToken
import java.io.StringReader

/** Pure v1 sensor parser. No device authentication, transport ACK or stateful dedupe. */
class Esp32PacketDecoder {
    private data class NumberToken(val raw: String)
    fun decodeSensor(json: String): Esp32SensorPacket? {
        if (json.length !in 2..4096) return null
        return try {
            val fields = mutableMapOf<String, Any?>()
            JsonReader(StringReader(json)).use { reader ->
                reader.strictness = Strictness.STRICT
                reader.beginObject()
                while (reader.hasNext()) {
                    val key = reader.nextName()
                    require(!fields.containsKey(key)) { "Duplicate key" }
                    fields[key] = when (reader.peek()) {
                        JsonToken.STRING -> reader.nextString()
                        JsonToken.NUMBER -> NumberToken(reader.nextString())
                        JsonToken.BOOLEAN -> reader.nextBoolean()
                        JsonToken.NULL -> { reader.nextNull(); null }
                        else -> throw IllegalArgumentException("Primitive fields only")
                    }
                }
                reader.endObject()
                require(reader.peek() == JsonToken.END_DOCUMENT)
            }
            fun integer(name: String): Long {
                val raw = (fields[name] as? NumberToken)?.raw ?: error("Required integer")
                require(raw.matches(Regex("0|[1-9][0-9]*")))
                return raw.toLong()
            }
            fun percentage(name: String): Int = integer(name).also { require(it in 0..100) }.toInt()
            // S3 official firmware reports batteryPercent=-1 when no battery is
            // present. Accept exactly -1 (unknown); sensorQuality stays 0..100.
            fun batteryLevel(name: String): Int {
                val raw = (fields[name] as? NumberToken)?.raw ?: error("Required integer")
                if (raw == "-1") return -1
                require(raw.matches(Regex("0|[1-9][0-9]*")))
                return raw.toLong().also { require(it in 0..100) }.toInt()
            }
            fun decimal(name: String): Float = ((fields[name] as? NumberToken)?.raw?.toFloatOrNull()
                ?: error("Required number")).also { require(it.isFinite()) }
            fun optional(name: String): Float? = if (fields[name] == null) null else decimal(name)
            fun flag(name: String): Boolean = fields[name] as? Boolean ?: error("Required boolean")
            val version = integer("protocolVersion"); require(version == 1L)
            val id = fields["deviceId"] as? String ?: error("Required ID")
            require(id.isNotBlank() && id.length <= 128)
            val voltage = if (fields["batteryVoltageMv"] == null) null else integer("batteryVoltageMv").also { require(it <= Int.MAX_VALUE) }.toInt()
            Esp32SensorPacket(version.toInt(), id, integer("sequenceNumber"), integer("timestampMs"),
                decimal("accelXMs2"), decimal("accelYMs2"), decimal("accelZMs2"),
                decimal("gyroXDps"), decimal("gyroYDps"), decimal("gyroZDps"),
                optional("pressurePa"), optional("temperatureC"), optional("altitudeDeltaM"),
                batteryLevel("batteryPercent"), voltage, flag("isCharging"), flag("sosButtonPressed"), percentage("sensorQuality"))
        } catch (_: Exception) {
            // Do not retain raw payload or exception text in logs.
            null
        }
    }
}
data class Esp32SensorPacket(
    val protocolVersion: Int, val deviceId: String, val sequenceNumber: Long, val timestampMs: Long,
    val accelXMs2: Float, val accelYMs2: Float, val accelZMs2: Float,
    val gyroXDps: Float, val gyroYDps: Float, val gyroZDps: Float,
    val pressurePa: Float?, val temperatureC: Float?, val altitudeDeltaM: Float?,
    val batteryPercent: Int, val batteryVoltageMv: Int?, val isCharging: Boolean,
    val sosButtonPressed: Boolean, val sensorQuality: Int
)
