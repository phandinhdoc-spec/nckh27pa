package vn.nckh27pa.fallsafe

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import java.util.Locale
import kotlin.math.roundToInt

/**
 * Định dạng Float dạng canonical không làm tròn làm mất độ chính xác,
 * bảo đảm round-trip tuyệt đối giữa Float <-> String <-> Float.
 */
fun formatCanonicalFloat(value: Float): String =
    if (value.isFinite()) {
        if (value % 1f == 0f) {
            value.toInt().toString()
        } else {
            value.toString()
        }
    } else {
        ""
    }

/**
 * Trạng thái bản nháp đang chỉnh sửa cho 7 thông số ngưỡng thử nghiệm.
 */
data class CalibrationDraft(
    val impactAccelerationText: String,
    val stillnessTargetAccelerationText: String,
    val stillnessToleranceText: String,
    val postImpactWindowMsText: String,
    val postImpactStillnessDurationMsText: String,
    val minimumStillnessSamplesText: String,
    val maximumSampleGapMsText: String
) {
    companion object {
        fun fromConfig(config: FallDetectionConfig): CalibrationDraft = CalibrationDraft(
            impactAccelerationText = formatCanonicalFloat(config.impactAccelerationMs2),
            stillnessTargetAccelerationText = formatCanonicalFloat(config.stillnessTargetAccelerationMs2),
            stillnessToleranceText = formatCanonicalFloat(config.stillnessToleranceMs2),
            postImpactWindowMsText = config.postImpactWindowMs.toString(),
            postImpactStillnessDurationMsText = config.postImpactStillnessDurationMs.toString(),
            minimumStillnessSamplesText = config.minimumStillnessSamples.toString(),
            maximumSampleGapMsText = config.maximumSampleGapMs.toString()
        )
    }
}

/**
 * Kết quả thẩm định bản nháp ngưỡng thử nghiệm.
 */
data class CalibrationValidation(
    val parsedConfig: FallDetectionConfig? = null,
    val impactError: String? = null,
    val stillnessTargetError: String? = null,
    val stillnessToleranceError: String? = null,
    val postImpactWindowError: String? = null,
    val postImpactStillnessDurationError: String? = null,
    val minimumStillnessSamplesError: String? = null,
    val maximumSampleGapError: String? = null,
    val globalError: String? = null
) {
    val isValid: Boolean
        get() = parsedConfig != null &&
            impactError == null &&
            stillnessTargetError == null &&
            stillnessToleranceError == null &&
            postImpactWindowError == null &&
            postImpactStillnessDurationError == null &&
            minimumStillnessSamplesError == null &&
            maximumSampleGapError == null &&
            globalError == null
}

/**
 * Thẩm định dữ liệu đầu vào cho 7 thông số ngưỡng thử nghiệm và quy tắc duration <= window.
 */
