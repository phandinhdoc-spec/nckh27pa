package vn.nckh27pa.fallsafe.bluetooth

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import vn.nckh27pa.fallsafe.protocol.Esp32PacketDecoder
import vn.nckh27pa.fallsafe.protocol.Esp32SensorPacket
import java.util.UUID

sealed class BleConnectionState {
    object Disconnected : BleConnectionState()
    object Scanning : BleConnectionState()
    data class Connecting(val deviceAddress: String) : BleConnectionState()
    data class Connected(val deviceAddress: String) : BleConnectionState()
    data class Subscribed(val deviceAddress: String) : BleConnectionState()
    data class Error(val message: String) : BleConnectionState()
}

/**
 * Thread-safe Android BLE central client for FallSafe ESP32.
 * Connects to ESP32 GATT service, configures MTU 512, performs sequential CCCD
 * subscriptions, reassembles IF-003 frames, and emits decoded telemetry and event packets.
 */
class FallSafeBleClient(
    private val context: Context? = null,
    private val customBluetoothAdapter: BluetoothAdapter? = null,
    val reassembler: Reassembler = Reassembler(),
    val packetDecoder: Esp32PacketDecoder = Esp32PacketDecoder(),
    val timeProvider: () -> Long = { System.nanoTime() / 1_000_000L }
) {
    private enum class SubscriptionStage { NONE, TELEMETRY, EVENT, ACK, COMPLETE }

    private val _connectionState = MutableStateFlow<BleConnectionState>(BleConnectionState.Disconnected)
    val connectionState: StateFlow<BleConnectionState> = _connectionState.asStateFlow()

    private val _latestSensorPacket = MutableStateFlow<Esp32SensorPacket?>(null)
    val latestSensorPacket: StateFlow<Esp32SensorPacket?> = _latestSensorPacket.asStateFlow()

    private val _sensorPackets = MutableSharedFlow<Esp32SensorPacket>(
        extraBufferCapacity = 64,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val sensorPackets: SharedFlow<Esp32SensorPacket> = _sensorPackets.asSharedFlow()

    private val _eventPackets = MutableSharedFlow<BleEventPacket>(
        extraBufferCapacity = 64,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val eventPackets: SharedFlow<BleEventPacket> = _eventPackets.asSharedFlow()

    var onSensorPacket: ((Esp32SensorPacket) -> Unit)? = null
    var onEventPacket: ((BleEventPacket) -> Unit)? = null
    var onConnectionStateChanged: ((BleConnectionState) -> Unit)? = null
    /** Debug callback for plain-JSON ACKs on char 0006 (S3 official firmware). */
    var onAckJson: ((String) -> Unit)? = null

    private var currentGatt: BluetoothGatt? = null
    private var isScanning = false
    private var subscriptionStage = SubscriptionStage.NONE
    private val mainHandler = Handler(Looper.getMainLooper())

    // W2.1 connection-lifecycle state: delayed connect coalescing, MTU-gated
    // discovery, and bounded reconnect backoff.
    private var connectAttempts = 0
    private var mtuResolved = false
    private var pendingConnectRunnable: Runnable? = null
    private var mtuFallbackRunnable: Runnable? = null
    private var pendingRetryRunnable: Runnable? = null

    companion object {
        private const val CONNECT_DELAY_MS = 250L
        private const val MTU_FALLBACK_DELAY_MS = 2500L
        private const val MAX_RETRY_ATTEMPTS = 3
        private val RETRY_BACKOFF_MS = longArrayOf(500L, 1000L, 2000L)
    }

    private val bluetoothAdapter: BluetoothAdapter?
        get() {
            if (customBluetoothAdapter != null) return customBluetoothAdapter
            val manager = context?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
            return manager?.adapter ?: BluetoothAdapter.getDefaultAdapter()
        }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            stopScan()
            connect(device)
        }

        @SuppressLint("MissingPermission")
        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            val first = results.firstOrNull()?.device ?: return
            stopScan()
            connect(first)
        }

        override fun onScanFailed(errorCode: Int) {
            isScanning = false
            _connectionState.value = BleConnectionState.Error("Scan failed with code: $errorCode")
            onConnectionStateChanged?.invoke(_connectionState.value)
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                // W2.1-S3: connection error (notably status 133). Safely refresh,
                // close the stale gatt, then bound-retry with backoff. Report
                // Error only after retries are exhausted.
                safeRefresh(gatt)
                closeGatt(gatt)
                scheduleReconnect(gatt.device)
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    // W2.1-S3: successful connection resets the retry budget.
                    connectAttempts = 0
                    mtuResolved = false
                    _connectionState.value = BleConnectionState.Connecting(gatt.device.address)
                    onConnectionStateChanged?.invoke(_connectionState.value)
                    // W2.1-S2: MTU negotiation gates discovery. discoverServices
                    // runs from onMtuChanged (any status); fallback below covers
                    // stacks that never invoke the callback.
                    try {
                        gatt.requestMtu(512)
                    } catch (_: Exception) {
                    }
                    // Fallback covers stacks that never invoke onMtuChanged;
                    // flag-gated so discovery runs at most once via fallback.
                    scheduleMtuFallback(gatt)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    reassembler.disconnect()
                    cancelMtuFallback()
                    mtuResolved = false
                    try {
                        gatt.close()
                    } catch (_: Exception) {}
                    if (currentGatt === gatt) {
                        currentGatt = null
                    }
                    subscriptionStage = SubscriptionStage.NONE
                    _connectionState.value = BleConnectionState.Disconnected
                    onConnectionStateChanged?.invoke(_connectionState.value)
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            // W2.1-S2: discover on every MTU outcome; flag prevents a second
            // discovery if the delayed fallback already fired.
            if (!mtuResolved) {
                mtuResolved = true
                cancelMtuFallback()
                gatt.discoverServices()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _connectionState.value = BleConnectionState.Error("Service discovery failed: status=$status")
                onConnectionStateChanged?.invoke(_connectionState.value)
                return
            }

            val service = gatt.getService(BleGattUuids.SERVICE_UUID)
            if (service == null) {
                _connectionState.value = BleConnectionState.Error("ESP32 FallSafe service not found")
                onConnectionStateChanged?.invoke(_connectionState.value)
                return
            }

            _connectionState.value = BleConnectionState.Connected(gatt.device.address)
            onConnectionStateChanged?.invoke(_connectionState.value)

            // Step 1 of sequential CCCD subscription: Telemetry
            val telemetryChar = service.getCharacteristic(BleGattUuids.TELEMETRY_NOTIFY_UUID)
            if (telemetryChar != null) {
                subscriptionStage = SubscriptionStage.TELEMETRY
                subscribeCharacteristic(gatt, telemetryChar)
            } else {
                // If no telemetry char, try event directly
                val eventChar = service.getCharacteristic(BleGattUuids.EVENT_NOTIFY_UUID)
                if (eventChar != null) {
                    subscriptionStage = SubscriptionStage.EVENT
                    subscribeCharacteristic(gatt, eventChar)
                } else {
                    completeSubscription(gatt)
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            val service = gatt.getService(BleGattUuids.SERVICE_UUID)
            when (subscriptionStage) {
                SubscriptionStage.TELEMETRY -> {
                    // Step 2 of sequential CCCD subscription: Event
                    val eventChar = service?.getCharacteristic(BleGattUuids.EVENT_NOTIFY_UUID)
                    if (eventChar != null) {
                        subscriptionStage = SubscriptionStage.EVENT
                        subscribeCharacteristic(gatt, eventChar)
                    } else {
                        completeSubscription(gatt)
                    }
                }
                SubscriptionStage.EVENT -> {
                    // Step 3 (S3 official firmware): ACK 0006 plain JSON.
                    // PROFILE_WRITE_UUID is retained as a legacy name; 0006 is ACK Notify-only.
                    // Missing char must not break the flow.
                    val ackChar = service?.getCharacteristic(BleGattUuids.PROFILE_WRITE_UUID)
                    if (ackChar != null) {
                        subscriptionStage = SubscriptionStage.ACK
                        subscribeCharacteristic(gatt, ackChar)
                    } else {
                        completeSubscription(gatt)
                    }
                }
                SubscriptionStage.ACK -> {
                    completeSubscription(gatt)
                }
                else -> {}
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            super.onCharacteristicChanged(gatt, characteristic)
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                @Suppress("DEPRECATION")
                val value = characteristic.value ?: return
                processNotification(characteristic.uuid, value)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            super.onCharacteristicChanged(gatt, characteristic, value)
            processNotification(characteristic.uuid, value)
        }
    }

    @SuppressLint("MissingPermission")
    private fun subscribeCharacteristic(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic
    ) {
        try {
            gatt.setCharacteristicNotification(characteristic, true)
            val descriptor = characteristic.getDescriptor(BleGattUuids.CCCD_UUID) ?: return
            // S3 Event char is INDICATE: pick the CCCD value from the
            // characteristic properties so Indicate actually arrives.
            val enableValue =
                if ((characteristic.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0) {
                    BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                } else {
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(descriptor, enableValue)
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = enableValue
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(descriptor)
            }
        } catch (e: Exception) {
            _connectionState.value = BleConnectionState.Error("Subscription error: ${e.message}")
            onConnectionStateChanged?.invoke(_connectionState.value)
        }
    }

    @SuppressLint("MissingPermission")
    private fun completeSubscription(gatt: BluetoothGatt) {
        subscriptionStage = SubscriptionStage.COMPLETE
        _connectionState.value = BleConnectionState.Subscribed(gatt.device.address)
        onConnectionStateChanged?.invoke(_connectionState.value)
        // S3 official firmware only streams after START_STREAM command.
        // Best-effort: ignore the result, user can retry from BleTestScreen.
        try {
            sendStartStream()
        } catch (_: Exception) {}
    }

    /**
     * Feed incoming notification/indication into the right path:
     * - Plain JSON starting with '{' (S3 official firmware) bypasses the
     *   IF-003 Reassembler and decodes directly; telemetry via
     *   packetDecoder.decodeSensor, event via BleEventPacket.parse.
     * - Otherwise keep the legacy IF-003 path (test-rig compatibility).
     */
    fun processNotification(uuid: UUID, value: ByteArray) {
        val kind = when (uuid) {
            BleGattUuids.TELEMETRY_NOTIFY_UUID -> BleGattUuids.KIND_TELEMETRY
            BleGattUuids.EVENT_NOTIFY_UUID -> BleGattUuids.KIND_EVENT
            BleGattUuids.PROFILE_WRITE_UUID -> BleGattUuids.KIND_ACK
            else -> return
        }

        if (isPlainJson(value)) {
            decodePlainJson(kind, value)
            return
        }

        val now = timeProvider()
        val result = reassembler.receive(now, reassembler.generation, kind, value)
        if (result.status == "COMPLETE" && result.payload != null) {
            decodePlainJson(kind, result.payload)
        }
    }

    /** First non-whitespace byte is '{' -> plain JSON (maybe with trailing NUL/space). */
    private fun isPlainJson(value: ByteArray): Boolean {
        for (b in value) {
            when (b.toInt().toChar()) {
                ' ', '\t', '\n', '\r', '\u0000' -> continue
                '{' -> return true
                else -> return false
            }
        }
        return false
    }

    private fun decodePlainJson(kind: Int, payload: ByteArray) {
        val payloadJson = String(payload, Charsets.UTF_8).trim().trimEnd('\u0000').trim()
        when (kind) {
            BleGattUuids.KIND_TELEMETRY -> {
                val packet = packetDecoder.decodeSensor(payloadJson)
                if (packet != null) {
                    _latestSensorPacket.value = packet
                    _sensorPackets.tryEmit(packet)
                    onSensorPacket?.invoke(packet)
                }
            }
            BleGattUuids.KIND_EVENT -> {
                val event = BleEventPacket.parse(payloadJson)
                if (event != null) {
                    _eventPackets.tryEmit(event)
                    onEventPacket?.invoke(event)
                }
            }
            BleGattUuids.KIND_ACK -> {
                onAckJson?.invoke(payloadJson)
            }
        }
    }

    /**
     * Start scanning for FallSafe ESP32 service and connect to first match.
     */
    @SuppressLint("MissingPermission")
    fun startScan(timeoutMs: Long = 10000L): Boolean {
        val adapter = bluetoothAdapter
        if (adapter == null || !adapter.isEnabled) {
            _connectionState.value = BleConnectionState.Error("Bluetooth not available or disabled")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return false
        }
        val scanner = adapter.bluetoothLeScanner
        if (scanner == null) {
            _connectionState.value = BleConnectionState.Error("BluetoothLeScanner unavailable")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return false
        }

        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(BleGattUuids.SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        isScanning = true
        _connectionState.value = BleConnectionState.Scanning
        onConnectionStateChanged?.invoke(_connectionState.value)

        try {
            scanner.startScan(listOf(filter), settings, scanCallback)
            mainHandler.postDelayed({
                if (isScanning) {
                    stopScan()
                    if (_connectionState.value is BleConnectionState.Scanning) {
                        _connectionState.value = BleConnectionState.Disconnected
                        onConnectionStateChanged?.invoke(_connectionState.value)
                    }
                }
            }, timeoutMs)
            return true
        } catch (e: Exception) {
            isScanning = false
            _connectionState.value = BleConnectionState.Error("Failed to start scan: ${e.message}")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return false
        }
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!isScanning) return
        isScanning = false
        try {
            bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
        } catch (_: Exception) {}
    }

    /**
     * Connect directly to a BluetoothDevice. Coalesces rapid calls: any pending
     * delayed connect is cancelled and the new one fires 250ms after teardown
     * so stopScan()/disconnect() can settle first.
     */
    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice): Boolean {
        cancelPendingConnect()
        cancelPendingRetry()
        connectAttempts = 0
        mtuResolved = false
        stopScan()
        closeCurrentGatt()
        val ctx = context ?: run {
            _connectionState.value = BleConnectionState.Error("Context required for BLE connection")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return false
        }
        _connectionState.value = BleConnectionState.Connecting(device.address)
        onConnectionStateChanged?.invoke(_connectionState.value)
        val runnable = Runnable {
            pendingConnectRunnable = null
            try {
                currentGatt = device.connectGatt(ctx, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            } catch (e: Exception) {
                _connectionState.value = BleConnectionState.Error("Connect failed: ${e.message}")
                onConnectionStateChanged?.invoke(_connectionState.value)
            }
        }
        pendingConnectRunnable = runnable
        mainHandler.postDelayed(runnable, CONNECT_DELAY_MS)
        return true
    }

    /**
     * Connect directly by device MAC address.
     */
    @SuppressLint("MissingPermission")
    fun connect(address: String): Boolean {
        val adapter = bluetoothAdapter ?: return false
        val device = try {
            adapter.getRemoteDevice(address)
        } catch (e: Exception) {
            _connectionState.value = BleConnectionState.Error("Invalid MAC address: $address")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return false
        }
        return connect(device)
    }

    /**
     * Write raw string to request characteristic (UUID: 7d2a0005).
     */
    fun writeRequest(request: String): Boolean {
        val gatt = currentGatt ?: return false
        val service = gatt.getService(BleGattUuids.SERVICE_UUID) ?: return false
        val char = service.getCharacteristic(BleGattUuids.REQUEST_WRITE_UUID) ?: return false
        return writeCharacteristicCompat(gatt, char, request.toByteArray(Charsets.UTF_8))
    }

    /**
     * S3 official firmware stream-control commands (plain JSON over 0005).
     * START_STREAM is auto-sent after Subscribed and can be retried manually;
     * STOP_STREAM is manual from BleTestScreen.
     */
    fun buildStreamCommand(commandType: String, nowMs: Long = System.currentTimeMillis()): String {
        val cmdId = "cmd-$nowMs"
        return "{\"protocolVersion\":1," +
            "\"commandId\":\"$cmdId\"," +
            "\"timestampMs\":$nowMs," +
            "\"commandType\":\"$commandType\"}"
    }

    fun sendStartStream(): Boolean =
        writeRequest(buildStreamCommand("START_STREAM"))

    fun sendStopStream(): Boolean =
        writeRequest(buildStreamCommand("STOP_STREAM"))

    /**
     * Profile writes are unsupported by the official firmware. In
     * esp-s3/esp-s3.ino:83 and esp/esp32-plan.md:270, 7d2a0006 is the ACK
     * characteristic and is Notify-only, so writing a profile there always fails.
     */
    fun writeProfile(profileJson: String): Boolean = false

    /** Keep this overload while profile configuration remains unavailable. */
    fun writeProfile(profile: BleFallProfile): Boolean = false

    @SuppressLint("MissingPermission")
    private fun writeCharacteristicCompat(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray
    ): Boolean {
        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeCharacteristic(
                    characteristic,
                    value,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                characteristic.value = value
                @Suppress("DEPRECATION")
                gatt.writeCharacteristic(characteristic)
            }
        } catch (e: Exception) {
            false
        }
    }

    /**
     * Cleanly disconnects and resets the reassembler session.
     */
    @SuppressLint("MissingPermission")
    fun disconnect() {
        cancelPendingConnect()
        cancelPendingRetry()
        cancelMtuFallback()
        connectAttempts = 0
        stopScan()
        reassembler.disconnect()
        closeCurrentGatt()
        subscriptionStage = SubscriptionStage.NONE
        mtuResolved = false
        _connectionState.value = BleConnectionState.Disconnected
        onConnectionStateChanged?.invoke(_connectionState.value)
    }

    // ---- W2.1 connection-lifecycle helpers (no UUID/protocol changes) ----

    private fun cancelPendingConnect() {
        pendingConnectRunnable?.let { mainHandler.removeCallbacks(it) }
        pendingConnectRunnable = null
    }

    private fun cancelPendingRetry() {
        pendingRetryRunnable?.let { mainHandler.removeCallbacks(it) }
        pendingRetryRunnable = null
    }

    private fun cancelMtuFallback() {
        mtuFallbackRunnable?.let { mainHandler.removeCallbacks(it) }
        mtuFallbackRunnable = null
    }

    @SuppressLint("MissingPermission")
    private fun closeCurrentGatt() {
        val gatt = currentGatt
        currentGatt = null
        if (gatt == null) return
        try {
            gatt.disconnect()
        } catch (_: Exception) {}
        closeGatt(gatt)
    }

    private fun closeGatt(gatt: BluetoothGatt) {
        try {
            gatt.close()
        } catch (_: Exception) {}
        if (currentGatt === gatt) {
            currentGatt = null
        }
    }

    /** Best-effort cache refresh via hidden API; ignored when unavailable. */
    private fun safeRefresh(gatt: BluetoothGatt) {
        try {
            val method = gatt.javaClass.getMethod("refresh")
            method.invoke(gatt)
        } catch (_: Exception) {}
    }

    @SuppressLint("MissingPermission")
    private fun scheduleMtuFallback(gatt: BluetoothGatt) {
        cancelMtuFallback()
        val runnable = Runnable {
            mtuFallbackRunnable = null
            if (!mtuResolved) {
                mtuResolved = true
                try {
                    gatt.discoverServices()
                } catch (_: Exception) {}
            }
        }
        mtuFallbackRunnable = runnable
        mainHandler.postDelayed(runnable, MTU_FALLBACK_DELAY_MS)
    }

    @SuppressLint("MissingPermission")
    private fun scheduleReconnect(device: BluetoothDevice) {
        if (connectAttempts >= MAX_RETRY_ATTEMPTS) {
            connectAttempts = 0
            _connectionState.value =
                BleConnectionState.Error("GATT connection failed after $MAX_RETRY_ATTEMPTS retries")
            onConnectionStateChanged?.invoke(_connectionState.value)
            return
        }
        val backoff = RETRY_BACKOFF_MS.getOrElse(connectAttempts) { RETRY_BACKOFF_MS.last() }
        connectAttempts++
        cancelPendingRetry()
        val runnable = Runnable {
            pendingRetryRunnable = null
            val ctx = context ?: run {
                connectAttempts = 0
                _connectionState.value = BleConnectionState.Error("Context required for BLE connection")
                onConnectionStateChanged?.invoke(_connectionState.value)
                return@Runnable
            }
            try {
                currentGatt = device.connectGatt(ctx, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
                _connectionState.value = BleConnectionState.Connecting(device.address)
                onConnectionStateChanged?.invoke(_connectionState.value)
            } catch (e: Exception) {
                scheduleReconnect(device)
            }
        }
        pendingRetryRunnable = runnable
        mainHandler.postDelayed(runnable, backoff)
    }
}
