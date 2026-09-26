package vn.nckh27pa.fallsafe

import android.content.Context
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import kotlin.math.roundToInt
import vn.nckh27pa.fallsafe.device.DeviceDetails
import vn.nckh27pa.fallsafe.device.DeviceValueSource
import vn.nckh27pa.fallsafe.emergency.*
import vn.nckh27pa.fallsafe.permissions.Capability

// High-contrast, WCAG AAA compliant color scheme for elderly readability
val SafeGreen = Color(0xFF1B5E20)
val SafeGreenContainer = Color(0xFFE8F5E9)
val SafeGreenBorder = Color(0xFF81C784)

val WarningOrange = Color(0xFFBF360C)
val WarningOrangeContainer = Color(0xFFFFF3E0)
val WarningOrangeBorder = Color(0xFFFFB74D)

val SosRed = Color(0xFFD32F2F)
val SosRedDark = Color(0xFFB71C1C)
val SosRedContainer = Color(0xFFFFEBEE)
val SosRedBorder = Color(0xFFEF9A9A)

val DisconnectedGray = Color(0xFF37474F)
val DisconnectedGrayContainer = Color(0xFFECEFF1)
val DisconnectedGrayBorder = Color(0xFFB0BEC5)

val ContactBlue = Color(0xFF0D47A1)
val ContactBlueContainer = Color(0xFFE3F2FD)
val ContactBlueBorder = Color(0xFF90CAF9)

const val FIRST_RUN_EXPLANATION_TITLE = "Cho phép ứng dụng hoạt động khi khẩn cấp"
const val FIRST_RUN_EXPLANATION_BODY =
    "Để gửi cảnh báo khi phát hiện té ngã, ứng dụng cần quyền định vị, gửi tin nhắn và gọi người thân."
const val FIRST_RUN_CONTINUE_BUTTON = "TIẾP TỤC CẤP QUYỀN"
const val FIRST_RUN_LATER_BUTTON = "ĐỂ SAU"

internal fun shouldShowFirstRunExplanation(
    status: MainScreenStatus,
    permissionSetupSeen: Boolean,
    hasUngrantedCapability: Boolean
): Boolean = (status == MainScreenStatus.SAFE || status == MainScreenStatus.DEVICE_DISCONNECTED) &&
    !permissionSetupSeen &&
    hasUngrantedCapability

internal fun resolveSosStepStatusLabel(status: SosStepStatus): String = when (status) {
    SosStepStatus.SUCCESS -> "Thành công"
    SosStepStatus.PARTIAL -> "Một phần"
    SosStepStatus.PERMISSION_MISSING -> "Chưa cấp quyền"
    SosStepStatus.UNAVAILABLE -> "Không khả dụng"
    SosStepStatus.FAILED -> "Thất bại"
    SosStepStatus.SKIPPED -> "Bỏ qua"
}

internal fun resolveSosDispatchSummaryText(report: SosDispatchReport?, fallbackMessage: String): String {
    if (report == null || report.steps.isEmpty()) return fallbackMessage
    return report.steps.joinToString(" • ") { result ->
        "${result.step.vietnameseLabel}: ${resolveSosStepStatusLabel(result.status)}"
    }
}

internal data class LocationCardView(
    val title: String,
    val detail: String,
    val hint: String?
)

internal fun resolveLocationCardView(
    loc: LocationState,
    nowMs: Long = System.currentTimeMillis()
): LocationCardView {
    val fix = loc.fix
    if (fix == null) {
        return when (loc.cause) {
            LocationFailureCause.PERMISSION_DENIED -> LocationCardView(
                title = "Chưa cấp quyền vị trí",
                detail = "Ứng dụng chưa được phép dùng vị trí",
                hint = loc.remediation
            )
            LocationFailureCause.PROVIDER_DISABLED -> LocationCardView(
                title = "Vị trí đang tắt",
                detail = "Dịch vụ vị trí của máy đang tắt",
                hint = loc.remediation
            )
            LocationFailureCause.NO_FIX,
            LocationFailureCause.TIMEOUT,
            LocationFailureCause.INVALID_FIX,
            null -> LocationCardView(
                title = "Đang xác định...",
                detail = "Vị trí sẽ được gửi ngay khi có",
                hint = null
            )
        }
    }

    val accuracyPart = if (fix.accuracyM != null) {
        "± ${fix.accuracyM.roundToInt()} m"
    } else {
        "Độ chính xác chưa rõ"
    }

    val ageSec = (nowMs - fix.fixTimeMs).coerceAtLeast(0L) / 1000L

    return if (loc.freshness == LocationFreshness.FRESH) {
        val agePart = when {
            ageSec < 5L -> "vừa xong"
            ageSec < 60L -> "Cập nhật $ageSec giây trước"
            else -> "Cập nhật ${ageSec / 60L} phút trước"
        }
        LocationCardView(
            title = "Đã xác định",
            detail = "$accuracyPart • $agePart",
            hint = null
        )
    } else {
        val ageMin = maxOf(1L, ageSec / 60L)
        val agePart = "Cập nhật $ageMin phút trước"
        LocationCardView(
            title = "Vị trí gần đúng",
            detail = "$accuracyPart • $agePart",
            hint = loc.remediation
        )
    }
}

internal fun resolveDeviceCardStatus(details: DeviceDetails): String = when (details.connected) {
    true -> details.batteryPercent?.let { "Pin $it%" } ?: "Đã kết nối"
    false -> "Mất kết nối"
    null -> details.statusText.ifBlank { "Chưa có dữ liệu" }
}

internal fun resolveSmsDispatchStatusText(dispatch: SmsDispatchState): String {
    val base = when (dispatch.status) {
        SmsDeliveryStatus.QUEUED -> "Đã xếp hàng (QUEUED)"
        SmsDeliveryStatus.SENDING -> "Đang gửi (SENDING)..."
        SmsDeliveryStatus.SENT -> "Đã gửi tới nhà mạng (SENT) - chờ xác nhận giao"
        SmsDeliveryStatus.DELIVERED -> "Đã giao SMS thành công (DELIVERED)"
        SmsDeliveryStatus.FAILED -> "Gửi SMS thất bại (FAILED)"
    }
    return if (!dispatch.detail.isNullOrBlank()) "$base\nChi tiết: ${dispatch.detail}" else base
}

internal fun buildDeviceDetailsFieldList(details: DeviceDetails): List<String> = buildList {
    details.deviceId?.let { add("Mã thiết bị: $it") }
    details.connected?.let { add("Kết nối: ${if (it) "Đang kết nối" else "Mất kết nối"}") }
    details.batteryPercent?.let { add("Mức pin: $it%") }
    details.firmwareVersion?.let { add("Phiên bản firmware: $it") }
    details.gnssStatus?.let { add("Trạng thái GNSS: $it") }
    if (details.source != DeviceValueSource.UNKNOWN) {
        val srcName = when (details.source) {
            DeviceValueSource.BACKEND_HEARTBEAT -> "Máy chủ (Heartbeat)"
            DeviceValueSource.ESP32_PACKET -> "Gói tin trực tiếp ESP32"
            DeviceValueSource.UNKNOWN -> "Không xác định"
        }
        add("Nguồn dữ liệu: $srcName")
    }
    details.lastHeartbeatMs?.let { add("Heartbeat cuối: ${it}ms") }
}

