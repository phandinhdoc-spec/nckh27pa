# Hợp đồng Android–ESP32
Trạng thái: trích xuất yêu cầu v1; KHÔNG phải chứng nhận tương thích hoặc phê duyệt các bổ sung. Hai kế hoạch giữ nguyên. Các mục chưa chốt ở dưới chặn tích hợp BLE, không chặn PHONE_ONLY.

## Phần đã định nghĩa
Nguồn Android §6 (dòng403–647), ESP32 §8.1–8.5. Trích nguyên văn phần Android để giữ nguyên mọi tên, kiểu và UUID:

## 6. Chuẩn giao tiếp Android – ESP32

### 6.1. Quy ước chung

- Tên trường API dùng `lowerCamelCase`.
- Hằng số Kotlin dùng `UPPER_SNAKE_CASE`.
- Đơn vị nằm ngay trong tên biến để tránh hiểu nhầm: `pressurePa`, `altitudeDeltaM`, `batteryPercent`.
- Thời gian dùng Unix milliseconds: `timestampMs`.
- Mỗi gói có `protocolVersion`, `deviceId` và `sequenceNumber`.
- Android bỏ qua gói trùng `sequenceNumber`, đánh dấu gói mất và kiểm tra phiên bản trước khi giải mã.
- Dữ liệu nhị phân nên dùng cho luồng mẫu tần số cao; JSON dùng cho sự kiện, trạng thái và giai đoạn phát triển vì dễ đọc.

### 6.2. Tên biến cấu hình giao tiếp trong Android

```kotlin
object Esp32ApiConfig {
    const val PROTOCOL_VERSION = 1

    // BLE identification
    const val ESP32_DEVICE_NAME_PREFIX = "FALLSAFE-"
    const val ESP32_SERVICE_UUID = "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001"

    // ESP32 -> Android: dữ liệu cảm biến liên tục (Notify)
    const val SENSOR_STREAM_CHARACTERISTIC_UUID =
        "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001"

    // ESP32 -> Android: sự kiện ưu tiên như SOS/va đập/pin yếu (Indicate)
    const val EVENT_CHARACTERISTIC_UUID =
        "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001"

    // ESP32 -> Android: pin, sạc, cảm biến, firmware (Read/Notify)
    const val DEVICE_STATUS_CHARACTERISTIC_UUID =
        "7d2a0004-6f45-4c2b-9a1e-38a8f5c10001"

    // Android -> ESP32: lệnh điều khiển và cấu hình (Write)
    const val COMMAND_CHARACTERISTIC_UUID =
        "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001"

    // ESP32 -> Android: xác nhận lệnh (Notify)
    const val COMMAND_ACK_CHARACTERISTIC_UUID =
        "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001"

    // Chỉ dùng khi thử nghiệm qua Wi-Fi
    const val ESP32_HTTP_PORT = 80
    const val ESP32_HTTP_BASE_URL = "http://192.168.4.1"
    const val API_SENSOR_LATEST = "/api/v1/sensors/latest"
    const val API_DEVICE_STATUS = "/api/v1/device/status"
    const val API_DEVICE_COMMAND = "/api/v1/device/command"
    const val API_EVENT_STREAM = "/api/v1/events"
}
```

Các UUID trên là UUID riêng của dự án và phải được khai báo giống hệt trong firmware ESP32. Không thay đổi tùy ý sau khi đã thu thập dữ liệu; nếu thay đổi cấu trúc gói, tăng `protocolVersion`.

### 6.3. Biến dữ liệu cảm biến

```kotlin
data class Esp32SensorPacket(
    val protocolVersion: Int,
    val deviceId: String,
    val sequenceNumber: Long,
    val timestampMs: Long,

    val accelXMs2: Float,
    val accelYMs2: Float,
    val accelZMs2: Float,
    val gyroXDps: Float,
    val gyroYDps: Float,
    val gyroZDps: Float,

    val pressurePa: Float?,
    val temperatureC: Float?,
    val altitudeDeltaM: Float?,

    val batteryPercent: Int,
    val batteryVoltageMv: Int?,
    val isCharging: Boolean,
    val sosButtonPressed: Boolean,
    val sensorQuality: Int
)
```

Ý nghĩa và đơn vị:

