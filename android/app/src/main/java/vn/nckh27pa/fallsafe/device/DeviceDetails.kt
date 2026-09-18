package vn.nckh27pa.fallsafe.device

enum class DeviceValueSource { BACKEND_HEARTBEAT, ESP32_PACKET, UNKNOWN }
data class DeviceDetails(
    val deviceId:String?=null,
    val connected:Boolean?=null,
    val batteryPercent:Int?=null,
    val firmwareVersion:String?=null,
    val gnssStatus:String?=null,
    val source:DeviceValueSource=DeviceValueSource.UNKNOWN,
    val lastHeartbeatMs:Long?=null,
    val statusText:String="Chưa có trạng thái ESP32 từ máy chủ"
) {
    companion object { val Unknown=DeviceDetails() }
}

/** Holds only values explicitly supplied by backend heartbeat or a real ESP32 packet. */
class DeviceDetailsStore {
    @Volatile var value:DeviceDetails=DeviceDetails.Unknown;private set
    fun updateFromBackend(details:DeviceDetails){require(details.source==DeviceValueSource.BACKEND_HEARTBEAT);value=details}
    fun updateFromEsp32(details:DeviceDetails){require(details.source==DeviceValueSource.ESP32_PACKET);value=details}
    fun markRefreshFailed(detail:String){value=if(value.source==DeviceValueSource.UNKNOWN)DeviceDetails(statusText=detail)
        else value.copy(statusText="Dữ liệu ESP32 cũ • $detail")}
    fun clear(){value=DeviceDetails.Unknown}
}
