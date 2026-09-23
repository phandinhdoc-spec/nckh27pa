package vn.nckh27pa.fallsafe

import android.os.Bundle
import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.style.TextAlign
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import core.State
import core.Status

import androidx.core.view.WindowCompat

class MainActivity : ComponentActivity() {
    private val callingPermissionRequest = registerForActivityResult(ActivityResultContracts.RequestPermission()) {
        controller.refreshCapabilityTruth()
    }
    private val messagingPermissionRequest = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        controller.refreshCapabilityTruth()
    }
    private val locationPermissionRequest = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        controller.refreshCapabilityTruth()
    }
    private val contactsViewModel by lazy {
        androidx.lifecycle.ViewModelProvider(this, object : androidx.lifecycle.ViewModelProvider.Factory {
            override fun <T : androidx.lifecycle.ViewModel> create(modelClass: Class<T>): T {
                @Suppress("UNCHECKED_CAST")
                return ContactsViewModel(controller) as T
            }
        })[ContactsViewModel::class.java]
    }
    private lateinit var collector: PhoneSensorCollector
    private var backgroundStartPending = false
    private lateinit var ownership: SensorOwnership
    private val controller get() = (application as DemoApplication).controller
    private val handler = Handler(Looper.getMainLooper())
    private var sensorSummary by mutableStateOf("Chưa thu cảm biến")
    private val poll = object : Runnable {
        override fun run() {
            if (!controller.backgroundMonitoring) controller.displayPhone(collector.latest())
            handler.postDelayed(this, 250)
        }
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, true)
        collector = PhoneSensorCollector(this, controller::acceptPhone)
        ownership = SensorOwnership({ collector.start() }, { collector.stop() })
        controller.bindCapabilityAccess(
            vn.nckh27pa.fallsafe.permissions.CapabilityAccessController(
                vn.nckh27pa.fallsafe.permissions.AndroidCapabilityPlatform(this, ::launchCapabilityRequest),
                vn.nckh27pa.fallsafe.permissions.SharedPreferencesCapabilityAttemptStore(this)
            )
        )
        setContent {
            val darkTheme = isSystemInDarkTheme()
            val view = androidx.compose.ui.platform.LocalView.current
            if (!view.isInEditMode) {
                SideEffect {
                    val window = (view.context as? android.app.Activity)?.window
                    if (window != null) {
                        val insetsController = WindowCompat.getInsetsController(window, view)
                        insetsController.isAppearanceLightStatusBars = !darkTheme
                        insetsController.isAppearanceLightNavigationBars = !darkTheme
                    }
                }
            }
            MaterialTheme(colorScheme = if (darkTheme) darkColorScheme() else lightColorScheme()) {
                DemoScreen(controller, contactsViewModel, sensorSummary, ::startBackground, ::stopBackground)
            }
        }
    }
    private fun launchCapabilityRequest(capability: vn.nckh27pa.fallsafe.permissions.Capability) {
        when (capability) {
            vn.nckh27pa.fallsafe.permissions.Capability.CALLING ->
                callingPermissionRequest.launch(Manifest.permission.CALL_PHONE)
            vn.nckh27pa.fallsafe.permissions.Capability.MESSAGING ->
                messagingPermissionRequest.launch(arrayOf(Manifest.permission.SEND_SMS))
            vn.nckh27pa.fallsafe.permissions.Capability.LOCATION ->
                locationPermissionRequest.launch(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION))
        }
    }
    override fun onResume() {
        super.onResume()
        controller.foreground = true
        controller.onMonitoringChanged = {
            ownership.update(controller.foreground, controller.backgroundMonitoring)
            sensorSummary = collector.activeSensors.joinToString().ifEmpty { NO_SENSOR_SUMMARY }
        }
        ownership.update(true, controller.backgroundMonitoring)
        sensorSummary = if (controller.backgroundMonitoring) controller.backgroundSensors else collector.activeSensors.joinToString().ifEmpty { NO_SENSOR_SUMMARY }
        AndroidTrace.logPermission(this) // TEMPORARY DIAGNOSTIC
        controller.refreshCapabilityTruth()
        controller.resumed()
        handler.post(poll)
        if (backgroundStartPending) { backgroundStartPending = false; beginMonitoring() }
    }
    private fun startBackground() {
        if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 27)
        } else beginMonitoring()
    }
    private fun beginMonitoring() {
        try { startForegroundService(Intent(this, MonitoringService::class.java)) }
        catch (_: RuntimeException) {
            controller.backgroundMessage = "Hệ thống từ chối giám sát nền; vẫn thu khi mở app."
            ownership.update(controller.foreground, controller.backgroundMonitoring)
        }
    }
    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 27) {
            if (grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
                // Resume after the system permission dialog, never start from a paused Activity.
                if (controller.foreground) beginMonitoring() else backgroundStartPending = true
            } else {
                backgroundStartPending = false
                controller.backgroundMessage = "Chưa bật giám sát nền vì quyền thông báo bị từ chối; cảm biến vẫn dùng khi mở app."
            }
        }
    }
    private fun stopBackground() {
        if (controller.snapshot.state != State.MONITORING) return
        stopService(Intent(this, MonitoringService::class.java))
    }
    override fun onPause() {
        controller.foreground = false
        controller.onMonitoringChanged = null
        ownership.update(false, controller.backgroundMonitoring)
        handler.removeCallbacks(poll)
        controller.paused()
        super.onPause()
    }
}

