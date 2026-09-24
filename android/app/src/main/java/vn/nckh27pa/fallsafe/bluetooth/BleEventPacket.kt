package vn.nckh27pa.fallsafe.bluetooth

import com.google.gson.JsonParser

/**
 * Event frame packet emitted by ESP32 over BLE kind 2 (EVENT_NOTIFY).
 */
data class BleEventPacket(
    val protocolVersion: Int,
    val eventId: String,
    val deviceId: String,
    val sequenceNumber: Long,
    val timestampMs: Long,
    val eventType: String,
    val severity: String,
    val alertState: String? = null,
    val peakAccelerationMs2: Float? = null,
    val orientationChangeDeg: Float? = null,
    val altitudeDeltaM: Float? = null,
    val sosButtonPressed: Boolean = false,
    val eventConfidence: Int = 100,
    val checksum: String? = null,
    val rawJson: String = ""
) {
    companion object {
        fun parse(json: String): BleEventPacket? {
            if (json.isBlank()) return null
            return try {
                val element = JsonParser.parseString(json)
                if (!element.isJsonObject) return null
                val obj = element.asJsonObject

                val version = obj.get("protocolVersion")?.let { if (it.isJsonNull) 1 else it.asInt } ?: 1
                val eventId = obj.get("eventId")?.let { if (it.isJsonNull) null else it.asString } ?: return null
                if (eventId.isBlank()) return null
                val deviceId = obj.get("deviceId")?.let { if (it.isJsonNull) null else it.asString } ?: "ble_server"
                val seq = obj.get("sequenceNumber")?.let { if (it.isJsonNull) 0L else it.asLong } ?: 0L
                val time = obj.get("timestampMs")?.let { if (it.isJsonNull) 0L else it.asLong } ?: 0L
                val eventType = obj.get("eventType")?.let { if (it.isJsonNull) null else it.asString } ?: return null
                if (eventType.isBlank()) return null

                val severity = (obj.get("severity") ?: obj.get("eventSeverity"))?.let {
                    if (it.isJsonNull) "CRITICAL" else it.asString
                } ?: "CRITICAL"

                val alertState = obj.get("alertState")?.let { if (it.isJsonNull) null else it.asString }
                val peakAccel = obj.get("peakAccelerationMs2")?.let { if (it.isJsonNull) null else it.asFloat }
                val orient = obj.get("orientationChangeDeg")?.let { if (it.isJsonNull) null else it.asFloat }
                val alt = obj.get("altitudeDeltaM")?.let { if (it.isJsonNull) null else it.asFloat }
                val sos = obj.get("sosButtonPressed")?.let { if (it.isJsonNull) false else it.asBoolean } ?: false
                val conf = obj.get("eventConfidence")?.let { if (it.isJsonNull) 100 else it.asInt } ?: 100
                val checksum = obj.get("checksum")?.let { if (it.isJsonNull) null else it.asString }

                BleEventPacket(
                    protocolVersion = version,
                    eventId = eventId,
                    deviceId = deviceId,
                    sequenceNumber = seq,
                    timestampMs = time,
                    eventType = eventType,
                    severity = severity,
                    alertState = alertState,
                    peakAccelerationMs2 = peakAccel,
                    orientationChangeDeg = orient,
                    altitudeDeltaM = alt,
                    sosButtonPressed = sos,
                    eventConfidence = conf,
                    checksum = checksum,
                    rawJson = json
                )
            } catch (_: Exception) {
                null
            }
        }
    }
}
