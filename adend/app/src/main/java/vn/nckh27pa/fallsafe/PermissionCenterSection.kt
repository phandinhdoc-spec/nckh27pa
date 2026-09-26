package vn.nckh27pa.fallsafe

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import vn.nckh27pa.fallsafe.permissions.Capability
import vn.nckh27pa.fallsafe.permissions.CapabilityDisplay
import vn.nckh27pa.fallsafe.permissions.CapabilityDisplayState
import vn.nckh27pa.fallsafe.permissions.LocationPrecision
import vn.nckh27pa.fallsafe.permissions.PermissionRequestResult

const val PERMISSION_NEEDS_SETTINGS_BODY = "Quyền này đang bị tắt. Hãy bật trong Cài đặt."

enum class PermissionFollowUp {
    NONE,
    SHOW_SETTINGS_DIALOG,
    REQUEST_WITH_EXPLANATION
}

internal fun resolveRequestOutcome(result: PermissionRequestResult): PermissionFollowUp = when (result) {
    PermissionRequestResult.GRANTED,
    PermissionRequestResult.SYSTEM_PROMPT_STARTED -> PermissionFollowUp.NONE
    PermissionRequestResult.SETTINGS_REQUIRED -> PermissionFollowUp.SHOW_SETTINGS_DIALOG
    PermissionRequestResult.EXPLANATION_REQUIRED -> PermissionFollowUp.REQUEST_WITH_EXPLANATION
}

internal fun resolveCapabilityItemTitle(capability: Capability): String = when (capability) {
    Capability.LOCATION -> "📍 Vị trí"
    Capability.CALLING -> "📞 Điện thoại"
    Capability.MESSAGING -> "💬 SMS"
}

internal fun resolveCapabilityStateLabel(display: CapabilityDisplay): String = when (display.state) {
    CapabilityDisplayState.GRANTED -> {
        if (display.capability == Capability.LOCATION && display.locationPrecision == LocationPrecision.APPROXIMATE) {
            "Đã cấp (vị trí gần đúng)"
        } else {
            "Đã cấp"
        }
    }
    CapabilityDisplayState.CAN_REQUEST -> "Chưa cấp"
    CapabilityDisplayState.NEEDS_SETTINGS -> "Bị từ chối — cần mở Cài đặt ứng dụng"
}

internal fun resolveCapabilityActionLabel(state: CapabilityDisplayState): String? = when (state) {
    CapabilityDisplayState.GRANTED -> null
    CapabilityDisplayState.CAN_REQUEST -> "CẤP QUYỀN"
    CapabilityDisplayState.NEEDS_SETTINGS -> "MỞ CÀI ĐẶT ỨNG DỤNG"
}

internal fun resolveCapabilityDescription(display: CapabilityDisplay): String {
    if (display.capability == Capability.LOCATION &&
        display.state == CapabilityDisplayState.GRANTED &&
        display.locationPrecision == LocationPrecision.APPROXIMATE
    ) {
        return display.note?.takeIf { it.isNotBlank() }
            ?: "Vị trí gần đúng vẫn dùng được; người thân có thể thấy khu vực thay vì điểm chính xác."
    }
    val parts = mutableListOf<String>()
    if (!display.note.isNullOrBlank()) {
        parts += display.note
    }
    if (display.reason.isNotBlank()) {
        parts += display.reason
    }
    if (!display.remediation.isNullOrBlank() && display.state != CapabilityDisplayState.GRANTED) {
        parts += display.remediation
    }
    return parts.joinToString("\n")
}

internal fun resolveCapabilityTalkBackLabel(display: CapabilityDisplay): String {
    val title = resolveCapabilityItemTitle(display.capability)
    val state = resolveCapabilityStateLabel(display)
    val desc = resolveCapabilityDescription(display)
    return if (desc.isNotBlank()) "$title: $state. $desc" else "$title: $state"
}

@Composable
internal fun PermissionSettingsDialog(
    onOpenSettings: () -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(
                text = "Quyền ứng dụng",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold
            )
        },
        text = {
            Text(
                text = PERMISSION_NEEDS_SETTINGS_BODY,
                fontSize = 16.sp,
                lineHeight = 22.sp
            )
        },
        confirmButton = {
            Button(
                onClick = onOpenSettings,
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 56.dp),
                colors = ButtonDefaults.buttonColors(containerColor = SafeGreen)
            ) {
                Text(
                    text = "MỞ CÀI ĐẶT ỨNG DỤNG",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
            }
        },
        dismissButton = {
            OutlinedButton(
                onClick = onDismiss,
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 56.dp)
            ) {
                Text(
                    text = "ĐÓNG",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    )
}

