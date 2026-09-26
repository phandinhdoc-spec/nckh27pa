package vn.nckh27pa.fallsafe

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun ContactsScreen(
    controller: DemoController,
    viewModel: ContactsViewModel,
    onBack: (() -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()
    val contacts = controller.contacts

    var showAddDialog by remember { mutableStateOf(false) }
    var editingContact by remember { mutableStateOf<EmergencyContact?>(null) }
    var deletingContact by remember { mutableStateOf<EmergencyContact?>(null) }
    var showCannotDeleteLastDialog by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = "NGƯỜI THÂN NHẬN CẢNH BÁO",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "Quản lý danh sách liên hệ nhận cuộc gọi & tin nhắn khẩn cấp khi phát hiện sự cố.",
                    fontSize = 16.sp,
                    color = Color(0xFF455A64)
                )
            }
            if (onBack != null) {
                OutlinedButton(
                    onClick = onBack,
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text("QUAY LẠI", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        }

        // Add Contact Button
        Button(
            onClick = { showAddDialog = true },
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = 56.dp),
            colors = ButtonDefaults.buttonColors(containerColor = SafeGreen),
            shape = RoundedCornerShape(12.dp)
        ) {
            Text("➕ THÊM NGƯỜI THÂN MỚI", fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
        }

        HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp))

        // Contact Cards List
        if (contacts.isEmpty()) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = WarningOrangeContainer),
                border = BorderStroke(1.dp, WarningOrangeBorder)
            ) {
                Text(
                    text = "Chưa có người thân nào. Vui lòng thêm ít nhất một liên hệ để nhận cảnh báo!",
                    fontSize = 18.sp,
                    color = WarningOrange,
                    modifier = Modifier.padding(16.dp)
                )
            }
        } else {
            contacts.forEach { contact ->
                ContactItemCard(
                    contact = contact,
                    onSetPrimary = { viewModel.primary(contact.id) },
                    onToggleReceiveSos = { viewModel.toggle(contact.id) },
                    onEdit = { editingContact = contact },
                    onDelete = {
                        if (contacts.size <= 1) {
                            showCannotDeleteLastDialog = true
                        } else {
                            deletingContact = contact
                        }
                    }
                )
            }
        }

        // Summary note
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            shape = RoundedCornerShape(12.dp)
        ) {
            Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(
                    text = "Ghi chú an toàn:",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = "• Người nhận chính sẽ hiển thị trực tiếp tại màn hình chính.\n" +
                            "• Tất cả người thân đang bật 'Nhận SOS' đều được gửi cảnh báo khi xảy ra sự cố.\n" +
                            "• Cần duy trì ít nhất một liên hệ nhận cảnh báo khẩn cấp.",
                    fontSize = 16.sp
                )
            }
        }

        // Bottom clearance spacer so that the last card and note are never obscured by bottom bar
        Spacer(modifier = Modifier.height(56.dp))
    }

    // Add Contact Dialog
    if (showAddDialog) {
        ContactEditDialog(
            title = "Thêm người thân mới",
            initialName = "",
            initialRelationship = "Con gái",
            initialPhone = "",
            initialReceiveSos = true,
            initialIsPrimary = contacts.isEmpty(),
            isPrimaryLocked = contacts.isEmpty(),
            onDismiss = { showAddDialog = false },
            onSave = { name, rel, phone, receiveSos, isPrimary ->
                val success = viewModel.add(name, rel, phone, receiveSos, isPrimary)
                if (success) {
                    showAddDialog = false
                }
                success
            }
        )
    }

    // Edit Contact Dialog
    editingContact?.let { contact ->
        ContactEditDialog(
            title = "Chỉnh sửa người thân",
            initialName = contact.name,
            initialRelationship = contact.relationship,
            initialPhone = contact.phone,
            initialReceiveSos = contact.receiveSos,
            initialIsPrimary = contact.isPrimary,
            isPrimaryLocked = contact.isPrimary && contacts.size > 1,
            onDismiss = { editingContact = null },
            onSave = { name, rel, phone, receiveSos, isPrimary ->
                val updated = contact.copy(
                    name = name,
                    relationship = rel,
                    phone = phone,
                    receiveSos = receiveSos,
                    isPrimary = isPrimary
                )
                val success = viewModel.update(updated)
                if (success) {
                    editingContact = null
                }
                success
            }
        )
    }

    // Delete Confirmation Dialog
    deletingContact?.let { contact ->
        AlertDialog(
            onDismissRequest = { deletingContact = null },
            title = {
                Text("Xác nhận xóa người thân?", fontSize = 22.sp, fontWeight = FontWeight.Bold)
            },
            text = {
                Text(
                    text = "Bạn có chắc chắn muốn xóa ${contact.name} (${contact.relationship}) khỏi danh sách nhận cảnh báo khẩn cấp?",
                    fontSize = 18.sp
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        viewModel.delete(contact.id)
                        deletingContact = null
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = SosRed),
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text("XÓA LIÊN HỆ", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            },
            dismissButton = {
                OutlinedButton(
                    onClick = { deletingContact = null },
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text("HỦY", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        )
    }

    // Cannot Delete Last Contact Warning Dialog
    if (showCannotDeleteLastDialog) {
        AlertDialog(
            onDismissRequest = { showCannotDeleteLastDialog = false },
            title = {
                Text("Không thể xóa", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = WarningOrange)
            },
            text = {
                Text(
                    text = "Không thể xóa người thân duy nhất. Hệ thống FallSafe luôn yêu cầu ít nhất một liên hệ khẩn cấp để đảm bảo an toàn.",
                    fontSize = 18.sp
                )
            },
            confirmButton = {
                Button(
                    onClick = { showCannotDeleteLastDialog = false },
                    modifier = Modifier.heightIn(min = 56.dp)
                ) {
                    Text("ĐÃ HIỂU", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        )
    }
}

@Composable
private fun ContactItemCard(
    contact: EmergencyContact,
    onSetPrimary: () -> Unit,
    onToggleReceiveSos: () -> Unit,
    onEdit: () -> Unit,
    onDelete: () -> Unit
) {
    val containerColor = if (contact.isPrimary) ContactBlueContainer else MaterialTheme.colorScheme.surface
    val borderColor = if (contact.isPrimary) ContactBlueBorder else Color(0xFFCFD8DC)

    Card(
        modifier = Modifier
            .fillMaxWidth()
            .semantics {
                contentDescription = "Người thân ${contact.name}, quan hệ ${contact.relationship}, số ${ContactValidator.mask(contact.phone)}"
            },
        colors = CardDefaults.cardColors(containerColor = containerColor),
        border = BorderStroke(if (contact.isPrimary) 2.dp else 1.dp, borderColor),
        shape = RoundedCornerShape(14.dp)
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // Header row: Icon, Name, and Badges
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text("👤", fontSize = 28.sp)
                    Column {
                        Text(
                            text = contact.name,
                            fontSize = 20.sp,
                            fontWeight = FontWeight.Bold,
                            color = if (contact.isPrimary) ContactBlue else Color.Black
                        )
                        Text(
                            text = "Quan hệ: ${contact.relationship}",
                            fontSize = 16.sp,
                            color = Color(0xFF37474F),
                            fontWeight = FontWeight.Medium
                        )
                    }
                }

                // Primary badge
                if (contact.isPrimary) {
                    Surface(
                        color = ContactBlue,
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text(
                            text = "Người nhận chính",
                            color = Color.White,
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                        )
                    }
                }
            }

            // Phone number
            Text(
                text = "Số điện thoại: ${ContactValidator.mask(contact.phone)}",
                fontSize = 17.sp,
                fontWeight = FontWeight.SemiBold
            )

            // Status and SOS Toggle Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Surface(
                    color = if (contact.receiveSos) SafeGreenContainer else DisconnectedGrayContainer,
                    shape = RoundedCornerShape(6.dp),
                    border = BorderStroke(1.dp, if (contact.receiveSos) SafeGreenBorder else DisconnectedGrayBorder)
                ) {
                    Text(
                        text = if (contact.receiveSos) "Đang nhận SOS" else "Tắt nhận SOS",
                        color = if (contact.receiveSos) SafeGreen else DisconnectedGray,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp)
                    )
                }

                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Nhận SOS", fontSize = 14.sp, fontWeight = FontWeight.Medium)
                    Spacer(Modifier.width(6.dp))
                    Switch(
                        checked = contact.receiveSos,
                        onCheckedChange = { onToggleReceiveSos() },
                        modifier = Modifier.semantics {
                            contentDescription = "Bật hoặc tắt nhận cảnh báo SOS cho ${contact.name}"
                        }
                    )
                }
            }

            HorizontalDivider(color = Color(0xFFE0E0E0))

            // Action buttons row: Set primary, Edit, Delete
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                if (!contact.isPrimary) {
                    OutlinedButton(
                        onClick = onSetPrimary,
                        modifier = Modifier
                            .weight(1f)
                            .heightIn(min = 56.dp)
                    ) {
                        Text("Chọn làm chính", fontSize = 14.sp, fontWeight = FontWeight.Bold)
                    }
                }

                Button(
                    onClick = onEdit,
                    modifier = Modifier
                        .weight(1f)
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.secondary)
                ) {
                    Text("SỬA", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }

                OutlinedButton(
                    onClick = onDelete,
                    modifier = Modifier
                        .weight(1f)
                        .heightIn(min = 56.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = SosRed),
                    border = BorderStroke(1.dp, SosRedBorder)
                ) {
                    Text("XÓA", fontSize = 16.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

@Composable
private fun ContactEditDialog(
    title: String,
    initialName: String,
    initialRelationship: String,
    initialPhone: String,
    initialReceiveSos: Boolean,
    initialIsPrimary: Boolean,
    isPrimaryLocked: Boolean,
    onDismiss: () -> Unit,
    onSave: (name: String, relationship: String, phone: String, receiveSos: Boolean, isPrimary: Boolean) -> Boolean
) {
    var name by remember { mutableStateOf(initialName) }
    var relationship by remember { mutableStateOf(initialRelationship) }
    var phone by remember { mutableStateOf(initialPhone) }
    var receiveSos by remember { mutableStateOf(initialReceiveSos) }
    var isPrimary by remember { mutableStateOf(initialIsPrimary) }
    var errorMessage by remember { mutableStateOf<String?>(null) }

    val quickRelationships = listOf("Con gái", "Con trai", "Vợ", "Chồng", "Cháu", "Người chăm sóc")

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text(title, fontSize = 22.sp, fontWeight = FontWeight.Bold)
        },
        text = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                OutlinedTextField(
                    value = name,
                    onValueChange = { name = it; errorMessage = null },
                    label = { Text("Họ và tên") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                OutlinedTextField(
                    value = relationship,
                    onValueChange = { relationship = it; errorMessage = null },
                    label = { Text("Mối quan hệ") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                // Quick relationship chips
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    quickRelationships.take(3).forEach { rel ->
                        FilterChip(
                            selected = relationship == rel,
                            onClick = { relationship = rel },
                            label = { Text(rel, fontSize = 14.sp) }
                        )
                    }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    quickRelationships.drop(3).forEach { rel ->
                        FilterChip(
                            selected = relationship == rel,
                            onClick = { relationship = rel },
                            label = { Text(rel, fontSize = 14.sp) }
                        )
                    }
                }

                OutlinedTextField(
                    value = phone,
                    onValueChange = { phone = it; errorMessage = null },
                    label = { Text("Số điện thoại (đầu 0 hoặc +84)") },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Phone),
                    singleLine = true,
                    isError = errorMessage != null,
                    modifier = Modifier.fillMaxWidth()
                )

                errorMessage?.let { msg ->
                    Text(
                        text = msg,
                        color = MaterialTheme.colorScheme.error,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold
                    )
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text("Nhận cảnh báo SOS", fontSize = 16.sp)
                    Switch(checked = receiveSos, onCheckedChange = { receiveSos = it })
                }

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Column {
                        Text("Người nhận chính", fontSize = 16.sp)
                        if (isPrimaryLocked) {
                            Text(
                                "(Bắt buộc có 1 người nhận chính)",
                                fontSize = 12.sp,
                                color = Color.Gray
                            )
                        }
                    }
                    Switch(
                        checked = isPrimary,
                        onCheckedChange = { if (!isPrimaryLocked) isPrimary = it },
                        enabled = !isPrimaryLocked
                    )
                }
            }
        },
        confirmButton = {
            Button(
                onClick = {
                    if (name.isBlank()) {
                        errorMessage = "Vui lòng nhập họ và tên"
                        return@Button
                    }
                    if (relationship.isBlank()) {
                        errorMessage = "Vui lòng nhập mối quan hệ"
                        return@Button
                    }
                    val validationError = ContactValidator.validate(phone)
                    if (validationError != null) {
                        errorMessage = validationError
                        return@Button
                    }
                    val saved = onSave(name, relationship, phone, receiveSos, isPrimary)
                    if (!saved) {
                        errorMessage = "Không thể lưu thông tin. Vui lòng kiểm tra lại."
                    }
                },
                modifier = Modifier.heightIn(min = 56.dp)
            ) {
                Text("LƯU", fontSize = 16.sp, fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            OutlinedButton(
                onClick = onDismiss,
                modifier = Modifier.heightIn(min = 56.dp)
            ) {
                Text("HỦY", fontSize = 16.sp, fontWeight = FontWeight.Bold)
            }
        }
    )
}
