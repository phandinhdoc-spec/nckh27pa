package vn.nckh27pa.fallsafe.bluetooth

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Divider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import vn.nckh27pa.fallsafe.protocol.Esp32SensorPacket
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Data class representing a BLE device discovered during scanning.
 */
data class DiscoveredBleDevice(
    val device: BluetoothDevice,
    val name: String,
    val address: String,
    val rssi: Int
)

/**
 * Test & Control Screen for ESP32 BLE peripheral integration.
 * Convention: PHONE_ONLY research / test build.
 */
@Composable
fun BleTestScreen(
    bleClient: FallSafeBleClient,
    onBack: () -> Unit,
    modifier: Modifier = Modifier,
    onRequestPermissions: (() -> Unit)? = null
) {
    val context = LocalContext.current
    val connectionState by bleClient.connectionState.collectAsState()
    val latestSensorPacket by bleClient.latestSensorPacket.collectAsState()

    // Discovered devices list
    val discoveredDevices = remember { mutableStateListOf<DiscoveredBleDevice>() }
    var isManualScanning by remember { mutableStateOf(false) }

    // Live list of received events
    val receivedEvents = remember { mutableStateListOf<BleEventPacket>() }

    // Collect eventPackets from FallSafeBleClient
    LaunchedEffect(bleClient) {
        bleClient.eventPackets.onEach { event ->
            receivedEvents.add(0, event)
            if (receivedEvents.size > 50) {
                receivedEvents.removeAt(receivedEvents.lastIndex)
            }
        }.launchIn(this)
    }

    // Profile editor parameter states (14 parameters)
    var impactAcceleration by remember { mutableStateOf(BleFallProfile.DEFAULT.impactAccelerationMs2.toString()) }
    var stillnessTarget by remember { mutableStateOf(BleFallProfile.DEFAULT.stillnessTargetAccelerationMs2.toString()) }
    var stillnessTolerance by remember { mutableStateOf(BleFallProfile.DEFAULT.stillnessToleranceMs2.toString()) }
    var postImpactWindow by remember { mutableStateOf(BleFallProfile.DEFAULT.postImpactWindowMs.toString()) }
    var postImpactStillnessDuration by remember { mutableStateOf(BleFallProfile.DEFAULT.postImpactStillnessDurationMs.toString()) }
    var minStillnessSamples by remember { mutableStateOf(BleFallProfile.DEFAULT.minimumStillnessSamples.toString()) }
    var maxSampleGap by remember { mutableStateOf(BleFallProfile.DEFAULT.maximumSampleGapMs.toString()) }
    var freeFallThreshold by remember { mutableStateOf(BleFallProfile.DEFAULT.freeFallThresholdMs2.toString()) }
    var freeFallMinDuration by remember { mutableStateOf(BleFallProfile.DEFAULT.freeFallMinDurationMs.toString()) }
    var gyroTurnThreshold by remember { mutableStateOf(BleFallProfile.DEFAULT.gyroTurnThresholdDps.toString()) }
    var pressureEvidenceRise by remember { mutableStateOf(BleFallProfile.DEFAULT.pressureEvidenceMinRisePa.toString()) }
    var pressureWindow by remember { mutableStateOf(BleFallProfile.DEFAULT.pressureWindowMs.toString()) }
    var altitudeDropMin by remember { mutableStateOf(BleFallProfile.DEFAULT.altitudeDropMinM.toString()) }
    var sampleWatchdog by remember { mutableStateOf(BleFallProfile.DEFAULT.sampleWatchdogMs.toString()) }

    var profileStatusMessage by remember { mutableStateOf<String?>(null) }
    var requestStatusMessage by remember { mutableStateOf<String?>(null) }

    // Reset profile form values
    val resetProfileToDefaults: () -> Unit = {
        impactAcceleration = BleFallProfile.DEFAULT.impactAccelerationMs2.toString()
        stillnessTarget = BleFallProfile.DEFAULT.stillnessTargetAccelerationMs2.toString()
        stillnessTolerance = BleFallProfile.DEFAULT.stillnessToleranceMs2.toString()
        postImpactWindow = BleFallProfile.DEFAULT.postImpactWindowMs.toString()
        postImpactStillnessDuration = BleFallProfile.DEFAULT.postImpactStillnessDurationMs.toString()
        minStillnessSamples = BleFallProfile.DEFAULT.minimumStillnessSamples.toString()
        maxSampleGap = BleFallProfile.DEFAULT.maximumSampleGapMs.toString()
        freeFallThreshold = BleFallProfile.DEFAULT.freeFallThresholdMs2.toString()
        freeFallMinDuration = BleFallProfile.DEFAULT.freeFallMinDurationMs.toString()
        gyroTurnThreshold = BleFallProfile.DEFAULT.gyroTurnThresholdDps.toString()
        pressureEvidenceRise = BleFallProfile.DEFAULT.pressureEvidenceMinRisePa.toString()
        pressureWindow = BleFallProfile.DEFAULT.pressureWindowMs.toString()
        altitudeDropMin = BleFallProfile.DEFAULT.altitudeDropMinM.toString()
        sampleWatchdog = BleFallProfile.DEFAULT.sampleWatchdogMs.toString()
        profileStatusMessage = "Đã khôi phục thông số mặc định"
    }

    // BLE scanner implementation for discovered devices list
    val bluetoothManager = remember { context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager }
    val bluetoothAdapter = remember { bluetoothManager?.adapter ?: BluetoothAdapter.getDefaultAdapter() }
    val handler = remember { Handler(Looper.getMainLooper()) }

    val scanCallback = remember {
        object : ScanCallback() {
            @SuppressLint("MissingPermission")
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                val dev = result.device ?: return
                val address = dev.address ?: return
                val name = try { dev.name } catch (_: Exception) { null } ?: "ESP32 FallSafe"
                val existingIndex = discoveredDevices.indexOfFirst { it.address == address }
                val discovered = DiscoveredBleDevice(
                    device = dev,
                    name = name,
                    address = address,
                    rssi = result.rssi
                )
                if (existingIndex >= 0) {
                    discoveredDevices[existingIndex] = discovered
                } else {
                    discoveredDevices.add(discovered)
                }
            }

            override fun onScanFailed(errorCode: Int) {
                isManualScanning = false
                profileStatusMessage = "Quét BLE thất bại (Mã lỗi: $errorCode)"
            }
        }
    }

    val startDeviceScan: () -> Unit = {
        onRequestPermissions?.invoke()
        discoveredDevices.clear()
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        if (scanner != null && bluetoothAdapter.isEnabled) {
            try {
                val filter = ScanFilter.Builder()
                    .setServiceUuid(ParcelUuid(BleGattUuids.SERVICE_UUID))
                    .build()
                val settings = ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                    .build()
                scanner.startScan(listOf(filter), settings, scanCallback)
                isManualScanning = true
                handler.postDelayed({
                    if (isManualScanning) {
                        try {
                            scanner.stopScan(scanCallback)
                        } catch (_: Exception) {}
                        isManualScanning = false
                    }
                }, 10000L)
            } catch (e: Exception) {
                isManualScanning = false
                profileStatusMessage = "Lỗi khởi động quét: ${e.message}"
            }
        } else {
            profileStatusMessage = "Bluetooth chưa được bật hoặc không khả dụng"
        }
    }

    val stopDeviceScan: () -> Unit = {
        try {
            bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
        } catch (_: Exception) {}
        isManualScanning = false
    }

    DisposableEffect(Unit) {
        onDispose {
            stopDeviceScan()
        }
    }

    val scrollState = rememberScrollState()

    Surface(
        modifier = modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        text = "KIỂM THỬ BLE ESP32",
                        fontSize = 22.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.primary
                    )
                    Text(
                        text = "PHONE_ONLY Test & Research Surface",
                        fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.outline
                    )
                }
                OutlinedButton(onClick = onBack) {
                    Text("Đóng")
                }
            }

            // SECTION 1 & 2: Connection Status & Scan Controls
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "1. TRẠNG THÁI KẾT NỐI & THIẾT BỊ",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )

                    // Status Badge
                    ConnectionStatusBadge(connectionState = connectionState)

                    // Controls Row
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        if (!isManualScanning) {
                            Button(
                                onClick = startDeviceScan,
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Quét thiết bị")
                            }
                        } else {
                            Button(
                                onClick = stopDeviceScan,
                                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Dừng quét")
                            }
                        }

                        if (connectionState !is BleConnectionState.Disconnected) {
                            OutlinedButton(
                                onClick = {
                                    bleClient.disconnect()
                                    stopDeviceScan()
                                },
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Ngắt kết nối")
                            }
                        }
                    }

                    if (isManualScanning) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            CircularProgressIndicator(
                                modifier = Modifier.height(18.dp).width(18.dp),
                                strokeWidth = 2.dp
                            )
                            Text(
                                "Đang quét thiết bị có UUID ${BleGattUuids.SERVICE_UUID}...",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.outline
                            )
                        }
                    }

                    // Discovered devices list
                    if (discoveredDevices.isNotEmpty()) {
                        Text(
                            text = "Thiết bị tìm thấy (${discoveredDevices.size}):",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                        discoveredDevices.forEach { dev ->
                            Card(
                                modifier = Modifier.fillMaxWidth(),
                                colors = CardDefaults.cardColors(
                                    containerColor = MaterialTheme.colorScheme.surface
                                ),
                                shape = RoundedCornerShape(8.dp)
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(8.dp),
                                    horizontalArrangement = Arrangement.SpaceBetween,
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Column(modifier = Modifier.weight(1f)) {
                                        Text(dev.name, fontWeight = FontWeight.Bold, fontSize = 14.sp)
                                        Text(dev.address, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                                        Text("RSSI: ${dev.rssi} dBm", fontSize = 11.sp, color = MaterialTheme.colorScheme.outline)
                                    }
                                    Button(
                                        onClick = {
                                            stopDeviceScan()
                                            bleClient.connect(dev.device)
                                        },
                                        enabled = connectionState !is BleConnectionState.Connecting
                                    ) {
                                        Text("Kết nối", fontSize = 13.sp)
                                    }
                                }
                            }
                        }
                    } else if (!isManualScanning) {
                        Text(
                            text = "Chưa phát hiện thiết bị. Nhấn 'Quét thiết bị' để tìm ESP32.",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.outline
                        )
                    }
                }
            }

            // SECTION 3: Live Telemetry Stream & Request
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Text(
                        text = "2. DỮ LIỆU CẢM BIẾN REAL-TIME (TELEMETRY)",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Button(
                            onClick = {
                                val success = bleClient.sendStartStream()
                                requestStatusMessage = if (success) {
                                    "Đã gửi START_STREAM (JSON hợp đồng S3)"
                                } else {
                                    "Gửi thất bại (chưa kết nối hoặc lỗi ghi GATT)"
                                }
                            },
                            enabled = connectionState is BleConnectionState.Subscribed || connectionState is BleConnectionState.Connected
                        ) {
                            Text("Yêu cầu Telemetry")
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        OutlinedButton(
                            onClick = {
                                val success = bleClient.sendStartStream()
                                requestStatusMessage = if (success) {
                                    "Đã gửi START_STREAM (S3 JSON)"
                                } else {
                                    "Gửi thất bại (chưa kết nối hoặc lỗi ghi GATT)"
                                }
                            },
                            enabled = connectionState is BleConnectionState.Subscribed || connectionState is BleConnectionState.Connected
                        ) {
                            Text("START_STREAM")
                        }
                        OutlinedButton(
                            onClick = {
                                val success = bleClient.sendStopStream()
                                requestStatusMessage = if (success) {
                                    "Đã gửi STOP_STREAM (S3 JSON)"
                                } else {
                                    "Gửi thất bại (chưa kết nối hoặc lỗi ghi GATT)"
                                }
                            },
                            enabled = connectionState is BleConnectionState.Subscribed || connectionState is BleConnectionState.Connected
                        ) {
                            Text("STOP_STREAM")
                        }
                    }

                    // ACK 0006 debug view (S3 official firmware, plain JSON).
                    AckDebugView(bleClient = bleClient)

                    requestStatusMessage?.let { msg ->
                        Text(
                            text = msg,
                            fontSize = 12.sp,
                            color = if (msg.contains("thất bại")) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary
                        )
                    }

                    // Sensor data display
                    TelemetryDisplay(packet = latestSensorPacket)
                }
            }

            // SECTION 4: FallProfile Editor (14 params)
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = "3. CẤU HÌNH THUẬT TOÁN (14 THÔNG SỐ FALL PROFILE)",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "Firmware chính thức định nghĩa 7d2a0006 là đặc trưng ACK (chỉ Notify), chưa có đặc trưng/lệnh nhận profile; sẽ bật lại khi firmware bổ sung.",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.outline
                    )

                    ProfileParamField("impactAccelerationMs2 (m/s²)", impactAcceleration) { impactAcceleration = it }
                    ProfileParamField("stillnessTargetAccelerationMs2 (m/s²)", stillnessTarget) { stillnessTarget = it }
                    ProfileParamField("stillnessToleranceMs2 (m/s²)", stillnessTolerance) { stillnessTolerance = it }
                    ProfileParamField("postImpactWindowMs (ms)", postImpactWindow) { postImpactWindow = it }
                    ProfileParamField("postImpactStillnessDurationMs (ms)", postImpactStillnessDuration) { postImpactStillnessDuration = it }
                    ProfileParamField("minimumStillnessSamples", minStillnessSamples) { minStillnessSamples = it }
                    ProfileParamField("maximumSampleGapMs (ms)", maxSampleGap) { maxSampleGap = it }
                    ProfileParamField("freeFallThresholdMs2 (m/s²)", freeFallThreshold) { freeFallThreshold = it }
                    ProfileParamField("freeFallMinDurationMs (ms)", freeFallMinDuration) { freeFallMinDuration = it }
                    ProfileParamField("gyroTurnThresholdDps (dps)", gyroTurnThreshold) { gyroTurnThreshold = it }
                    ProfileParamField("pressureEvidenceMinRisePa (Pa)", pressureEvidenceRise) { pressureEvidenceRise = it }
                    ProfileParamField("pressureWindowMs (ms)", pressureWindow) { pressureWindow = it }
                    ProfileParamField("altitudeDropMinM (m)", altitudeDropMin) { altitudeDropMin = it }
                    ProfileParamField("sampleWatchdogMs (ms)", sampleWatchdog) { sampleWatchdog = it }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = {
                                try {
                                    val profile = BleFallProfile(
                                        impactAccelerationMs2 = impactAcceleration.toFloatOrNull() ?: BleFallProfile.DEFAULT.impactAccelerationMs2,
                                        stillnessTargetAccelerationMs2 = stillnessTarget.toFloatOrNull() ?: BleFallProfile.DEFAULT.stillnessTargetAccelerationMs2,
                                        stillnessToleranceMs2 = stillnessTolerance.toFloatOrNull() ?: BleFallProfile.DEFAULT.stillnessToleranceMs2,
                                        postImpactWindowMs = postImpactWindow.toLongOrNull() ?: BleFallProfile.DEFAULT.postImpactWindowMs,
                                        postImpactStillnessDurationMs = postImpactStillnessDuration.toLongOrNull() ?: BleFallProfile.DEFAULT.postImpactStillnessDurationMs,
                                        minimumStillnessSamples = minStillnessSamples.toIntOrNull() ?: BleFallProfile.DEFAULT.minimumStillnessSamples,
                                        maximumSampleGapMs = maxSampleGap.toLongOrNull() ?: BleFallProfile.DEFAULT.maximumSampleGapMs,
                                        freeFallThresholdMs2 = freeFallThreshold.toFloatOrNull() ?: BleFallProfile.DEFAULT.freeFallThresholdMs2,
                                        freeFallMinDurationMs = freeFallMinDuration.toLongOrNull() ?: BleFallProfile.DEFAULT.freeFallMinDurationMs,
                                        gyroTurnThresholdDps = gyroTurnThreshold.toFloatOrNull() ?: BleFallProfile.DEFAULT.gyroTurnThresholdDps,
                                        pressureEvidenceMinRisePa = pressureEvidenceRise.toFloatOrNull() ?: BleFallProfile.DEFAULT.pressureEvidenceMinRisePa,
                                        pressureWindowMs = pressureWindow.toLongOrNull() ?: BleFallProfile.DEFAULT.pressureWindowMs,
                                        altitudeDropMinM = altitudeDropMin.toFloatOrNull() ?: BleFallProfile.DEFAULT.altitudeDropMinM,
                                        sampleWatchdogMs = sampleWatchdog.toLongOrNull() ?: BleFallProfile.DEFAULT.sampleWatchdogMs
                                    )
                                    val success = bleClient.writeProfile(profile)
                                    profileStatusMessage = if (success) {
                                        "Đã gửi Profile thành công!"
                                    } else {
                                        "Gửi Profile thất bại (kiểm tra kết nối GATT)"
                                    }
                                } catch (e: Exception) {
                                    profileStatusMessage = "Lỗi dữ liệu: ${e.message}"
                                }
                            },
                            enabled = false,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Gửi Profile (chưa hỗ trợ)")
                        }

                        OutlinedButton(
                            onClick = resetProfileToDefaults,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Đặt lại mặc định")
                        }
                    }

                    profileStatusMessage?.let { msg ->
                        Text(
                            text = msg,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.Medium,
                            color = if (msg.contains("thất bại") || msg.contains("Lỗi")) {
                                MaterialTheme.colorScheme.error
                            } else {
                                MaterialTheme.colorScheme.primary
                            }
                        )
                    }
                }
            }

            // SECTION 5: Received Fall Events Log
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "4. SỰ KIỆN NGÃ (EVENT STREAM)",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold
                        )
                        if (receivedEvents.isNotEmpty()) {
                            TextButton(onClick = { receivedEvents.clear() }) {
                                Text("Xóa danh sách")
                            }
                        }
                    }

                    if (receivedEvents.isEmpty()) {
                        Text(
                            text = "Chưa nhận sự kiện ngã nào từ ESP32.",
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.outline
                        )
                    } else {
                        receivedEvents.forEach { event ->
                            EventItemCard(event = event)
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(24.dp))
        }
    }
}