@Composable
fun HomeScreen(
    controller: DemoController,
    onOpenContacts: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val scrollState = rememberScrollState()
    val status = controller.mainScreenStatus

    // Dialog & Feedback states
    var showLocationErrorDialog by remember { mutableStateOf(false) }
    var showShareContactsDialog by remember { mutableStateOf(false) }
    var pendingConfirmation by remember { mutableStateOf<ManualShareConfirmation?>(null) }
    var shareDispatchState by remember { mutableStateOf<SmsDispatchState?>(null) }
    var shareErrorMessage by remember { mutableStateOf<String?>(null) }
    var showCallContactsDialog by remember { mutableStateOf(false) }
    var simCallResult by remember { mutableStateOf<SimCallResult?>(null) }
    var showDeviceDetailsDialog by remember { mutableStateOf(false) }
    var showSosReportDialog by remember { mutableStateOf(false) }
    var settingsDialogCapability by remember { mutableStateOf<Capability?>(null) }

    // Vibration manager
    val vibrator = remember(context) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vm = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
                vm?.defaultVibrator ?: (context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)
            } else {
                @Suppress("DEPRECATION")
                context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
            }
        } catch (_: Exception) {
            null
        }
    }

    // Sound & Vibration during countdown with leak-free ToneGenerator release
    DisposableEffect(status) {
        var localToneGen: ToneGenerator? = null
        if (status == MainScreenStatus.WARNING_COUNTDOWN) {
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    vibrator?.vibrate(VibrationEffect.createWaveform(longArrayOf(0, 400, 200, 400), 0))
                } else {
                    @Suppress("DEPRECATION")
                    vibrator?.vibrate(longArrayOf(0, 400, 200, 400), 0)
                }
            } catch (_: Exception) {}

            try {
                localToneGen = ToneGenerator(AudioManager.STREAM_ALARM, 85)
                localToneGen.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 1000)
            } catch (_: Exception) {}
        }

        onDispose {
            try { vibrator?.cancel() } catch (_: Exception) {}
            try {
                localToneGen?.stopTone()
                localToneGen?.release()
                localToneGen = null
            } catch (_: Exception) {}
        }
    }

    // Backing states for cards
    val locState = controller.emergencyLocationState
    val locCardView = resolveLocationCardView(locState, System.currentTimeMillis())
    val locStatusText = locCardView.title
    val locSubline = locCardView.detail
    val locColor = if (locState.fix != null) SafeGreen else DisconnectedGray
    val locBg = if (locState.fix != null) SafeGreenContainer else DisconnectedGrayContainer
    val locBorder = if (locState.fix != null) SafeGreenBorder else DisconnectedGrayBorder

    val sendStatusText = if (locState.fix != null) "Sẵn sàng gửi" else "Chưa có GPS"

    val callStatusText = if (controller.contacts.isNotEmpty()) controller.primaryContactName else "Chưa có liên hệ"

    val devDetails = controller.emergencyDeviceDetails
    val devStatusText = resolveDeviceCardStatus(devDetails)
    val devConnected = devDetails.connected == true
    val devColor = if (devConnected) SafeGreen else DisconnectedGray
    val devBg = if (devConnected) SafeGreenContainer else DisconnectedGrayContainer
    val devBorder = if (devConnected) SafeGreenBorder else DisconnectedGrayBorder
    val devIcon = if (devConnected) "🔋" else "📟"

    // Setup protective status banner
    val (protectText, protectColor, protectBg, protectBorder, protectIcon) = when (status) {
        MainScreenStatus.SAFE -> Quint(
            "Đang bảo vệ", SafeGreen, SafeGreenContainer, SafeGreenBorder, "🛡️"
        )
        MainScreenStatus.WARNING_COUNTDOWN -> Quint(
            "Cần kiểm tra • Nguy cơ ngã", WarningOrange, WarningOrangeContainer, WarningOrangeBorder, "⚠️"
        )
        MainScreenStatus.SOS_SENT -> Quint(
            "Cần kiểm tra • SOS đã kích hoạt", SosRed, SosRedContainer, SosRedBorder, "🚨"
        )
        MainScreenStatus.HELP_ACKNOWLEDGED -> Quint(
            "Đang bảo vệ • Đã nhận tin", SafeGreen, SafeGreenContainer, SafeGreenBorder, "🛡️"
        )
        MainScreenStatus.DEVICE_DISCONNECTED -> Quint(
            "Cần kiểm tra • Mất kết nối thiết bị", DisconnectedGray, DisconnectedGrayContainer, DisconnectedGrayBorder, "❗"
        )
    }

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val screenWidth = maxWidth
        val screenHeight = maxHeight
        val fontScale = LocalDensity.current.fontScale

        // Responsive layout detection
        val isLandscape = screenWidth > screenHeight
        val shouldReflow = fontScale >= 1.3f || screenWidth < 360.dp

        // Grid spacing between cards
        val gridSpacing = if (isLandscape) 4.dp else 8.dp
        val gapHalf = gridSpacing / 2

        // Dynamic center button diameter: sized proportionally to available dimension
        val buttonDiameter = if (isLandscape) {
            (screenHeight * 0.46f).coerceIn(94.dp, 106.dp)
        } else if (shouldReflow) {
            (screenWidth * 0.42f).coerceIn(136.dp, 160.dp)
        } else {
            (minOf(screenWidth, screenHeight) * 0.44f).coerceIn(148.dp, 180.dp)
        }
        val buttonRadius = buttonDiameter / 2
        val cutoutRadius = buttonRadius + 9.dp

        val containerModifier = Modifier
            .fillMaxSize()
            .padding(
                horizontal = if (isLandscape) 8.dp else 10.dp,
                vertical = if (isLandscape) 4.dp else 8.dp
            )

        if (isLandscape) {
            // ==================== LANDSCAPE LAYOUT ====================
            Row(
                modifier = containerModifier,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Left Column: VỊ TRÍ CỦA TÔI & GỌI NGƯỜI THÂN
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight(),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    StandardRoundedCard(
                        containerColor = locBg,
                        borderColor = locBorder,
                        talkBackLabel = "Vị trí của tôi: $locStatusText. Chạm để mở bản đồ.",
                        onClick = {
                            val ok = controller.openMyLocation()
                            if (!ok) showLocationErrorDialog = true
                        },
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        PeripheralCardContent(
                            icon = "📍",
                            title = "VỊ TRÍ CỦA TÔI",
                            statusText = locStatusText,
                            subline = locSubline,
                            titleColor = locColor
                        )
                    }

                    StandardRoundedCard(
                        containerColor = ContactBlueContainer,
                        borderColor = ContactBlueBorder,
                        talkBackLabel = "Gọi người thân: $callStatusText. Chạm để mở danh bạ gọi.",
                        onClick = { showCallContactsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        PeripheralCardContent(
                            icon = "📞",
                            title = "GỌI NGƯỜI THÂN",
                            statusText = callStatusText,
                            subline = "Cuộc gọi SIM",
                            titleColor = ContactBlue
                        )
                    }
                }

                // Center Column: Status badge + Center SOS button
                Column(
                    modifier = Modifier
                        .wrapContentWidth()
                        .fillMaxHeight(),
                    verticalArrangement = Arrangement.SpaceBetween,
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    val landscapeReport = controller.latestSosDispatchReport
                    val landscapeStatusText = if (status == MainScreenStatus.SOS_SENT) {
                        resolveSosDispatchSummaryText(landscapeReport, controller.sosDeliveryMessage)
                    } else "$protectIcon $protectText"

                    Card(
                        colors = CardDefaults.cardColors(containerColor = protectBg),
                        border = BorderStroke(1.dp, protectBorder),
                        shape = RoundedCornerShape(8.dp),
                        modifier = Modifier
                            .padding(bottom = 2.dp)
                            .then(
                                if (status == MainScreenStatus.SOS_SENT && landscapeReport != null) {
                                    Modifier.clickable { showSosReportDialog = true }
                                } else Modifier
                            )
                    ) {
                        Text(
                            text = landscapeStatusText,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold,
                            color = protectColor,
                            textAlign = TextAlign.Center,
                            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp)
                        )
                    }

                    CenterActionButton(
                        status = status,
                        controller = controller,
                        diameter = buttonDiameter
                    )

                    if (status == MainScreenStatus.HELP_ACKNOWLEDGED) {
                        Button(
                            onClick = {
                                controller.complete()
                                controller.safe()
                            },
                            modifier = Modifier.heightIn(min = 36.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                        ) {
                            Text("HOÀN TẤT", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = Color.White)
                        }
                    } else {
                        Spacer(modifier = Modifier.height(2.dp))
                    }
                }

                // Right Column: GỬI VỊ TRÍ & THIẾT BỊ
                Column(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxHeight(),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    StandardRoundedCard(
                        containerColor = ContactBlueContainer,
                        borderColor = ContactBlueBorder,
                        talkBackLabel = "Gửi vị trí: $sendStatusText. Chạm để chọn người nhận.",
                        onClick = { showShareContactsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        PeripheralCardContent(
                            icon = "📤",
                            title = "GỬI VỊ TRÍ",
                            statusText = sendStatusText,
                            subline = "Gửi SMS tọa độ",
                            titleColor = ContactBlue
                        )
                    }

                    StandardRoundedCard(
                        containerColor = devBg,
                        borderColor = devBorder,
                        talkBackLabel = "Thiết bị: $devStatusText. Chạm để xem chi tiết.",
                        onClick = { showDeviceDetailsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxWidth()
                    ) {
                        PeripheralCardContent(
                            icon = devIcon,
                            title = "THIẾT BỊ",
                            statusText = devStatusText,
                            subline = "Chạm xem chi tiết",
                            titleColor = devColor
                        )
                    }
                }
            }
        } else if (shouldReflow) {
            // ==================== REFLOW PORTRAIT LAYOUT ====================
            // For fontScale >= 1.3f or width < 360dp: Scrollable vertical/two-column layout
            Column(
                modifier = containerModifier.verticalScroll(scrollState),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Top compact protective status banner
                ProtectiveStatusBanner(
                    protectIcon = protectIcon,
                    protectText = protectText,
                    protectColor = protectColor,
                    protectBg = protectBg,
                    protectBorder = protectBorder,
                    status = status,
                    sosDeliveryMessage = controller.sosDeliveryMessage,
                    dispatchReport = controller.latestSosDispatchReport,
                    onOpenReport = { showSosReportDialog = true },
                    onComplete = {
                        controller.complete()
                        controller.safe()
                    }
                )

                // Top Two-Column Row: VỊ TRÍ CỦA TÔI & GỬI VỊ TRÍ
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    StandardRoundedCard(
                        containerColor = locBg,
                        borderColor = locBorder,
                        talkBackLabel = "Vị trí của tôi: $locStatusText. Chạm để mở bản đồ.",
                        onClick = {
                            val ok = controller.openMyLocation()
                            if (!ok) showLocationErrorDialog = true
                        },
                        modifier = Modifier
                            .weight(1f)
                            .heightIn(min = 76.dp)
                    ) {
                        PeripheralCardContent(
                            icon = "📍",
                            title = "VỊ TRÍ CỦA TÔI",
                            statusText = locStatusText,
                            subline = locSubline,
                            titleColor = locColor
                        )
                    }

                    StandardRoundedCard(
                        containerColor = ContactBlueContainer,
                        borderColor = ContactBlueBorder,
                        talkBackLabel = "Gửi vị trí: $sendStatusText. Chạm để chọn người nhận.",
                        onClick = { showShareContactsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .heightIn(min = 76.dp)
                    ) {
                        PeripheralCardContent(
                            icon = "📤",
                            title = "GỬI VỊ TRÍ",
                            statusText = sendStatusText,
                            subline = "Gửi SMS tọa độ",
                            titleColor = ContactBlue
                        )
                    }
                }

                // Central SOS Button
                CenterActionButton(
                    status = status,
                    controller = controller,
                    diameter = buttonDiameter,
                    modifier = Modifier.padding(vertical = 4.dp)
                )

                // Bottom Two-Column Row: GỌI NGƯỜI THÂN & THIẾT BỊ
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    StandardRoundedCard(
                        containerColor = ContactBlueContainer,
                        borderColor = ContactBlueBorder,
                        talkBackLabel = "Gọi người thân: $callStatusText. Chạm để mở danh bạ gọi.",
                        onClick = { showCallContactsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .heightIn(min = 76.dp)
                    ) {
                        PeripheralCardContent(
                            icon = "📞",
                            title = "GỌI NGƯỜI THÂN",
                            statusText = callStatusText,
                            subline = "Cuộc gọi SIM",
                            titleColor = ContactBlue
                        )
                    }

                    StandardRoundedCard(
                        containerColor = devBg,
                        borderColor = devBorder,
                        talkBackLabel = "Thiết bị: $devStatusText. Chạm để xem chi tiết.",
                        onClick = { showDeviceDetailsDialog = true },
                        modifier = Modifier
                            .weight(1f)
                            .heightIn(min = 76.dp)
                    ) {
                        PeripheralCardContent(
                            icon = devIcon,
                            title = "THIẾT BỊ",
                            statusText = devStatusText,
                            subline = "Chạm xem chi tiết",
                            titleColor = devColor
                        )
                    }
                }
            }
        } else {
            // ==================== STANDARD PORTRAIT LAYOUT ====================
            // 4 Concave cards hugging central circular SOS button
            Column(
                modifier = containerModifier,
                verticalArrangement = Arrangement.SpaceBetween,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Top compact protective status banner
                ProtectiveStatusBanner(
                    protectIcon = protectIcon,
                    protectText = protectText,
                    protectColor = protectColor,
                    protectBg = protectBg,
                    protectBorder = protectBorder,
                    status = status,
                    sosDeliveryMessage = controller.sosDeliveryMessage,
                    dispatchReport = controller.latestSosDispatchReport,
                    onOpenReport = { showSosReportDialog = true },
                    onComplete = {
                        controller.complete()
                        controller.safe()
                    }
                )

                // 2x2 Grid with Central SOS Button
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .weight(1f),
                    contentAlignment = Alignment.Center
                ) {
                    Column(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(gridSpacing)
                    ) {
                        // TOP ROW: Vị trí của tôi (TL) | Gửi vị trí (TR)
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            horizontalArrangement = Arrangement.spacedBy(gridSpacing)
                        ) {
                            // 1. Top-Left: VỊ TRÍ CỦA TÔI
                            ConcaveCard(
                                cutoutCorner = CutoutCorner.BOTTOM_RIGHT,
                                cutoutRadius = cutoutRadius,
                                containerColor = locBg,
                                borderColor = locBorder,
                                contentAlignment = Alignment.TopStart,
                                contentPadding = PaddingValues(start = 14.dp, top = 12.dp, end = 8.dp, bottom = 8.dp),
                                gapX = gapHalf,
                                gapY = gapHalf,
                                talkBackLabel = "Vị trí của tôi: $locStatusText. Chạm để mở bản đồ.",
                                onClick = {
                                    val ok = controller.openMyLocation()
                                    if (!ok) showLocationErrorDialog = true
                                },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                            ) {
                                PeripheralCardContent(
                                    icon = "📍",
                                    title = "VỊ TRÍ CỦA TÔI",
                                    statusText = locStatusText,
                                    subline = locSubline,
                                    titleColor = locColor,
                                    alignment = Alignment.Start
                                )
                            }

                            // 2. Top-Right: GỬI VỊ TRÍ
                            ConcaveCard(
                                cutoutCorner = CutoutCorner.BOTTOM_LEFT,
                                cutoutRadius = cutoutRadius,
                                containerColor = ContactBlueContainer,
                                borderColor = ContactBlueBorder,
                                contentAlignment = Alignment.TopEnd,
                                contentPadding = PaddingValues(end = 14.dp, top = 12.dp, start = 8.dp, bottom = 8.dp),
                                gapX = gapHalf,
                                gapY = gapHalf,
                                talkBackLabel = "Gửi vị trí: $sendStatusText. Chạm để chọn người nhận.",
                                onClick = { showShareContactsDialog = true },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                            ) {
                                PeripheralCardContent(
                                    icon = "📤",
                                    title = "GỬI VỊ TRÍ",
                                    statusText = sendStatusText,
                                    subline = "Gửi SMS tọa độ",
                                    titleColor = ContactBlue,
                                    alignment = Alignment.End
                                )
                            }
                        }

                        // BOTTOM ROW: Gọi người thân (BL) | Thiết bị (BR)
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            horizontalArrangement = Arrangement.spacedBy(gridSpacing)
                        ) {
                            // 3. Bottom-Left: GỌI NGƯỜI THÂN
                            ConcaveCard(
                                cutoutCorner = CutoutCorner.TOP_RIGHT,
                                cutoutRadius = cutoutRadius,
                                containerColor = ContactBlueContainer,
                                borderColor = ContactBlueBorder,
                                contentAlignment = Alignment.BottomStart,
                                contentPadding = PaddingValues(start = 14.dp, bottom = 12.dp, end = 8.dp, top = 8.dp),
                                gapX = gapHalf,
                                gapY = gapHalf,
                                talkBackLabel = "Gọi người thân: $callStatusText. Chạm để mở danh bạ gọi.",
                                onClick = { showCallContactsDialog = true },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                            ) {
                                PeripheralCardContent(
                                    icon = "📞",
                                    title = "GỌI NGƯỜI THÂN",
                                    statusText = callStatusText,
                                    subline = "Cuộc gọi SIM",
                                    titleColor = ContactBlue,
                                    alignment = Alignment.Start
                                )
                            }

                            // 4. Bottom-Right: THIẾT BỊ
                            ConcaveCard(
                                cutoutCorner = CutoutCorner.TOP_LEFT,
                                cutoutRadius = cutoutRadius,
                                containerColor = devBg,
                                borderColor = devBorder,
                                contentAlignment = Alignment.BottomEnd,
                                contentPadding = PaddingValues(end = 14.dp, bottom = 12.dp, start = 8.dp, top = 8.dp),
                                gapX = gapHalf,
                                gapY = gapHalf,
                                talkBackLabel = "Thiết bị: $devStatusText. Chạm để xem chi tiết.",
                                onClick = { showDeviceDetailsDialog = true },
                                modifier = Modifier
                                    .weight(1f)
                                    .fillMaxHeight()
                            ) {
                                PeripheralCardContent(
                                    icon = devIcon,
                                    title = "THIẾT BỊ",
                                    statusText = devStatusText,
                                    subline = "Chạm xem chi tiết",
                                    titleColor = devColor,
                                    alignment = Alignment.End
                                )
                            }
                        }
                    }

                    // Center Action Button: elevated on top of the concave junction
                    CenterActionButton(
                        status = status,
                        controller = controller,
                        diameter = buttonDiameter,
                        modifier = Modifier
                            .align(Alignment.Center)
                            .zIndex(2f)
                    )
                }
            }
        }
    }

    // ==================== DIALOGS & USER ACTIONS ====================

    // 1. Vị trí của tôi — Error Dialog
    if (showLocationErrorDialog) {
        val loc = controller.emergencyLocationState
        AlertDialog(
            onDismissRequest = { showLocationErrorDialog = false },
            title = {
                Text(text = "Vị trí của tôi", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Column {
                    Text(text = loc.explanation, fontSize = 16.sp)
                    if (!loc.remediation.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(text = "Gợi ý: ${loc.remediation}", fontSize = 15.sp, color = Color(0xFF37474F))
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showLocationErrorDialog = false },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 2. Gửi vị trí — Select Contact Dialog
    if (showShareContactsDialog) {
        AlertDialog(
            onDismissRequest = { showShareContactsDialog = false },
            title = {
                Text(text = "Gửi vị trí cho người thân", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                if (controller.contacts.isEmpty()) {
                    Text(text = "Chưa có liên hệ khẩn cấp nào trong danh sách.", fontSize = 16.sp)
                } else {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 350.dp)
                            .verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        controller.contacts.forEach { contact ->
                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .heightIn(min = 56.dp)
                                    .clickable {
                                        showShareContactsDialog = false
                                        val conf = controller.requestManualLocationShare(contact.id)
                                        if (conf != null) {
                                            pendingConfirmation = conf
                                        } else {
                                            val loc = controller.emergencyLocationState
                                            shareErrorMessage = loc.explanation + (loc.remediation?.let { "\n\n$it" } ?: "")
                                        }
                                    }
                                    .semantics {
                                        role = Role.Button
                                        contentDescription = "Gửi vị trí cho ${contact.name} (${contact.relationship})"
                                    },
                                colors = CardDefaults.cardColors(containerColor = ContactBlueContainer),
                                border = BorderStroke(1.dp, ContactBlueBorder),
                                shape = RoundedCornerShape(10.dp)
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 12.dp, vertical = 10.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.SpaceBetween
                                ) {
                                    Column(modifier = Modifier.weight(1f)) {
                                        Text(text = contact.name, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                                        Text(
                                            text = "${contact.relationship.ifBlank { "Người thân" }} • ${ContactValidator.mask(contact.phone)}",
                                            fontSize = 14.sp,
                                            color = DisconnectedGray
                                        )
                                    }
                                    Text(
                                        text = "CHỌN",
                                        fontSize = 15.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = ContactBlue
                                    )
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = {
                OutlinedButton(
                    onClick = { showShareContactsDialog = false },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        )
    }

    // 2b. Gửi vị trí — Confirmation Dialog showing preview
    pendingConfirmation?.let { conf ->
        val contact = controller.contacts.firstOrNull { it.id == conf.contactId }
        AlertDialog(
            onDismissRequest = { pendingConfirmation = null },
            title = {
                Text(text = "Xác nhận gửi vị trí", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Column {
                    if (contact != null) {
                        Text(
                            text = "Người nhận: ${contact.name} (${contact.relationship.ifBlank { "Người thân" }} - ${ContactValidator.mask(contact.phone)})",
                            fontSize = 15.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                    }
                    Text(text = "Nội dung tin nhắn SMS:", fontSize = 14.sp, color = DisconnectedGray)
                    Spacer(modifier = Modifier.height(4.dp))
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(containerColor = ContactBlueContainer),
                        border = BorderStroke(1.dp, ContactBlueBorder),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text(
                            text = conf.preview,
                            fontSize = 15.sp,
                            modifier = Modifier.padding(10.dp)
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val token = conf.token
                        pendingConfirmation = null
                        val state = controller.confirmManualLocationShare(token)
                        shareDispatchState = state
                    },
                    modifier = Modifier.heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                ) {
                    Text(text = "GỬI SMS", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            },
            dismissButton = {
                OutlinedButton(
                    onClick = { pendingConfirmation = null },
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text(text = "HỦY", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        )
    }

    // 2c. Gửi vị trí — Truthful Dispatch Status Dialog
    shareDispatchState?.let { state ->
        AlertDialog(
            onDismissRequest = { shareDispatchState = null },
            title = {
                Text(text = "Trạng thái gửi SMS", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Text(text = resolveSmsDispatchStatusText(state), fontSize = 16.sp)
            },
            confirmButton = {
                Button(
                    onClick = { shareDispatchState = null },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 2d. Gửi vị trí — Error Dialog
    shareErrorMessage?.let { msg ->
        AlertDialog(
            onDismissRequest = { shareErrorMessage = null },
            title = {
                Text(text = "Không thể gửi vị trí", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Text(text = msg, fontSize = 16.sp)
            },
            confirmButton = {
                Button(
                    onClick = { shareErrorMessage = null },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 3. Gọi người thân — Contact List Dialog
    if (showCallContactsDialog) {
        AlertDialog(
            onDismissRequest = { showCallContactsDialog = false },
            title = {
                Text(text = "Gọi người thân (SIM)", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                if (controller.contacts.isEmpty()) {
                    Text(text = "Chưa có liên hệ khẩn cấp nào trong danh sách.", fontSize = 16.sp)
                } else {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 350.dp)
                            .verticalScroll(rememberScrollState()),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        controller.contacts.forEach { contact ->
                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .heightIn(min = 56.dp),
                                colors = CardDefaults.cardColors(containerColor = ContactBlueContainer),
                                border = BorderStroke(1.dp, ContactBlueBorder),
                                shape = RoundedCornerShape(10.dp)
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 10.dp, vertical = 8.dp),
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.SpaceBetween
                                ) {
                                    Column(
                                        modifier = Modifier
                                            .weight(1f)
                                            .padding(end = 8.dp)
                                    ) {
                                        Text(text = contact.name, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                                        Text(
                                            text = "${contact.relationship.ifBlank { "Người thân" }} • ${contact.phone}",
                                            fontSize = 14.sp,
                                            color = DisconnectedGray
                                        )
                                    }
                                    Button(
                                        onClick = {
                                            showCallContactsDialog = false
                                            val result = controller.callContactViaSim(contact.id)
                                            simCallResult = result
                                        },
                                        modifier = Modifier.heightIn(min = 56.dp),
                                        colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                                    ) {
                                        Text(text = "GỌI", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                                    }
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = {
                OutlinedButton(
                    onClick = { showCallContactsDialog = false },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        )
    }

    // 3b. Gọi người thân — Call Result Dialog
    simCallResult?.let { result ->
        AlertDialog(
            onDismissRequest = { simCallResult = null },
            title = {
                Text(
                    text = if (result.started) "Cuộc gọi SIM" else "Không thể gọi",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
            },
            text = {
                Text(text = result.detail, fontSize = 16.sp)
            },
            confirmButton = {
                Button(
                    onClick = { simCallResult = null },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 4. Thiết bị — Details Dialog
    if (showDeviceDetailsDialog) {
        val details = controller.emergencyDeviceDetails
        AlertDialog(
            onDismissRequest = { showDeviceDetailsDialog = false },
            title = {
                Text(text = "Thông tin thiết bị ngoại vi", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .verticalScroll(rememberScrollState())
                ) {
                    val fieldList = buildDeviceDetailsFieldList(details)
                    if (fieldList.isNotEmpty()) {
                        fieldList.forEach { field ->
                            Text(
                                text = field,
                                fontSize = 15.sp,
                                modifier = Modifier.padding(vertical = 2.dp)
                            )
                        }
                        if (details.statusText.isNotBlank()) {
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(text = details.statusText, fontSize = 14.sp, color = DisconnectedGray)
                        }
                    } else {
                        Text(
                            text = details.statusText.ifBlank { "Chưa có thông tin thiết bị ngoại vi từ máy chủ." },
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Medium
                        )
                    }

                    Spacer(modifier = Modifier.height(12.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = "Lưu ý: Thiết bị ngoại vi ESP32 hoạt động độc lập với nút SOS trên điện thoại. Khi thiết bị ngoại vi vắng mặt hoặc mất kết nối, nút SOS trên điện thoại vẫn hoạt động bình thường.",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = DisconnectedGray
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = { showDeviceDetailsDialog = false },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 5. Kết quả từng bước SOS — Truthful Dispatch Report Dialog
    if (showSosReportDialog) {
        val report = controller.latestSosDispatchReport
        AlertDialog(
            onDismissRequest = { showSosReportDialog = false },
            title = {
                Text(text = "Tiến trình gửi SOS", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 400.dp)
                        .verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    if (report != null && report.steps.isNotEmpty()) {
                        report.steps.forEach { stepResult ->
                            val stepStatusLabel = resolveSosStepStatusLabel(stepResult.status)
                            val stepContainerColor = when (stepResult.status) {
                                SosStepStatus.SUCCESS -> SafeGreenContainer
                                SosStepStatus.PERMISSION_MISSING, SosStepStatus.FAILED -> SosRedContainer
                                else -> WarningOrangeContainer
                            }
                            val stepBorderColor = when (stepResult.status) {
                                SosStepStatus.SUCCESS -> SafeGreenBorder
                                SosStepStatus.PERMISSION_MISSING, SosStepStatus.FAILED -> SosRedBorder
                                else -> WarningOrangeBorder
                            }
                            val stepTextColor = when (stepResult.status) {
                                SosStepStatus.SUCCESS -> SafeGreen
                                SosStepStatus.PERMISSION_MISSING, SosStepStatus.FAILED -> SosRed
                                else -> WarningOrange
                            }

                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .semantics {
                                        contentDescription = "${stepResult.step.vietnameseLabel}: $stepStatusLabel. ${stepResult.detail ?: ""}"
                                    },
                                colors = CardDefaults.cardColors(containerColor = stepContainerColor),
                                border = BorderStroke(1.dp, stepBorderColor),
                                shape = RoundedCornerShape(10.dp)
                            ) {
                                Column(modifier = Modifier.padding(12.dp)) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = stepResult.step.vietnameseLabel,
                                            fontSize = 18.sp,
                                            fontWeight = FontWeight.Bold
                                        )
                                        Text(
                                            text = stepStatusLabel,
                                            fontSize = 16.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = stepTextColor
                                        )
                                    }
                                    if (!stepResult.detail.isNullOrBlank()) {
                                        Spacer(modifier = Modifier.height(6.dp))
                                        Text(
                                            text = stepResult.detail,
                                            fontSize = 16.sp,
                                            color = Color(0xFF263238),
                                            lineHeight = 22.sp
                                        )
                                    }
                                }
                            }
                        }
                    } else {
                        Text(
                            text = controller.sosDeliveryMessage,
                            fontSize = 16.sp
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showSosReportDialog = false },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text(text = "ĐÓNG", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        )
    }

    // 6. First-run permission explanation dialog
    val nextSetupStep = controller.nextPermissionSetupStep()
    val showFirstRunDialog = shouldShowFirstRunExplanation(
        status = status,
        permissionSetupSeen = controller.permissionSetupSeen,
        hasUngrantedCapability = nextSetupStep != null
    )

    if (showFirstRunDialog) {
        AlertDialog(
            onDismissRequest = {
                controller.markPermissionSetupSeen()
            },
            title = {
                Text(
                    text = FIRST_RUN_EXPLANATION_TITLE,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
            },
            text = {
                Text(
                    text = FIRST_RUN_EXPLANATION_BODY,
                    fontSize = 16.sp,
                    lineHeight = 22.sp
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        val step = controller.nextPermissionSetupStep()
                        controller.markPermissionSetupSeen()
                        if (step != null) {
                            val requestFn = { ack: Boolean ->
                                when (step) {
                                    Capability.LOCATION -> controller.requestLocationPermission(ack)
                                    Capability.MESSAGING -> controller.requestMessagingPermission(ack)
                                    Capability.CALLING -> controller.requestCallingPermission(ack)
                                }
                            }
                            val result = requestFn(true)
                            when (resolveRequestOutcome(result)) {
                                PermissionFollowUp.NONE -> Unit
                                PermissionFollowUp.SHOW_SETTINGS_DIALOG -> {
                                    settingsDialogCapability = step
                                }
                                PermissionFollowUp.REQUEST_WITH_EXPLANATION -> {
                                    val retry = requestFn(true)
                                    if (resolveRequestOutcome(retry) == PermissionFollowUp.SHOW_SETTINGS_DIALOG) {
                                        settingsDialogCapability = step
                                    }
                                }
                            }
                        }
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                ) {
                    Text(
                        text = FIRST_RUN_CONTINUE_BUTTON,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                }
            },
            dismissButton = {
                OutlinedButton(
                    onClick = {
                        controller.markPermissionSetupSeen()
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp)
                ) {
                    Text(
                        text = FIRST_RUN_LATER_BUTTON,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
        )
    }

    settingsDialogCapability?.let { cap ->
        PermissionSettingsDialog(
            onOpenSettings = {
                settingsDialogCapability = null
                when (cap) {
                    Capability.LOCATION -> controller.openLocationPermissionSettings()
                    Capability.CALLING -> controller.openCallingPermissionSettings()
                    Capability.MESSAGING -> controller.openMessagingPermissionSettings()
                }
            },
            onDismiss = { settingsDialogCapability = null }
        )
    }
}

/**
 * Compact protective status area for top banner.
 */
@Composable
private fun ProtectiveStatusBanner(
    protectIcon: String,
    protectText: String,
    protectColor: Color,
    protectBg: Color,
    protectBorder: Color,
    status: MainScreenStatus,
    sosDeliveryMessage: String,
    dispatchReport: SosDispatchReport? = null,
    onOpenReport: (() -> Unit)? = null,
    onComplete: () -> Unit
) {
    val bannerText = if (status == MainScreenStatus.SOS_SENT) {
        resolveSosDispatchSummaryText(dispatchReport, sosDeliveryMessage)
    } else {
        "BẢO VỆ: $protectText"
    }

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 4.dp)
            .then(
                if (status == MainScreenStatus.SOS_SENT && dispatchReport != null && onOpenReport != null) {
                    Modifier.clickable(onClick = onOpenReport)
                } else {
                    Modifier
                }
            )
            .semantics {
                contentDescription = if (status == MainScreenStatus.SOS_SENT) {
                    "Trạng thái SOS: $bannerText. Chạm để xem chi tiết từng bước."
                } else {
                    "Trạng thái bảo vệ: $protectText"
                }
            },
        colors = CardDefaults.cardColors(containerColor = protectBg),
        border = BorderStroke(1.5.dp, protectBorder),
        shape = RoundedCornerShape(12.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 10.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                modifier = Modifier.weight(1f)
            ) {
                Text(text = protectIcon, fontSize = 16.sp)
                Text(
                    text = bannerText,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    color = protectColor,
                    maxLines = 3,
                    softWrap = true
                )
            }
            if (status == MainScreenStatus.HELP_ACKNOWLEDGED) {
                Button(
                    onClick = onComplete,
                    modifier = Modifier.heightIn(min = 40.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                ) {
                    Text("HOÀN TẤT", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            } else if (status == MainScreenStatus.SOS_SENT && dispatchReport != null && onOpenReport != null) {
                Button(
                    onClick = onOpenReport,
                    modifier = Modifier.heightIn(min = 40.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = SosRedDark)
                ) {
                    Text("CHI TIẾT", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        }
    }
}

/**
 * Reusable peripheral card content with guaranteed >= 14sp typography and softWrap.
 */
@Composable
private fun PeripheralCardContent(
    icon: String,
    title: String,
    statusText: String,
    subline: String,
    titleColor: Color,
    alignment: Alignment.Horizontal = Alignment.Start
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(2.dp),
        horizontalAlignment = alignment
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            if (alignment == Alignment.Start) {
                Text(text = icon, fontSize = 18.sp)
                Text(
                    text = title,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    color = titleColor,
                    maxLines = 2,
                    softWrap = true
                )
            } else {
                Text(
                    text = title,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    color = titleColor,
                    textAlign = TextAlign.End,
                    maxLines = 2,
                    softWrap = true
                )
                Text(text = icon, fontSize = 18.sp)
            }
        }
        Text(
            text = statusText,
            fontSize = 15.sp,
            fontWeight = FontWeight.Bold,
            color = Color.Black,
            textAlign = if (alignment == Alignment.Start) TextAlign.Start else TextAlign.End,
            maxLines = 2,
            softWrap = true
        )
        Text(
            text = subline,
            fontSize = 14.sp,
            fontWeight = FontWeight.Medium,
            color = Color(0xFF37474F),
            textAlign = if (alignment == Alignment.Start) TextAlign.Start else TextAlign.End,
            maxLines = 2,
            softWrap = true
        )
    }
}

/**
 * Reusable Concave Card wrapping the center SOS button.
 * Uses ConcaveCutoutShape to carve out the inner corner facing the center circular SOS button.
 */
@Composable
fun ConcaveCard(
    cutoutCorner: CutoutCorner,
    cutoutRadius: Dp,
    containerColor: Color,
    borderColor: Color,
    modifier: Modifier = Modifier,
    borderWidth: Dp = 2.dp,
    talkBackLabel: String = "",
    onClick: (() -> Unit)? = null,
    contentAlignment: Alignment = Alignment.TopStart,
    contentPadding: PaddingValues = PaddingValues(10.dp),
    gapX: Dp = 4.dp,
    gapY: Dp = 4.dp,
    content: @Composable BoxScope.() -> Unit
) {
    val shape = remember(cutoutCorner, cutoutRadius, gapX, gapY) {
        ConcaveCutoutShape(
            cutoutCorner = cutoutCorner,
            cutoutRadius = cutoutRadius,
            outerCornerRadius = 18.dp,
            gapX = gapX,
            gapY = gapY
        )
    }

    Box(
        modifier = modifier
            .shadow(3.dp, shape)
            .clip(shape)
            .background(containerColor)
            .border(borderWidth, borderColor, shape)
            .then(
                if (onClick != null) {
                    Modifier
                        .clickable(onClick = onClick)
                        .semantics {
                            role = Role.Button
                            contentDescription = talkBackLabel
                        }
                } else {
                    Modifier.semantics {
                        contentDescription = talkBackLabel
                    }
                }
            )
            .padding(contentPadding),
        contentAlignment = contentAlignment,
        content = content
    )
}

/**
 * Standard rounded card used in reflow mode and landscape mode.
 */
@Composable
fun StandardRoundedCard(
    containerColor: Color,
    borderColor: Color,
    modifier: Modifier = Modifier,
    borderWidth: Dp = 2.dp,
    talkBackLabel: String = "",
    onClick: (() -> Unit)? = null,
    contentPadding: PaddingValues = PaddingValues(10.dp),
    content: @Composable BoxScope.() -> Unit
) {
    val shape = RoundedCornerShape(16.dp)
    Box(
        modifier = modifier
            .shadow(3.dp, shape)
            .clip(shape)
            .background(containerColor)
            .border(borderWidth, borderColor, shape)
            .then(
                if (onClick != null) {
                    Modifier
                        .clickable(onClick = onClick)
                        .semantics {
                            role = Role.Button
                            contentDescription = talkBackLabel
                        }
                } else {
                    Modifier.semantics {
                        contentDescription = talkBackLabel
                    }
                }
            )
            .padding(contentPadding),
        contentAlignment = Alignment.CenterStart,
        content = content
    )
}

/**
 * Center circular button adapting to all 5 states with responsive diameter.
 */
@Composable
fun CenterActionButton(
    status: MainScreenStatus,
    controller: DemoController,
    diameter: Dp = 170.dp,
    modifier: Modifier = Modifier
) {
    val haptic = LocalHapticFeedback.current
    val handler = remember { Handler(Looper.getMainLooper()) }
    val fontScale = LocalDensity.current.fontScale

    when (status) {
        MainScreenStatus.SAFE, MainScreenStatus.DEVICE_DISCONNECTED -> {
            // Normal State: SOS - GIỮ 3 GIÂY - ĐỂ GỌI GIÚP
            val hold = remember { SosHold(3000L) }
            var holding by remember { mutableStateOf(false) }
            var accessibleArmed by remember { mutableStateOf(false) }

            val fireSos = remember {
                Runnable {
                    if (hold.ready(SystemClock.elapsedRealtime())) {
                        holding = false
                        accessibleArmed = false
                        haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        controller.sos()
                    }
                }
            }

            DisposableEffect(Unit) {
                onDispose {
                    handler.removeCallbacks(fireSos)
                    hold.cancel()
                }
            }

            val progress by animateFloatAsState(
                targetValue = if (holding) 1f else 0f,
                animationSpec = if (holding) tween(durationMillis = 3000, easing = LinearEasing)
                else tween(durationMillis = 150, easing = LinearEasing),
                label = "sosProgress"
            )

            val isCompact = diameter < 110.dp
            val isMedium = diameter < 155.dp

            CircularHoldButton(
                diameter = diameter,
                backgroundColor = SosRed,
                progress = progress,
                progressColor = Color(0xFFFFD54F),
                talkBackLabel = "Nút SOS khẩn cấp. Nhấn và giữ 3 giây để gọi trợ giúp.",
                onAccessibilityClick = {
                    if (accessibleArmed) {
                        hold.cancel()
                        handler.removeCallbacks(fireSos)
                        accessibleArmed = false
                        holding = false
                    } else {
                        accessibleArmed = true
                        holding = true
                        hold.start(SystemClock.elapsedRealtime())
                        handler.postDelayed(fireSos, 3000)
                    }
                },
                onPressStart = {
                    holding = true
                    hold.start(SystemClock.elapsedRealtime())
                    handler.postDelayed(fireSos, 3000)
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                },
                onPressEnd = {
                    handler.removeCallbacks(fireSos)
                    hold.cancel()
                    holding = false
                },
                modifier = modifier
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                    modifier = Modifier.padding(horizontal = 4.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = "SOS",
                        fontSize = if (isCompact) 22.sp else if (isMedium) (if (fontScale >= 1.4f) 22.sp else 26.sp) else 32.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        lineHeight = if (isCompact) 22.sp else 32.sp
                    )
                    Spacer(modifier = Modifier.height(1.dp))
                    Text(
                        text = if (holding) "ĐANG GIỮ..." else "GIỮ 3 GIÂY",
                        fontSize = if (isCompact) 13.sp else if (isMedium) (if (fontScale >= 1.4f) 13.sp else 15.sp) else 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White,
                        lineHeight = if (isCompact) 14.sp else 18.sp
                    )
                    Spacer(modifier = Modifier.height(1.dp))
                    Text(
                        text = "ĐỂ GỌI GIÚP",
                        fontSize = if (isCompact) 12.sp else if (isMedium) (if (fontScale >= 1.4f) 12.sp else 14.sp) else 18.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color.White,
                        textAlign = TextAlign.Center,
                        lineHeight = if (isCompact) 13.sp else 18.sp
                    )
                }
            }
        }

        MainScreenStatus.WARNING_COUNTDOWN -> {
            // Danger Countdown: CẢNH BÁO SAU X GIÂY - TÔI AN TOÀN - GIỮ ĐỂ HỦY (2 GIÂY)
            val holdCancel = remember { SosHold(2000L) }
            var holdingCancel by remember { mutableStateOf(false) }
            var accessibleArmed by remember { mutableStateOf(false) }

            val fireCancel = remember {
                Runnable {
                    if (holdCancel.ready(SystemClock.elapsedRealtime())) {
                        holdingCancel = false
                        accessibleArmed = false
                        haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        controller.safe()
                    }
                }
            }

            DisposableEffect(Unit) {
                onDispose {
                    handler.removeCallbacks(fireCancel)
                    holdCancel.cancel()
                }
            }

            val cancelProgress by animateFloatAsState(
                targetValue = if (holdingCancel) 1f else 0f,
                animationSpec = if (holdingCancel) tween(durationMillis = 2000, easing = LinearEasing)
                else tween(durationMillis = 150, easing = LinearEasing),
                label = "cancelProgress"
            )

            val remainingMs = controller.snapshot.remainingMs ?: 10000L
            val seconds = ((remainingMs + 999L) / 1000L).coerceAtLeast(0L)

            val isCompact = diameter < 110.dp
            val isMedium = diameter < 155.dp

            CircularHoldButton(
                diameter = diameter,
                backgroundColor = WarningOrange,
                progress = cancelProgress,
                progressColor = Color.White,
                talkBackLabel = "Phát hiện nguy hiểm. Tự động gửi SOS sau $seconds giây. Nhấn và giữ Tôi an toàn 2 giây để hủy trong thời gian đếm ngược 10 giây.",
                onAccessibilityClick = {
                    if (accessibleArmed) {
                        holdCancel.cancel()
                        handler.removeCallbacks(fireCancel)
                        accessibleArmed = false
                        holdingCancel = false
                    } else {
                        accessibleArmed = true
                        holdingCancel = true
                        holdCancel.start(SystemClock.elapsedRealtime())
                        handler.postDelayed(fireCancel, 2000)
                    }
                },
                onPressStart = {
                    holdingCancel = true
                    holdCancel.start(SystemClock.elapsedRealtime())
                    handler.postDelayed(fireCancel, 2000)
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                },
                onPressEnd = {
                    handler.removeCallbacks(fireCancel)
                    holdCancel.cancel()
                    holdingCancel = false
                },
                modifier = modifier
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                    modifier = Modifier.padding(if (isCompact) 4.dp else 8.dp)
                ) {
                    Text(
                        text = "CẢNH BÁO",
                        fontSize = if (isCompact) 11.5.sp else if (isMedium) 14.sp else 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = "$seconds GIÂY",
                        fontSize = if (isCompact) 18.sp else if (isMedium) 24.sp else 30.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White
                    )
                    Text(
                        text = "TÔI AN TOÀN",
                        fontSize = if (isCompact) 12.5.sp else if (isMedium) 15.sp else 19.sp,
                        fontWeight = FontWeight.Black,
                        color = Color(0xFFFFEB3B)
                    )
                    Text(
                        text = if (holdingCancel) "ĐANG HỦY..." else "GIỮ ĐỂ HỦY (10S)",
                        fontSize = if (isCompact) 10.sp else if (isMedium) 12.sp else 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color.White
                    )
                }
            }
        }

        MainScreenStatus.SOS_SENT -> {
            // SOS Sent: ĐÃ KÍCH HOẠT - reflect truthful sosDeliveryMessage
            val isCompact = diameter < 110.dp
            val isMedium = diameter < 155.dp

            Box(
                contentAlignment = Alignment.Center,
                modifier = modifier
                    .size(diameter)
                    .shadow(8.dp, CircleShape)
                    .clip(CircleShape)
                    .background(SosRedDark)
                    .semantics {
                        role = Role.Button
                        contentDescription = "Đã kích hoạt SOS: ${controller.sosDeliveryMessage}"
                    }
                    .padding(if (isCompact) 4.dp else 8.dp)
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Text(
                        text = "SOS",
                        fontSize = if (isCompact) 20.sp else if (isMedium) 26.sp else 32.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.height(1.dp))
                    Text(
                        text = "ĐÃ KÍCH HOẠT",
                        fontSize = if (isCompact) 11.sp else if (isMedium) 15.sp else 18.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.height(if (isCompact) 1.dp else 2.dp))
                    Text(
                        text = "CẦN TRỢ GIÚP",
                        fontSize = if (isCompact) 10.sp else if (isMedium) 13.sp else 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFFFFD54F),
                        textAlign = TextAlign.Center
                    )
                }
            }
        }

        MainScreenStatus.HELP_ACKNOWLEDGED -> {
            // Caregiver Acknowledged: NGƯỜI THÂN ĐÃ NHẬN TIN - Chạm để hoàn tất
            val isCompact = diameter < 110.dp
            val isMedium = diameter < 155.dp

            Box(
                contentAlignment = Alignment.Center,
                modifier = modifier
                    .size(diameter)
                    .shadow(8.dp, CircleShape)
                    .clip(CircleShape)
                    .background(SafeGreen)
                    .clickable {
                        controller.complete()
                        controller.safe()
                    }
                    .semantics {
                        role = Role.Button
                        contentDescription = "Người thân đã nhận tin. Nhấn để hoàn tất sự kiện."
                    }
                    .padding(if (isCompact) 4.dp else 8.dp)
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Text(
                        text = "NGƯỜI THÂN",
                        fontSize = if (isCompact) 13.sp else if (isMedium) 18.sp else 24.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.height(if (isCompact) 1.dp else 2.dp))
                    Text(
                        text = "ĐÃ NHẬN TIN",
                        fontSize = if (isCompact) 11.sp else if (isMedium) 16.sp else 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFFFFEB3B),
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.height(if (isCompact) 1.dp else 2.dp))
                    Text(
                        text = "ĐANG ĐẾN GIÚP",
                        fontSize = if (isCompact) 10.sp else if (isMedium) 14.sp else 18.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color.White
                    )
                }
            }
        }
    }
}

/**
 * Reusable circular button with animated progress arc around the circumference.
 */
@Composable
fun CircularHoldButton(
    diameter: Dp,
    backgroundColor: Color,
    progress: Float,
    progressColor: Color,
    talkBackLabel: String,
    onAccessibilityClick: () -> Unit,
    onPressStart: () -> Unit,
    onPressEnd: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit
) {
    Box(
        contentAlignment = Alignment.Center,
        modifier = modifier
            .size(diameter)
            .shadow(6.dp, CircleShape)
            .clip(CircleShape)
            .background(backgroundColor)
            .semantics {
                role = Role.Button
                contentDescription = talkBackLabel
                onClick {
                    onAccessibilityClick()
                    true
                }
            }
            .pointerInput(Unit) {
                detectTapGestures(
                    onPress = {
                        onPressStart()
                        try {
                            tryAwaitRelease()
                        } finally {
                            onPressEnd()
                        }
                    }
                )
            }
    ) {
        Canvas(modifier = Modifier.matchParentSize()) {
            val strokeWidth = if (diameter < 110.dp) 5.dp.toPx() else 8.dp.toPx()
            if (progress > 0f) {
                drawArc(
                    color = progressColor,
                    startAngle = -90f,
                    sweepAngle = 360f * progress,
                    useCenter = false,
                    style = Stroke(width = strokeWidth, cap = StrokeCap.Round)
                )
            }
        }
        content()
    }
}

// Helper data holder
private data class Quint(
    val status: String,
    val color: Color,
    val bg: Color,
    val border: Color,
    val icon: String
)

// ================= Previews required for verification =================

@Preview(name = "1. Tiêu chuẩn 360x740", widthDp = 360, heightDp = 740, showBackground = true)
@Composable
fun PreviewStandardScreen() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "2. Màn hình nhỏ 320x640", widthDp = 320, heightDp = 640, showBackground = true)
@Composable
fun PreviewSmallScreen() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "3. Font lớn 1.35x", widthDp = 360, heightDp = 740, fontScale = 1.35f, showBackground = true)
@Composable
fun PreviewLargeFontScreen() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "3b. Màn nhỏ 320x640 + Font 1.35x", widthDp = 320, heightDp = 640, fontScale = 1.35f, showBackground = true)
@Composable
fun PreviewSmallScreenFont135() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "3c. Màn nhỏ 320x640 + Font 1.50x", widthDp = 320, heightDp = 640, fontScale = 1.5f, showBackground = true)
@Composable
fun PreviewSmallScreenFont150() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "4. Xoay ngang Landscape", widthDp = 640, heightDp = 360, showBackground = true)
@Composable
fun PreviewLandscapeScreen() {
    val controller = remember { DemoController() }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "5. Đang đếm ngược cảnh báo ngã", widthDp = 360, heightDp = 740, showBackground = true)
@Composable
fun PreviewWarningCountdownState() {
    val controller = remember {
        DemoController().apply {
            runReplay()
        }
    }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "6. Đã gửi SOS", widthDp = 360, heightDp = 740, showBackground = true)
@Composable
fun PreviewSosSentState() {
    val controller = remember {
        DemoController().apply {
            help()
        }
    }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "7. Người thân đã nhận tin", widthDp = 360, heightDp = 740, showBackground = true)
@Composable
fun PreviewHelpAcknowledgedState() {
    val controller = remember {
        DemoController().apply {
            help()
            acknowledgeHelp()
        }
    }
    HomeScreen(controller = controller, onOpenContacts = {})
}

@Preview(name = "8. Mất kết nối thiết bị", widthDp = 360, heightDp = 740, showBackground = true)
@Composable
fun PreviewDeviceDisconnectedState() {
    val controller = remember {
        DemoController().apply {
            setDeviceConnectedState(false)
        }
    }
    HomeScreen(controller = controller, onOpenContacts = {})
}
