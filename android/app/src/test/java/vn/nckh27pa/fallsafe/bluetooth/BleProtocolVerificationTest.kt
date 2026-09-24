package vn.nckh27pa.fallsafe.bluetooth

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.FallDetectionConfig
import vn.nckh27pa.fallsafe.protocol.Esp32PacketDecoder

class BleProtocolVerificationTest {

    @Test
    fun telemetryDecodePathReassemblesAndDecodesSensorPacket() {
        val telemetryJson = """
            {
              "protocolVersion": 1,
              "deviceId": "FALLSAFE-01A2",
              "sequenceNumber": 18422,
              "timestampMs": 1789363200123,
              "accelXMs2": 0.12,
              "accelYMs2": -0.34,
              "accelZMs2": 9.62,
              "gyroXDps": 1.5,
              "gyroYDps": -4.1,
              "gyroZDps": 0.8,
              "pressurePa": 100842.4,
              "temperatureC": 28.5,
              "altitudeDeltaM": 0.15,
              "batteryPercent": 78,
              "batteryVoltageMv": 3850,
              "isCharging": false,
              "sosButtonPressed": false,
              "sensorQuality": 100
            }
        """.trimIndent()

        val payloadBytes = telemetryJson.toByteArray(Charsets.UTF_8)
        val frames = requireNotNull(Framing.encode(mtu = 23, kind = BleGattUuids.KIND_TELEMETRY, id = 101L, payload = payloadBytes))
        assertTrue(frames.size > 1)

        val reassembler = Reassembler()
        var lastResult: Reassembler.Result? = null
        var now = 1000L
        for (frame in frames) {
            lastResult = reassembler.receive(now, 0L, BleGattUuids.KIND_TELEMETRY, frame)
            now += 5L
        }

        assertNotNull(lastResult)
        assertEquals("COMPLETE", lastResult!!.status)
        assertNotNull(lastResult.payload)

        val reassembledJson = String(lastResult.payload!!, Charsets.UTF_8)
        val decoder = Esp32PacketDecoder()
        val packet = decoder.decodeSensor(reassembledJson)

        assertNotNull("Expected decoded Esp32SensorPacket", packet)
        packet!!
        assertEquals(1, packet.protocolVersion)
        assertEquals("FALLSAFE-01A2", packet.deviceId)
        assertEquals(18422L, packet.sequenceNumber)
        assertEquals(1789363200123L, packet.timestampMs)
        assertEquals(9.62f, packet.accelZMs2, 0.001f)
        assertEquals(-4.1f, packet.gyroYDps, 0.001f)
        assertEquals(100842.4f, packet.pressurePa!!, 0.01f)
        assertEquals(78, packet.batteryPercent)
        assertEquals(3850, packet.batteryVoltageMv)
        assertFalse(packet.isCharging)
        assertFalse(packet.sosButtonPressed)
        assertEquals(100, packet.sensorQuality)
    }

    @Test
    fun eventDecodePathParsesFirmwareFormat() {
        val firmwareEventJson = """
            {
              "protocolVersion": 1,
              "eventId": "evt-1789363200123",
              "deviceId": "ble_server",
              "sequenceNumber": 42,
              "timestampMs": 1789363200123,
              "eventType": "IMPACT_DETECTED",
              "severity": "CRITICAL",
              "alertState": "VERIFYING",
              "peakAccelerationMs2": 28.500,
              "altitudeDeltaM": -0.500
            }
        """.trimIndent()

        val event = BleEventPacket.parse(firmwareEventJson)
        assertNotNull(event)
        event!!
        assertEquals(1, event.protocolVersion)
        assertEquals("evt-1789363200123", event.eventId)
        assertEquals("ble_server", event.deviceId)
        assertEquals(42L, event.sequenceNumber)
        assertEquals(1789363200123L, event.timestampMs)
        assertEquals("IMPACT_DETECTED", event.eventType)
        assertEquals("CRITICAL", event.severity)
        assertEquals("VERIFYING", event.alertState)
        assertEquals(28.5f, event.peakAccelerationMs2!!, 0.001f)
        assertEquals(-0.5f, event.altitudeDeltaM!!, 0.001f)
        assertFalse(event.sosButtonPressed)
    }

    @Test
    fun eventDecodePathParsesContractFormat() {
        val contractEventJson = """
            {
              "protocolVersion": 1,
              "eventId": "evt-esp-18423",
              "deviceId": "FALLSAFE-01A2",
              "sequenceNumber": 18423,
              "timestampMs": 1789363200123,
              "eventType": "SOS_PRESSED",
              "eventSeverity": "CRITICAL",
              "peakAccelerationMs2": 32.1,
              "orientationChangeDeg": 72.0,
              "altitudeDeltaM": -0.85,
              "sosButtonPressed": true,
              "eventConfidence": 98,
              "checksum": "a1b2c3d4"
            }
        """.trimIndent()

        val event = BleEventPacket.parse(contractEventJson)
        assertNotNull(event)
        event!!
        assertEquals(1, event.protocolVersion)
        assertEquals("evt-esp-18423", event.eventId)
        assertEquals("FALLSAFE-01A2", event.deviceId)
        assertEquals(18423L, event.sequenceNumber)
        assertEquals("SOS_PRESSED", event.eventType)
        assertEquals("CRITICAL", event.severity)
        assertTrue(event.sosButtonPressed)
        assertEquals(98, event.eventConfidence)
        assertEquals("a1b2c3d4", event.checksum)
        assertEquals(72.0f, event.orientationChangeDeg!!, 0.001f)
    }

