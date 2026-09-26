package vn.nckh27pa.fallsafe.bluetooth

import com.google.gson.JsonObject
import com.google.gson.JsonParser
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class If003FramingTest {

    private fun loadGoldenFixture(name: String): JsonObject {
        val stream = javaClass.getResourceAsStream("/golden.json")
        val text = if (stream != null) {
            stream.bufferedReader().use { it.readText() }
        } else {
            val candidates = listOf(
                File("../../protocol-lab/fixtures/golden.json"),
                File("../protocol-lab/fixtures/golden.json"),
                File("protocol-lab/fixtures/golden.json")
            )
            candidates.firstOrNull { it.exists() }?.readText()
                ?: error("golden.json fixture not found")
        }
        val array = JsonParser.parseString(text).asJsonArray
        return array.first { it.asJsonObject["name"].asString == name }.asJsonObject
    }

    private fun hexToBytes(hex: String): ByteArray {
        val clean = hex.trim()
        val len = clean.length
        val data = ByteArray(len / 2)
        var i = 0
        while (i < len) {
            data[i / 2] = ((Character.digit(clean[i], 16) shl 4) + Character.digit(clean[i + 1], 16)).toByte()
            i += 2
        }
        return data
    }

    private fun bytesToHex(bytes: ByteArray): String {
        return bytes.joinToString("") { "%02x".format(it) }
    }

    @Test
    fun goldenScenarioMtu23V1MatchesAndReassembles() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val mtu = fixture["mtu"].asInt
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val expectedPayloadHex = fixture["payload"].asString
        val expectedPayloadBytes = hexToBytes(expectedPayloadHex)
        val expectedPayloadStr = String(expectedPayloadBytes, Charsets.UTF_8)
        assertEquals("{\"protocolVersion\":1}", expectedPayloadStr)

        val expectedFramesHex = fixture["frames"].asJsonArray.map { it.asString }
        assertEquals(6, expectedFramesHex.size)

        // 1. Assert Framing.encode reproduces the exact golden frames
        val encodedFrames = requireNotNull(Framing.encode(mtu, kind, id, expectedPayloadBytes))
        assertEquals(6, encodedFrames.size)
        for (i in encodedFrames.indices) {
            assertEquals("Frame $i mismatch", expectedFramesHex[i], bytesToHex(encodedFrames[i]))
        }

        // 2. Assert Reassembler processes the frames into the complete payload
        val reassembler = Reassembler()
        var now = 1000L
        for (i in 0 until 5) {
            val res = reassembler.receive(now, 0L, kind, encodedFrames[i])
            assertEquals("Expected PENDING for frame $i", "PENDING", res.status)
            assertNull(res.payload)
            now += 10L
        }

        val lastRes = reassembler.receive(now, 0L, kind, encodedFrames[5])
        assertEquals("COMPLETE", lastRes.status)
        assertNotNull(lastRes.payload)
        assertArrayEquals(expectedPayloadBytes, lastRes.payload)
        assertEquals(expectedPayloadStr, String(lastRes.payload!!, Charsets.UTF_8))
    }

    @Test
    fun goldenScenarioMtu185Utf8Reassembles() {
        val fixture = loadGoldenFixture("mtu185-utf8")
        val mtu = fixture["mtu"].asInt
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val expectedPayloadBytes = hexToBytes(fixture["payload"].asString)
        val expectedFramesHex = fixture["frames"].asJsonArray.map { it.asString }

        val encodedFrames = requireNotNull(Framing.encode(mtu, kind, id, expectedPayloadBytes))
        assertEquals(expectedFramesHex.size, encodedFrames.size)
        for (i in encodedFrames.indices) {
            assertEquals(expectedFramesHex[i], bytesToHex(encodedFrames[i]))
        }

        val reassembler = Reassembler()
        var now = 1000L
        val res1 = reassembler.receive(now, 0L, kind, encodedFrames[0])
        assertEquals("PENDING", res1.status)

        val res2 = reassembler.receive(now + 15L, 0L, kind, encodedFrames[1])
        assertEquals("COMPLETE", res2.status)
        assertArrayEquals(expectedPayloadBytes, res2.payload)
    }

    @Test
    fun goldenScenarioMtu23MaxReassemblesAll256Frames() {
        val fixture = loadGoldenFixture("mtu23-max")
        val mtu = fixture["mtu"].asInt
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val expectedPayloadBytes = hexToBytes(fixture["payload"].asString)
        assertEquals(1024, expectedPayloadBytes.size)

        val encodedFrames = requireNotNull(Framing.encode(mtu, kind, id, expectedPayloadBytes))
        assertEquals(256, encodedFrames.size)

        val reassembler = Reassembler()
        var now = 1000L
        for (i in 0 until 255) {
            val res = reassembler.receive(now, 0L, kind, encodedFrames[i])
            assertEquals("PENDING", res.status)
            now += 5L
        }

        val finalRes = reassembler.receive(now + 5L, 0L, kind, encodedFrames[255])
        assertEquals("COMPLETE", finalRes.status)
        assertArrayEquals(expectedPayloadBytes, finalRes.payload)
    }

    @Test
    fun outOfOrderFramesReassembleSuccessfully() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val expectedPayloadBytes = hexToBytes(fixture["payload"].asString)
        val frames = requireNotNull(Framing.encode(23, kind, id, expectedPayloadBytes))

        val reassembler = Reassembler()
        val permutedIndices = listOf(2, 0, 4, 1, 5, 3)
        var now = 2000L

        for (i in 0 until permutedIndices.size - 1) {
            val idx = permutedIndices[i]
            val res = reassembler.receive(now, 0L, kind, frames[idx])
            assertEquals("PENDING", res.status)
            now += 10L
        }

        val lastIdx = permutedIndices.last()
        val finalRes = reassembler.receive(now, 0L, kind, frames[lastIdx])
        assertEquals("COMPLETE", finalRes.status)
        assertArrayEquals(expectedPayloadBytes, finalRes.payload)
    }

    @Test
    fun duplicateFrameReturnsDuplicateStatus() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val frames = requireNotNull(Framing.encode(23, kind, id, hexToBytes(fixture["payload"].asString)))

        val reassembler = Reassembler()
        val res1 = reassembler.receive(1000L, 0L, kind, frames[0])
        assertEquals("PENDING", res1.status)

        val dupRes = reassembler.receive(1050L, 0L, kind, frames[0])
        assertEquals("DUPLICATE", dupRes.status)
    }

    @Test
    fun timeoutAfter2000MsClearsSlot() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val frames = requireNotNull(Framing.encode(23, kind, id, hexToBytes(fixture["payload"].asString)))

        val reassembler = Reassembler()
        val res1 = reassembler.receive(1000L, 0L, kind, frames[0])
        assertEquals("PENDING", res1.status)
        assertEquals(1, reassembler.active)

        // Advance clock by 2000ms: now - start >= 2000
        val tickRes = reassembler.tick(3000L)
        assertEquals("TICK", tickRes.status)
        assertEquals(0, reassembler.active)
    }

    @Test
    fun nonMonotonicClockReturnsClockStatus() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val frames = requireNotNull(Framing.encode(23, kind, id, hexToBytes(fixture["payload"].asString)))

        val reassembler = Reassembler()
        reassembler.receive(5000L, 0L, kind, frames[0])

        // Clock goes backwards
        val clockRes = reassembler.receive(4999L, 0L, kind, frames[1])
        assertEquals("CLOCK", clockRes.status)

        val tickClockRes = reassembler.tick(4998L)
        assertEquals("CLOCK", tickClockRes.status)
    }

    @Test
    fun disconnectClearsSlotsAndIncrementsGeneration() {
        val fixture = loadGoldenFixture("mtu23-v1")
        val kind = fixture["kind"].asInt
        val id = fixture["id"].asLong
        val frames = requireNotNull(Framing.encode(23, kind, id, hexToBytes(fixture["payload"].asString)))

        val reassembler = Reassembler()
        assertEquals(0L, reassembler.generation)

        reassembler.receive(1000L, 0L, kind, frames[0])
        assertEquals(1, reassembler.active)

        val discRes = reassembler.disconnect()
        assertEquals("DISCONNECTED", discRes.status)
        assertEquals(1L, reassembler.generation)
        assertEquals(0, reassembler.active)

        // Receiving with old session generation 0 returns STALE
        val staleRes = reassembler.receive(1100L, 0L, kind, frames[1])
        assertEquals("STALE", staleRes.status)

        // Receiving with new generation 1 is accepted
        val newRes = reassembler.receive(1200L, 1L, kind, frames[0])
        assertEquals("PENDING", newRes.status)
        assertEquals(1, reassembler.active)
    }
}
