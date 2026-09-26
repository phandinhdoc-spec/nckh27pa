package vn.nckh27pa.fallsafe.permissions

enum class Capability { CALLING, MESSAGING, LOCATION }

enum class CapabilityDisplayState { GRANTED, CAN_REQUEST, NEEDS_SETTINGS }
enum class LocationPrecision { PRECISE, APPROXIMATE, NONE }

data class CapabilityDisplay(
    val capability: Capability,
    val state: CapabilityDisplayState,
    val reason: String,
    val remediation: String?,
    val locationPrecision: LocationPrecision? = null,
    val note: String? = null
)

data class CapabilitySnapshot(
    val calling: Boolean,
    val messaging: Boolean,
    val location: Boolean,
    val locationPrecision: LocationPrecision = LocationPrecision.NONE
)

fun interface CapabilitySnapshotProvider { fun snapshot(): CapabilitySnapshot }

enum class PermissionRequestResult {
    GRANTED,
    EXPLANATION_REQUIRED,
    SYSTEM_PROMPT_STARTED,
    SETTINGS_REQUIRED
}

data class SosReadiness(
    val capabilities: Map<Capability, CapabilityDisplay>,
    val eligibleContactCount: Int
) {
    val missingCapabilities: Set<Capability>
        get() = capabilities.filterValues { it.state != CapabilityDisplayState.GRANTED }.keys
    val allCapabilitiesGranted: Boolean get() = missingCapabilities.isEmpty()
    val hasEmergencyContact: Boolean get() = eligibleContactCount > 0
    val canSendSmsNow: Boolean
        get() = hasEmergencyContact && capabilities.getValue(Capability.MESSAGING).state == CapabilityDisplayState.GRANTED
    val canCallNow: Boolean
        get() = hasEmergencyContact && capabilities.getValue(Capability.CALLING).state == CapabilityDisplayState.GRANTED
    val canAttachLocationNow: Boolean
        get() = capabilities.getValue(Capability.LOCATION).state == CapabilityDisplayState.GRANTED
}

interface CapabilityPlatform {
    fun isGranted(capability: Capability): Boolean
    fun isSupported(capability: Capability): Boolean = true
    fun shouldShowRationale(capability: Capability): Boolean
    fun locationPrecision(): LocationPrecision =
        if (isGranted(Capability.LOCATION)) LocationPrecision.PRECISE else LocationPrecision.NONE
    fun hasPhoneStateAccess(): Boolean = true
    fun request(capability: Capability)
    fun openSettings(capability: Capability)
}

object UnboundCapabilityPlatform : CapabilityPlatform {
    override fun isGranted(capability: Capability) = false
    override fun shouldShowRationale(capability: Capability) = false
    override fun request(capability: Capability) = Unit
    override fun openSettings(capability: Capability) = Unit
}

interface CapabilityAttemptStore {
    fun wasAttempted(capability: Capability): Boolean
    fun markAttempted(capability: Capability)
}

interface PermissionSetupStore {
    var seen: Boolean
}

class MemoryPermissionSetupStore(override var seen: Boolean = false) : PermissionSetupStore

const val PERMISSION_SETUP_EXPLANATION =
    "Hãy cho phép vị trí, nhắn tin và gọi điện để FallSafe có thể báo người thân khi cần."

class MemoryCapabilityAttemptStore : CapabilityAttemptStore {
    private val attempted = mutableSetOf<Capability>()
    override fun wasAttempted(capability: Capability) = capability in attempted
    override fun markAttempted(capability: Capability) { attempted += capability }
}

