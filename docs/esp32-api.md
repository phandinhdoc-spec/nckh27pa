# Hướng dẫn Tích hợp REST API dành cho ESP32 Firmware

> **Trạng thái:** TÀI LIỆU KỸ THUẬT PHẦN CỨNG & FIRMWARE  
> **Nguồn contract gốc:** [`docs/api-contract.md`](api-contract.md)  
> **Giao thức gói tin BLE:** Xem [`docs/interface-contract.md`](interface-contract.md)  
> **Đối tượng:** Kỹ sư Firmware ESP32 (Arduino / C++17)

---

## 1. Phân biệt Hai Kênh Giao tiếp (BLE vs Wi-Fi REST)

Thiết bị đeo ESP32 FallSafe hỗ trợ hai kênh giao tiếp vật lý riêng biệt:

1. **Kênh BLE GATT (Bluetooth Low Energy):**
   - Dùng khi kết nối trực tiếp với điện thoại Android ở khoảng cách gần.
   - Định dạng: JSON đóng gói / binary framing chia mảnh MTU.
   - Chi tiết UUID, Characteristic và gói tin xem tại [`docs/interface-contract.md`](interface-contract.md).
2. **Kênh Wi-Fi HTTP REST (Mạng cục bộ / Internet):**
   - Dùng khi ESP32 kết nối Wi-Fi phát dữ liệu trực tiếp lên Backend Monolith (trạm y tế, gia đình) hoặc chạy ở chế độ SoftAP kỹ thuật (`192.168.4.1`).
   - Định dạng: Chuẩn RESTful JSON v1 (`/api/v1`).
   - **Tài liệu này hướng dẫn chi tiết kênh Wi-Fi HTTP REST** để đội ngũ firmware lập trình độc lập mà không cần đọc mã nguồn Android.

---

## 2. Quy ước Kết nối & Header HTTP

- **Base URL:** `http://<backend-host>:<port>/api/v1` (Mặc định trong môi trường test: `http://192.168.1.100:3000/api/v1` hoặc SoftAP: `http://192.168.4.1/api/v1`).
- **Headers bắt buộc cho mọi request:**
  ```http
  Content-Type: application/json; charset=utf-8
  X-Device-Id: FALLSAFE-01A2
  ```
- **Cấu trúc Envelope phản hồi:**
  - Thành công: `{"success": true, "data": { ... }, "timestamp": <unixMs>}`
  - Lỗi: `{"success": false, "error": {"code": "...", "message": "..."}, "timestamp": <unixMs>}`

---

## 3. Ánh xạ Máy trạng thái ESP32 C++ với API Backend

Trong mã nguồn firmware (`esp/core/local_alert.h`), lớp `fallsafe::LocalAlertStateMachine` quản lý trạng thái tại chỗ. Bảng ánh xạ trạng thái và hành vi còi/nút:

| Trạng thái ESP32 (`fallsafe::State`) | `alertState` trên API Backend | Trạng thái Còi (`buzzer`) | Hành vi Firmware |
|---|---|---|---|
| `MONITORING` | `MONITORING` | Tắt (`false`) | Đọc IMU liên tục, gửi định kỳ sensor hoặc heartbeat. |
| `SUSPECTED` | `SUSPECTED` | Tắt (`false`) | Phát hiện va chạm/nghiêng; chuẩn bị xác nhận. Gửi `POST /api/v1/events`. |
| `VERIFYING` | `VERIFYING` | **BẬT (`true`)** | Bật còi cảnh báo tại chỗ (bíp ngắt quãng). Đếm ngược 10.000 ms. Chờ nút bấm an toàn. |
| `LOCAL_ALERTING` | `ALERTING` / `AWAITING_HELP` | **BẬT liên tục (`true`)** | Hết 10s hoặc bấm SOS. Còi kêu to liên tục. Gửi `POST /api/v1/alerts/sos`. |
| `DEGRADED` | `MONITORING` (kèm `SENSOR_ERROR`) | Tắt (`false`) | Lỗi phần cứng IMU/khí áp; gửi mã lỗi lên `POST /api/v1/devices/{deviceId}/status`. |

---

## 4. Chi tiết các Endpoint ESP32 Gọi lên Backend