| Biến API | Kiểu | Ý nghĩa |
|---|---:|---|
| `accelXMs2`, `accelYMs2`, `accelZMs2` | float | Gia tốc ba trục, m/s² |
| `gyroXDps`, `gyroYDps`, `gyroZDps` | float | Tốc độ góc, độ/giây |
| `pressurePa` | float/null | Áp suất, Pa; null nếu không có cảm biến |
| `altitudeDeltaM` | float/null | Độ cao tương đối so với mốc gần nhất, m |
| `batteryPercent` | int | Pin 0–100% |
| `batteryVoltageMv` | int/null | Điện áp pin, mV |
| `isCharging` | boolean | Thiết bị đang sạc |
| `sosButtonPressed` | boolean | Trạng thái nút SOS |
| `sensorQuality` | int | Chất lượng dữ liệu 0–100 |

### 6.4. Biến trạng thái thiết bị

```kotlin
data class Esp32DeviceStatus(
    val protocolVersion: Int,
    val deviceId: String,
    val timestampMs: Long,
    val firmwareVersion: String,
    val uptimeSeconds: Long,
    val batteryPercent: Int,
    val batteryVoltageMv: Int?,
    val isCharging: Boolean,
    val imuStatus: String,
    val barometerStatus: String,
    val gnssStatus: String,
    val bufferUsagePercent: Int,
    val lastErrorCode: String?
)
```

Giá trị trạng thái cảm biến thống nhất: `OK`, `CALIBRATING`, `UNAVAILABLE`, `ERROR`.

### 6.5. Biến sự kiện ưu tiên từ ESP32

```kotlin
data class Esp32EventPacket(
    val protocolVersion: Int,
    val eventId: String,
    val deviceId: String,
    val sequenceNumber: Long,
    val timestampMs: Long,
    val eventType: String,
    val eventSeverity: String,
    val peakAccelerationMs2: Float?,
    val orientationChangeDeg: Float?,
    val altitudeDeltaM: Float?,
    val sosButtonPressed: Boolean,
    val eventConfidence: Int,
    val checksum: String?
)
```

Giá trị `eventType`:

- `IMPACT_DETECTED`
- `FREE_FALL_SUSPECTED`
- `POSTURE_CHANGED`
- `INSTABILITY_DETECTED`
- `INACTIVITY_DETECTED`
- `SOS_PRESSED`
- `SOS_CANCELLED`
- `LOW_BATTERY`
- `SENSOR_ERROR`

Giá trị `eventSeverity`: `INFO`, `WARNING`, `CRITICAL`.

### 6.6. Lệnh Android gửi cho ESP32

```kotlin
data class Esp32Command(
    val protocolVersion: Int,
    val commandId: String,
    val timestampMs: Long,
    val commandType: String,
    val parameters: Map<String, String> = emptyMap()
)
```

Giá trị `commandType`:

- `PING`
- `GET_STATUS`
- `START_STREAM`
- `STOP_STREAM`
- `SET_SAMPLE_RATE`
- `SET_REFERENCE_ALTITUDE`
- `SET_DEVICE_TIME`
- `START_SELF_TEST`
- `TRIGGER_BUZZER`
- `STOP_BUZZER`
- `ACK_EVENT`
- `CANCEL_ALERT`
- `REBOOT_DEVICE`

Ví dụ đặt tần số lấy mẫu:

```json
{
  "protocolVersion": 1,
  "commandId": "cmd-20260914-0001",
  "timestampMs": 1789363200000,
  "commandType": "SET_SAMPLE_RATE",
  "parameters": {
    "sampleRateHz": "50"
  }
}
```

### 6.7. Gói xác nhận lệnh

```kotlin
data class Esp32CommandAck(
    val protocolVersion: Int,
    val commandId: String,
    val deviceId: String,
    val timestampMs: Long,
    val commandStatus: String,
    val errorCode: String?,
    val message: String?
)
```

Giá trị `commandStatus`: `ACCEPTED`, `COMPLETED`, `REJECTED`, `FAILED`.

Android chỉ coi lệnh cấu hình thành công khi nhận `COMPLETED` đúng `commandId`. Nếu hết thời gian chờ, được thử lại tối đa theo cấu hình; lệnh phải an toàn khi nhận lặp.

### 6.8. Ví dụ JSON dữ liệu ESP32 gửi Android