@Composable
private fun ConnectionStatusBadge(connectionState: BleConnectionState) {
    val (label, bgColor, textColor) = when (connectionState) {
        is BleConnectionState.Disconnected -> Triple("Ngắt kết nối (Disconnected)", Color(0xFFE0E0E0), Color(0xFF424242))
        is BleConnectionState.Scanning -> Triple("Đang quét tìm ESP32...", Color(0xFFFFF9C4), Color(0xFFF57F17))
        is BleConnectionState.Connecting -> Triple("Đang kết nối: ${connectionState.deviceAddress}", Color(0xFFBBDEFB), Color(0xFF0D47A1))
        is BleConnectionState.Connected -> Triple("Đã kết nối: ${connectionState.deviceAddress}", Color(0xFFC8E6C9), Color(0xFF1B5E20))
        is BleConnectionState.Subscribed -> Triple("Dịch vụ sẵn sàng (Subscribed): ${connectionState.deviceAddress}", Color(0xFF81C784), Color(0xFF1B5E20))
        is BleConnectionState.Error -> Triple("Lỗi: ${connectionState.message}", Color(0xFFFFCDD2), Color(0xFFB71C1C))
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(bgColor, RoundedCornerShape(8.dp))
            .padding(horizontal = 12.dp, vertical = 8.dp)
    ) {
        Text(
            text = label,
            color = textColor,
            fontWeight = FontWeight.Bold,
            fontSize = 13.sp
        )
    }
}

