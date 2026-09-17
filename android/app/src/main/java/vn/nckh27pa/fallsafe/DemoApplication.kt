package vn.nckh27pa.fallsafe

import android.app.Application
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import core.MonotonicClock
import core.State
import core.Status

/** Unified UI state answering the 4 questions and center button modes. */
enum class MainScreenStatus {
    SAFE,                  // 1. Bình thường: Đang bảo vệ
    WARNING_COUNTDOWN,     // 2. Đang đếm ngược cảnh báo ngã (10s)
    SOS_SENT,              // 3. Đã gửi SOS / Đang gọi trợ giúp
    HELP_ACKNOWLEDGED,     // 4. Người thân đã nhận tin
    DEVICE_DISCONNECTED    // 5. Mất kết nối thiết bị
}

/** Pure mapping function from domain state to 5 main screen UI states.
 * Safety priority: Emergency states (VERIFYING, ALERTING, AWAITING_HELP, HELP_ACKNOWLEDGED)
 * always take precedence at the center action button so critical alerts and cancel/help operations
 * are never masked by device disconnection. DEVICE_DISCONNECTED only governs the center button
 * when the system is otherwise in normal MONITORING.
 */
fun resolveMainScreenStatus(
    deviceConnected: Boolean,
    state: State,
    status: Status,
    caregiverAcknowledged: Boolean
): MainScreenStatus {
    return when (state) {
        State.VERIFYING, State.SUSPECTED -> MainScreenStatus.WARNING_COUNTDOWN
        State.ALERTING -> MainScreenStatus.SOS_SENT
        State.AWAITING_HELP -> {
            if (caregiverAcknowledged) MainScreenStatus.HELP_ACKNOWLEDGED
            else MainScreenStatus.SOS_SENT
        }
        State.MONITORING -> {
            if (caregiverAcknowledged || status == Status.ACKNOWLEDGED) {
                MainScreenStatus.HELP_ACKNOWLEDGED
            } else if (!deviceConnected) {
                MainScreenStatus.DEVICE_DISCONNECTED
            } else {
                MainScreenStatus.SAFE
            }
        }
    }
}