```json
{
  "protocolVersion": 1,
  "deviceId": "FALLSAFE-01A2",
  "sequenceNumber": 18422,
  "timestampMs": 1789363200123,
  "accelXMs2": 0.31,
  "accelYMs2": -1.14,
  "accelZMs2": 9.62,
  "gyroXDps": 2.8,
  "gyroYDps": -4.1,
  "gyroZDps": 0.7,
  "pressurePa": 100842.4,
  "temperatureC": 31.2,
  "altitudeDeltaM": -0.46,
  "batteryPercent": 78,
  "batteryVoltageMv": 3970,
  "isCharging": false,
  "sosButtonPressed": false,
  "sensorQuality": 96
}
```

### 6.9. Tần số và truyền dữ liệu

- IMU dự kiến đọc 50–100 Hz trên ESP32.
- Không nhất thiết gửi JSON từng mẫu ở 100 Hz vì tốn băng thông và pin.
- Giai đoạn mẫu thử có thể gửi 25–50 Hz để dễ kiểm tra.
- Phiên bản tối ưu nên đóng gói nhiều mẫu IMU vào một gói nhị phân, còn trạng thái/sự kiện vẫn dùng cấu trúc rõ ràng.
- Khi nghi ngờ có ngã, ESP32 ưu tiên gửi sự kiện và dữ liệu trước/sau sự kiện từ bộ đệm.
- Android theo dõi `sequenceNumber`, thời gian nhận và RSSI để đánh giá chất lượng kết nối.

---


## Ràng buộc thời gian, ACK và an toàn
- timestampMs: Unix milliseconds; timestampNs điện thoại: monotonic nanoseconds. Đếm ngược dùng monotonic, chỉnh giờ không được kéo dài hạn.
- eventId xác định sự kiện; truyền lại giữ eventId, sequenceNumber, payload. Không khử trùng bằng “nhỏ hơn số lớn nhất”; sự kiện có thể tới muộn.
- ACK GATT ≠ ACK_EVENT ≠ lệnh COMPLETED ≠ người thân nhận. ACCEPTED chưa thành công cấu hình. ACK_EVENT không hủy, STOP_BUZZER chỉ tắt âm.
- Sensor/event có sequenceNumber; Status/ACK không có (mâu thuẫn câu “mỗi gói” Android §6.1 được ghi C02, không tự thêm trường).
- Mất BLE không được reset xác minh, mất Internet không tắt báo tại chỗ, thiếu GPS không chặn cảnh báo.
- Cảm biến không có: null cho trường nullable; IMU/pin lỗi không phát mẫu giả. Pin cũ phải ghi rõ cũ. Chưa từng có pin hợp lệ: SENSOR_ERROR thay vì pin0% giả.
- gnssStatus chỉ phản ánh tình trạng. V1 chưa có gói tọa độ ESP32; vị trí điện thoại dùng latitude/longitude (độ), locationAccuracyM (m), locationTimestampMs (Unix ms), nullable khi không có (Android §9). Không nhét tọa độ mới vào v1.

## Tham số lệnh bổ sung và từ vựng lỗi (chưa duyệt)
Nguồn ESP §8.3,8.5. temperatureC là °C cảm biến khí áp, không thân nhiệt. lastErrorCode đề xuất: IMU_READ_FAILED, BAROMETER_READ_FAILED, BATTERY_READ_FAILED, TIME_NOT_SYNCED, BUFFER_OVERFLOW. Các lớp UI/decoder hai phía cần cập nhật và kiểm thử trước dùng; không coi là từ vựng đã có ở Android.

| `commandType` | Tham số đề xuất để hai nhóm triển khai | Hành vi |
|---|---|---|
| `PING` | Không | Kiểm tra phản hồi |
| `GET_STATUS` | Không | Phát trạng thái hiện tại |
| `START_STREAM` | Không | Bắt đầu truyền mẫu |
| `STOP_STREAM` | Không | Dừng truyền mẫu, vẫn đo và xử lý SOS |
| `SET_SAMPLE_RATE` | `sampleRateHz` | Chấp nhận các mức thực sự hỗ trợ |
| `SET_REFERENCE_ALTITUDE` | Không | Lấy áp suất ổn định hiện tại làm mốc 0 m |
| `SET_DEVICE_TIME` | `unixTimeMs` | Đồng bộ giờ; thuật toán vẫn dùng đồng hồ đơn điệu |
| `START_SELF_TEST` | Không | Kiểm tra có giới hạn thời gian |
| `TRIGGER_BUZZER` | `eventId`, `pattern`, `remainingMs` | Mẫu báo `VERIFYING`, `CRITICAL` hoặc `TEST` |
| `STOP_BUZZER` | `eventId` | Tắt âm, không xóa sự kiện |
| `ACK_EVENT` | `eventId` | Android đã nhận/lưu sự kiện |
| `CANCEL_ALERT` | `eventId`, `reason` | Hủy cảnh báo đang hoạt động đúng ID |
| `REBOOT_DEVICE` | Không | Từ chối nếu đang cảnh báo; ghi nhận trước khởi động lại |

