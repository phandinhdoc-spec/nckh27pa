package vn.nckh27pa.fallsafe

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.emergency.SosDispatchReport
import vn.nckh27pa.fallsafe.emergency.SosStep
import vn.nckh27pa.fallsafe.emergency.SosStepResult
import vn.nckh27pa.fallsafe.emergency.SosStepStatus
import vn.nckh27pa.fallsafe.permissions.Capability
import vn.nckh27pa.fallsafe.permissions.CapabilityDisplay
import vn.nckh27pa.fallsafe.permissions.CapabilityDisplayState
import vn.nckh27pa.fallsafe.permissions.LocationPrecision

class PermissionCenterUiTest {

    private val forbiddenConstants = listOf("ACCESS_", "SEND_SMS", "CALL_PHONE")

    private fun assertNoTechnicalConstants(text: String, context: String) {
        for (constant in forbiddenConstants) {
            assertFalse(
                "User-visible text for $context must not contain '$constant': $text",
                text.contains(constant)
            )
        }
    }

    // 1. Granted label
    @Test
    fun `resolveCapabilityStateLabel returns Da cap when granted and precise or standard`() {
        val display = CapabilityDisplay(
            capability = Capability.CALLING,
            state = CapabilityDisplayState.GRANTED,
            reason = "Cho phép gọi",
            remediation = null
        )
        val label = resolveCapabilityStateLabel(display)
        assertEquals("Đã cấp", label)
        assertNoTechnicalConstants(label, "calling granted label")
    }

    // 2. Approximate location label
    @Test
    fun `resolveCapabilityStateLabel returns Da cap vi tri gan dung for approximate location`() {
        val display = CapabilityDisplay(
            capability = Capability.LOCATION,
            state = CapabilityDisplayState.GRANTED,
            reason = "Cho phép vị trí",
            remediation = null,
            locationPrecision = LocationPrecision.APPROXIMATE,
            note = "Vị trí gần đúng vẫn dùng được; người thân có thể thấy khu vực thay vì điểm chính xác."
        )
        val label = resolveCapabilityStateLabel(display)
        assertEquals("Đã cấp (vị trí gần đúng)", label)
        assertNoTechnicalConstants(label, "location approximate label")

        val desc = resolveCapabilityDescription(display)
        assertTrue(desc.contains("gần đúng"))
        assertNoTechnicalConstants(desc, "location approximate description")
    }

    // 3. Never-asked / can ask again label
    @Test
    fun `resolveCapabilityStateLabel returns Chua cap when can request`() {
        val display = CapabilityDisplay(
            capability = Capability.MESSAGING,
            state = CapabilityDisplayState.CAN_REQUEST,
            reason = "Cho phép nhắn tin",
            remediation = "Chọn tiếp tục để Android hỏi quyền nhắn tin."
        )
        val label = resolveCapabilityStateLabel(display)
        assertEquals("Chưa cấp", label)
        assertNoTechnicalConstants(label, "messaging can request label")
    }

    // 4. Permanently denied label
    @Test
    fun `resolveCapabilityStateLabel returns Bi tu choi - can mo Cai dat ung dung when needs settings`() {
        val display = CapabilityDisplay(
            capability = Capability.LOCATION,
            state = CapabilityDisplayState.NEEDS_SETTINGS,
            reason = "Cho phép vị trí",
            remediation = "Quyền vị trí đã bị từ chối lâu dài. Bạn có thể mở Cài đặt ứng dụng để bật lại."
        )
        val label = resolveCapabilityStateLabel(display)
        assertEquals("Bị từ chối — cần mở Cài đặt ứng dụng", label)
        assertNoTechnicalConstants(label, "location needs settings label")
    }

    // 5. Button choice per state
    @Test
    fun `resolveCapabilityActionLabel returns correct button action per state`() {
        assertEquals("CẤP QUYỀN", resolveCapabilityActionLabel(CapabilityDisplayState.CAN_REQUEST))
        assertEquals("MỞ CÀI ĐẶT ỨNG DỤNG", resolveCapabilityActionLabel(CapabilityDisplayState.NEEDS_SETTINGS))
        assertNull(resolveCapabilityActionLabel(CapabilityDisplayState.GRANTED))
    }

    // 6. Capability item titles
    @Test
    fun `resolveCapabilityItemTitle returns expected Vietnamese title with emoji`() {
        assertEquals("📍 Vị trí", resolveCapabilityItemTitle(Capability.LOCATION))
        assertEquals("📞 Điện thoại", resolveCapabilityItemTitle(Capability.CALLING))
        assertEquals("💬 SMS", resolveCapabilityItemTitle(Capability.MESSAGING))
    }