@Composable
private fun DemoScreen(c: DemoController, contactsViewModel: ContactsViewModel, sensorSummary: String, startBackground: () -> Unit, stopBackground: () -> Unit) {
    var tab by rememberSaveable { mutableIntStateOf(0) }
    var inCalibrationScreen by rememberSaveable { mutableStateOf(false) }
    val tabs = listOf("Trang chủ", "Sự kiện", "Người thân", "Cài đặt")
    val scrollState = rememberScrollState()
    LaunchedEffect(tab, c.snapshot.state, inCalibrationScreen) { scrollState.scrollTo(0) }

    // When back button is pressed in calibration, return to Settings screen
    androidx.activity.compose.BackHandler(enabled = inCalibrationScreen) {
        inCalibrationScreen = false
    }

    // When back button is pressed on other tabs, navigate back to Home
    androidx.activity.compose.BackHandler(enabled = !inCalibrationScreen && tab != 0) {
        tab = 0
    }

    // When danger is detected, automatically switch to Home tab so the large countdown is front and center
    LaunchedEffect(c.snapshot.state) {
        if (c.snapshot.state == State.VERIFYING) {
            inCalibrationScreen = false
            tab = 0
        }
    }

    Scaffold(bottomBar = {
        if (!inCalibrationScreen) {
            val fontScale = LocalDensity.current.fontScale
            val navLabelSize = when {
                fontScale >= 1.45f -> 10.sp
                fontScale >= 1.25f -> 10.5.sp
                else -> 13.sp
            }
            val navIconSize = when {
                fontScale >= 1.45f -> 18.sp
                fontScale >= 1.25f -> 19.sp
                else -> 22.sp
            }

            NavigationBar {
                tabs.forEachIndexed { i, title ->
                    NavigationBarItem(
                        selected = tab == i,
                        onClick = { tab = i },
                        icon = { Text(listOf("⌂", "≡", "♡", "⚙")[i], fontSize = navIconSize) },
                        label = {
                            Text(
                                text = title,
                                fontSize = navLabelSize,
                                fontWeight = if (tab == i) FontWeight.Bold else FontWeight.Normal,
                                maxLines = 1,
                                softWrap = false,
                                letterSpacing = if (fontScale >= 1.25f) (-0.2).sp else 0.sp
                            )
                        }
                    )
                }
            }
        }
    }) { inset ->
        if (inCalibrationScreen) {
            FallDetectionCalibrationScreen(
                controller = c,
                onBack = { inCalibrationScreen = false },
                modifier = Modifier.fillMaxSize().padding(inset)
            )
        } else {
            when (tab) {
                0 -> {
                    HomeScreen(
                        controller = c,
                        onOpenContacts = { tab = 2 },
                        modifier = Modifier.fillMaxSize().padding(inset)
                    )
                }
                1 -> {
                    EventsScreen(c, Modifier.fillMaxSize().padding(inset))
                }
                2 -> {
                    ContactsScreen(
                        controller = c,
                        viewModel = contactsViewModel,
                        onBack = { tab = 0 },
                        modifier = Modifier.fillMaxSize().padding(inset)
                    )
                }
                3 -> {
                    SettingsScreen(
                        c = c,
                        sensorSummary = sensorSummary,
                        startBackground = startBackground,
                        stopBackground = stopBackground,
                        onOpenContacts = { tab = 2 },
                        onOpenCalibration = { inCalibrationScreen = true },
                        modifier = Modifier.fillMaxSize().padding(inset)
                    )
                }
            }
        }
    }
}