fun validateCalibrationDraft(draft: CalibrationDraft): CalibrationValidation {
    var impactVal: Float? = null
    var impactErr: String? = null
    val impactRaw = draft.impactAccelerationText.trim().replace(',', '.')
    if (impactRaw.isEmpty()) {
        impactErr = "Không được để trống"
    } else {
        val f = impactRaw.toFloatOrNull()
        if (f == null || !f.isFinite()) {
            impactErr = "Giá trị không hợp lệ"
        } else if (f < FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2 || f > FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2) {
            impactErr = "Phải từ ${FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2.toInt()} đến ${FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2.toInt()} m/s²"
        } else {
            impactVal = f
        }
    }

    var stillnessTargetVal: Float? = null
    var stillnessTargetErr: String? = null
    val targetRaw = draft.stillnessTargetAccelerationText.trim().replace(',', '.')
    if (targetRaw.isEmpty()) {
        stillnessTargetErr = "Không được để trống"
    } else {
        val f = targetRaw.toFloatOrNull()
        if (f == null || !f.isFinite()) {
            stillnessTargetErr = "Giá trị không hợp lệ"
        } else if (f < FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2 || f > FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2) {
            stillnessTargetErr = "Phải từ ${FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2.toInt()} đến ${FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2.toInt()} m/s²"
        } else {
            stillnessTargetVal = f
        }
    }

    var stillnessToleranceVal: Float? = null
    var stillnessToleranceErr: String? = null
    val tolRaw = draft.stillnessToleranceText.trim().replace(',', '.')
    if (tolRaw.isEmpty()) {
        stillnessToleranceErr = "Không được để trống"
    } else {
        val f = tolRaw.toFloatOrNull()
        if (f == null || !f.isFinite()) {
            stillnessToleranceErr = "Giá trị không hợp lệ"
        } else if (f < FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2 || f > FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2) {
            stillnessToleranceErr = "Phải từ ${FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2} đến ${FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2.toInt()} m/s²"
        } else {
            stillnessToleranceVal = f
        }
    }

    var windowVal: Long? = null
    var windowErr: String? = null
    val windowRaw = draft.postImpactWindowMsText.trim()
    if (windowRaw.isEmpty()) {
        windowErr = "Không được để trống"
    } else {
        val l = windowRaw.toLongOrNull()
        if (l == null) {
            windowErr = "Giá trị phải là số nguyên"
        } else if (l < FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS || l > FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS) {
            windowErr = "Phải từ ${FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS} đến ${FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS} ms"
        } else {
            windowVal = l
        }
    }

    var durationVal: Long? = null
    var durationErr: String? = null
    val durationRaw = draft.postImpactStillnessDurationMsText.trim()
    if (durationRaw.isEmpty()) {
        durationErr = "Không được để trống"
    } else {
        val l = durationRaw.toLongOrNull()
        if (l == null) {
            durationErr = "Giá trị phải là số nguyên"
        } else if (l < FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS || l > FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS) {
            durationErr = "Phải từ ${FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS} đến ${FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS} ms"
        } else {
            durationVal = l
        }
    }

    // Quy tắc bất biến: postImpactStillnessDurationMs <= postImpactWindowMs
    if (durationVal != null && windowVal != null && durationVal > windowVal) {
        durationErr = "Thời gian bất động ($durationVal ms) phải ≤ cửa sổ theo dõi ($windowVal ms)"
    }

    var samplesVal: Int? = null
    var samplesErr: String? = null
    val samplesRaw = draft.minimumStillnessSamplesText.trim()
    if (samplesRaw.isEmpty()) {
        samplesErr = "Không được để trống"
    } else {
        val i = samplesRaw.toIntOrNull()
        if (i == null) {
            samplesErr = "Giá trị phải là số nguyên"
        } else if (i < FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN || i > FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX) {
            samplesErr = "Phải từ ${FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN} đến ${FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX} mẫu"
        } else {
            samplesVal = i
        }
    }

    var gapVal: Long? = null
    var gapErr: String? = null
    val gapRaw = draft.maximumSampleGapMsText.trim()
    if (gapRaw.isEmpty()) {
        gapErr = "Không được để trống"
    } else {
        val l = gapRaw.toLongOrNull()
        if (l == null) {
            gapErr = "Giá trị phải là số nguyên"
        } else if (l < FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS || l > FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS) {
            gapErr = "Phải từ ${FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS} đến ${FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS} ms"
        } else {
            gapVal = l
        }
    }

    var parsedConfig: FallDetectionConfig? = null
    var globalErr: String? = null
    if (impactVal != null && stillnessTargetVal != null && stillnessToleranceVal != null &&
        windowVal != null && durationVal != null && durationVal <= windowVal &&
        samplesVal != null && gapVal != null
    ) {
        try {
            parsedConfig = FallDetectionConfig(
                impactAccelerationMs2 = impactVal,
                stillnessTargetAccelerationMs2 = stillnessTargetVal,
                stillnessToleranceMs2 = stillnessToleranceVal,
                postImpactWindowMs = windowVal,
                postImpactStillnessDurationMs = durationVal,
                minimumStillnessSamples = samplesVal,
                maximumSampleGapMs = gapVal
            )
        } catch (e: Exception) {
            globalErr = e.message ?: "Cấu hình không hợp lệ"
        }
    }

    return CalibrationValidation(
        parsedConfig = parsedConfig,
        impactError = impactErr,
        stillnessTargetError = stillnessTargetErr,
        stillnessToleranceError = stillnessToleranceErr,
        postImpactWindowError = windowErr,
        postImpactStillnessDurationError = durationErr,
        minimumStillnessSamplesError = samplesErr,
        maximumSampleGapError = gapErr,
        globalError = globalErr
    )
}

/**
 * Kiểm tra xem bản nháp có thay đổi chưa lưu so với cấu hình đã lưu của profile hay không.
 * Nếu bản nháp không hợp lệ (validation.isValid == false) thì cũng được coi là có thay đổi chưa lưu.
 */
fun hasUnsavedChanges(validation: CalibrationValidation, savedConfig: FallDetectionConfig): Boolean {
    if (!validation.isValid) return true
    return validation.parsedConfig != savedConfig
}

/**
 * Điều kiện cho phép kích hoạt profile:
 * - Profile chưa phải là profile đang kích hoạt.
 * - Không có thay đổi chưa lưu (bản nháp phải khớp với dữ liệu đã lưu).
 * - Dữ liệu bản nháp hợp lệ.
 */
fun canActivateProfile(
    profile: FallDetectionProfile,
    hasUnsavedChanges: Boolean,
    isValid: Boolean
): Boolean {
    if (profile.isActive) return false
    if (hasUnsavedChanges || !isValid) return false
    return true
}

/**
 * Quy tắc xóa profile: Không xóa profile đang hoạt động và không xóa profile cuối cùng.
 */
fun canDeleteProfile(profile: FallDetectionProfile, allProfiles: List<FallDetectionProfile>): Boolean {
    if (allProfiles.size <= 1) return false
    if (profile.isActive) return false
    return true
}

/**
 * Lựa chọn profile ID an toàn khi danh sách thay đổi hoặc sau khi xóa profile.
 */
fun resolveSelectedProfileId(
    currentSelectedId: String?,
    profiles: List<FallDetectionProfile>,
    activeProfileId: String?
): String {
    if (profiles.isEmpty()) return ""
    if (currentSelectedId != null && profiles.any { it.id == currentSelectedId }) {
        return currentSelectedId
    }
    if (activeProfileId != null && profiles.any { it.id == activeProfileId }) {
        return activeProfileId
    }
    return profiles.first().id
}

/**
 * Nhãn tiếng Việt cho các giai đoạn phát hiện té ngã.
 */
