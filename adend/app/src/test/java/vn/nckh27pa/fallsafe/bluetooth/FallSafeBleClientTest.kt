package vn.nckh27pa.fallsafe.bluetooth

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.protocol.Esp32SensorPacket
import java.util.UUID

class FallSafeBleClientTest {

    @Test
    fun authoritativeUuidConstantsMatchContract() {
        assertEquals(
            UUID.fromString("7d2a0001-6f45-4c2b-9a1e-38a8f5c10001"),
            BleGattUuids.SERVICE_UUID
        )
        assertEquals(
            UUID.fromString("7d2a0002-6f45-4c2b-9a1e-38a8f5c10001"),
            BleGattUuids.TELEMETRY_NOTIFY_UUID
        )
        assertEquals(
            UUID.fromString("7d2a0003-6f45-4c2b-9a1e-38a8f5c10001"),
            BleGattUuids.EVENT_NOTIFY_UUID
        )
        assertEquals(
            UUID.fromString("7d2a0005-6f45-4c2b-9a1e-38a8f5c10001"),
            BleGattUuids.REQUEST_WRITE_UUID
        )
        assertEquals(
            UUID.fromString("7d2a0006-6f45-4c2b-9a1e-38a8f5c10001"),
            BleGattUuids.PROFILE_WRITE_UUID
        )
        assertEquals(
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"),
            BleGattUuids.CCCD_UUID
        )

        assertEquals(1, BleGattUuids.KIND_TELEMETRY)
        assertEquals(2, BleGattUuids.KIND_EVENT)
        assertEquals(3, BleGattUuids.KIND_STATUS)
        assertEquals(4, BleGattUuids.KIND_COMMAND)
        assertEquals(5, BleGattUuids.KIND_ACK)
    }

    @Test
    fun clientInitialStateAndDisconnect() {
        val client = FallSafeBleClient()
        assertEquals(BleConnectionState.Disconnected, client.connectionState.value)
        assertNull(client.latestSensorPacket.value)

        var stateNotification: BleConnectionState? = null
        client.onConnectionStateChanged = { stateNotification = it }

        client.disconnect()
        assertEquals(BleConnectionState.Disconnected, client.connectionState.value)
        assertEquals(BleConnectionState.Disconnected, stateNotification)
    }

    @Test
    fun telemetryNotificationProcessingUpdatesStateFlowAndCallback() {
        var monotonicTime = 10000L
        val client = FallSafeBleClient(timeProvider = { monotonicTime += 10L; monotonicTime })

        var callbackReceived: Esp32SensorPacket? = null
        client.onSensorPacket = { callbackReceived = it }

        val json = """
            {
              "protocolVersion": 1,
              "deviceId": "FALLSAFE-01A2",
              "sequenceNumber": 777,
              "timestampMs": 1789363200500,
              "accelXMs2": 0.05,
              "accelYMs2": 0.02,
              "accelZMs2": 9.80,
              "gyroXDps": 0.1,
              "gyroYDps": 0.2,
              "gyroZDps": -0.3,
              "batteryPercent": 92,
              "isCharging": false,
              "sosButtonPressed": false,
              "sensorQuality": 95
            }
        """.trimIndent()

        val frames = requireNotNull(
            Framing.encode(
                mtu = 23,
                kind = BleGattUuids.KIND_TELEMETRY,
                id = 12L,
                payload = json.toByteArray(Charsets.UTF_8)
            )
        )

        for (frame in frames) {
            client.processNotification(BleGattUuids.TELEMETRY_NOTIFY_UUID, frame)
        }

        assertNotNull("Callback should have received packet", callbackReceived)
        val packet = callbackReceived!!
        assertEquals(1, packet.protocolVersion)
        assertEquals("FALLSAFE-01A2", packet.deviceId)
        assertEquals(777L, packet.sequenceNumber)
        assertEquals(9.80f, packet.accelZMs2, 0.001f)
        assertEquals(92, packet.batteryPercent)

        // Latest sensor packet StateFlow must also be updated
        assertEquals(packet, client.latestSensorPacket.value)
    }

    @Test
    fun eventNotificationProcessingEmitsEventPacketAndTriggersCallback() {
        var monotonicTime = 20000L
        val client = FallSafeBleClient(timeProvider = { monotonicTime += 10L; monotonicTime })

        var eventReceived: BleEventPacket? = null
        client.onEventPacket = { eventReceived = it }

        val eventJson = """
            {
              "protocolVersion": 1,
              "eventId": "evt-esp-999",
              "deviceId": "ble_server",
              "sequenceNumber": 88,
              "timestampMs": 1789363200700,
              "eventType": "IMPACT_DETECTED",
              "severity": "CRITICAL",
              "alertState": "VERIFYING",
              "peakAccelerationMs2": 31.4,
              "altitudeDeltaM": -0.75
            }
        """.trimIndent()

        val frames = requireNotNull(
            Framing.encode(
                mtu = 23,
                kind = BleGattUuids.KIND_EVENT,
                id = 25L,
                payload = eventJson.toByteArray(Charsets.UTF_8)
            )
        )

        for (frame in frames) {
            client.processNotification(BleGattUuids.EVENT_NOTIFY_UUID, frame)
        }

        assertNotNull("Callback should have received event", eventReceived)
        val event = eventReceived!!
        assertEquals("evt-esp-999", event.eventId)
        assertEquals("ble_server", event.deviceId)
        assertEquals(88L, event.sequenceNumber)
        assertEquals("IMPACT_DETECTED", event.eventType)
        assertEquals("CRITICAL", event.severity)
        assertEquals("VERIFYING", event.alertState)
        assertEquals(31.4f, event.peakAccelerationMs2!!, 0.001f)
        assertEquals(-0.75f, event.altitudeDeltaM!!, 0.001f)
    }

    @Test
    fun profileSerializationAndFraming() {
        val profile = BleFallProfile(
            impactAccelerationMs2 = 26.0f,
            stillnessTargetAccelerationMs2 = 9.81f,
            stillnessToleranceMs2 = 1.0f,
            postImpactWindowMs = 3000L,
            postImpactStillnessDurationMs = 1000L,
            minimumStillnessSamples = 6,
            maximumSampleGapMs = 250L,
            freeFallThresholdMs2 = 4.90f,
            freeFallMinDurationMs = 80L,
            gyroTurnThresholdDps = 120.0f,
            pressureEvidenceMinRisePa = 12.0f,
            pressureWindowMs = 5000L,
            altitudeDropMinM = -0.40f,
            sampleWatchdogMs = 100L
        )

        val json = BleProfilePayload.buildJson(profile)
        assertTrue(json.startsWith("{") && json.endsWith("}"))
        assertTrue(json.contains("\"impactAccelerationMs2\":26.000"))
        assertTrue(json.contains("\"altitudeDropMinM\":-0.400"))

        val framed = BleProfilePayload.buildFramed(mtu = 23, messageId = 1L, profile = profile)
        assertNotNull(framed)
        assertTrue(framed!!.size > 1)
    }
}