### 4.1. Gửi Nhịp tim & Trạng thái Thiết bị (Device Heartbeat)
- **Mục đích:** Báo cáo định kỳ mức pin, điện áp, tình trạng cảm biến (chu kỳ: mỗi 60 giây khi bình thường).
- **Phương thức:** `POST /api/v1/devices/{deviceId}/status`
- **Request Body:**
```json
{
  "firmwareVersion": "1.0.0",
  "timestampMs": 1789363200123,
  "uptimeSeconds": 14200,
  "batteryPercent": 78,
  "batteryVoltageMv": 3970,
  "isCharging": false,
  "imuStatus": "OK",
  "barometerStatus": "OK",
  "gnssStatus": "UNAVAILABLE",
  "bufferUsagePercent": 5,
  "lastErrorCode": null
}
```
*Đặc tả trường:*
- `firmwareVersion`: Chuỗi phiên bản (bắt buộc, ví dụ: `"1.0.0"`).
- `timestampMs`: Unix epoch ms (nếu chưa đồng bộ giờ, gửi `0`).
- `uptimeSeconds`: Thời gian chạy từ lúc khởi động lại (`esp_timer_get_time() / 1000000ULL`).
- `batteryPercent`: Số nguyên 0..100 (bắt buộc).
- `batteryVoltageMv`: Điện áp pin theo mV (số nguyên hoặc `null` nếu mạch không đo ADC).
- `isCharging`: `true` nếu chân sạc phát hiện mức logic cao, ngược lại `false`.
- `imuStatus`: `"OK"`, `"CALIBRATING"`, `"UNAVAILABLE"`, `"ERROR"`.
- `barometerStatus`: `"OK"`, `"UNAVAILABLE"`, `"ERROR"`.
- `gnssStatus`: `"UNAVAILABLE"` (v1 chưa gắn GPS).
- `bufferUsagePercent`: 0..100 (mức chiếm dụng hàng đợi RAM).
- `lastErrorCode`: Chuỗi mã lỗi gần nhất hoặc `null`.
- **Response mẫu (200 OK):**
```json
{
  "success": true,
  "data": {
    "deviceId": "FALLSAFE-01A2",
    "recorded": true,
    "serverTimeMs": 1789363200123
  },
  "timestamp": 1789363200123
}
```
*Lưu ý:* ESP32 có thể tận dụng `serverTimeMs` trong phản hồi để đồng bộ lại đồng hồ thời gian thực (`struct timeval`).

---