@Composable
private fun EventsScreen(c: DemoController, modifier: Modifier = Modifier) {
    val scroll = rememberScrollState()
    Column(
        modifier = modifier.verticalScroll(scroll).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text("LỊCH SỬ SỰ KIỆN", fontSize = 24.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)
        Text("Danh sách các lần thay đổi trạng thái và cảnh báo gần nhất trong phiên làm việc:", fontSize = 20.sp)
        if (c.events.isEmpty()) {
            Text("Chưa ghi nhận sự kiện nào.", fontSize = 20.sp, color = androidx.compose.ui.graphics.Color.Gray)
        } else {
            c.events.asReversed().forEach {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
                    shape = androidx.compose.foundation.shape.RoundedCornerShape(12.dp)
                ) {
                    Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Text("Sự kiện #${it.eventId}", fontSize = 22.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)
                        Text("Trạng thái: ${stateVietnamese(it.state)}", fontSize = 20.sp)
                        Text("Tiến trình: ${statusLabel(it.status)}", fontSize = 20.sp)
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsScreen(
    c: DemoController,
    sensorSummary: String,
    startBackground: () -> Unit,
    stopBackground: () -> Unit,
    onOpenContacts: () -> Unit,
    onOpenCalibration: () -> Unit,
    modifier: Modifier = Modifier
) {
    val scroll = rememberScrollState()
    Column(
        modifier = modifier.verticalScroll(scroll).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text("CÀI ĐẶT & KIỂM TRA HỆ THỐNG", fontSize = 22.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)

        PermissionCenterSection(controller = c)

        // Section: Emergency Contact Management
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = ContactBlueContainer),
            border = BorderStroke(1.5.dp, ContactBlueBorder),
            shape = androidx.compose.foundation.shape.RoundedCornerShape(14.dp)
        ) {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("NGƯỜI THÂN NHẬN CẢNH BÁO", fontSize = 20.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold, color = ContactBlue)
                Text(
                    text = "Người nhận chính: ${c.primaryContactFullName} (${c.primaryContactName})\n" +
                            "Số điện thoại: ${ContactValidator.mask(c.primaryContactPhone)}",
                    fontSize = 17.sp,
                    fontWeight = FontWeight.Medium
                )
                Button(
                    onClick = onOpenContacts,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = ContactBlue)
                ) {
                    Text("QUẢN LÝ DANH SÁCH NGƯỜI THÂN", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        }

        // Section: Fall Detection Calibration Entry
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.secondaryContainer),
            shape = androidx.compose.foundation.shape.RoundedCornerShape(14.dp)
        ) {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    text = "PHÁT HIỆN TÉ NGÃ",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSecondaryContainer
                )
                Text(
                    text = "Cấu hình bảng ngưỡng thử nghiệm và theo dõi cảm biến gia tốc phục vụ nghiên cứu.",
                    fontSize = 16.sp,
                    color = MaterialTheme.colorScheme.onSecondaryContainer
                )
                Text(
                    text = "Đang sử dụng: ${c.activeProfile.displayName}",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Medium
                )
                Button(
                    onClick = onOpenCalibration,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp)
                ) {
                    Text("HIỆU CHỈNH THỬ NGHIỆM", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                }
            }
        }

        // Test panel for verifying all 5 required states easily
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.primaryContainer),
            shape = androidx.compose.foundation.shape.RoundedCornerShape(14.dp)
        ) {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("MÔ PHỎNG 5 TRẠNG THÁI GIAO DIỆN", fontSize = 20.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.Bold)
                Action("1. An toàn (Bình thường)", {
                    c.safe()
                    c.setDeviceConnectedState(true)
                    c.resetCaregiverAcknowledged()
                })
                Action("2. Mô phỏng ngã (Đếm ngược 10s)", {
                    c.setDeviceConnectedState(true)
                    c.runReplay()
                }, c.snapshot.state == State.MONITORING && !c.replaying)
                Action("3. Kích hoạt đã gửi SOS", {
                    c.setDeviceConnectedState(true)
                    c.help()
                })
                Action("4. Người thân đã nhận tin", {
                    c.setDeviceConnectedState(true)
                    c.acknowledgeHelp()
                })
                Action(
                    if (c.deviceConnected) "5. Mô phỏng MẤT KẾT NỐI thiết bị" else "5. Khôi phục KẾT NỐI thiết bị",
                    { c.setDeviceConnectedState(!c.deviceConnected) }
                )
                Action("Hoàn tất sự kiện / Đặt lại", c::complete)
            }
        }

        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text("Giả lập lỗi gửi cảnh báo", Modifier.weight(1f), fontSize = 20.sp)
            Switch(c.fail, c::setFailure, Modifier.semantics { contentDescription = "Giả lập lỗi gửi cảnh báo" })
        }

        Text(c.syncStatus, fontSize = 16.sp)
        Text(c.backgroundMessage, fontSize = 20.sp)
        Action("Bật giám sát nền", startBackground, !c.backgroundMonitoring)
        Action("Dừng giám sát nền", stopBackground, c.backgroundMonitoring && c.snapshot.state == State.MONITORING)

        Action("Dùng cảm biến điện thoại thật", c::usePhone, c.snapshot.state == State.MONITORING)
        Text("Cảm biến đã đăng ký: ${if (c.backgroundMonitoring) c.backgroundSensors else sensorSummary}", fontSize = 16.sp)
        SensorDiagnosticsSection(
            controller = c,
            sensorsAvailable = sensorSummary.isNotBlank() && sensorSummary != NO_SENSOR_SUMMARY
        )
    }
}

