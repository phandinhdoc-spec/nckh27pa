package vn.nckh27pa.fallsafe.bluetooth

import java.util.UUID

/** Authoritative BLE GATT UUID constants and framing kind constants for FallSafe ESP32. */
object BleGattUuids {
    val SERVICE_UUID: UUID = UUID.fromString("7d2a0001-6f45-4c2b-9a1e-38a8f5c10001")
    val TELEMETRY_NOTIFY_UUID: UUID = UUID.fromString("7d2a0002-6f45-4c2b-9a1e-38a8f5c10001")
    val EVENT_NOTIFY_UUID: UUID = UUID.fromString("7d2a0003-6f45-4c2b-9a1e-38a8f5c10001")
    val REQUEST_WRITE_UUID: UUID = UUID.fromString("7d2a0005-6f45-4c2b-9a1e-38a8f5c10001")
    val PROFILE_WRITE_UUID: UUID = UUID.fromString("7d2a0006-6f45-4c2b-9a1e-38a8f5c10001")
    val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    const val KIND_TELEMETRY = 1
    const val KIND_EVENT = 2
    const val KIND_STATUS = 3
    const val KIND_COMMAND = 4
    const val KIND_ACK = 5
}
