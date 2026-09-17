package vn.nckh27pa.fallsafe
import org.junit.Test
import org.junit.Assert.*
class OwnershipTests {
    @Test fun failedStartKeepsLocalAndCompletionRestoresWithoutTimer() {
        val calls=mutableListOf<String>(); val owner=SensorOwnership({calls+="start"},{calls+="stop"})
        owner.update(true,false); owner.update(true,false) // rejected asynchronous start: service never took ownership
        assertEquals(listOf("start"), calls)
        owner.update(true,true); assertEquals(listOf("start","stop"), calls)
        owner.update(true,false); assertEquals(listOf("start","stop","start"), calls)
        owner.update(false,false); assertEquals(listOf("start","stop","start","stop"), calls)
        owner.update(false,true); owner.update(false,false)
        assertEquals(4,calls.size)
        owner.update(true,false); assertEquals("start",calls.last())
    }
}