    // 7. No technical permission constants in any capability strings
    @Test
    fun `no user-visible capability string contains Android permission constants`() {
        for (cap in Capability.entries) {
            val title = resolveCapabilityItemTitle(cap)
            assertNoTechnicalConstants(title, "title for $cap")

            for (state in CapabilityDisplayState.entries) {
                val display = CapabilityDisplay(
                    capability = cap,
                    state = state,
                    reason = "Lý do cấp quyền cho $cap",
                    remediation = "Khắc phục cho $cap",
                    locationPrecision = if (cap == Capability.LOCATION) LocationPrecision.APPROXIMATE else null,
                    note = "Ghi chú cho $cap"
                )
                assertNoTechnicalConstants(resolveCapabilityStateLabel(display), "stateLabel for $cap, $state")
                resolveCapabilityActionLabel(state)?.let {
                    assertNoTechnicalConstants(it, "actionLabel for $state")
                }
                assertNoTechnicalConstants(resolveCapabilityDescription(display), "description for $cap, $state")
                assertNoTechnicalConstants(resolveCapabilityTalkBackLabel(display), "talkBackLabel for $cap, $state")
            }
        }
    }

    // 8. First-run explanation strings and condition
    @Test
    fun `first-run explanation strings are non-technical and match specification`() {
        assertEquals("Cho phép ứng dụng hoạt động khi khẩn cấp", FIRST_RUN_EXPLANATION_TITLE)
        assertEquals(
            "Để gửi cảnh báo khi phát hiện té ngã, ứng dụng cần quyền định vị, gửi tin nhắn và gọi người thân.",
            FIRST_RUN_EXPLANATION_BODY
        )
        assertEquals("TIẾP TỤC CẤP QUYỀN", FIRST_RUN_CONTINUE_BUTTON)
        assertEquals("ĐỂ SAU", FIRST_RUN_LATER_BUTTON)

        assertNoTechnicalConstants(FIRST_RUN_EXPLANATION_TITLE, "FIRST_RUN_EXPLANATION_TITLE")
        assertNoTechnicalConstants(FIRST_RUN_EXPLANATION_BODY, "FIRST_RUN_EXPLANATION_BODY")
        assertNoTechnicalConstants(FIRST_RUN_CONTINUE_BUTTON, "FIRST_RUN_CONTINUE_BUTTON")
        assertNoTechnicalConstants(FIRST_RUN_LATER_BUTTON, "FIRST_RUN_LATER_BUTTON")
    }