@Composable
private fun TelemetryDisplay(packet: Esp32SensorPacket?) {
    if (packet == null) {
        Text(
            text = "Chưa có dữ liệu Telemetry từ thiết bị.",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.outline
        )
        return
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column(
            modifier = Modifier.padding(10.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Text(
                text = "Gói tin #${packet.sequenceNumber} (Thiết bị: ${packet.deviceId})",
                fontWeight = FontWeight.Bold,
                fontSize = 13.sp
            )
            Divider(modifier = Modifier.padding(vertical = 4.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Gia tốc X/Y/Z:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    String.format(Locale.US, "%.2f, %.2f, %.2f m/s²", packet.accelXMs2, packet.accelYMs2, packet.accelZMs2),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Con quay (Gyro) X/Y/Z:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    String.format(Locale.US, "%.1f, %.1f, %.1f dps", packet.gyroXDps, packet.gyroYDps, packet.gyroZDps),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Áp suất khí quyển:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    packet.pressurePa?.let { String.format(Locale.US, "%.1f Pa", it) } ?: "N/A",
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Nhiệt độ:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    packet.temperatureC?.let { String.format(Locale.US, "%.1f °C", it) } ?: "N/A",
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Độ dốc cao độ:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    packet.altitudeDeltaM?.let { String.format(Locale.US, "%.2f m", it) } ?: "N/A",
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Pin / Sạc:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    // S3 reports batteryPercent=-1 when no battery is present.
                    (if (packet.batteryPercent < 0) "N/A" else "${packet.batteryPercent}%") +
                        " (${if (packet.isCharging) "Đang sạc" else "Pin thường"})",
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text("Chất lượng cảm biến:", fontSize = 12.sp, color = MaterialTheme.colorScheme.outline)
                Text(
                    "${packet.sensorQuality}%",
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.SemiBold
                )
            }
        }
    }
}

@Composable
private fun ProfileParamField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label, fontSize = 12.sp) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        modifier = Modifier.fillMaxWidth(),
        singleLine = true
    )
}