/** Process-scoped session survives Activity recreation. No foreground service. */
class DemoApplication : Application() {
    lateinit var controller: DemoController; private set
    private lateinit var apiClient: vn.nckh27pa.fallsafe.api.ApiClient
    private lateinit var sync: vn.nckh27pa.fallsafe.api.SyncCoordinator
    override fun onCreate() {
        super.onCreate()
        val repo = SharedPrefsContactRepository(this)
        controller = DemoController(repo)
        val config = vn.nckh27pa.fallsafe.api.ApiConfig.generated()
        apiClient = vn.nckh27pa.fallsafe.api.ApiClient(config)
        val outbox = vn.nckh27pa.fallsafe.api.SyncOutbox(
            vn.nckh27pa.fallsafe.api.PreferencesSyncStore(this, "${config.userId}_${config.deviceId}"))
        val info = vn.nckh27pa.fallsafe.api.AndroidDeviceInfo(this)
        sync = vn.nckh27pa.fallsafe.api.SyncCoordinator(controller,
            vn.nckh27pa.fallsafe.api.ApiRepository(apiClient.service), config, outbox,
            battery = info::battery, heartbeat = info::heartbeat)
        androidx.lifecycle.ProcessLifecycleOwner.get().lifecycle.addObserver(sync)
        sync.start()
    }
    override fun onTerminate() {
        androidx.lifecycle.ProcessLifecycleOwner.get().lifecycle.removeObserver(sync)
        sync.close(); apiClient.close(); controller.close()
        super.onTerminate()
    }
}
class DemoController(val contactRepository: ContactRepository = InMemoryContactRepository(),
    clock: MonotonicClock = MonotonicClock { SystemClock.elapsedRealtime() }) {
    var onContactMutation: ((vn.nckh27pa.fallsafe.api.OperationKind, EmergencyContact?, String) -> Unit)? = null
    var onSyncStateChanged: (() -> Unit)? = null
    var onPhoneReading: ((PhoneSensorPacket) -> Unit)? = null
    var onSyncResume: (() -> Unit)? = null
    var apiState by mutableStateOf<vn.nckh27pa.fallsafe.api.ApiResult<*>>(vn.nckh27pa.fallsafe.api.ApiResult.Loading)
    var syncStatus by mutableStateOf("Chưa đồng bộ máy chủ")
    var sosDeliveryMessage by mutableStateOf("SOS đã lưu trên điện thoại; chưa xác nhận gửi ra ngoài.")
    var foreground by mutableStateOf(false)
    var backgroundMonitoring by mutableStateOf(false)
    var backgroundMessage by mutableStateOf("Giám sát nền chưa bật")
    var backgroundSensors by mutableStateOf("")
    var onStateChanged: (() -> Unit)? = null
    var onMonitoringChanged: (() -> Unit)? = null
    val session = DemoSession(clock)
    var snapshot by mutableStateOf(session.snapshot()); private set
    var events by mutableStateOf(session.events()); private set
    var source by mutableStateOf("PHONE_ONLY • cảm biến thật"); private set
    var packet by mutableStateOf<PhoneSensorPacket?>(null); private set
    var replaying by mutableStateOf(false); private set
    var fail by mutableStateOf(false); private set

    // Redesigned Home Screen state properties
    var deviceConnected by mutableStateOf(true)
    var batteryStatus by mutableStateOf("Pin tốt")
    var locationStatus by mutableStateOf("Chưa xác định vị trí")
    var caregiverAcknowledged by mutableStateOf(false)

    // Dynamic emergency contacts backed by repository
    var contacts by mutableStateOf(contactRepository.getContacts())
        private set

    fun reloadContactsFromRepo() {
        contacts = contactRepository.getContacts()
    }

    val primaryContact: EmergencyContact?
        get() = contacts.firstOrNull { it.isPrimary } ?: contacts.firstOrNull()

    val primaryContactName: String
        get() = primaryContact?.let { it.relationship.ifBlank { it.name } } ?: "Chưa có liên hệ"

    val primaryContactFullName: String
        get() = primaryContact?.name ?: "Chưa có liên hệ"

    val primaryContactPhone: String
        get() = primaryContact?.phone ?: ""

    val primaryContactStatus: String
        get() = if (primaryContact?.receiveSos == true) "Sẵn sàng nhận SOS" else "Tắt nhận SOS"

    fun addContact(name: String, relationship: String, phone: String, receiveSos: Boolean, isPrimary: Boolean): Boolean {
        val normPhone = ContactValidator.normalize(phone)
        if (name.trim().isEmpty() || name.trim().length > 100 || ContactValidator.validate(normPhone) != null) return false
        val newContact = EmergencyContact(
            name = name.trim(),
            relationship = relationship.trim(),
            phone = normPhone,
            receiveSos = receiveSos,
            isPrimary = isPrimary || contacts.isEmpty()
        )
        val updated = if (newContact.isPrimary) {
            contacts.map { it.copy(isPrimary = false) } + newContact
        } else {
            contacts + newContact
        }
        contacts = updated
        contactRepository.saveContacts(updated)
        onContactMutation?.invoke(vn.nckh27pa.fallsafe.api.OperationKind.CONTACT_ADD, newContact, newContact.id)
        return true
    }

    fun updateContact(contact: EmergencyContact): Boolean {
        val normPhone = ContactValidator.normalize(contact.phone)
        if (contact.name.trim().isEmpty() || contact.name.trim().length > 100 || contacts.none { it.id == contact.id } || ContactValidator.validate(normPhone) != null) return false
        val cleanContact = contact.copy(name = contact.name.trim(), relationship = contact.relationship.trim(), phone = normPhone)
        val updated = contacts.map {
            if (it.id == cleanContact.id) {
                cleanContact
            } else if (cleanContact.isPrimary) {
                it.copy(isPrimary = false)
            } else {
                it
            }
        }
        val verified = if (updated.none { it.isPrimary } && updated.isNotEmpty()) {
            updated.mapIndexed { idx, c -> if (idx == 0) c.copy(isPrimary = true) else c }
        } else updated
        contacts = verified
        contactRepository.saveContacts(verified)
        onContactMutation?.invoke(vn.nckh27pa.fallsafe.api.OperationKind.CONTACT_UPDATE, verified.first { it.id == contact.id }, contact.id)
        return true
    }

    fun deleteContact(id: String): Boolean {
        if (contacts.size <= 1 || contacts.none { it.id == id }) return false
        val remaining = contacts.filterNot { it.id == id }
        val verified = if (remaining.none { it.isPrimary } && remaining.isNotEmpty()) {
            remaining.mapIndexed { idx, c -> if (idx == 0) c.copy(isPrimary = true) else c }
        } else remaining
        contacts = verified
        contactRepository.saveContacts(verified)
        onContactMutation?.invoke(vn.nckh27pa.fallsafe.api.OperationKind.CONTACT_DELETE, null, id)
        return true
    }

    fun setPrimaryContact(id: String) {
        if (contacts.none { it.id == id }) return
        val updated = contacts.map { it.copy(isPrimary = (it.id == id)) }
        contacts = updated
        contactRepository.saveContacts(updated)
        updated.firstOrNull { it.id == id }?.let { onContactMutation?.invoke(vn.nckh27pa.fallsafe.api.OperationKind.CONTACT_UPDATE, it, id) }
    }

    fun toggleReceiveSos(id: String) {
        val updated = contacts.map {
            if (it.id == id) it.copy(receiveSos = !it.receiveSos) else it
        }
        contacts = updated
        contactRepository.saveContacts(updated)
        updated.firstOrNull { it.id == id }?.let { onContactMutation?.invoke(vn.nckh27pa.fallsafe.api.OperationKind.CONTACT_UPDATE, it, id) }
    }

    val mainScreenStatus: MainScreenStatus
        get() = resolveMainScreenStatus(deviceConnected, snapshot.state, snapshot.status, caregiverAcknowledged)
    private val input = DemoInputAdapter(session, { source.startsWith("PHONE_ONLY") }, { packet = it })
    private var replay: DemoReplay? = null
    private val handler by lazy {
        try {
            Handler(Looper.getMainLooper())
        } catch (_: Throwable) {
            null
        }
    }
    private val tick = object : Runnable {
        override fun run() {
            val now = SystemClock.elapsedRealtime()
            replay?.let { r ->
                r.due(now).forEach(input::acceptReplay)
                if (r.finished) { replay = null; replaying = false }
            }
            session.tick()
            refresh()
            if (replay != null || snapshot.state == State.VERIFYING) handler?.postDelayed(this, 50)
        }
    }
    fun refresh() { snapshot = session.snapshot(); events = session.events(); onStateChanged?.invoke(); onSyncStateChanged?.invoke() }
    private fun schedule() { handler?.removeCallbacks(tick); handler?.post(tick) }
    fun acceptPhone(p: PhoneSensorPacket?) {
        if (!source.startsWith("PHONE_ONLY")) return
        input.acceptPhone(p)
        p?.let { onPhoneReading?.invoke(it) }
        refresh()
        if (snapshot.state == State.VERIFYING) schedule()
    }
    fun displayPhone(p: PhoneSensorPacket?) {
        if (source.startsWith("PHONE_ONLY")) packet = p
    }
    fun runReplay() {
        if (snapshot.state != State.MONITORING || replaying) return
        source = "MÔ PHỎNG • dữ liệu giả"
        session.resetDetection()
        packet = null
        replay = DemoReplay(SystemClock.elapsedRealtime())
        replaying = true
        schedule()
    }
    fun usePhone() {
        if (snapshot.state != State.MONITORING) return
        replay = null; replaying = false; packet = null
        source = "PHONE_ONLY • cảm biến thật"
        session.resetDetection()
    }
    fun safe() { caregiverAcknowledged = false; session.safe(); refresh() }
    fun sos() { if (foreground) help() }
    fun help() { caregiverAcknowledged = false; session.needHelp(); refresh() }
    fun complete() { replay = null; replaying = false; caregiverAcknowledged = false; session.complete(); refresh() }
    fun acknowledgeHelp() { caregiverAcknowledged = true; refresh() }
    fun resetCaregiverAcknowledged() { caregiverAcknowledged = false; refresh() }
    fun setDeviceConnectedState(connected: Boolean) { deviceConnected = connected; refresh() }
    fun setFailure(value: Boolean) { fail = value; session.sink.fail = value }
    fun paused() { input.paused(backgroundMonitoring); refresh() }
    fun resumed() { session.tick(); refresh(); schedule(); onSyncResume?.invoke() }
    fun close() {
        handler?.removeCallbacks(tick)
        onStateChanged = null; onMonitoringChanged = null; onSyncStateChanged = null
        onPhoneReading = null; onContactMutation = null; onSyncResume = null
    }
}