Ngoài `sampleRateHz` đã có ví dụ trong tài liệu Android, các tên tham số trên là đề xuất hoàn thiện phần còn thiếu. Chưa được tuyên bố tương thích thực thi cho đến khi kiểm thử hai phía.

Gói `Esp32CommandAck` giữ nguyên: `protocolVersion`, `commandId`, `deviceId`, `timestampMs`, `commandStatus`, `errorCode`, `message`. Kiểu lần lượt: int, string, string, long, string, string/null, string/null.

`commandStatus`: `ACCEPTED`, `COMPLETED`, `REJECTED`, `FAILED`. Nhận lệnh chưa có nghĩa hoàn thành. Lưu kết quả các `commandId` gần nhất để lệnh lặp trả lại ACK và không lặp tác dụng, đặc biệt với hủy và reboot.


## Bổ sung từ ESP32 — đề xuất, chưa chốt tương thích
### 8.6. Thời gian, thứ tự và truyền lại

Đây là phần bổ sung quy ước vận hành còn thiếu trong v1, cần được Android áp dụng cùng lúc:

- Firmware dùng đồng hồ đơn điệu 64 bit cho cửa sổ đo và đếm ngược; chỉnh giờ không làm kéo dài xác minh.
- Sau kết nối, Android gửi `SET_DEVICE_TIME`. Timestamp không đồng bộ đề xuất dùng `0` và trạng thái `TIME_NOT_SYNCED`; Android dùng giờ nhận kèm nhãn thời gian chưa xác định. SOS vẫn được truyền ngay trước đồng bộ.
- Mẫu chưa có giờ chỉ giữ timestamp đơn điệu nội bộ; không giả đó là Unix time. Khi đồng bộ có thể quy đổi mẫu cùng phiên khởi động từ mốc đồng hồ.
- Số thứ tự dùng chung cho sensor/event. Dành trước từng dải số trong bộ nhớ bền vững; sau reboot bỏ phần dải chưa dùng và tăng sang dải mới, tránh quay về 0 trùng gói cũ. Không ghi flash từng mẫu.
- Khi khôi phục cài đặt làm mất bộ đếm, cần một phiên ghép nối mới và xóa trạng thái khử trùng tương ứng trên Android. Không tái sử dụng ID sự kiện đã phát.
- Gói trạng thái và ACK trong v1 không có `sequenceNumber`; không tự thêm rồi coi là quy ước đã tồn tại.
- Sự kiện gửi lại có thể đến sai thứ tự. Android khử trùng bằng tập ID/số đã thấy, không bỏ mọi gói thấp hơn số lớn nhất.
- Khoảng trống số chỉ xác nhận mất gói sau cửa sổ chờ sắp xếp lại; mẫu sự kiện ưu tiên có thể vượt mẫu thường.

### 8.7. Kích thước gói BLE là điểm phải hoàn thiện trước lập trình tích hợp

Bản Android định nghĩa cấu trúc logic nhưng chưa định nghĩa frame nhị phân, chia mảnh, thứ tự byte và mã checksum. JSON ví dụ có thể lớn hơn tải BLE thực tế; không được giả định mỗi JSON luôn nằm trong một notification.

Phương án cho nguyên mẫu: JSON UTF-8 được chia mảnh theo MTU đã thương lượng, có lớp frame ngoài gồm ID thông điệp, chỉ số mảnh, tổng số mảnh và độ dài toàn bộ. Bộ nhận giới hạn bộ nhớ và thời gian ráp, bỏ thông điệp thiếu mảnh; sự kiện có cơ chế ACK ứng dụng và thử lại. Phải viết đặc tả byte và vector kiểm thử chung trước khi bật truyền này trên Android.

Giữ `checksum=null` ở payload v1 nếu chưa thống nhất cách tính; không tự chọn CRC khác nhau ở hai phía. Không lấy checksum thay xác thực. Giai đoạn tối ưu có thể đóng lô mẫu nhị phân, nhưng phải công bố endian, kiểu số, hệ số đổi đơn vị, biểu diễn null và tăng phiên bản khi thay đổi không tương thích.