@Composable
fun PermissionCenterSection(
    controller: DemoController,
    modifier: Modifier = Modifier
) {
    var settingsDialogCapability by remember { mutableStateOf<Capability?>(null) }

    fun handleRequest(
        capability: Capability,
        requestAction: (Boolean) -> PermissionRequestResult
    ) {
        val result = requestAction(true)
        when (resolveRequestOutcome(result)) {
            PermissionFollowUp.NONE -> Unit
            PermissionFollowUp.SHOW_SETTINGS_DIALOG -> {
                settingsDialogCapability = capability
            }
            PermissionFollowUp.REQUEST_WITH_EXPLANATION -> {
                val retry = requestAction(true)
                if (resolveRequestOutcome(retry) == PermissionFollowUp.SHOW_SETTINGS_DIALOG) {
                    settingsDialogCapability = capability
                }
            }
        }
    }

    Card(
        modifier = modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        shape = RoundedCornerShape(14.dp)
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = "QUYỀN ỨNG DỤNG",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = "Các quyền cần thiết để ứng dụng gửi cảnh báo, gọi và định vị khi có sự cố khẩn cấp.",
                fontSize = 16.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            // Three rows in exact order: 1. Vị trí, 2. Điện thoại, 3. SMS
            PermissionRow(
                display = controller.locationCapability,
                onRequest = {
                    handleRequest(Capability.LOCATION) { ack ->
                        controller.requestLocationPermission(ack)
                    }
                },
                onOpenSettings = { controller.openLocationPermissionSettings() }
            )

            PermissionRow(
                display = controller.callingCapability,
                onRequest = {
                    handleRequest(Capability.CALLING) { ack ->
                        controller.requestCallingPermission(ack)
                    }
                },
                onOpenSettings = { controller.openCallingPermissionSettings() }
            )

            PermissionRow(
                display = controller.messagingCapability,
                onRequest = {
                    handleRequest(Capability.MESSAGING) { ack ->
                        controller.requestMessagingPermission(ack)
                    }
                },
                onOpenSettings = { controller.openMessagingPermissionSettings() }
            )
        }
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

@Composable
private fun PermissionRow(
    display: CapabilityDisplay,
    onRequest: () -> Unit,
    onOpenSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    val title = resolveCapabilityItemTitle(display.capability)
    val stateLabel = resolveCapabilityStateLabel(display)
    val actionLabel = resolveCapabilityActionLabel(display.state)
    val description = resolveCapabilityDescription(display)
    val talkBackLabel = resolveCapabilityTalkBackLabel(display)

    val stateColor = when (display.state) {
        CapabilityDisplayState.GRANTED -> SafeGreen
        CapabilityDisplayState.CAN_REQUEST -> WarningOrange
        CapabilityDisplayState.NEEDS_SETTINGS -> SosRed
    }

    val containerColor = when (display.state) {
        CapabilityDisplayState.GRANTED -> SafeGreenContainer
        CapabilityDisplayState.CAN_REQUEST -> WarningOrangeContainer
        CapabilityDisplayState.NEEDS_SETTINGS -> SosRedContainer
    }

    val borderColor = when (display.state) {
        CapabilityDisplayState.GRANTED -> SafeGreenBorder
        CapabilityDisplayState.CAN_REQUEST -> WarningOrangeBorder
        CapabilityDisplayState.NEEDS_SETTINGS -> SosRedBorder
    }

    Card(
        modifier = modifier
            .fillMaxWidth()
            .semantics { contentDescription = talkBackLabel },
        colors = CardDefaults.cardColors(containerColor = containerColor),
        border = BorderStroke(1.5.dp, borderColor),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = title,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    if (display.state == CapabilityDisplayState.GRANTED) {
                        Text(
                            text = "✓",
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            color = stateColor
                        )
                    }
                    Text(
                        text = stateLabel,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = stateColor
                    )
                }
            }

            if (description.isNotBlank()) {
                Text(
                    text = description,
                    fontSize = 16.sp,
                    color = Color(0xFF263238),
                    lineHeight = 22.sp
                )
            }

            if (actionLabel != null) {
                Button(
                    onClick = {
                        if (display.state == CapabilityDisplayState.CAN_REQUEST) {
                            onRequest()
                        } else if (display.state == CapabilityDisplayState.NEEDS_SETTINGS) {
                            onOpenSettings()
                        }
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp)
                        .semantics {
                            role = Role.Button
                            contentDescription = "$actionLabel cho $title"
                        },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (display.state == CapabilityDisplayState.CAN_REQUEST) SafeGreen else SosRedDark
                    )
                ) {
                    Text(
                        text = actionLabel,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                }
            }
        }
    }
}