class CapabilityAccessController(
    private val platform: CapabilityPlatform,
    private val attempts: CapabilityAttemptStore
) {
    fun display(capability: Capability): CapabilityDisplay {
        val state = when {
            !platform.isSupported(capability) -> CapabilityDisplayState.NEEDS_SETTINGS
            platform.isGranted(capability) -> CapabilityDisplayState.GRANTED
            !attempts.wasAttempted(capability) -> CapabilityDisplayState.CAN_REQUEST
            platform.shouldShowRationale(capability) -> CapabilityDisplayState.CAN_REQUEST
            else -> CapabilityDisplayState.NEEDS_SETTINGS
        }
        val copy = COPY.getValue(capability)
        val remediation = when (state) {
            CapabilityDisplayState.GRANTED -> null
            CapabilityDisplayState.CAN_REQUEST -> copy.requestRemediation
            CapabilityDisplayState.NEEDS_SETTINGS -> if (!platform.isSupported(capability)) "Thiết bị này không hỗ trợ chức năng này." else copy.settingsRemediation
        }
        val precision = if (capability == Capability.LOCATION) platform.locationPrecision() else null
        val note = when {
            capability == Capability.LOCATION && state == CapabilityDisplayState.GRANTED && precision == LocationPrecision.APPROXIMATE ->
                "Vị trí gần đúng vẫn dùng được; người thân có thể thấy khu vực thay vì điểm chính xác."
            capability == Capability.LOCATION && state == CapabilityDisplayState.GRANTED && precision == LocationPrecision.PRECISE ->
                "Vị trí chính xác đã sẵn sàng."
            capability == Capability.MESSAGING && state == CapabilityDisplayState.GRANTED && !platform.hasPhoneStateAccess() ->
                "Chưa cho phép kiểm tra SIM; nếu máy có nhiều SIM, ứng dụng cần chọn SIM để gửi."
            else -> null
        }
        return CapabilityDisplay(capability, state, copy.reason, remediation, precision, note)
    }

    fun request(capability: Capability, explanationAcknowledged: Boolean): PermissionRequestResult {
        if (!platform.isSupported(capability)) return PermissionRequestResult.SETTINGS_REQUIRED
        if (platform.isGranted(capability)) return PermissionRequestResult.GRANTED
        if (!explanationAcknowledged) return PermissionRequestResult.EXPLANATION_REQUIRED
        if (display(capability).state == CapabilityDisplayState.NEEDS_SETTINGS) {
            return PermissionRequestResult.SETTINGS_REQUIRED
        }
        attempts.markAttempted(capability)
        platform.request(capability)
        return PermissionRequestResult.SYSTEM_PROMPT_STARTED
    }

    /** This is intentionally separate from [request] so a denial can never open Settings itself. */
    fun openSettings(capability: Capability) = platform.openSettings(capability)

    /** Pure readiness snapshot: this method never requests access or opens Settings. */
    fun checkSosReadiness(eligibleContactCount: Int): SosReadiness = SosReadiness(
        capabilities = Capability.entries.associateWith(::display),
        eligibleContactCount = eligibleContactCount.coerceAtLeast(0)
    )

    fun snapshot(): CapabilitySnapshot {
        val snap = CapabilitySnapshot(
            calling = platform.isSupported(Capability.CALLING) && platform.isGranted(Capability.CALLING),
            messaging = platform.isSupported(Capability.MESSAGING) && platform.isGranted(Capability.MESSAGING),
            location = platform.isGranted(Capability.LOCATION),
            locationPrecision = platform.locationPrecision()
        )
        // TEMPORARY DIAGNOSTIC
        try {
            val locD = display(Capability.LOCATION)
            val msgD = display(Capability.MESSAGING)
            val callD = display(Capability.CALLING)
            vn.nckh27pa.fallsafe.AndroidTrace.logCapabilities(
                locationCapability = locD.state.name,
                smsCapability = msgD.state.name,
                callCapability = callD.state.name,
                snapshotCalling = snap.calling,
                snapshotMessaging = snap.messaging,
                snapshotLocation = snap.location,
                locationPrecision = snap.locationPrecision.name
            )
        } catch (_: Throwable) {}
        return snap
    }

    private data class Copy(
        val reason: String,
        val requestRemediation: String,
        val settingsRemediation: String
    )

    private companion object {
        val COPY = mapOf(
            Capability.CALLING to Copy(
                reason = "Cho phép gọi giúp FallSafe mở cuộc gọi khẩn cấp bạn chủ động chọn.",
                requestRemediation = "Chọn tiếp tục để Android hỏi quyền gọi điện.",
                settingsRemediation = "Quyền này đang bị tắt. Hãy bật trong Cài đặt."
            ),
            Capability.MESSAGING to Copy(
                reason = "Cho phép nhắn tin giúp FallSafe gửi cảnh báo tới người thân và chọn đúng SIM khi cần.",
                requestRemediation = "Chọn tiếp tục để Android hỏi quyền nhắn tin.",
                settingsRemediation = "Quyền này đang bị tắt. Hãy bật trong Cài đặt."
            ),
            Capability.LOCATION to Copy(
                reason = "Cho phép vị trí giúp cảnh báo kèm tọa độ để người thân dễ tìm bạn hơn.",
                requestRemediation = "Chọn tiếp tục để Android hỏi quyền vị trí khi đang dùng ứng dụng.",
                settingsRemediation = "Quyền này đang bị tắt. Hãy bật trong Cài đặt."
            )
        )
    }
}