### 4.2. Gửi Dữ liệu Cảm biến IMU & Áp suất (Sensor Ingestion)
- **Mục đích:** Truyền dữ liệu cảm biến đo được lên server.
- **Phương thức:** `POST /api/v1/sensors/ingest`
- **Request Body:** Cùng mô hình `SensorReading` canonical; metadata và ba trục gia tốc vẫn bắt buộc, không được `null`.
```json
{
  "sensorSource": "ESP32",
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
*Đặc tả đơn vị & kiểu dữ liệu:*
- `accelXMs2`, `accelYMs2`, `accelZMs2`: float, đơn vị $m/s^2$ (trọng trường Trái Đất xấp xỉ $9.81 m/s^2$).
- `gyroXDps`, `gyroYDps`, `gyroZDps`: `number | null`, đơn vị độ/giây (dps), theo duy nhất `SensorReading` tại `docs/api-contract.md §3.2`. Cả ba trường bắt buộc xuất hiện: ESP32 gửi ba số khi cảm biến khả dụng như ví dụ trên; nếu không có/không đọc được con quay hồi chuyển, gửi cả ba `null`. PHONE không có gyroscope cũng dùng quy tắc này. Không bỏ trục, không trộn số với `null`, không thay dữ liệu thiếu bằng 0.
- `pressurePa`: float hoặc `null` nếu không có cảm biến BMP280/SPL06 (Pascal).
- `temperatureC`: float hoặc `null` (độ C từ cảm biến môi trường).
- `altitudeDeltaM`: float hoặc `null` (độ cao chênh lệch tính theo công thức khí áp so với mốc khởi động).
- `sequenceNumber`: Số nguyên tăng dần không âm (64-bit hoặc 32-bit tăng liên tục).
- `sensorQuality`: Số nguyên 0..100 đánh giá chất lượng tín hiệu.
- **Response mẫu (200 OK):**
```json
{
  "success": true,
  "data": {
    "deviceId": "FALLSAFE-01A2",
    "sequenceNumber": 18422,
    "stored": true
  },
  "timestamp": 1789363200123
}
```

---

### 4.3. Phát Sự kiện Nghi ngờ Ngã / Va đập Mạnh (Event Ingestion)
- **Mục đích:** Gửi ngay khi thuật toán phát hiện va đập vượt ngưỡng ($|a| \ge 25 m/s^2$) kèm giai đoạn nằm yên, chuyển ESP32 sang `VERIFYING` và bật còi đếm ngược 10 giây.
- **Phương thức:** `POST /api/v1/events`
- **Request Body:**
```json
{
  "eventId": "evt-esp-18423",
  "deviceId": "FALLSAFE-01A2",
  "userId": "user-01",
  "sequenceNumber": 18423,
  "timestampMs": 1789363200123,
  "eventType": "IMPACT_DETECTED",
  "severity": "CRITICAL",
  "alertState": "VERIFYING",
  "sensorSource": "ESP32",
  "peakAccelerationMs2": 28.5,
  "orientationChangeDeg": 72.0,
  "altitudeDeltaM": -0.85,
  "sosButtonPressed": false,
  "confidencePercent": 95
}
```
- **Response mẫu (201 Created):**
```json
{
  "success": true,
  "data": {
    "eventId": "evt-esp-18423",
    "alertState": "VERIFYING",
    "active": true,
    "watchdogDeadlineMs": 1789363215123
  },
  "timestamp": 1789363200123
}
```

---

### 4.4. Kích hoạt Cấp cứu Khẩn cấp (SOS Trigger)
- **Mục đích:** Gửi khi người dùng nhấn giữ nút SOS vật lý trên bo mạch, hoặc khi hết 10 giây đếm ngược mà không có ai bấm nút hủy.
- **Phương thức:** `POST /api/v1/alerts/sos`
- **Request Body:**
```json
{
  "deviceId": "FALLSAFE-01A2",
  "userId": "user-01",
  "eventId": "evt-esp-18423",
  "timestampMs": 1789363210123,
  "triggerSource": "COUNTDOWN_TIMEOUT",
  "response": "NO_RESPONSE"
}
```
*(Nếu là do nhấn giữ nút vật lý, gửi `"triggerSource": "MANUAL_HARDWARE_BUTTON"`, `"response": "NEED_HELP"`)*.
- **Response mẫu (200 OK):**
```json
{
  "success": true,
  "data": {
    "eventId": "evt-esp-18423",
    "alertState": "AWAITING_HELP",
    "dispatchStatus": "RECORDED",
    "eligibleContactsCount": 2,
    "recordedAtMs": 1789363210150
  },
  "timestamp": 1789363210150
}
```

---

### 4.5. Hủy Cảnh báo An toàn khi Người dùng Nhấn Nút Hủy (Safe Cancel)
- **Mục đích:** Khi đang trong thời gian đếm ngược 10s (còi đang kêu), người dùng nhấn giữ nút hủy an toàn (Safe button) trong 2 giây. Firmware tắt còi và gửi bản tin hủy lên server.
- **Phương thức:** `POST /api/v1/alerts/cancel`
- **Request Body:**
```json
{
  "deviceId": "FALLSAFE-01A2",
  "eventId": "evt-esp-18423",
  "timestampMs": 1789363204200,
  "reason": "SAFE",
  "note": "Nút Safe trên ESP32 được giữ 2 giây"
}
```
- **Response mẫu (200 OK):**
```json
{
  "success": true,
  "data": {
    "eventId": "evt-esp-18423",
    "alertState": "MONITORING",
    "outcome": "CANCELLED_SAFE",
    "cancelledAtMs": 1789363204220
  },
  "timestamp": 1789363204220
}
```

---

### 4.6. Kiểm tra Trạng thái Cảnh báo từ Server (Alert Polling)
- **Mục đích:** Định kỳ (ví dụ mỗi 5 giây) khi đang ở trạng thái `AWAITING_HELP`, ESP32 có thể poll endpoint này để biết người thân đã xác nhận (`caregiverAcknowledged = true`) nhằm chuyển đổi hiệu ứng âm thanh/đèn LED thông báo cho người già yên tâm.
- **Phương thức:** `GET /api/v1/alerts/active?deviceId=FALLSAFE-01A2`
- **Response mẫu (200 OK):**
```json
{
  "success": true,
  "data": {
    "deviceId": "FALLSAFE-01A2",
    "alertState": "AWAITING_HELP",
    "activeEventId": "evt-esp-18423",
    "remainingMs": 0,
    "response": "NO_RESPONSE",
    "caregiverAcknowledged": true,
    "lastUpdatedMs": 1789363225000
  },
  "timestamp": 1789363225100
}
```

---

## 5. Xử lý Mất kết nối Mạng (Offline Resilience) & Bộ đệm RAM

1. **Nguyên tắc an toàn tối cao:**
   - **Mất mạng Wi-Fi KHÔNG ĐƯỢC làm dừng máy trạng thái hoặc còi báo động tại chỗ.**
   - Bộ đếm 10 giây và còi buzzer vận hành hoàn toàn độc lập qua ngắt timer phần cứng hoặc `millis()`.
2. **Hàng đợi lưu tạm trong RAM (Ring Buffer):**
   - Giữ tối đa 16 sự kiện khẩn cấp (`SafetyEvent`) trong bộ nhớ RAM tĩnh để không phân mảnh heap.
   - Khi Wi-Fi rớt, lưu các sự kiện `IMPACT_DETECTED`, `SOS_PRESSED`, `SOS_CANCELLED` vào buffer.
   - Khi Wi-Fi có lại, gửi toàn bộ sự kiện theo thứ tự FIFO trước khi tiếp tục gửi dữ liệu cảm biến định kỳ.
3. **Quản lý Số thứ tự (`sequenceNumber`):**
   - Sử dụng một biến 64-bit tăng dần trong RAM RTC hoặc phân vùng NVS (đặt trước dải số 1000 số/lần ghi flash để chống mòn bộ nhớ). Không ghi flash từng mẫu cảm biến.

---

## 6. Tham chiếu Hợp đồng Chuẩn
Để biết thêm chi tiết về định nghĩa lỗi, quy tắc định danh và kiến trúc tổng thể, xem tài liệu gốc:
- [Đặc tả REST API Chuẩn (v1)](api-contract.md)
- [Bản đồ Kiến trúc Toàn hệ thống](../.ai/architecture.md)
- [Quy ước Giao tiếp BLE GATT v1](interface-contract.md)