    @Test
    fun eventDecodeRejectsMalformedOrIncompleteJson() {
        assertNull(BleEventPacket.parse(""))
        assertNull(BleEventPacket.parse("   "))
        assertNull(BleEventPacket.parse("not json"))
        assertNull(BleEventPacket.parse("[]"))
        assertNull(BleEventPacket.parse("{}")) // missing eventId and eventType
        assertNull(BleEventPacket.parse("""{"eventId":"evt-1"}""")) // missing eventType
    }

    @Test
    fun profileJsonBuilderRoundTripDefault() {
        val original = BleFallProfile.DEFAULT
        val json = BleProfilePayload.buildJson(original)

        // Ensure all 14 required keys from esp32-ble applyProfileJson exist
        val requiredKeys = listOf(
            "impactAccelerationMs2", "stillnessTargetAccelerationMs2", "stillnessToleranceMs2",
            "postImpactWindowMs", "postImpactStillnessDurationMs", "minimumStillnessSamples",
            "maximumSampleGapMs", "freeFallThresholdMs2", "freeFallMinDurationMs",
            "gyroTurnThresholdDps", "pressureEvidenceMinRisePa", "pressureWindowMs",
            "altitudeDropMinM", "sampleWatchdogMs"
        )
        for (k in requiredKeys) {
            assertTrue("JSON must contain key $k", json.contains("\"$k\":"))
        }

        val parsed = BleProfilePayload.parseJson(json)
        assertNotNull(parsed)
        parsed!!

        assertEquals(original.impactAccelerationMs2, parsed.impactAccelerationMs2, 0.001f)
        assertEquals(original.stillnessTargetAccelerationMs2, parsed.stillnessTargetAccelerationMs2, 0.001f)
        assertEquals(original.stillnessToleranceMs2, parsed.stillnessToleranceMs2, 0.001f)
        assertEquals(original.postImpactWindowMs, parsed.postImpactWindowMs)
        assertEquals(original.postImpactStillnessDurationMs, parsed.postImpactStillnessDurationMs)
        assertEquals(original.minimumStillnessSamples, parsed.minimumStillnessSamples)
        assertEquals(original.maximumSampleGapMs, parsed.maximumSampleGapMs)
        assertEquals(original.freeFallThresholdMs2, parsed.freeFallThresholdMs2, 0.001f)
        assertEquals(original.freeFallMinDurationMs, parsed.freeFallMinDurationMs)
        assertEquals(original.gyroTurnThresholdDps, parsed.gyroTurnThresholdDps, 0.001f)
        assertEquals(original.pressureEvidenceMinRisePa, parsed.pressureEvidenceMinRisePa, 0.001f)
        assertEquals(original.pressureWindowMs, parsed.pressureWindowMs)
        assertEquals(original.altitudeDropMinM, parsed.altitudeDropMinM, 0.001f)
        assertEquals(original.sampleWatchdogMs, parsed.sampleWatchdogMs)
    }

    @Test
    fun profileJsonBuilderRoundTripCustomAndFramed() {
        val custom = BleFallProfile(
            impactAccelerationMs2 = 35.0f,
            stillnessTargetAccelerationMs2 = 9.80f,
            stillnessToleranceMs2 = 1.5f,
            postImpactWindowMs = 4500L,
            postImpactStillnessDurationMs = 1200L,
            minimumStillnessSamples = 10,
            maximumSampleGapMs = 200L,
            freeFallThresholdMs2 = 4.5f,
            freeFallMinDurationMs = 100L,
            gyroTurnThresholdDps = 150.0f,
            pressureEvidenceMinRisePa = 14.5f,
            pressureWindowMs = 6000L,
            altitudeDropMinM = -0.55f,
            sampleWatchdogMs = 80L
        )

        val json = BleProfilePayload.buildJson(custom)
        val parsed = BleProfilePayload.parseJson(json)
        assertNotNull(parsed)
        parsed!!
        assertEquals(35.0f, parsed.impactAccelerationMs2, 0.001f)
        assertEquals(-0.55f, parsed.altitudeDropMinM, 0.001f)
        assertEquals(10, parsed.minimumStillnessSamples)

        // Framed profile round-trip
        val frames = requireNotNull(BleProfilePayload.buildFramed(mtu = 23, messageId = 555L, profile = custom))
        val reassembler = Reassembler()
        var lastRes: Reassembler.Result? = null
        var now = 1000L
        for (f in frames) {
            lastRes = reassembler.receive(now, 0L, BleGattUuids.KIND_COMMAND, f)
            now += 5L
        }

        assertNotNull(lastRes)
        assertEquals("COMPLETE", lastRes!!.status)
        val reassembledJson = String(lastRes.payload!!, Charsets.UTF_8)
        assertEquals(json, reassembledJson)
    }

    @Test
    fun profileConversionFromFallDetectionConfig() {
        val config = FallDetectionConfig(
            impactAccelerationMs2 = 28f,
            stillnessTargetAccelerationMs2 = 9.81f,
            stillnessToleranceMs2 = 1.2f,
            postImpactWindowMs = 3500L,
            postImpactStillnessDurationMs = 1100L,
            minimumStillnessSamples = 7,
            maximumSampleGapMs = 220L,
            phonePressureEvidenceEnabled = false,
            phonePressureMinimumRisePa = 14f
        )

        val profile = BleFallProfile.fromFallDetectionConfig(config)
        assertEquals(28f, profile.impactAccelerationMs2, 0.001f)
        assertEquals(14f, profile.pressureEvidenceMinRisePa, 0.001f)

        val back = profile.toFallDetectionConfig()
        assertEquals(config.impactAccelerationMs2, back.impactAccelerationMs2, 0.001f)
        assertEquals(config.postImpactWindowMs, back.postImpactWindowMs)
        assertEquals(config.phonePressureMinimumRisePa, back.phonePressureMinimumRisePa, 0.001f)
    }
}
