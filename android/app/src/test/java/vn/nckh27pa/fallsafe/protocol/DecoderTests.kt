package vn.nckh27pa.fallsafe.protocol

import com.google.gson.JsonParser
import org.junit.Assert.*
import org.junit.Test

class DecoderTests {
    private val decoder = Esp32PacketDecoder()
    private val fixtures = JsonParser.parseString(requireNotNull(javaClass.getResourceAsStream("/protocol-v1.json")).bufferedReader().use { it.readText() }).asJsonArray
    private fun valid() = fixtures.first().asJsonObject.getAsJsonObject("packet").toString()
    @Test fun sourceFixtureKeepsNamesAndUnits() {
        val p = requireNotNull(decoder.decodeSensor(valid()))
        assertEquals(1, p.protocolVersion); assertEquals("FALLSAFE-01A2",p.deviceId)
        assertEquals(18422L,p.sequenceNumber); assertEquals(1789363200123L,p.timestampMs)
        assertEquals(9.62f,p.accelZMs2); assertEquals(-4.1f,p.gyroYDps)
        assertEquals(100842.4f,p.pressurePa); assertEquals(78,p.batteryPercent)
        assertFalse(p.isCharging); assertFalse(p.sosButtonPressed)
    }
    @Test fun sharedVectorsAcceptOrReject() {
        for (v in fixtures) {
            val o=v.asJsonObject
            if(o.has("packet")) assertEquals(o["id"].asString,o["expect"].asString=="accept",decoder.decodeSensor(o["packet"].toString())!=null)
        }
    }
    @Test fun pureDecodeDoesNotHideDuplicatesFromRepository() {
        val packets=fixtures.last().asJsonObject.getAsJsonArray("packets")
        val a=decoder.decodeSensor(packets[0].toString()); val b=decoder.decodeSensor(packets[1].toString())
        assertNotNull(a); assertEquals(a,b) // caller must dedupe, decoder has no hidden mutable cache
    }
    @Test fun strictMalformedAndDuplicateKeys() {
        for (bad in listOf("", "[]", "null", valid()+" {}", valid().replace("9.62","NaN"), valid().replace("9.62","1e999"), valid().dropLast(1)+",\"deviceId\":\"other\"}", valid().replace("false","\"false\""))) {
            assertNull(bad,decoder.decodeSensor(bad))
        }
    }
    @Test fun integralRequiredAndRanges() {
        val original=fixtures.first().asJsonObject.getAsJsonObject("packet")
        for ((key,value) in listOf("sequenceNumber" to "-1", "sequenceNumber" to "9223372036854775808", "batteryPercent" to "101", "sensorQuality" to "-1", "timestampMs" to "1.5", "accelXMs2" to "null")) {
            val p=original.deepCopy(); p.add(key,JsonParser.parseString(value));assertNull(decoder.decodeSensor(p.toString()))
        }
    }
    @Test fun nullableFieldsStayNullAndBoundsReject() {
        val p=fixtures.first().asJsonObject.getAsJsonObject("packet").deepCopy()
        for(k in listOf("pressurePa","temperatureC","altitudeDeltaM","batteryVoltageMv")) p.remove(k)
        val v=requireNotNull(decoder.decodeSensor(p.toString()))
        assertNull(v.pressurePa); assertNull(v.temperatureC); assertNull(v.altitudeDeltaM); assertNull(v.batteryVoltageMv)
        p.addProperty("deviceId", "");assertNull(decoder.decodeSensor(p.toString()))
        p.addProperty("deviceId", "x".repeat(129));assertNull(decoder.decodeSensor(p.toString()))
        assertNull(decoder.decodeSensor(" ".repeat(4097)))
    }
}