    @Test
    fun `shouldShowFirstRunExplanation shows dialog in non-emergency states when unseen and ungranted`() {
        // (a) Must show in SAFE and DEVICE_DISCONNECTED when unseen and has ungranted capability
        assertTrue(shouldShowFirstRunExplanation(MainScreenStatus.SAFE, permissionSetupSeen = false, hasUngrantedCapability = true))
        assertTrue(shouldShowFirstRunExplanation(MainScreenStatus.DEVICE_DISCONNECTED, permissionSetupSeen = false, hasUngrantedCapability = true))

        // (b) Must NEVER show during emergencies (CRITICAL SAFETY RULE)
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.WARNING_COUNTDOWN, permissionSetupSeen = false, hasUngrantedCapability = true))
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.SOS_SENT, permissionSetupSeen = false, hasUngrantedCapability = true))
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.HELP_ACKNOWLEDGED, permissionSetupSeen = false, hasUngrantedCapability = true))

        // (c) seen=true suppresses it in both SAFE and DEVICE_DISCONNECTED
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.SAFE, permissionSetupSeen = true, hasUngrantedCapability = true))
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.DEVICE_DISCONNECTED, permissionSetupSeen = true, hasUngrantedCapability = true))

        // All capabilities granted suppresses it
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.SAFE, permissionSetupSeen = false, hasUngrantedCapability = false))
        assertFalse(shouldShowFirstRunExplanation(MainScreenStatus.DEVICE_DISCONNECTED, permissionSetupSeen = false, hasUngrantedCapability = false))
    }

    // 9. Per-step SOS feedback helpers
    @Test
    fun `resolveSosStepStatusLabel maps all SosStepStatus to plain Vietnamese`() {
        assertEquals("Thành công", resolveSosStepStatusLabel(SosStepStatus.SUCCESS))
        assertEquals("Một phần", resolveSosStepStatusLabel(SosStepStatus.PARTIAL))
        assertEquals("Chưa cấp quyền", resolveSosStepStatusLabel(SosStepStatus.PERMISSION_MISSING))
        assertEquals("Không khả dụng", resolveSosStepStatusLabel(SosStepStatus.UNAVAILABLE))
        assertEquals("Thất bại", resolveSosStepStatusLabel(SosStepStatus.FAILED))
        assertEquals("Bỏ qua", resolveSosStepStatusLabel(SosStepStatus.SKIPPED))

        for (status in SosStepStatus.entries) {
            val label = resolveSosStepStatusLabel(status)
            assertNoTechnicalConstants(label, "SosStepStatus $status")
        }
    }

    @Test
    fun `resolveSosDispatchSummaryText reflects truthful step outcomes`() {
        val fallback = "SOS đã ghi nhận tại chỗ."
        assertEquals(fallback, resolveSosDispatchSummaryText(null, fallback))

        val report = SosDispatchReport(
            eventId = "evt-123",
            steps = listOf(
                SosStepResult(SosStep.LOCATION, SosStepStatus.SUCCESS, "Đã xác định vị trí."),
                SosStepResult(SosStep.SMS, SosStepStatus.PERMISSION_MISSING, "Chưa cho phép gửi tin nhắn."),
                SosStepResult(SosStep.VOICE_CALL, SosStepStatus.UNAVAILABLE, "Gọi tự động chưa cấu hình."),
                SosStepResult(SosStep.MAP_LINK, SosStepStatus.FAILED, "Chưa gửi được bản đồ.")
            )
        )

        val summary = resolveSosDispatchSummaryText(report, fallback)
        assertTrue(summary.contains("Vị trí: Thành công"))
        assertTrue(summary.contains("Tin nhắn: Chưa cấp quyền"))
        assertTrue(summary.contains("Cuộc gọi trợ giúp: Không khả dụng"))
        assertTrue(summary.contains("Liên kết bản đồ: Thất bại"))

        // Ensure truthful reporting: never claim success on a missing permission step
        assertFalse(summary.contains("Tin nhắn: Thành công"))
        assertNoTechnicalConstants(summary, "SOS dispatch summary")
    }

    // 10. resolveRequestOutcome mapping
    @Test
    fun `resolveRequestOutcome maps each PermissionRequestResult correctly`() {
        assertEquals(
            PermissionFollowUp.NONE,
            resolveRequestOutcome(vn.nckh27pa.fallsafe.permissions.PermissionRequestResult.GRANTED)
        )
        assertEquals(
            PermissionFollowUp.NONE,
            resolveRequestOutcome(vn.nckh27pa.fallsafe.permissions.PermissionRequestResult.SYSTEM_PROMPT_STARTED)
        )
        assertEquals(
            PermissionFollowUp.SHOW_SETTINGS_DIALOG,
            resolveRequestOutcome(vn.nckh27pa.fallsafe.permissions.PermissionRequestResult.SETTINGS_REQUIRED)
        )
        assertEquals(
            PermissionFollowUp.REQUEST_WITH_EXPLANATION,
            resolveRequestOutcome(vn.nckh27pa.fallsafe.permissions.PermissionRequestResult.EXPLANATION_REQUIRED)
        )
    }

    // 11. exact sentence in NEEDS_SETTINGS copy for all three capabilities
    @Test
    fun `exact sentence Quyen nay dang bi tat Hay bat trong Cai dat appears in NEEDS_SETTINGS copy for all three capabilities`() {
        val expectedSentence = "Quyền này đang bị tắt. Hãy bật trong Cài đặt."
        assertEquals(expectedSentence, PERMISSION_NEEDS_SETTINGS_BODY)
        assertNoTechnicalConstants(expectedSentence, "PERMISSION_NEEDS_SETTINGS_BODY")

        val access = vn.nckh27pa.fallsafe.permissions.CapabilityAccessController(
            object : vn.nckh27pa.fallsafe.permissions.CapabilityPlatform {
                override fun isGranted(capability: Capability) = false
                override fun shouldShowRationale(capability: Capability) = false
                override fun request(capability: Capability) = Unit
                override fun openSettings(capability: Capability) = Unit
            },
            object : vn.nckh27pa.fallsafe.permissions.CapabilityAttemptStore {
                override fun wasAttempted(capability: Capability) = true
                override fun markAttempted(capability: Capability) = Unit
            }
        )

        for (cap in Capability.entries) {
            val display = access.display(cap)
            assertEquals(CapabilityDisplayState.NEEDS_SETTINGS, display.state)
            val remediation = display.remediation
            org.junit.Assert.assertNotNull("Remediation for $cap must not be null in NEEDS_SETTINGS", remediation)
            assertTrue(
                "Remediation for $cap must contain exact sentence '$expectedSentence', but was: '$remediation'",
                remediation!!.contains(expectedSentence)
            )
            assertNoTechnicalConstants(remediation, "remediation for $cap")
        }
    }
}