private const val NO_SENSOR_SUMMARY = "Không có cảm biến khả dụng"

/**
 * Diagnostics only: reads the SAME packet/observation the fall detector consumes.
 * No new sensor listener; no detection logic; updates at ~4 Hz so the page does not jump.
 */
@Composable
private fun SensorDiagnosticsSection(
    controller: DemoController,
    sensorsAvailable: Boolean
) {
    var diagnostics by remember {
        mutableStateOf(
            settingsSensorDiagnostics(controller.packet, controller.observation, sensorsAvailable)
        )
    }
    LaunchedEffect(sensorsAvailable) {
        while (isActive) {
            diagnostics = settingsSensorDiagnostics(controller.packet, controller.observation, sensorsAvailable)
            delay(250L)
        }
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        border = BorderStroke(1.5.dp, MaterialTheme.colorScheme.outlineVariant),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(14.dp)
    ) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(
                "DỮ LIỆU CẢM BIẾN",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            // Gia tốc kế
            Text("Gia tốc kế", fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
            DiagnosticAxisRow("X", formatDiagnosticNumber(diagnostics.accelX, 2) + " m/s²")
            DiagnosticAxisRow("Y", formatDiagnosticNumber(diagnostics.accelY, 2) + " m/s²")
            DiagnosticAxisRow("Z", formatDiagnosticNumber(diagnostics.accelZ, 2) + " m/s²")
            DiagnosticAxisRow("Độ lớn", formatDiagnosticNumber(diagnostics.accelMagnitudeMs2, 2) + " m/s²")

            // Con quay hồi chuyển
            Text("Con quay hồi chuyển", fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
            DiagnosticAxisRow("X", formatDiagnosticNumber(diagnostics.gyroXRadS, 3) + " rad/s")
            DiagnosticAxisRow("Y", formatDiagnosticNumber(diagnostics.gyroYRadS, 3) + " rad/s")
            DiagnosticAxisRow("Z", formatDiagnosticNumber(diagnostics.gyroZRadS, 3) + " rad/s")
            DiagnosticAxisRow("Độ lớn", formatDiagnosticNumber(diagnostics.gyroMagnitudeRadS, 3) + " rad/s")

            Text("Nguồn cảm biến: ${diagnostics.source}", fontSize = 15.sp)
            Text(
                "Trạng thái dữ liệu: ${sensorDataStateVietnamese(diagnostics.dataState)} (${diagnostics.dataState.name})",
                fontSize = 15.sp
            )
            Text(
                "FALL state: ${diagnostics.fallPhase.name} · ${detectionPhaseVietnamese(diagnostics.fallPhase)}",
                fontSize = 15.sp
            )
            Text(
                "Hệ thống: ${controller.snapshot.state.name}",
                fontSize = 15.sp
            )
        }
    }
}

@Composable
private fun DiagnosticAxisRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, modifier = Modifier.weight(0.35f), fontSize = 16.sp)
        Text(
            value,
            modifier = Modifier.weight(0.65f),
            fontSize = 16.sp,
            textAlign = TextAlign.End,
            fontWeight = FontWeight.Medium
        )
    }
}

@Composable
private fun Action(label: String, action: () -> Unit, enabled: Boolean = true) {
    Button(
        onClick = action,
        enabled = enabled,
        modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp)
    ) {
        Text(label, fontSize = 20.sp, fontWeight = androidx.compose.ui.text.font.FontWeight.SemiBold)
    }
}

private fun statusLabel(status: Status): String = when (status) {
    Status.NOT_REQUIRED -> "Chưa yêu cầu gửi"
    Status.COUNTDOWN -> "Đang đếm ngược"
    Status.SENDING -> "Đang gửi cảnh báo"
    Status.SENT -> "Đã ghi nhận SOS tại chỗ"
    Status.FAILED -> "Gửi cảnh báo thất bại"
    Status.ACKNOWLEDGED -> "Đã hoàn tất sự kiện"
}

private fun stateVietnamese(state: State): String = when (state) {
    State.MONITORING -> "Đang giám sát"
    State.SUSPECTED -> "Nghi ngờ té ngã"
    State.VERIFYING -> "Đang đếm ngược xác minh"
    State.ALERTING -> "Đang kích hoạt cảnh báo"
    State.AWAITING_HELP -> "Đang chờ trợ giúp"
}
