package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.device.DeviceDetails
import vn.nckh27pa.fallsafe.device.DeviceValueSource
import vn.nckh27pa.fallsafe.emergency.*

class HomeScreenPureUiTest {

    @Test
    fun testLocationCardStatusTruthfulAndNeverFabricatesDetermined() {
        // 1. When fix is null, never says "Đã xác định"
        val noFixState = LocationState(fix = null)
        assertEquals("Chưa có vị trí", resolveLocationCardStatus(noFixState))
        assertNotEquals("Đã xác định", resolveLocationCardStatus(noFixState))

        // 2. When fix is fresh
        val fix = LocationFix.validated(10.7769, 106.7009, 15f, 1000L, LocationSource.PHONE)!!
        val freshState = LocationState(fix = fix, freshness = LocationFreshness.FRESH)
        assertEquals("Vị trí mới", resolveLocationCardStatus(freshState))

        // 3. When fix is stale
        val staleState = LocationState(fix = fix, freshness = LocationFreshness.STALE)
        assertEquals("Vị trí cũ", resolveLocationCardStatus(staleState))
    }

    @Test
    fun testDeviceCardStatusTruthfulAndNeverFabricatesConnectedOrBattery() {
        // 1. Unknown / no data: must NOT fabricate "Đã kết nối" or "Pin tốt"
        val unknownDevice = DeviceDetails.Unknown
        val unknownStatus = resolveDeviceCardStatus(unknownDevice)
        assertFalse("Must not say Pin tốt when no battery data", unknownStatus.contains("Pin tốt"))
        assertFalse("Must not say Đã kết nối when connected is null", unknownStatus.contains("Đã kết nối"))
        assertEquals("Chưa có trạng thái ESP32 từ máy chủ", unknownStatus)

        // 2. Connected with battery
        val connectedWithBattery = DeviceDetails(
            deviceId = "esp32-01",
            connected = true,
            batteryPercent = 85,
            source = DeviceValueSource.ESP32_PACKET
        )
        assertEquals("Pin 85%", resolveDeviceCardStatus(connectedWithBattery))

        // 3. Connected without battery data
        val connectedNoBattery = DeviceDetails(
            deviceId = "esp32-01",
            connected = true,
            batteryPercent = null,
            source = DeviceValueSource.BACKEND_HEARTBEAT
        )
        assertEquals("Đã kết nối", resolveDeviceCardStatus(connectedNoBattery))

        // 4. Disconnected
        val disconnected = DeviceDetails(
            deviceId = "esp32-01",
            connected = false,
            source = DeviceValueSource.BACKEND_HEARTBEAT
        )
        assertEquals("Mất kết nối", resolveDeviceCardStatus(disconnected))
    }

    @Test
    fun testSmsDispatchStatusTextTruthfulForAllStates() {
        val queued = SmsDispatchState("ev-1", "c-1", SmsDeliveryStatus.QUEUED)
        assertTrue(resolveSmsDispatchStatusText(queued).contains("QUEUED"))
        assertFalse(resolveSmsDispatchStatusText(queued).contains("thành công"))

        val sending = SmsDispatchState("ev-1", "c-1", SmsDeliveryStatus.SENDING)
        assertTrue(resolveSmsDispatchStatusText(sending).contains("SENDING"))
        assertFalse(resolveSmsDispatchStatusText(sending).contains("thành công"))

        val sent = SmsDispatchState("ev-1", "c-1", SmsDeliveryStatus.SENT)
        assertTrue(resolveSmsDispatchStatusText(sent).contains("SENT"))
        assertFalse(resolveSmsDispatchStatusText(sent).contains("thành công"))

        val delivered = SmsDispatchState("ev-1", "c-1", SmsDeliveryStatus.DELIVERED, "Nhà mạng đã giao tới người nhận")
        assertTrue(resolveSmsDispatchStatusText(delivered).contains("DELIVERED"))
        assertTrue(resolveSmsDispatchStatusText(delivered).contains("thành công"))
        assertTrue(resolveSmsDispatchStatusText(delivered).contains("Nhà mạng đã giao tới người nhận"))

        val failed = SmsDispatchState("ev-1", "c-1", SmsDeliveryStatus.FAILED, "Lỗi sóng SIM")
        assertTrue(resolveSmsDispatchStatusText(failed).contains("FAILED"))
        assertTrue(resolveSmsDispatchStatusText(failed).contains("Lỗi sóng SIM"))
    }

    @Test
    fun testDeviceDetailsFieldListOnlyIncludesNonNullFields() {
        val emptyDetails = DeviceDetails.Unknown
        val emptyList = buildDeviceDetailsFieldList(emptyDetails)
        assertTrue("Empty device details must yield empty non-null fields", emptyList.isEmpty())

        val partialDetails = DeviceDetails(
            deviceId = "ESP-DEMO-01",
            connected = true,
            batteryPercent = 90,
            firmwareVersion = null,
            gnssStatus = null,
            source = DeviceValueSource.ESP32_PACKET,
            lastHeartbeatMs = 12345678L
        )
        val fields = buildDeviceDetailsFieldList(partialDetails)
        assertEquals(5, fields.size)
        assertTrue(fields.any { it.contains("ESP-DEMO-01") })
        assertTrue(fields.any { it.contains("Đang kết nối") })
        assertTrue(fields.any { it.contains("90%") })
        assertTrue(fields.any { it.contains("Gói tin trực tiếp ESP32") })
        assertTrue(fields.any { it.contains("12345678ms") })
        assertFalse(fields.any { it.contains("firmware") })
        assertFalse(fields.any { it.contains("GNSS") })
    }
}
