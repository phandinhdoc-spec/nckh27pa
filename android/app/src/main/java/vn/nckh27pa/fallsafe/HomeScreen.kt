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

@Composable
fun HomeScreen(
    controller: DemoController,
    onOpenContacts: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val scrollState = rememberScrollState()
    val status = controller.mainScreenStatus
    var showCallConfirmDialog by remember { mutableStateOf(false) }
    var callToastMessage by remember { mutableStateOf<String?>(null) }

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

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val screenWidth = maxWidth
        val screenHeight = maxHeight
        val fontScale = LocalDensity.current.fontScale

        // Responsive layout detection
        val isLandscape = screenWidth > screenHeight
        val isSmallPortrait = !isLandscape && screenWidth <= 340.dp
        val isLargeFont = fontScale > 1.25f

        // Grid spacing between cards
        val gridSpacing = if (isLandscape) 4.dp else if (isSmallPortrait) 6.dp else 8.dp
        val gapHalf = gridSpacing / 2

        // Dynamic center button diameter: sized proportionally to available dimension
        val buttonDiameter = if (isLandscape) {
            (screenHeight * 0.46f).coerceIn(94.dp, 100.dp)
        } else if (isSmallPortrait) {
            (screenWidth * 0.44f).coerceIn(132.dp, 150.dp)
        } else {
            (minOf(screenWidth, screenHeight) * 0.44f).coerceIn(146.dp, 185.dp)
        }
        val buttonRadius = buttonDiameter / 2
        // Cutout radius conforms to SOS radius with an 8-9dp uniform clearance gap
        val cutoutRadius = buttonRadius + if (isLandscape) 8.dp else 9.dp

        // Adaptive typography designed for elderly readability while guaranteeing NO ellipsis on mandatory strings:
        val titleFontSize = when {
            isLandscape -> 13.5.sp
            isSmallPortrait && fontScale >= 1.45f -> 10.sp
            isSmallPortrait && fontScale >= 1.3f -> 11.5.sp
            isSmallPortrait -> 13.sp
            fontScale >= 1.3f -> 13.sp
            else -> 15.sp
        }

        val statusFontSize = when {
            isLandscape -> 15.5.sp
            isSmallPortrait && fontScale >= 1.3f -> 13.sp
            isSmallPortrait -> 15.sp
            fontScale >= 1.3f -> 15.sp
            else -> 17.sp
        }

        val sublineFontSize = when {
            isLandscape -> 12.5.sp
            isSmallPortrait && fontScale >= 1.3f -> 11.sp
            isSmallPortrait -> 12.sp
            fontScale >= 1.3f -> 12.sp
            else -> 14.sp
        }

        val iconSize = when {
            isLandscape -> 18.sp
            isSmallPortrait && fontScale >= 1.3f -> 16.sp
            isSmallPortrait -> 18.sp
            else -> 20.sp
        }

        // Setup status information for 4 blocks
        val (protectText, protectColor, protectBg, protectBorder, protectIcon) = when (status) {
            MainScreenStatus.SAFE -> Quint(
                "Đang bảo vệ", SafeGreen, SafeGreenContainer, SafeGreenBorder, "🛡️"
            )
            MainScreenStatus.WARNING_COUNTDOWN -> Quint(
                "Cần kiểm tra", WarningOrange, WarningOrangeContainer, WarningOrangeBorder, "⚠️"
            )
            MainScreenStatus.SOS_SENT -> Quint(
                "Cần kiểm tra", SosRed, SosRedContainer, SosRedBorder, "🚨"
            )
            MainScreenStatus.HELP_ACKNOWLEDGED -> Quint(
                "Đang bảo vệ", SafeGreen, SafeGreenContainer, SafeGreenBorder, "🛡️"
            )
            MainScreenStatus.DEVICE_DISCONNECTED -> Quint(
                "Cần kiểm tra", DisconnectedGray, DisconnectedGrayContainer, DisconnectedGrayBorder, "❗"
            )
        }

        val (deviceText, deviceColor, deviceBg, deviceBorder, deviceIcon) = if (!controller.deviceConnected) {
            Quint("Mất kết nối", DisconnectedGray, DisconnectedGrayContainer, DisconnectedGrayBorder, "🔌")
        } else {
            Quint("Pin tốt", SafeGreen, SafeGreenContainer, SafeGreenBorder, "🔋")
        }

        val containerModifier = Modifier
            .fillMaxSize()
            .padding(
                horizontal = if (isLandscape) 8.dp else if (isSmallPortrait) 6.dp else 10.dp,
                vertical = if (isLandscape) 2.dp else 6.dp
            )

        Column(
            modifier = containerModifier,
            verticalArrangement = Arrangement.SpaceBetween,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Optional top status message banner for active alerts
            when (status) {
                MainScreenStatus.SOS_SENT -> {
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(bottom = 4.dp),
                        colors = CardDefaults.cardColors(containerColor = SosRedContainer),
                        border = BorderStroke(2.dp, SosRedBorder),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Text(
                            text = controller.sosDeliveryMessage,
                            fontSize = if (isLandscape) 14.sp else 17.sp,
                            fontWeight = FontWeight.Bold,
                            color = SosRedDark,
                            textAlign = TextAlign.Center,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp).fillMaxWidth()
                        )
                    }
                }
                MainScreenStatus.HELP_ACKNOWLEDGED -> {
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(bottom = 4.dp),
                        colors = CardDefaults.cardColors(containerColor = SafeGreenContainer),
                        border = BorderStroke(2.dp, SafeGreenBorder),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text(
                                text = "Người thân đã nhận tin.",
                                fontSize = if (isLandscape) 14.sp else 16.sp,
                                fontWeight = FontWeight.Bold,
                                color = SafeGreen,
                                modifier = Modifier.weight(1f)
                            )
                            Button(
                                onClick = {
                                    controller.complete()
                                    controller.safe()
                                },
                                modifier = Modifier.heightIn(min = if (isLandscape) 38.dp else 44.dp),
                                colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                            ) {
                                Text("HOÀN TẤT", fontSize = if (isLandscape) 12.sp else 14.sp, fontWeight = FontWeight.Bold, color = Color.White)
                            }
                        }
                    }
                }
                MainScreenStatus.DEVICE_DISCONNECTED -> {
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(bottom = 4.dp),
                        colors = CardDefaults.cardColors(containerColor = DisconnectedGrayContainer),
                        border = BorderStroke(1.dp, DisconnectedGrayBorder),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Text(
                            text = "Mất kết nối thiết bị ngoại vi — Nút SOS vẫn hoạt động.",
                            fontSize = if (isLandscape) 13.sp else 15.sp,
                            fontWeight = FontWeight.Medium,
                            color = DisconnectedGray,
                            textAlign = TextAlign.Center,
                            modifier = Modifier.padding(horizontal = 6.dp, vertical = 4.dp).fillMaxWidth()
                        )
                    }
                }
                else -> {}
            }

            // Central Box containing the 4 concave blocks hugging the central SOS button
            val gridModifier = Modifier
                .fillMaxWidth()
                .weight(1f)

            // Card internal content paddings keeping content at the 4 outer corners
            val cardPaddingTL = if (isLandscape) {
                PaddingValues(start = 14.dp, top = 5.dp, end = 6.dp, bottom = 3.dp)
            } else if (isSmallPortrait) {
                PaddingValues(start = 10.dp, top = 8.dp, end = 6.dp, bottom = 6.dp)
            } else {
                PaddingValues(start = 14.dp, top = 12.dp, end = 8.dp, bottom = 8.dp)
            }

            val cardPaddingTR = if (isLandscape) {
                PaddingValues(end = 14.dp, top = 5.dp, start = 6.dp, bottom = 3.dp)
            } else if (isSmallPortrait) {
                PaddingValues(end = 10.dp, top = 8.dp, start = 6.dp, bottom = 6.dp)
            } else {
                PaddingValues(end = 14.dp, top = 12.dp, start = 8.dp, bottom = 8.dp)
            }

            val cardPaddingBL = if (isLandscape) {
                PaddingValues(start = 14.dp, bottom = 5.dp, end = 6.dp, top = 3.dp)
            } else if (isSmallPortrait) {
                PaddingValues(start = 8.dp, bottom = 8.dp, end = 6.dp, top = 6.dp)
            } else {
                PaddingValues(start = 14.dp, bottom = 12.dp, end = 8.dp, top = 8.dp)
            }

            val cardPaddingBR = if (isLandscape) {
                PaddingValues(end = 14.dp, bottom = 5.dp, start = 6.dp, top = 3.dp)
            } else if (isSmallPortrait) {
                PaddingValues(end = 10.dp, bottom = 8.dp, start = 6.dp, top = 6.dp)
            } else {
                PaddingValues(end = 14.dp, bottom = 12.dp, start = 8.dp, top = 8.dp)
            }

            Box(
                modifier = gridModifier,
                contentAlignment = Alignment.Center
            ) {
                // 2x2 Grid of cards
                Column(
                    modifier = Modifier.fillMaxSize(),
                    verticalArrangement = Arrangement.spacedBy(gridSpacing)
                ) {
                    // TOP ROW: Bảo vệ (TL) | Thiết bị (TR)
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(gridSpacing)
                    ) {
                        // 1. Top-Left: BẢO VỆ (Concave cutout at BOTTOM_RIGHT, content anchored at TopStart)
                        ConcaveCard(
                            cutoutCorner = CutoutCorner.BOTTOM_RIGHT,
                            cutoutRadius = cutoutRadius,
                            containerColor = protectBg,
                            borderColor = protectBorder,
                            contentAlignment = Alignment.TopStart,
                            contentPadding = cardPaddingTL,
                            gapX = gapHalf,
                            gapY = gapHalf,
                            talkBackLabel = "Trạng thái bảo vệ: $protectText",
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                        ) {
                            Column(
                                verticalArrangement = Arrangement.spacedBy(if (isLandscape) 1.dp else 2.dp),
                                horizontalAlignment = Alignment.Start
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Text(text = protectIcon, fontSize = iconSize)
                                    Text(
                                        text = "BẢO VỆ",
                                        fontSize = titleFontSize,
                                        fontWeight = FontWeight.Bold,
                                        color = protectColor,
                                        maxLines = 1,
                                        softWrap = false
                                    )
                                }
                                Text(
                                    text = protectText,
                                    fontSize = statusFontSize,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.Black,
                                    maxLines = 1,
                                    softWrap = false
                                )
                            }
                        }

                        // 2. Top-Right: THIẾT BỊ (Concave cutout at BOTTOM_LEFT, content anchored at TopEnd)
                        ConcaveCard(
                            cutoutCorner = CutoutCorner.BOTTOM_LEFT,
                            cutoutRadius = cutoutRadius,
                            containerColor = deviceBg,
                            borderColor = deviceBorder,
                            contentAlignment = Alignment.TopEnd,
                            contentPadding = cardPaddingTR,
                            gapX = gapHalf,
                            gapY = gapHalf,
                            talkBackLabel = "Trạng thái thiết bị và pin: $deviceText",
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                        ) {
                            Column(
                                verticalArrangement = Arrangement.spacedBy(if (isLandscape) 1.dp else 2.dp),
                                horizontalAlignment = Alignment.End
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Text(
                                        text = "THIẾT BỊ",
                                        fontSize = titleFontSize,
                                        fontWeight = FontWeight.Bold,
                                        color = deviceColor,
                                        maxLines = 1,
                                        softWrap = false
                                    )
                                    Text(text = deviceIcon, fontSize = iconSize)
                                }
                                Text(
                                    text = deviceText,
                                    fontSize = statusFontSize,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.Black,
                                    textAlign = TextAlign.End,
                                    maxLines = 1,
                                    softWrap = false
                                )
                            }
                        }
                    }

                    // BOTTOM ROW: Người nhận (BL) | Vị trí (BR)
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(gridSpacing)
                    ) {
                        // 3. Bottom-Left: NGƯỜI NHẬN (Concave cutout at TOP_RIGHT, content anchored at BottomStart)
                        ConcaveCard(
                            cutoutCorner = CutoutCorner.TOP_RIGHT,
                            cutoutRadius = cutoutRadius,
                            containerColor = ContactBlueContainer,
                            borderColor = ContactBlueBorder,
                            contentAlignment = Alignment.BottomStart,
                            contentPadding = cardPaddingBL,
                            gapX = gapHalf,
                            gapY = gapHalf,
                            talkBackLabel = "Người nhận: ${controller.primaryContactName}. Chạm để xem danh sách.",
                            onClick = onOpenContacts,
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                        ) {
                            Column(
                                verticalArrangement = Arrangement.spacedBy(if (isLandscape) 1.dp else 2.dp),
                                horizontalAlignment = Alignment.Start
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(if (isSmallPortrait) 2.dp else 4.dp)
                                ) {
                                    Text(text = "👤", fontSize = if (isSmallPortrait && fontScale >= 1.4f) 14.sp else iconSize)
                                    Text(
                                        text = "NGƯỜI NHẬN",
                                        fontSize = titleFontSize,
                                        fontWeight = FontWeight.Bold,
                                        color = ContactBlue,
                                        maxLines = 1,
                                        softWrap = false
                                    )
                                }
                                Text(
                                    text = controller.primaryContactName,
                                    fontSize = statusFontSize,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.Black,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                                Text(
                                    text = "Chạm để xem",
                                    fontSize = sublineFontSize,
                                    fontWeight = FontWeight.Medium,
                                    color = Color(0xFF37474F),
                                    maxLines = 1,
                                    softWrap = false
                                )
                            }
                        }

                        // 4. Bottom-Right: VỊ TRÍ (Concave cutout at TOP_LEFT, content anchored at BottomEnd)
                        ConcaveCard(
                            cutoutCorner = CutoutCorner.TOP_LEFT,
                            cutoutRadius = cutoutRadius,
                            containerColor = SafeGreenContainer,
                            borderColor = SafeGreenBorder,
                            contentAlignment = Alignment.BottomEnd,
                            contentPadding = cardPaddingBR,
                            gapX = gapHalf,
                            gapY = gapHalf,
                            talkBackLabel = "Vị trí đã xác định. Chạm để gọi khẩn cấp.",
                            onClick = { showCallConfirmDialog = true },
                            modifier = Modifier
                                .weight(1f)
                                .fillMaxHeight()
                        ) {
                            Column(
                                verticalArrangement = Arrangement.spacedBy(if (isLandscape) 1.dp else 2.dp),
                                horizontalAlignment = Alignment.End
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Text(
                                        text = "VỊ TRÍ",
                                        fontSize = titleFontSize,
                                        fontWeight = FontWeight.Bold,
                                        color = SafeGreen,
                                        maxLines = 1,
                                        softWrap = false
                                    )
                                    Text(text = "📍", fontSize = iconSize)
                                }
                                Text(
                                    text = "Đã xác định",
                                    fontSize = statusFontSize,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.Black,
                                    textAlign = TextAlign.End,
                                    maxLines = 1,
                                    softWrap = false
                                )
                                Text(
                                    text = "Chạm để gọi",
                                    fontSize = sublineFontSize,
                                    fontWeight = FontWeight.Medium,
                                    color = Color(0xFF37474F),
                                    textAlign = TextAlign.End,
                                    maxLines = 1,
                                    softWrap = false
                                )
                            }
                        }
                    }
                }

                // CENTER REGION: Large Circular SOS Button elevated as visual center
                CenterActionButton(
                    status = status,
                    controller = controller,
                    diameter = buttonDiameter,
                    modifier = Modifier
                        .align(Alignment.Center)
                        .zIndex(2f)
                )
            }

            // Quick call feedback banner (if triggered)
            callToastMessage?.let { msg ->
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 6.dp),
                    colors = CardDefaults.cardColors(containerColor = ContactBlueContainer),
                    border = BorderStroke(1.dp, ContactBlueBorder),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(10.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(msg, fontSize = 16.sp, color = ContactBlue, modifier = Modifier.weight(1f))
                        TextButton(onClick = { callToastMessage = null }) {
                            Text("ĐÓNG", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }
    }

    // Confirmation dialog before calling to avoid accidental phone calls
    if (showCallConfirmDialog) {
        AlertDialog(
            onDismissRequest = { showCallConfirmDialog = false },
            title = {
                Text("Gọi cho người thân?", fontSize = 22.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Text(
                    text = "Bạn có muốn thực hiện cuộc gọi khẩn cấp cho ${controller.primaryContactFullName} (${ContactValidator.mask(controller.primaryContactPhone)}) ngay bây giờ không?",
                    fontSize = 18.sp
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        showCallConfirmDialog = false
                        callToastMessage = "Đang kết nối thử nghiệm tới ${controller.primaryContactFullName} (chế độ demo, không phát cuộc gọi thật)"
                    },
                    modifier = Modifier.heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
                ) {
                    Text("GỌI NGAY", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            },
            dismissButton = {
                OutlinedButton(
                    onClick = { showCallConfirmDialog = false },
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text("HỦY", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                }
            }
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
            // Normal State: SOS - GIỮ 3 GIÂY - ĐỂ GỌI GIÚP (exactly 3 lines)
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
                        fontSize = if (isCompact) 13.sp else if (isMedium) (if (fontScale >= 1.4f) 12.sp else 15.sp) else 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White,
                        lineHeight = if (isCompact) 14.sp else 18.sp
                    )
                    Spacer(modifier = Modifier.height(1.dp))
                    Text(
                        text = "ĐỂ GỌI GIÚP",
                        fontSize = if (isCompact) 12.sp else if (isMedium) (if (fontScale >= 1.4f) 11.sp else 14.sp) else 18.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color.White,
                        textAlign = TextAlign.Center,
                        lineHeight = if (isCompact) 13.sp else 18.sp
                    )
                }
            }
        }

        MainScreenStatus.WARNING_COUNTDOWN -> {
            // Danger Countdown: CẢNH BÁO SAU X GIÂY - TÔI VẪN ỔN - GIỮ ĐỂ HỦY (2 GIÂY)
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
                talkBackLabel = "Phát hiện nguy hiểm. Tự động gửi SOS sau $seconds giây. Nhấn và giữ Tôi vẫn ổn 2 giây để hủy.",
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
                        fontSize = if (isCompact) 11.5.sp else if (isMedium) 15.sp else 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                    Text(
                        text = "$seconds GIÂY",
                        fontSize = if (isCompact) 18.sp else if (isMedium) 26.sp else 32.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White
                    )
                    Text(
                        text = "TÔI VẪN ỔN",
                        fontSize = if (isCompact) 12.5.sp else if (isMedium) 16.sp else 20.sp,
                        fontWeight = FontWeight.Black,
                        color = Color(0xFFFFEB3B)
                    )
                    Text(
                        text = if (holdingCancel) "ĐANG HỦY..." else "GIỮ ĐỂ HỦY",
                        fontSize = if (isCompact) 11.sp else if (isMedium) 14.sp else 18.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color.White
                    )
                }
            }
        }

        MainScreenStatus.SOS_SENT -> {
            // SOS Sent: ĐÃ GỬI SOS - ĐANG GỌI TRỢ GIÚP
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
                        contentDescription = "Đã bật SOS. Chưa xác nhận gửi ra ngoài."
                    }
                    .padding(if (isCompact) 4.dp else 8.dp)
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Text(
                        text = "ĐÃ BẬT",
                        fontSize = if (isCompact) 14.sp else if (isMedium) 20.sp else 26.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        textAlign = TextAlign.Center
                    )
                    Text(
                        text = "SOS",
                        fontSize = if (isCompact) 18.sp else if (isMedium) 26.sp else 32.sp,
                        fontWeight = FontWeight.Black,
                        color = Color.White,
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.height(if (isCompact) 1.dp else 2.dp))
                    Text(
                        text = "CẦN TRỢ GIÚP",
                        fontSize = if (isCompact) 10.sp else if (isMedium) 14.sp else 18.sp,
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
        // Draw progress ring around border
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
