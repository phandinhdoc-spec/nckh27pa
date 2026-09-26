package vn.nckh27pa.fallsafe.bluetooth

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.protocol.Esp32PacketDecoder
import vn.nckh27pa.fallsafe.protocol.Esp32SensorPacket

/**
 * S3 official-firmware compatibility: plain JSON (no IF-003 framing) on
 * Stream 0002 / Event 0003 / ACK 0006, batteryPercent=-1, stream commands.
 */
class S3CompatibilityTest {

    private val decoder = Esp32PacketDecoder()

    private fun telemetryJson(batteryPercent: Int = 78, sensorQuality: Int = 96): String = """
        {
          "protocolVersion": 1,
          "deviceId": "FALLSAFE-AB12",
          "sequenceNumber": 4242,
          "timestampMs": 1789363200999,
          "accelXMs2": 0.31,
          "accelYMs2": -1.14,
          "accelZMs2": 9.62,
          "gyroXDps": 2.8,
          "gyroYDps": -4.1,
          "gyroZDps": 0.7,
          "pressurePa": 100842.4,
          "temperatureC": 31.2,
          "altitudeDeltaM": -0.46,
          "batteryPercent": $batteryPercent,
          "batteryVoltageMv": 3970,
          "isCharging": false,
          "sosButtonPressed": false,
          "sensorQuality": $sensorQuality
        }
    """.trimIndent()

    private fun eventJson(): String = """
        {
          "protocolVersion": 1,
          "eventId": "evt-s3-001",
          "deviceId": "FALLSAFE-AB12",
          "sequenceNumber": 99,
          "timestampMs": 1789363200999,
          "eventType": "SOS_PRESSED",
          "severity": "CRITICAL",
          "sosButtonPressed": true
        }
    """.trimIndent()

    @Test
    fun plainJsonTelemetryBypassesReassembler() {
        var monotonicTime = 50000L
        val client = FallSafeBleClient(timeProvider = { monotonicTime += 10L; monotonicTime })
        var received: Esp32SensorPacket? = null
        client.onSensorPacket = { received = it }

        // Raw JSON bytes, no IF-003 framing at all.
        client.processNotification(
            BleGattUuids.TELEMETRY_NOTIFY_UUID,
            telemetryJson().toByteArray(Charsets.UTF_8)
        )

        assertNotNull("Plain-JSON telemetry must decode directly", received)
        assertEquals("FALLSAFE-AB12", received!!.deviceId)
        assertEquals(4242L, received!!.sequenceNumber)
        assertEquals(received, client.latestSensorPacket.value)
    }

    @Test
    fun plainJsonEventBypassesReassembler() {
        var monotonicTime = 60000L
        val client = FallSafeBleClient(timeProvider = { monotonicTime += 10L; monotonicTime })
        var received: BleEventPacket? = null
        client.onEventPacket = { received = it }

        client.processNotification(
            BleGattUuids.EVENT_NOTIFY_UUID,
            eventJson().toByteArray(Charsets.UTF_8)
        )

        assertNotNull("Plain-JSON event must decode directly", received)
        assertEquals("evt-s3-001", received!!.eventId)
        assertEquals("SOS_PRESSED", received!!.eventType)
    }

    @Test
    fun plainJsonAckTriggersDebugCallback() {
        val client = FallSafeBleClient(timeProvider = { 70000L })
        var ack: String? = null
        client.onAckJson = { ack = it }

        val ackJson = "{\"protocolVersion\":1,\"commandId\":\"cmd-1\",\"status\":\"OK\"}"
        client.processNotification(
            BleGattUuids.PROFILE_WRITE_UUID,
            ackJson.toByteArray(Charsets.UTF_8)
        )

        assertEquals(ackJson, ack)
    }

    @Test
    fun if003FramedBytesStillUseLegacyPath() {
        var monotonicTime = 80000L
        val client = FallSafeBleClient(timeProvider = { monotonicTime += 10L; monotonicTime })
        var received: Esp32SensorPacket? = null
        client.onSensorPacket = { received = it }

        // IF-003 frame header starts with 0x46 'F', not '{' -> legacy path.
        val frames = requireNotNull(
            Framing.encode(
                mtu = 23,
                kind = BleGattUuids.KIND_TELEMETRY,
                id = 77L,
                payload = telemetryJson().toByteArray(Charsets.UTF_8)
            )
        )
        assertTrue("Fixture must span multiple frames", frames.size > 1)
        client.processNotification(BleGattUuids.TELEMETRY_NOTIFY_UUID, frames.first())
        assertNull("Single IF-003 frame must not emit yet (PENDING)", received)
        for (frame in frames.drop(1)) {
            client.processNotification(BleGattUuids.TELEMETRY_NOTIFY_UUID, frame)
        }
        assertNotNull("Completed IF-003 reassembly must still decode", received)
        assertEquals(4242L, received!!.sequenceNumber)
    }

    @Test
    fun batteryMinusOneDecodesButSensorQualityMinusOneRejects() {
        val packet = decoder.decodeSensor(telemetryJson(batteryPercent = -1))
        assertNotNull("batteryPercent=-1 (S3 no-battery) must decode", packet)
        assertEquals(-1, packet!!.batteryPercent)
        assertEquals(96, packet.sensorQuality)

        assertNull(
            "sensorQuality must stay 0..100",
            decoder.decodeSensor(telemetryJson(sensorQuality = -1))
        )
        assertNull(
            "batteryPercent other negatives still reject",
            decoder.decodeSensor(telemetryJson(batteryPercent = 101))
        )
    }

    @Test
    fun streamCommandJsonFormat() {
        val client = FallSafeBleClient(timeProvider = { 90000L })
        val now = 1789363200999L
        val start = client.buildStreamCommand("START_STREAM", now)
        assertTrue(start.startsWith("{"))
        assertTrue(start.contains("\"commandType\":\"START_STREAM\""))
        assertTrue(start.contains("\"commandId\":\"cmd-$now\""))
        assertTrue(start.contains("\"timestampMs\":$now"))
        assertTrue(start.contains("\"protocolVersion\":1"))

        val stop = client.buildStreamCommand("STOP_STREAM", now)
        assertTrue(stop.contains("\"commandType\":\"STOP_STREAM\""))
    }
}