fun detectionPhaseVietnamese(phase: DetectionPhase): String = when (phase) {
    DetectionPhase.NORMAL -> "Bình thường"
    DetectionPhase.IMPACT_DETECTED -> "Phát hiện va chạm"
    DetectionPhase.POST_IMPACT_STILLNESS -> "Theo dõi bất động sau va chạm"
    DetectionPhase.FALL_CONFIRMED -> "Xác nhận té ngã"
}

/**
 * Nhãn trạng thái ngưỡng va chạm trung thực theo ngữ nghĩa.
 */
fun thresholdStatusLabel(impactOverThreshold: Boolean, thresholdMs2: Float): String =
    if (impactOverThreshold) {
        "⚠ VƯỢT NGƯỠNG VA CHẠM (≥ $thresholdMs2 m/s²)"
    } else {
        "✓ CHƯA VƯỢT NGƯỠNG VA CHẠM (< $thresholdMs2 m/s²)"
    }

/**
 * Snapshot hiển thị cảm biến phục vụ màn hình hiệu chỉnh (chu kỳ ~4 Hz).
 */
data class SensorUiSnapshot(
    val accelX: Float?,
    val accelY: Float?,
    val accelZ: Float?,
    val magnitude: Double?,
    val activeThreshold: Float,
    val activeProfileDisplayName: String,
    val phase: DetectionPhase,
    val impactOverThreshold: Boolean,
    val withinStillness: Boolean,
    val stillnessProgress: Float,
    val stillnessSampleCount: Int
)

/**
 * Trích xuất snapshot dữ liệu cảm biến cho UI từ packet và observation của detector.
 */
fun takeSensorUiSnapshot(
    packet: PhoneSensorPacket?,
    observation: FallDetectionObservation
): SensorUiSnapshot {
    val magnitude = observation.accelerationMagnitudeMs2 ?: packet?.let { p ->
        kotlin.math.sqrt(
            p.accelXMs2.toDouble() * p.accelXMs2 +
                p.accelYMs2.toDouble() * p.accelYMs2 +
                p.accelZMs2.toDouble() * p.accelZMs2
        )
    }
    val overThreshold = observation.impactOverThreshold ||
        (magnitude != null && magnitude >= observation.config.impactAccelerationMs2)

    return SensorUiSnapshot(
        accelX = packet?.accelXMs2,
        accelY = packet?.accelYMs2,
        accelZ = packet?.accelZMs2,
        magnitude = magnitude,
        activeThreshold = observation.config.impactAccelerationMs2,
        activeProfileDisplayName = observation.activeProfileDisplayName,
        phase = observation.phase,
        impactOverThreshold = overThreshold,
        withinStillness = observation.withinStillness,
        stillnessProgress = observation.stillnessProgress,
        stillnessSampleCount = observation.stillnessSampleCount
    )
}

