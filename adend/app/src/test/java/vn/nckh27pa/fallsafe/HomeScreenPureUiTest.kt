package vn.nckh27pa.fallsafe

import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.device.DeviceDetails
import vn.nckh27pa.fallsafe.device.DeviceValueSource
import vn.nckh27pa.fallsafe.emergency.*

class HomeScreenPureUiTest {

    @Test
    fun testLocationCardViewAllStatesAndNeverFabricatesDeterminedOrThoangTitle() {
        // 1. FRESH with accuracy (age in seconds: 5 <= age < 60)
        val fix10s = LocationFix.validated(10.7769, 106.7009, 15f, 1_000L, LocationSource.PHONE)!!
        val freshState10s = LocationState(fix = fix10s, freshness = LocationFreshness.FRESH)
        val viewFresh10s = resolveLocationCardView(freshState10s, nowMs = 11_000L)
        assertEquals("Đã xác định", viewFresh10s.title)
        assertEquals("± 15 m • Cập nhật 10 giây trước", viewFresh10s.detail)
        assertNull("FRESH must have null hint", viewFresh10s.hint)

        // 2. FRESH vừa xong (age < 5 s)
        val fixVuaXong = LocationFix.validated(10.7769, 106.7009, 15f, 10_000L, LocationSource.PHONE)!!
        val freshStateVuaXong = LocationState(fix = fixVuaXong, freshness = LocationFreshness.FRESH)
        val viewVuaXong = resolveLocationCardView(freshStateVuaXong, nowMs = 12_000L)
        assertEquals("Đã xác định", viewVuaXong.title)
        assertEquals("± 15 m • vừa xong", viewVuaXong.detail)
        assertNull(viewVuaXong.hint)

        // 3. STALE (fix exists + STALE, age >= 60 s, remediation present)
        val fixStale = LocationFix.validated(10.7769, 106.7009, 20f, 1_000L, LocationSource.PHONE)!!
        val hintText = "Ra nơi thoáng hơn có thể tăng độ chính xác; cảnh báo vẫn được gửi."
        val staleState = LocationState(
            fix = fixStale,
            freshness = LocationFreshness.STALE,
            remediation = hintText
        )
        val viewStale = resolveLocationCardView(staleState, nowMs = 181_000L) // 180s = 3 minutes
        assertEquals("Vị trí gần đúng", viewStale.title)
        assertEquals("± 20 m • Cập nhật 3 phút trước", viewStale.detail)
        assertEquals(hintText, viewStale.hint)

        // 4. NO_FIX in-progress (cause NO_FIX and TIMEOUT)
        val noFixState = LocationState(
            fix = null,
            freshness = LocationFreshness.UNAVAILABLE,
            cause = LocationFailureCause.NO_FIX,
            remediation = hintText
        )
        val viewNoFix = resolveLocationCardView(noFixState, nowMs = 10_000L)
        assertEquals("Đang xác định...", viewNoFix.title)
        assertEquals("Vị trí sẽ được gửi ngay khi có", viewNoFix.detail)
        assertNull("NO_FIX in-progress hint must be null", viewNoFix.hint)
        assertNotEquals("Đã xác định", viewNoFix.title)

        val timeoutState = LocationState(
            fix = null,
            freshness = LocationFreshness.UNAVAILABLE,
            cause = LocationFailureCause.TIMEOUT
        )
        val viewTimeout = resolveLocationCardView(timeoutState, nowMs = 10_000L)
        assertEquals("Đang xác định...", viewTimeout.title)
        assertEquals("Vị trí sẽ được gửi ngay khi có", viewTimeout.detail)
        assertNull(viewTimeout.hint)

        // 5. PERMISSION_DENIED
        val permRemediation = "Vào Cài đặt để cấp quyền vị trí."
        val permDeniedState = LocationState(
            fix = null,
            freshness = LocationFreshness.UNAVAILABLE,
            cause = LocationFailureCause.PERMISSION_DENIED,
            remediation = permRemediation
        )
        val viewPerm = resolveLocationCardView(permDeniedState, nowMs = 10_000L)
        assertEquals("Chưa cấp quyền vị trí", viewPerm.title)
        assertEquals("Ứng dụng chưa được phép dùng vị trí", viewPerm.detail)
        assertEquals(permRemediation, viewPerm.hint)

        // 6. PROVIDER_DISABLED
        val providerRemediation = "Bật dịch vụ định vị trong Cài đặt."
        val providerDisabledState = LocationState(
            fix = null,
            freshness = LocationFreshness.UNAVAILABLE,
            cause = LocationFailureCause.PROVIDER_DISABLED,
            remediation = providerRemediation
        )
        val viewDisabled = resolveLocationCardView(providerDisabledState, nowMs = 10_000L)
        assertEquals("Vị trí đang tắt", viewDisabled.title)
        assertEquals("Dịch vụ vị trí của máy đang tắt", viewDisabled.detail)
        assertEquals(providerRemediation, viewDisabled.hint)

        // 7. Fix with accuracyM == null (both FRESH and STALE)
        val fixNoAcc = LocationFix.validated(10.7769, 106.7009, null, 1_000L, LocationSource.PHONE)!!
        val freshNoAccState = LocationState(fix = fixNoAcc, freshness = LocationFreshness.FRESH)
        val viewFreshNoAcc = resolveLocationCardView(freshNoAccState, nowMs = 11_000L)
        assertEquals("Đã xác định", viewFreshNoAcc.title)
        assertEquals("Độ chính xác chưa rõ • Cập nhật 10 giây trước", viewFreshNoAcc.detail)
        assertNull(viewFreshNoAcc.hint)

        val staleNoAccState = LocationState(fix = fixNoAcc, freshness = LocationFreshness.STALE)
        val viewStaleNoAcc = resolveLocationCardView(staleNoAccState, nowMs = 121_000L)
        assertEquals("Vị trí gần đúng", viewStaleNoAcc.title)
        assertEquals("Độ chính xác chưa rõ • Cập nhật 2 phút trước", viewStaleNoAcc.detail)

        // 8. Assert that no state returns a string containing 'thoáng' as an error title
        val allStates = listOf(
            freshState10s,
            freshStateVuaXong,
            staleState,
            noFixState,
            timeoutState,
            permDeniedState,
            providerDisabledState,
            freshNoAccState,
            staleNoAccState
        )
        for (state in allStates) {
            val v = resolveLocationCardView(state, nowMs = 200_000L)
            assertFalse(
                "Title must never contain 'thoáng': ${v.title}",
                v.title.contains("thoáng", ignoreCase = true)
            )
        }
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
