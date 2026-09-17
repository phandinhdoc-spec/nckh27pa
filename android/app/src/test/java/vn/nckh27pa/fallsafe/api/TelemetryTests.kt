package vn.nckh27pa.fallsafe.api

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.PhoneSensorPacket
import com.google.gson.GsonBuilder

class TelemetryTests {
    @Test fun gyroIsAllNullUnitsPreservedAndSubmissionBounded() {
        val telemetry = PhoneTelemetry(ApiConfig("http://localhost/", "phone", "user"), SyncOutbox(MemorySyncStore()))
        val battery = BatteryReading(67, 4100, true)
        val p = PhoneSensorPacket(1, 1234, 1f, 2f, 9.81f, gyroXDps = 50f, pressurePa = 100000f)
        val first = telemetry.sample(p, battery, 0)!!
        assertEquals(9.81f, first.accelZMs2)
        assertEquals(100000f, first.pressurePa)
        assertNull(first.gyroXDps); assertNull(first.gyroYDps); assertNull(first.gyroZDps)
        assertNull(first.location)
        assertEquals(67, first.batteryPercent)
        val json = GsonBuilder().serializeNulls().create().toJson(first)
        assertTrue(json.contains("\"gyroXDps\":null"))
        assertNull(telemetry.sample(p, battery, 999))
        val next = telemetry.sample(p.copy(gyroYDps = 20f, gyroZDps = 30f), battery, 1000)!!
        assertEquals(50f, next.gyroXDps)
        assertTrue(next.sequenceNumber > first.sequenceNumber)
    }
    @Test fun unavailableRequiredBatterySkipsRatherThanFabricatesAndSequenceSurvivesRestart() {
        val store = MemorySyncStore()
        val config = ApiConfig("http://localhost/", "phone", "user")
        val p = PhoneSensorPacket(1, 1, 0f, 0f, 9f)
        val t = PhoneTelemetry(config, SyncOutbox(store))
        assertNull(t.sample(p, null, 0))
        val first = t.sample(p, BatteryReading(50, null, false), 0)!!
        val next = PhoneTelemetry(config, SyncOutbox(store)).sample(p, BatteryReading(50, null, false), 0)!!
        assertTrue(next.sequenceNumber > first.sequenceNumber)
        assertNull(t.sample(p.copy(accelXMs2 = Float.NaN), BatteryReading(50, null, false), 1000))
    }
}