@Composable
private fun AckDebugView(bleClient: FallSafeBleClient) {
    var lastAck by remember { mutableStateOf<String?>(null) }
    DisposableEffect(bleClient) {
        bleClient.onAckJson = { json ->
            lastAck = if (json.length > 300) json.take(300) + "…" else json
        }
        onDispose { bleClient.onAckJson = null }
    }
    lastAck?.let { ack ->
        Text(
            text = "ACK 0006: $ack",
            fontSize = 11.sp,
            fontFamily = FontFamily.Monospace,
            color = MaterialTheme.colorScheme.outline
        )
    }
}

@Composable
private fun EventItemCard(event: BleEventPacket) {
    val timeStr = remember(event.timestampMs) {
        if (event.timestampMs > 0) {
            SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date(event.timestampMs))
        } else {
            "N/A"
        }
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column(
            modifier = Modifier.padding(10.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "Mã SK: ${event.eventId}",
                    fontWeight = FontWeight.Bold,
                    fontSize = 13.sp
                )
                Text(
                    text = event.severity,
                    fontWeight = FontWeight.Bold,
                    fontSize = 12.sp,
                    color = if (event.severity == "CRITICAL") MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary
                )
            }
            Text(
                text = "Loại: ${event.eventType} | Thời gian: $timeStr",
                fontSize = 12.sp,
                color = MaterialTheme.colorScheme.outline
            )
            event.alertState?.let {
                Text("Trạng thái cảnh báo: $it", fontSize = 12.sp)
            }
            event.peakAccelerationMs2?.let {
                Text(
                    String.format(Locale.US, "Gia tốc cực đại: %.2f m/s²", it),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
            event.altitudeDeltaM?.let {
                Text(
                    String.format(Locale.US, "Độ dốc cao độ: %.2f m", it),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
            if (event.sosButtonPressed) {
                Text("Nút SOS vật lý: ĐƯỢC NHẤN", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.error)
            }
        }
    }
}