### 8.8. HTTP phục vụ kỹ thuật

> **Ghi chú phạm vi (Scope Note - Phase 2):** Toàn bộ đặc tả REST API chính thức (endpoints, schema, envelopes, trạng thái) giữa Android, Backend và ESP32 hiện được chuẩn hóa duy nhất tại [`docs/api-contract.md`](api-contract.md) và hướng dẫn ESP32 tại [`docs/esp32-api.md`](esp32-api.md). Bảng và hằng số bên dưới chỉ phản ánh bản phác thảo thử nghiệm SoftAP ban đầu; không dùng làm đặc tả REST cạnh tranh.

Giữ `ESP32_HTTP_PORT=80`, `ESP32_HTTP_BASE_URL="http://192.168.4.1"` trong chế độ điểm truy cập thử nghiệm.

| Hằng số Android | Đường dẫn | Phương thức đề xuất |
|---|---|---|
| `API_SENSOR_LATEST` | `/api/v1/sensors/latest` | GET |
| `API_DEVICE_STATUS` | `/api/v1/device/status` | GET |
| `API_DEVICE_COMMAND` | `/api/v1/device/command` | POST |
| `API_EVENT_STREAM` | `/api/v1/events` | GET, SSE cho chế độ thử |

Phương thức và cách truyền SSE là đề xuất bổ sung. HTTP không phải đường truyền mặc định của thiết bị đeo; tắt tự động sau phiên kỹ thuật, chỉ mở sau thao tác vật lý và xác thực phù hợp. Không đưa cổng cấu hình ra Internet.


## Điểm phải quyết định trước BLE (IF-002)
1. Frame byte: magic/version ngoài, ID message, index/count, total length, endian; giới hạn kích thước và số message đồng thời. Chưa có giá trị chính thức, chưa bật transport.
2. ACK timeout, số retry, thời gian ráp mảnh, cache dedupe và cửa sổ reorder, reconnect backoff: hai kế hoạch chưa chốt giá trị. IF-002 phải trình bảng cấu hình + vector byte hai phía; không mặc định proposal là approved.
3. sequence dùng chung sensor/event, reservation qua reboot, time=0 trước đồng bộ: ESP §8.6 còn đề xuất; cần kiểm thử Android đi cùng.
4. Ngoài sampleRateHz, command parameters tại ESP §8.5 là đề xuất. Chưa dùng như API đã chốt. checksum=null cho test v1 cho tới khi có thuật toán chung.
5. SOS_CANCELLED thiếu relatedEventId: không phát lại hủy và đoán liên kết; chỉ một cảnh báo cục bộ theo ESP §8.4. Cần phiên bản mới nếu mở rộng.
6. CRITICAL severity của lỗi/pin có được coi là nguy cơ cơ thể? ESP §7.3 cần ánh xạ khẩn, Android §5 không coi mọi va đập là ngã. Cần ma trận theo eventType/severity, không dùng severity đơn lẻ cho chẩn đoán.

## Triển khai lab hiện có (chưa bật transport)
- docs/interface-framing-draft.md: đặc tả byte đề xuất; protocol-lab/ có Kotlin/C++ và golden tests. Đây không phải giao thức triển khai được duyệt.
- Android Esp32PacketDecoder.decodeSensor: JSON đầu vào tối đa4096 ký tự, deviceId1..128 không rỗng; numeric finite, seq/time Long nguyên không âm; percentage0..100. Optional field thiếu hoặc explicit null→null. Duplicate JSON key bị reject; primitive extra fields bỏ qua, object/array nested bị reject. Đây là giới hạn/policy parser nguyên mẫu phải chốt cùng firmware, không tự thêm wire fields. Decode thuần không khử trùng sensor/event; repository/lớp nhận chịu trách nhiệm.

## Dữ liệu kiểm thử chung
Tệp docs/fixtures/protocol-v1.json chứa mẫu sensor chính xác từ cả hai kế hoạch và các biến thể sai/thiếu/trùng. Đây là vector logic JSON, KHÔNG phải frame BLE đã thiết kế. DEC-001 đã chạy sensor decoder qua các vector accept/reject; duplicate sensor giữ nguyên để caller khử trùng. IF-003 bổ sung vector frame byte/reorder/thiếu/timeout/disconnect; ACK mất/reboot/durable dedupe chưa triển khai.