@Composable
fun FallDetectionCalibrationScreen(
    controller: DemoController,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    // Quản lý ID profile đang chọn hiệu chỉnh
    var selectedProfileId by rememberSaveable {
        mutableStateOf(controller.activeProfile.id)
    }

    // Đảm bảo ID đã chọn luôn hợp lệ khi danh sách profiles thay đổi
    LaunchedEffect(controller.profiles) {
        val safeId = resolveSelectedProfileId(selectedProfileId, controller.profiles, controller.activeProfile.id)
        if (safeId != selectedProfileId) {
            selectedProfileId = safeId
        }
    }

    val selectedProfile = controller.profiles.find { it.id == selectedProfileId } ?: controller.activeProfile

    // Quản lý bản nháp (draft) độc lập với selected profile
    var draftProfileId by remember { mutableStateOf<String?>(null) }
    var draft by remember { mutableStateOf(CalibrationDraft.fromConfig(selectedProfile.config)) }

    if (draftProfileId != selectedProfile.id) {
        draft = CalibrationDraft.fromConfig(selectedProfile.config)
        draftProfileId = selectedProfile.id
    }

    val validation = remember(draft) { validateCalibrationDraft(draft) }
    val isDirty = remember(validation, selectedProfile.config) {
        hasUnsavedChanges(validation, selectedProfile.config)
    }

    // Throttled sensor snapshot ~4 Hz (250ms)
    var sensorSnapshot by remember {
        mutableStateOf(takeSensorUiSnapshot(controller.packet, controller.observation))
    }
    LaunchedEffect(Unit) {
        while (isActive) {
            sensorSnapshot = takeSensorUiSnapshot(controller.packet, controller.observation)
            delay(250L)
        }
    }

    // Back handler hệ thống quay lại Settings
    androidx.activity.compose.BackHandler(onBack = onBack)

    var showResetDialog by remember { mutableStateOf(false) }
    var showDeleteDialog by remember { mutableStateOf(false) }
    var feedbackMessage by remember { mutableStateOf<String?>(null) }
    var feedbackIsError by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        // Top Bar: Tiêu đề + Nút quay lại (layout an toàn, không bị tràn trên màn hình hẹp/font lớn)
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "HIỆU CHỈNH NGƯỠNG",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.weight(1f)
                )
                OutlinedButton(
                    onClick = onBack,
                    modifier = Modifier.heightIn(min = 48.dp)
                ) {
                    Text("QUAY LẠI", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
            Text(
                text = "Cấu hình thử nghiệm thuật toán",
                fontSize = 15.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }

        // Cảnh báo khoa học và nghiên cứu
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
            shape = RoundedCornerShape(12.dp)
        ) {
            Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(
                    text = "LƯU Ý KHOA HỌC & NGHIÊN CỨU",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Text(
                    text = "Các thông số dưới đây là bảng cấu hình thử nghiệm và giá trị khởi tạo phục vụ nghiên cứu thuật toán; không phải là ngưỡng chuẩn hay ngưỡng y tế được chứng nhận lâm sàng.",
                    fontSize = 14.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }

        // Card chọn bảng cấu hình & Trạng thái kích hoạt tách biệt
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
            border = BorderStroke(1.5.dp, MaterialTheme.colorScheme.outlineVariant),
            shape = RoundedCornerShape(14.dp)
        ) {
            Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text(
                    text = "BẢNG CẤU HÌNH ĐANG CHỈNH SỬA",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurface
                )

                // Dropdown chọn profile (chỉ hiển thị displayName)
                var dropdownExpanded by remember { mutableStateOf(false) }
                Box(modifier = Modifier.fillMaxWidth()) {
                    OutlinedButton(
                        onClick = { dropdownExpanded = true },
                        modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                        shape = RoundedCornerShape(10.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = selectedProfile.displayName +
                                    if (selectedProfile.isActive) " • [ĐANG SỬ DỤNG]" else "",
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                modifier = Modifier.weight(1f)
                            )
                            Text("▼", fontSize = 14.sp)
                        }
                    }
                    DropdownMenu(
                        expanded = dropdownExpanded,
                        onDismissRequest = { dropdownExpanded = false },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        controller.profiles.forEach { profile ->
                            DropdownMenuItem(
                                text = {
                                    Column {
                                        Text(
                                            text = profile.displayName,
                                            fontWeight = if (profile.id == selectedProfile.id) FontWeight.Bold else FontWeight.Normal,
                                            fontSize = 15.sp,
                                            color = MaterialTheme.colorScheme.onSurface
                                        )
                                        if (profile.isActive) {
                                            Text(
                                                text = "✓ Đang sử dụng để phát hiện té ngã",
                                                color = SafeGreen,
                                                fontSize = 13.sp,
                                                fontWeight = FontWeight.SemiBold
                                            )
                                        }
                                    }
                                },
                                onClick = {
                                    dropdownExpanded = false
                                    if (selectedProfileId != profile.id) {
                                        selectedProfileId = profile.id
                                        draft = CalibrationDraft.fromConfig(profile.config)
                                        feedbackMessage = "Đã chọn ${profile.displayName}"
                                        feedbackIsError = false
                                    }
                                }
                            )
                        }
                    }
                }

                // Nhãn riêng biệt luôn hiển thị rõ bảng đang sử dụng
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = SafeGreenContainer),
                    border = BorderStroke(1.dp, SafeGreenBorder),
                    shape = RoundedCornerShape(10.dp)
                ) {
                    Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(
                            text = "Đang sử dụng để phát hiện té ngã: ${controller.activeProfile.displayName}",
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Bold,
                            color = SafeGreen
                        )
                        if (selectedProfile.isActive) {
                            if (isDirty) {
                                Text(
                                    text = "• Bảng này đang chạy nhưng có thay đổi chưa lưu. Detector vẫn dùng giá trị đã lưu cho đến khi bạn nhấn LƯU.",
                                    fontSize = 13.sp,
                                    color = WarningOrange
                                )
                            } else {
                                Text(
                                    text = "• Bảng đang chỉnh sửa chính là bảng đang chạy phát hiện.",
                                    fontSize = 13.sp,
                                    color = SafeGreen
                                )
                            }
                        } else {
                            Text(
                                text = "• Bảng đang chỉnh sửa chưa kích hoạt. Nhấn 'SỬ DỤNG BẢNG NÀY' bên dưới nếu muốn áp dụng vào detector.",
                                fontSize = 13.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }
        }

        // Cảnh báo khi có thay đổi chưa lưu
        if (isDirty) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = WarningOrangeContainer),
                border = BorderStroke(1.dp, WarningOrangeBorder),
                shape = RoundedCornerShape(10.dp)
            ) {
                Text(
                    text = "⚠ Có thay đổi chưa lưu; Detection Engine vẫn dùng giá trị đã lưu." +
                        if (selectedProfile.isActive) " Cấu hình đang chạy chưa áp dụng các thay đổi này cho đến khi bạn nhấn LƯU." else "",
                    color = WarningOrange,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.padding(12.dp)
                )
            }
        }

        // Feedback Banner (Touch target IconButton >= 48dp)
        if (feedbackMessage != null) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = if (feedbackIsError) WarningOrangeContainer else SafeGreenContainer
                ),
                border = BorderStroke(1.dp, if (feedbackIsError) WarningOrangeBorder else SafeGreenBorder)
            ) {
                Row(
                    modifier = Modifier.padding(start = 12.dp, top = 4.dp, end = 4.dp, bottom = 4.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = feedbackMessage!!,
                        color = if (feedbackIsError) WarningOrange else SafeGreen,
                        fontSize = 15.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier.weight(1f)
                    )
                    IconButton(
                        onClick = { feedbackMessage = null },
                        modifier = Modifier.sizeIn(minWidth = 48.dp, minHeight = 48.dp)
                    ) {
                        Text("✕", color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 16.sp)
                    }
                }
            }
        }

        // Realtime Sensor Panel
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            shape = RoundedCornerShape(14.dp)
        ) {
            Column(
                modifier = Modifier.padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Text(
                    text = "DỮ LIỆU CẢM BIẾN & TRẠNG THÁI PHÁT HIỆN",
                    fontSize = 17.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Text(
                    text = "Dữ liệu phục vụ kiểm thử theo chu kỳ ~4 Hz (250 ms) từ cảm biến gia tốc thật sự dùng:",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                // Highlight trạng thái ngưỡng bằng cả màu + chữ
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = if (sensorSnapshot.impactOverThreshold) WarningOrangeContainer else SafeGreenContainer
                    ),
                    border = BorderStroke(
                        1.5.dp,
                        if (sensorSnapshot.impactOverThreshold) WarningOrangeBorder else SafeGreenBorder
                    )
                ) {
                    Column(Modifier.padding(12.dp)) {
                        Text(
                            text = thresholdStatusLabel(sensorSnapshot.impactOverThreshold, sensorSnapshot.activeThreshold),
                            fontSize = 15.sp,
                            fontWeight = FontWeight.Bold,
                            color = if (sensorSnapshot.impactOverThreshold) WarningOrange else SafeGreen
                        )
                    }
                }

                Text(
                    text = "Bảng đang chạy phát hiện: ${sensorSnapshot.activeProfileDisplayName}",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Text(
                    text = "Gia tốc X / Y / Z: " +
                        (sensorSnapshot.accelX?.let { String.format(Locale.US, "%.2f", it) } ?: "—") + " / " +
                        (sensorSnapshot.accelY?.let { String.format(Locale.US, "%.2f", it) } ?: "—") + " / " +
                        (sensorSnapshot.accelZ?.let { String.format(Locale.US, "%.2f", it) } ?: "—") + " m/s²",
                    fontSize = 14.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Text(
                    text = "Độ lớn gia tốc |a|: " +
                        (sensorSnapshot.magnitude?.let { String.format(Locale.US, "%.2f m/s²", it) } ?: "Chưa có dữ liệu"),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Text(
                    text = "Ngưỡng va chạm đang áp dụng: ${sensorSnapshot.activeThreshold} m/s²",
                    fontSize = 14.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                Text(
                    text = "Giai đoạn phát hiện: ${detectionPhaseVietnamese(sensorSnapshot.phase)}",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )

                // Trạng thái tĩnh (bố trí dạng Column an toàn tránh chồng chéo trên màn hình nhỏ/font lớn)
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(
                        text = "Trạng thái tĩnh: " + if (sensorSnapshot.withinStillness) "ĐẠT (trong khoảng dung sai)" else "CHƯA ĐẠT",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        text = "Số mẫu tĩnh thu thập: ${sensorSnapshot.stillnessSampleCount}",
                        fontSize = 14.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    LinearProgressIndicator(
                        progress = { sensorSnapshot.stillnessProgress },
                        modifier = Modifier.fillMaxWidth().height(8.dp),
                    )
                    Text(
                        text = "Tiến độ thời gian tĩnh: ${(sensorSnapshot.stillnessProgress * 100).toInt()}%",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }

        // Section: Hiệu chỉnh 7 thông số
        Text(
            text = "CÁC THÔNG SỐ HIỆU CHỈNH (7 THÔNG SỐ)",
            fontSize = 18.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.onSurface
        )

        // 1. Ngưỡng gia tốc va chạm
        CalibrationFieldEditor(
            title = "1. Ngưỡng gia tốc va chạm",
            unit = "m/s²",
            textValue = draft.impactAccelerationText,
            onTextChange = { draft = draft.copy(impactAccelerationText = it) },
            sliderValue = draft.impactAccelerationText.trim().replace(',', '.').toFloatOrNull() ?: 25f,
            sliderRange = FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2..FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2,
            onSliderChange = { v ->
                val rounded = (v * 10).roundToInt() / 10f
                draft = draft.copy(
                    impactAccelerationText = formatCanonicalFloat(rounded)
                )
            },
            minLabel = "${FallDetectionConfig.MIN_IMPACT_ACCELERATION_MS2.toInt()} m/s²",
            maxLabel = "${FallDetectionConfig.MAX_IMPACT_ACCELERATION_MS2.toInt()} m/s²",
            helperText = "Ngưỡng phát hiện chấn động mạnh khi va chạm (Khởi tạo: 25.0 m/s²). Khoảng: 1 - 100 m/s².",
            errorMessage = validation.impactError
        )

        // 2. Gia tốc tĩnh mục tiêu (đã hiệu chỉnh wording theo dữ liệu cảm biến điện thoại)
        CalibrationFieldEditor(
            title = "2. Gia tốc tĩnh mục tiêu",
            unit = "m/s²",
            textValue = draft.stillnessTargetAccelerationText,
            onTextChange = { draft = draft.copy(stillnessTargetAccelerationText = it) },
            sliderValue = draft.stillnessTargetAccelerationText.trim().replace(',', '.').toFloatOrNull() ?: 9.81f,
            sliderRange = FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2..FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2,
            onSliderChange = { v ->
                val rounded = (v * 100).roundToInt() / 100f
                draft = draft.copy(stillnessTargetAccelerationText = formatCanonicalFloat(rounded))
            },
            minLabel = "${FallDetectionConfig.MIN_STILLNESS_TARGET_ACCELERATION_MS2.toInt()} m/s²",
            maxLabel = "${FallDetectionConfig.MAX_STILLNESS_TARGET_ACCELERATION_MS2.toInt()} m/s²",
            helperText = "Gia tốc tổng hợp kỳ vọng khi điện thoại ít chuyển động (Khởi tạo: 9.81 m/s²). Khoảng: 0 - 20 m/s².",
            errorMessage = validation.stillnessTargetError
        )

        // 3. Dung sai gia tốc tĩnh
        CalibrationFieldEditor(
            title = "3. Dung sai gia tốc tĩnh",
            unit = "m/s²",
            textValue = draft.stillnessToleranceText,
            onTextChange = { draft = draft.copy(stillnessToleranceText = it) },
            sliderValue = draft.stillnessToleranceText.trim().replace(',', '.').toFloatOrNull() ?: 1.0f,
            sliderRange = FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2..FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2,
            onSliderChange = { v ->
                val rounded = (v * 10).roundToInt() / 10f
                draft = draft.copy(
                    stillnessToleranceText = formatCanonicalFloat(rounded)
                )
            },
            minLabel = "${FallDetectionConfig.MIN_STILLNESS_TOLERANCE_MS2} m/s²",
            maxLabel = "${FallDetectionConfig.MAX_STILLNESS_TOLERANCE_MS2.toInt()} m/s²",
            helperText = "Độ lệch cho phép quanh gia tốc tĩnh mục tiêu (Khởi tạo: 1.0 m/s²). Khoảng: 0.1 - 10 m/s².",
            errorMessage = validation.stillnessToleranceError
        )

        // 4. Cửa sổ theo dõi sau va chạm
        CalibrationFieldEditor(
            title = "4. Cửa sổ theo dõi sau va chạm",
            unit = "ms",
            textValue = draft.postImpactWindowMsText,
            onTextChange = { draft = draft.copy(postImpactWindowMsText = it) },
            sliderValue = (draft.postImpactWindowMsText.trim().toLongOrNull() ?: 3000L).toFloat(),
            sliderRange = FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS.toFloat()..FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS.toFloat(),
            onSliderChange = { v ->
                val rounded = ((v / 50).roundToInt() * 50).toLong().coerceIn(FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS, FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS)
                draft = draft.copy(postImpactWindowMsText = rounded.toString())
            },
            minLabel = "${FallDetectionConfig.MIN_POST_IMPACT_WINDOW_MS} ms (0.5 s)",
            maxLabel = "${FallDetectionConfig.MAX_POST_IMPACT_WINDOW_MS} ms (10.0 s)",
            helperText = "Thời gian tối đa để kiểm tra bất động sau va chạm (Khởi tạo: 3000 ms / 3.0 s). Khoảng: 500 - 10000 ms.",
            errorMessage = validation.postImpactWindowError,
            keyboardType = KeyboardType.Number
        )

        // 5. Thời gian bất động yêu cầu
        CalibrationFieldEditor(
            title = "5. Thời gian bất động yêu cầu",
            unit = "ms",
            textValue = draft.postImpactStillnessDurationMsText,
            onTextChange = { draft = draft.copy(postImpactStillnessDurationMsText = it) },
            sliderValue = (draft.postImpactStillnessDurationMsText.trim().toLongOrNull() ?: 1000L).toFloat(),
            sliderRange = FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS.toFloat()..FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS.toFloat(),
            onSliderChange = { v ->
                val rounded = ((v / 50).roundToInt() * 50).toLong().coerceIn(FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS, FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS)
                draft = draft.copy(postImpactStillnessDurationMsText = rounded.toString())
            },
            minLabel = "${FallDetectionConfig.MIN_POST_IMPACT_STILLNESS_DURATION_MS} ms (0.1 s)",
            maxLabel = "${FallDetectionConfig.MAX_POST_IMPACT_STILLNESS_DURATION_MS} ms (10.0 s)",
            helperText = "Thời gian cần giữ bất động liên tục để xác nhận ngã (Khởi tạo: 1000 ms / 1.0 s). Bắt buộc phải ≤ Cửa sổ theo dõi.",
            errorMessage = validation.postImpactStillnessDurationError,
            keyboardType = KeyboardType.Number
        )

        // 6. Số mẫu tĩnh tối thiểu
        CalibrationFieldEditor(
            title = "6. Số mẫu tĩnh tối thiểu",
            unit = "mẫu",
            textValue = draft.minimumStillnessSamplesText,
            onTextChange = { draft = draft.copy(minimumStillnessSamplesText = it) },
            sliderValue = (draft.minimumStillnessSamplesText.trim().toIntOrNull() ?: 6).toFloat(),
            sliderRange = FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN.toFloat()..FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX.toFloat(),
            onSliderChange = { v ->
                val rounded = v.roundToInt().coerceIn(FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN, FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX)
                draft = draft.copy(minimumStillnessSamplesText = rounded.toString())
            },
            minLabel = "${FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MIN} mẫu",
            maxLabel = "${FallDetectionConfig.MINIMUM_STILLNESS_SAMPLES_MAX} mẫu",
            helperText = "Số mẫu cảm biến bất động tối thiểu cần thu thập (Khởi tạo: 6 mẫu). Khoảng: 2 - 100 mẫu.",
            errorMessage = validation.minimumStillnessSamplesError,
            keyboardType = KeyboardType.Number
        )

        // 7. Khoảng cách mẫu tối đa
        CalibrationFieldEditor(
            title = "7. Khoảng cách mẫu tối đa",
            unit = "ms",
            textValue = draft.maximumSampleGapMsText,
            onTextChange = { draft = draft.copy(maximumSampleGapMsText = it) },
            sliderValue = (draft.maximumSampleGapMsText.trim().toLongOrNull() ?: 250L).toFloat(),
            sliderRange = FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS.toFloat()..FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS.toFloat(),
            onSliderChange = { v ->
                val rounded = ((v / 10).roundToInt() * 10).toLong().coerceIn(FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS, FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS)
                draft = draft.copy(maximumSampleGapMsText = rounded.toString())
            },
            minLabel = "${FallDetectionConfig.MIN_MAXIMUM_SAMPLE_GAP_MS} ms",
            maxLabel = "${FallDetectionConfig.MAX_MAXIMUM_SAMPLE_GAP_MS} ms",
            helperText = "Khoảng thời gian tối đa giữa 2 mẫu liên tiếp trước khi đặt lại kiểm tra (Khởi tạo: 250 ms). Khoảng: 10 - 2000 ms.",
            errorMessage = validation.maximumSampleGapError,
            keyboardType = KeyboardType.Number
        )

        // Section Thao tác lưu / kích hoạt / xóa
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = "THAO TÁC CẤU HÌNH",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurface
            )

            if (!validation.isValid) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = WarningOrangeContainer),
                    border = BorderStroke(1.dp, WarningOrangeBorder)
                ) {
                    Text(
                        text = "⚠ Một số thông số chưa hợp lệ. Vui lòng kiểm tra các mục báo đỏ ở trên trước khi lưu.",
                        color = WarningOrange,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        modifier = Modifier.padding(12.dp)
                    )
                }
            }

            // 1. Nút LƯU
            Button(
                onClick = {
                    val config = validation.parsedConfig
                    if (config != null) {
                        val saved = controller.save(selectedProfile.id, config)
                        if (saved != null) {
                            draft = CalibrationDraft.fromConfig(saved.config)
                            feedbackMessage = "Đã lưu thay đổi cho ${saved.displayName}."
                            feedbackIsError = false
                        } else {
                            feedbackMessage = "Lưu thất bại."
                            feedbackIsError = true
                        }
                    }
                },
                enabled = validation.isValid,
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text("LƯU", fontSize = 18.sp, fontWeight = FontWeight.Bold)
            }

            // 2. Nút LƯU THÀNH BẢNG MỚI
            OutlinedButton(
                onClick = {
                    val config = validation.parsedConfig
                    if (config != null) {
                        val newProfile = controller.saveAs(config)
                        if (newProfile != null) {
                            selectedProfileId = newProfile.id
                            draft = CalibrationDraft.fromConfig(newProfile.config)
                            feedbackMessage = "Đã lưu thành bảng mới: ${newProfile.displayName} (Chưa kích hoạt)."
                            feedbackIsError = false
                        } else {
                            feedbackMessage = "Không thể lưu thành bảng mới."
                            feedbackIsError = true
                        }
                    }
                },
                enabled = validation.isValid,
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text("LƯU THÀNH BẢNG MỚI", fontSize = 18.sp, fontWeight = FontWeight.Bold)
            }

            // 3. Nút SỬ DỤNG BẢNG NÀY (Bị vô hiệu hóa nếu có thay đổi chưa lưu hoặc bảng đã active)
            val canActivate = canActivateProfile(selectedProfile, isDirty, validation.isValid)
            Button(
                onClick = {
                    val ok = controller.activate(selectedProfile.id)
                    if (ok) {
                        feedbackMessage = "Đã kích hoạt ${selectedProfile.displayName} để phát hiện té ngã!"
                        feedbackIsError = false
                    } else {
                        feedbackMessage = "Kích hoạt thất bại."
                        feedbackIsError = true
                    }
                },
                enabled = canActivate,
                colors = ButtonDefaults.buttonColors(containerColor = SafeGreen),
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text(
                    text = if (selectedProfile.isActive) "✓ BẢNG NÀY ĐANG ĐƯỢC SỬ DỤNG" else "SỬ DỤNG BẢNG NÀY",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
            }
            if (!selectedProfile.isActive && isDirty) {
                Text(
                    text = "Vui lòng LƯU hoặc LƯU THÀNH BẢNG MỚI trước khi kích hoạt bảng này.",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 4.dp)
                )
            } else if (selectedProfile.isActive && isDirty) {
                Text(
                    text = "Bảng này đang được kích hoạt nhưng có thay đổi chưa lưu. Thay đổi chưa áp dụng cho đến khi bạn nhấn LƯU.",
                    fontSize = 13.sp,
                    color = WarningOrange,
                    modifier = Modifier.padding(horizontal = 4.dp)
                )
            }

            // 4. Nút KHÔI PHỤC GIÁ TRỊ MẶC ĐỊNH
            OutlinedButton(
                onClick = { showResetDialog = true },
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text("KHÔI PHỤC GIÁ TRỊ MẶC ĐỊNH", fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
            }

            // 5. Nút XÓA BẢNG
            val canDelete = canDeleteProfile(selectedProfile, controller.profiles)
            Button(
                onClick = { showDeleteDialog = true },
                enabled = canDelete,
                colors = ButtonDefaults.buttonColors(containerColor = SosRed),
                modifier = Modifier.fillMaxWidth().heightIn(min = 56.dp),
                shape = RoundedCornerShape(12.dp)
            ) {
                Text("XÓA BẢNG", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
            }
            if (!canDelete) {
                Text(
                    text = if (selectedProfile.isActive)
                        "Không thể xóa bảng đang được sử dụng để phát hiện té ngã."
                    else
                        "Không thể xóa khi chỉ còn một bảng cấu hình duy nhất.",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(horizontal = 4.dp)
                )
            }
        }
    }

    // Dialog xác nhận khôi phục mặc định
    if (showResetDialog) {
        AlertDialog(
            onDismissRequest = { showResetDialog = false },
            title = { Text("Khôi phục giá trị khởi tạo?", fontWeight = FontWeight.Bold) },
            text = {
                Text("Bạn có chắc chắn muốn khôi phục tất cả thông số của ${selectedProfile.displayName} về giá trị khởi tạo thử nghiệm ban đầu?")
            },
            confirmButton = {
                Button(
                    onClick = {
                        showResetDialog = false
                        val reset = controller.resetToDefault(selectedProfile.id)
                        if (reset != null) {
                            draft = CalibrationDraft.fromConfig(reset.config)
                            feedbackMessage = "Đã khôi phục ${reset.displayName} về giá trị khởi tạo thử nghiệm."
                            feedbackIsError = false
                        } else {
                            feedbackMessage = "Khôi phục thất bại."
                            feedbackIsError = true
                        }
                    }
                ) {
                    Text("KHÔI PHỤC")
                }
            },
            dismissButton = {
                OutlinedButton(onClick = { showResetDialog = false }) {
                    Text("HỦY")
                }
            }
        )
    }

    // Dialog xác nhận xóa bảng
    if (showDeleteDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteDialog = false },
            title = { Text("Xóa bảng cấu hình?", fontWeight = FontWeight.Bold) },
            text = {
                Text("Bạn có chắc chắn muốn xóa ${selectedProfile.displayName}? Thao tác này không thể hoàn tác.")
            },
            confirmButton = {
                Button(
                    onClick = {
                        showDeleteDialog = false
                        val deletedName = selectedProfile.displayName
                        val success = controller.delete(selectedProfile.id)
                        if (success) {
                            val nextId = resolveSelectedProfileId(null, controller.profiles, controller.activeProfile.id)
                            selectedProfileId = nextId
                            val nextProfile = controller.profiles.find { it.id == nextId } ?: controller.activeProfile
                            draft = CalibrationDraft.fromConfig(nextProfile.config)
                            feedbackMessage = "Đã xóa $deletedName."
                            feedbackIsError = false
                        } else {
                            feedbackMessage = "Không thể xóa bảng này."
                            feedbackIsError = true
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = SosRed)
                ) {
                    Text("XÓA", color = Color.White)
                }
            },
            dismissButton = {
                OutlinedButton(onClick = { showDeleteDialog = false }) {
                    Text("HỦY")
                }
            }
        )
    }
}

/**
 * Component hiển thị và chỉnh sửa cho từng trường cấu hình.
 * Bố trí nhãn và giới hạn dạng cột xếp chồng an toàn, đảm bảo không bị tràn chữ khi font lớn hoặc màn hình hẹp.
 */
@Composable
private fun CalibrationFieldEditor(
    title: String,
    unit: String,
    textValue: String,
    onTextChange: (String) -> Unit,
    sliderValue: Float,
    sliderRange: ClosedFloatingPointRange<Float>,
    onSliderChange: (Float) -> Unit,
    minLabel: String,
    maxLabel: String,
    helperText: String,
    errorMessage: String?,
    keyboardType: KeyboardType = KeyboardType.Decimal
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
        border = BorderStroke(
            1.dp,
            if (errorMessage != null) WarningOrangeBorder else MaterialTheme.colorScheme.outlineVariant
        ),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                Text(
                    text = title,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onSurface
                )
                Text(
                    text = "Đơn vị: $unit",
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontWeight = FontWeight.Medium
                )
            }

            Text(
                text = helperText,
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            // Numeric TextField
            OutlinedTextField(
                value = textValue,
                onValueChange = onTextChange,
                label = { Text("Giá trị ($unit)") },
                isError = errorMessage != null,
                supportingText = {
                    if (errorMessage != null) {
                        Text(errorMessage, color = WarningOrange, fontSize = 13.sp, fontWeight = FontWeight.Medium)
                    }
                },
                keyboardOptions = KeyboardOptions(keyboardType = keyboardType),
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            // Slider
            Slider(
                value = sliderValue.coerceIn(sliderRange.start, sliderRange.endInclusive),
                onValueChange = onSliderChange,
                valueRange = sliderRange,
                modifier = Modifier.fillMaxWidth()
            )

            // Nhãn min/max bố trí dạng Column an toàn tránh va chạm trên màn hình hẹp
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                Text(text = "Tối thiểu: $minLabel", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(text = "Tối đa: $maxLabel", fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
