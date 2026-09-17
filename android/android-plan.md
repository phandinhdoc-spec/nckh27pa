# KẾ HOẠCH PHÁT TRIỂN ỨNG DỤNG ANDROID

## Hệ thống hỗ trợ phát hiện té ngã, choáng váng và gửi cảnh báo khẩn cấp

> **Phạm vi tài liệu:** Chỉ mô tả ứng dụng Android và giao tiếp giữa Android với nhóm thiết bị ESP32. Không trình bày chi tiết mạch điện, sơ đồ chân hoặc mã firmware ESP32.

---

## 1. Mục tiêu

Xây dựng một ứng dụng Android gọn, dễ dùng cho người lớn tuổi nhưng vẫn cung cấp đủ thông tin và quyền điều khiển cho người thân. **Ứng dụng trực tiếp sử dụng các cảm biến có sẵn trên điện thoại** để đo chuyển động, tư thế, áp suất/độ cao và vị trí. Khi có thiết bị đeo ESP32, Android nhận thêm nguồn dữ liệu thứ hai rồi hợp nhất hai nguồn để tăng độ tin cậy. Vì vậy, điện thoại không chỉ là màn hình nhận dữ liệu mà còn là một thiết bị cảm biến độc lập.

Ứng dụng cần xử lý được nhiều tình huống hơn một bộ phát hiện va đập đơn giản:

- Té ngã mạnh và nằm bất động.
- Trượt hoặc ngã từ tư thế ngồi, có độ cao rơi nhỏ.
- Choáng váng, loạng choạng hoặc phải bám vào đồ vật nhưng chưa ngã mạnh.
- Người dùng chủ động bấm nút cầu cứu.
- Thiết bị đeo sắp hết pin, đang sạc, mất kết nối hoặc có lỗi cảm biến.
- GPS yếu trong nhà: dùng vị trí cuối cùng, vị trí điện thoại và thông tin mạng thay vì phụ thuộc hoàn toàn vào GNSS của thiết bị đeo.

Mục tiêu phản hồi là phát hiện nhanh, mở bước xác minh trong khoảng vài giây và bắt đầu quy trình cảnh báo khẩn cấp nếu người dùng không phản hồi.

---

## 2. Nguyên tắc thiết kế tổng thể

### 2.1. Phân chia trách nhiệm

**ESP32 và nhóm cảm biến** chịu trách nhiệm:

- Đọc cảm biến chuyển động, áp suất/độ cao, nút SOS và trạng thái pin.
- Lọc nhiễu cơ bản, đóng gói mẫu dữ liệu và truyền sang Android.
- Phát tín hiệu tức thời khi có sự kiện phần cứng rõ ràng như nút SOS hoặc va đập mạnh.
- Duy trì bộ đệm ngắn để hạn chế mất dữ liệu khi kết nối chập chờn.

**Ứng dụng Android** chịu trách nhiệm:

- Trực tiếp đọc cảm biến của điện thoại: gia tốc kế, con quay hồi chuyển, cảm biến vectơ xoay, khí áp kế nếu có, bộ đếm bước và vị trí.
- Tạo luồng dữ liệu `PhoneSensorPacket` độc lập với ESP32.
- Quản lý kết nối với ESP32.
- Lưu dữ liệu ngắn hạn và nhật ký sự kiện.
- Đồng bộ thời gian và hợp nhất dữ liệu điện thoại với dữ liệu thiết bị đeo.
- Phân tích theo nhiều giai đoạn để giảm báo động giả.
- Hiển thị đếm ngược xác nhận an toàn.
- Gọi điện, nhắn tin hoặc gửi thông báo cho người thân theo cấu hình và quyền được cấp.
- Theo dõi mức pin, trạng thái sạc, chất lượng kết nối và tình trạng cảm biến.

### 2.2. Kiến trúc ưu tiên

- **Nguồn cảm biến thứ nhất:** cảm biến tích hợp trong điện thoại Android; ứng dụng vẫn phát hiện và cảnh báo được khi không có ESP32.
- **Nguồn cảm biến thứ hai:** thiết bị đeo ESP32; bổ sung dữ liệu sát cơ thể và nút SOS vật lý.
- **Giao tiếp chính:** Bluetooth Low Energy (BLE), phù hợp thiết bị đeo và mục tiêu pin 2–3 ngày.
- **Giao tiếp phụ:** Wi‑Fi/HTTP trong giai đoạn thử nghiệm, cấu hình hoặc cập nhật; không duy trì Wi‑Fi liên tục nếu không cần.
- **Phân tích ban đầu:** Luật rõ ràng, có ngưỡng cấu hình và ghi nhật ký để kiểm chứng.
- **Mô hình học máy:** Chỉ bổ sung sau khi đã thu đủ dữ liệu thực nghiệm có nhãn; không để AI đám mây quyết định cảnh báo sống còn.
- **Hoạt động ngoại tuyến:** Phát hiện và báo động tại chỗ phải hoạt động khi mất Internet.

### 2.3. Ba chế độ vận hành

| Chế độ | Nguồn dữ liệu | Cách sử dụng |
|---|---|---|
| `PHONE_ONLY` | Chỉ cảm biến điện thoại | Phiên bản phổ biến, không cần mua thiết bị đeo; độ tin cậy phụ thuộc vị trí đặt điện thoại |
| `HYBRID` | Điện thoại + ESP32 | Chế độ ưu tiên; hai nguồn kiểm chứng lẫn nhau và vẫn duy trì giám sát khi một nguồn tạm gián đoạn |
| `ESP32_ONLY_INPUT` | ESP32 là nguồn chuyển động chính, Android vẫn cung cấp vị trí và cảnh báo | Dùng khi điện thoại đặt trên bàn nhưng người dùng đang đeo ESP32 |

Ứng dụng tự chọn chế độ nhưng phải hiển thị rõ nguồn nào đang hoạt động. `HYBRID` không có nghĩa là cộng máy móc hai điểm nguy cơ; hệ thống phải xét chất lượng, thời điểm và vị trí của từng nguồn.

---

## 3. Thiết kế frontend Android

Frontend phải tạo cảm giác **gọn gàng, yên tâm và dễ thao tác**, không giống một bảng điều khiển kỹ thuật. Người lớn tuổi chỉ cần nhìn thấy trạng thái hiện tại và một nút cầu cứu lớn; thông số chuyên sâu được đặt ở màn hình dành cho người chăm sóc.

### 3.1. Phong cách giao diện

- Một màn hình chỉ có một nhiệm vụ chính.
- Chữ lớn, tương phản cao, khoảng cách nút rộng.
- Không dùng quá nhiều màu; màu thể hiện đúng ý nghĩa:
  - Xanh lá: an toàn, hoạt động bình thường.
  - Vàng/cam: cần chú ý.
  - Đỏ: nguy hiểm hoặc đang gửi cảnh báo.
  - Xám: chưa kết nối hoặc chưa có dữ liệu.
- Trạng thái luôn có cả **màu + biểu tượng + chữ**, không chỉ dựa vào màu.
- Nút quan trọng đủ lớn để bấm bằng một tay.
- Hạn chế nhập liệu; ưu tiên lựa chọn, công tắc và giá trị mặc định hợp lý.
- Hỗ trợ cỡ chữ hệ thống, chế độ sáng/tối và TalkBack.

### 3.2. Thanh điều hướng

Ứng dụng chỉ cần bốn mục chính:

1. **Trang chủ** – trạng thái an toàn, kết nối, pin và nút SOS.
2. **Sự kiện** – lịch sử cảnh báo và kết quả xác minh.
3. **Người thân** – danh sách người nhận cảnh báo, thứ tự ưu tiên.
4. **Cài đặt** – thiết bị, quyền, ngưỡng và kiểm tra hệ thống.

### 3.3. Màn hình Trang chủ

Các thành phần theo thứ tự từ trên xuống:

- Thẻ trạng thái lớn: **An toàn / Cần chú ý / Đang xác minh / Đang cảnh báo**.
- Tên thiết bị đeo và trạng thái BLE.
- Nguồn đang giám sát: **Điện thoại**, **ESP32** hoặc **Cả hai**.
- Trạng thái cảm biến điện thoại và cảnh báo “Điện thoại có thể đang đặt trên bàn” khi dữ liệu cho thấy máy không đi cùng người dùng.
- Pin thiết bị đeo, trạng thái đang sạc và pin điện thoại.
- Dòng thông tin ngắn: “Dữ liệu mới nhất cách đây 2 giây”.
- Nút đỏ **SOS – GIỮ 2 GIÂY** để tránh chạm nhầm.
- Nút **Kiểm tra hệ thống** nhỏ hơn để thử rung, âm thanh, vị trí và liên lạc.

Không đưa biểu đồ gia tốc liên tục lên Trang chủ. Biểu đồ chỉ xuất hiện trong chế độ kiểm thử dành cho giáo viên/học sinh nghiên cứu.

### 3.4. Màn hình xác minh nguy cơ

Khi phát hiện dấu hiệu đáng ngờ, ứng dụng mở giao diện toàn màn hình ngay cả khi máy đang khóa, nếu Android và quyền hệ thống cho phép:

- Tiêu đề lớn: **Bạn có ổn không?**
- Đếm ngược rõ ràng, mặc định 10 giây nhưng cho phép cấu hình trong thử nghiệm.
- Âm báo, rung và đọc bằng giọng nói.
- Hai nút lớn:
  - **TÔI VẪN ỔN** – hủy cảnh báo và ghi nhận báo động giả.
  - **CẦN GIÚP ĐỠ** – gửi cảnh báo ngay, không chờ hết thời gian.
- Có thể nhận lệnh hủy từ nút vật lý trên thiết bị đeo nếu đã thiết kế nút xác nhận.

### 3.5. Màn hình cảnh báo đã gửi

- Hiển thị người đã được liên hệ và kênh liên hệ.
- Hiển thị bản đồ/vị trí gần nhất cùng thời điểm lấy vị trí.
- Có nút gọi số khẩn cấp và gọi người thân ưu tiên.
- Có nút **Tôi đã được hỗ trợ** để kết thúc sự kiện nhưng không xóa nhật ký.

### 3.6. Màn hình Sự kiện

Mỗi sự kiện hiển thị ngắn gọn:

- Thời gian.
- Loại: té ngã, mất thăng bằng, SOS, mất kết nối hoặc pin yếu.
- Mức tin cậy/nguy cơ: thấp, vừa, cao.
- Kết quả: người dùng an toàn, đã gửi cảnh báo, người thân xác nhận hoặc chưa rõ.

Khi mở chi tiết mới hiển thị biểu đồ, dữ liệu cảm biến, vị trí và lý do thuật toán đưa ra quyết định.

### 3.7. Màn hình cấu hình thiết bị

- Quét và ghép nối ESP32.
- Hiển thị mã thiết bị để tránh ghép nhầm.
- Kiểm tra luồng dữ liệu theo thời gian thực.
- Kiểm tra nút SOS, pin, rung/còi và cảm biến.
- Không cho người dùng phổ thông sửa trực tiếp UUID hoặc địa chỉ API; đặt các mục này trong **Chế độ nhà phát triển**.

---

## 4. Backend bên trong ứng dụng Android

Backend cần **rành mạch, tinh giản, dễ kiểm thử**. Mỗi mô-đun làm đúng một nhiệm vụ; tránh tạo quá nhiều tầng trừu tượng hoặc phụ thuộc máy chủ khi chưa cần.

### 4.1. Công nghệ đề xuất

- Kotlin.
- Jetpack Compose cho giao diện.
- Coroutines và Flow cho dữ liệu thời gian thực.
- Room cho nhật ký sự kiện và mẫu dữ liệu cần giữ.
- DataStore cho cấu hình nhỏ.
- Foreground Service để duy trì giám sát BLE đúng quy định Android.
- WorkManager cho công việc trì hoãn như đồng bộ nhật ký khi có mạng.
- Android Location API cho vị trí điện thoại.

### 4.2. Cấu trúc mô-đun tối thiểu

```text
app/
├── ui/                 # Màn hình, thành phần giao diện, trạng thái hiển thị
├── phonesensors/       # Đọc và chuẩn hóa cảm biến tích hợp trong điện thoại
├── device/             # Quét, ghép nối, BLE, HTTP thử nghiệm
├── protocol/           # UUID, tên biến API, mã lệnh, giải mã gói tin
├── fusion/             # Đồng bộ và hợp nhất dữ liệu điện thoại với ESP32
├── detection/          # Luật phát hiện, điểm nguy cơ và máy trạng thái
├── alert/              # Đếm ngược, âm/rung, SMS/cuộc gọi/thông báo
├── location/           # Vị trí điện thoại và vị trí cuối cùng
├── data/               # Room, DataStore, repository
└── diagnostics/        # Kiểm tra cảm biến và xuất dữ liệu nghiên cứu
```

Không đặt thuật toán phát hiện trong Activity, ViewModel hoặc lớp BLE. Luồng dữ liệu chuẩn là:

```text
Điện thoại → PhoneSensorCollector ─┐
                                  ├→ SensorFusionEngine → DetectionEngine
ESP32 → DeviceClient → Decoder ───┘                     → AlertCoordinator
                                                        → UI/Người thân
```

### 4.3. Các lớp chính

| Lớp/Interface | Trách nhiệm duy nhất |
|---|---|
| `Esp32DeviceClient` | Kết nối, ngắt kết nối, nhận và gửi gói BLE |
| `Esp32HttpClient` | Giao tiếp HTTP khi thử nghiệm/cấu hình |
| `Esp32PacketDecoder` | Kiểm tra phiên bản và chuyển gói tin thành đối tượng Kotlin |
| `PhoneSensorCollector` | Đọc cảm biến Android theo tần số phù hợp và chuẩn hóa đơn vị |
| `SensorRepository` | Cung cấp luồng dữ liệu thống nhất cho ứng dụng |
| `SensorFusionEngine` | Căn thời gian, đánh giá chất lượng và hợp nhất hai nguồn |
| `DetectionEngine` | Tính đặc trưng, đối chiếu luật và phát sinh mức nguy cơ |
| `DetectionStateMachine` | Quản lý trình tự bình thường → nghi ngờ → xác minh → cảnh báo |
| `AlertCoordinator` | Âm/rung, đếm ngược và gửi cảnh báo theo thứ tự |
| `LocationProvider` | Lấy vị trí hiện tại hoặc vị trí tốt nhất gần nhất |
| `EventRepository` | Lưu sự kiện, kết quả xác minh và dữ liệu liên quan |

### 4.4. Máy trạng thái phát hiện

| Trạng thái | Ý nghĩa | Điều kiện chuyển chính |
|---|---|---|
| `MONITORING` | Theo dõi bình thường | Có dấu hiệu bất thường → `SUSPECTED` |
| `SUSPECTED` | Thu thập thêm dữ liệu vài giây | Hết nguy cơ → `MONITORING`; đủ bằng chứng → `VERIFYING` |
| `VERIFYING` | Hỏi người dùng và đếm ngược | Người dùng báo ổn → `MONITORING`; cần giúp/hết giờ → `ALERTING` |
| `ALERTING` | Đang gửi cảnh báo | Đã gửi → `AWAITING_HELP` |
| `AWAITING_HELP` | Chờ xác nhận đã được hỗ trợ | Kết thúc sự kiện → `MONITORING` |

Nút SOS được phép chuyển trực tiếp từ mọi trạng thái sang `ALERTING`.

---

## 4.5. Android trực tiếp đo bằng cảm biến điện thoại

### 4.5.1. Cảm biến và thông số sử dụng

| Cảm biến/API Android | Biến chính | Thông số suy ra | Vai trò |
|---|---|---|---|
| Gia tốc kế `TYPE_ACCELEROMETER` | `accelXMs2`, `accelYMs2`, `accelZMs2` | Gia tốc tổng hợp, giảm tải, va đập, mức bất động | Nguồn chính để phát hiện chuyển động bất thường |
| Gia tốc tuyến tính `TYPE_LINEAR_ACCELERATION` | `linearAccelXMs2`, `linearAccelYMs2`, `linearAccelZMs2` | Chuyển động sau khi đã loại gần đúng trọng lực | Bổ sung; không phải điện thoại nào cũng có cảm biến ảo ổn định |
| Con quay `TYPE_GYROSCOPE` | `gyroXDps`, `gyroYDps`, `gyroZDps` | Tốc độ xoay và đổi tư thế nhanh | Phân biệt đặt máy, xoay người và ngã |
| Vectơ xoay `TYPE_ROTATION_VECTOR` | `pitchDeg`, `rollDeg`, `yawDeg` | Hướng và tư thế của điện thoại | Đánh giá thay đổi tư thế trước/sau sự kiện |
| Khí áp kế `TYPE_PRESSURE` | `pressurePa`, `altitudeDeltaM` | Thay đổi độ cao tương đối | Hỗ trợ ước lượng xuống thấp; không dùng một mình để kết luận ngã |
| Bộ đếm/phát hiện bước | `stepCount`, `stepDetected` | Đang đi lại hay đã dừng | Bổ sung ngữ cảnh hoạt động |
| GNSS/vị trí mạng | `latitude`, `longitude`, `locationAccuracyM`, `speedMps` | Vị trí, tốc độ di chuyển ngoài trời | Gửi vị trí và bổ sung ngữ cảnh; không đủ nhanh để đo quãng rơi |

Từ kế không phải nguồn bắt buộc để phát hiện ngã vì dễ bị nhiễu bởi kim loại và môi trường trong nhà. Cảm biến tiệm cận chỉ nên hỗ trợ nhận biết điện thoại trong túi hoặc sát cơ thể trên một số thiết bị; không coi đây là bằng chứng chắc chắn.

### 4.5.2. Gói dữ liệu cảm biến điện thoại

```kotlin
data class PhoneSensorPacket(
    val timestampNs: Long,
    val wallClockTimestampMs: Long,

    val accelXMs2: Float,
    val accelYMs2: Float,
    val accelZMs2: Float,
    val linearAccelXMs2: Float?,
    val linearAccelYMs2: Float?,
    val linearAccelZMs2: Float?,
    val gyroXDps: Float?,
    val gyroYDps: Float?,
    val gyroZDps: Float?,
    val pitchDeg: Float?,
    val rollDeg: Float?,
    val yawDeg: Float?,
    val pressurePa: Float?,
    val altitudeDeltaM: Float?,
    val stepCount: Long?,
    val stepDetected: Boolean?,

    val phoneMotionState: String,
    val phonePlacementConfidence: Int,
    val sensorQuality: Int
)
```

Quy ước:

- `timestampNs` lấy từ đồng hồ đơn điệu của sự kiện cảm biến để tính khoảng thời gian chính xác.
- `wallClockTimestampMs` dùng để ghép với nhật ký và gói ESP32.
- Trường không được phần cứng hỗ trợ phải là `null`, không được tự điền `0`.
- `phoneMotionState`: `MOVING_WITH_USER`, `POSSIBLY_ON_BODY`, `POSSIBLY_STATIONARY`, `UNKNOWN`.
- `phonePlacementConfidence`: độ tin cậy 0–100 rằng điện thoại đang mang theo người.
- `sensorQuality`: chất lượng tổng thể 0–100, xét độ trễ, cảm biến bị thiếu và mẫu bị mất.

### 4.5.3. Đặc trưng Android tự tính

```kotlin
data class PhoneMotionFeatures(
    val windowStartMs: Long,
    val windowEndMs: Long,
    val accelerationMagnitudeMs2: Float,
    val peakAccelerationMs2: Float,
    val minimumAccelerationMs2: Float,
    val angularSpeedDps: Float?,
    val orientationChangeDeg: Float?,
    val altitudeDeltaM: Float?,
    val inactivityDurationMs: Long,
    val instabilityIndex: Float,
    val phoneSourceConfidence: Int
)
```

Gia tốc tổng hợp:

```text
accelerationMagnitudeMs2 = sqrt(
    accelXMs2² + accelYMs2² + accelZMs2²
)
```

Android dùng cửa sổ thời gian ngắn để tìm chuỗi diễn biến, không kết luận từ một mẫu đơn lẻ. Áp suất được lọc và quy đổi thành **độ cao tương đối trong một khoảng ngắn**; không coi đó là độ cao tuyệt đối vì thời tiết và điều hòa có thể làm áp suất trôi.

### 4.5.4. Hợp nhất dữ liệu điện thoại và ESP32

```kotlin
data class FusedSensorFrame(
    val timestampMs: Long,
    val operatingMode: String,
    val phoneFeatures: PhoneMotionFeatures?,
    val wearableFeatures: WearableMotionFeatures?,
    val phoneWeight: Float,
    val wearableWeight: Float,
    val sourcesTimeDifferenceMs: Long?,
    val fusedRiskScore: Int,
    val fusionReasons: List<String>
)
```

Quy tắc hợp nhất:

1. Quy đổi hai nguồn về cùng đơn vị và cùng trục thời gian.
2. Chỉ ghép các sự kiện nằm trong cửa sổ thời gian cho phép, ví dụ ±500 ms; giá trị này phải được kiểm chứng thực nghiệm.
3. Tính `phoneWeight` và `wearableWeight` từ chất lượng dữ liệu, độ trễ, tình trạng mang thiết bị và cảm biến hiện có.
4. Nếu cả hai cùng ghi nhận va đập/đổi tư thế gần nhau, tăng độ tin cậy.
5. Nếu ESP32 cho thấy cơ thể chuyển động nhưng điện thoại nằm yên lâu trên bàn, giảm trọng số điện thoại chứ không phủ nhận ESP32.
6. Nếu điện thoại ghi nhận sự kiện nhưng ESP32 mất kết nối, vẫn tiếp tục quy trình bằng `PHONE_ONLY`.
7. Nếu hai nguồn mâu thuẫn, chuyển sang xác minh người dùng thay vì tự kết luận an toàn.

Ví dụ các `fusionReasons` cần lưu:

- `PHONE_IMPACT_CONFIRMED_BY_ESP32`
- `PHONE_ORIENTATION_CHANGED`
- `ESP32_BODY_MOVEMENT_PHONE_STATIONARY`
- `PHONE_ONLY_NO_WEARABLE_CONNECTION`
- `BAROMETER_HEIGHT_CHANGE_CONFIRMED`
- `SOURCES_DISAGREE_USER_VERIFICATION_REQUIRED`

### 4.5.5. Hạn chế bắt buộc phải thể hiện trong ứng dụng

Cảm biến điện thoại chỉ phản ánh chuyển động cơ thể tốt khi người dùng mang điện thoại trong túi, đeo ở thắt lưng hoặc cầm theo. Điện thoại đặt trên bàn không thể phát hiện chính xác việc người dùng ngã ở nơi khác. Vì vậy:

- Ứng dụng phải nhắc người dùng mang điện thoại theo khi chạy `PHONE_ONLY`.
- Không được quảng bá rằng điện thoại luôn phát hiện ngã dù để xa cơ thể.
- Trước mỗi buổi đo nghiên cứu, phải ghi `phonePlacement`: `TROUSER_POCKET`, `SHIRT_POCKET`, `WAIST_BAG`, `HAND`, `ON_TABLE`, `OTHER`.
- Khi vị trí đặt thay đổi, dữ liệu phải được phân nhóm riêng; không trộn tất cả để đánh giá độ chính xác chung.
- ESP32 đeo ở thắt lưng vẫn là nguồn gần cơ thể và ổn định hơn; Android cung cấp nguồn độc lập, khả năng đối chiếu, vị trí và kênh cảnh báo.

### 4.5.6. Quản lý pin của điện thoại

- Khi màn hình bật hoặc đang xác minh: lấy mẫu nhanh để phân tích chi tiết.
- Khi giám sát bình thường: dùng tốc độ lấy mẫu vừa đủ, xử lý theo lô và tránh giữ CPU thức liên tục nếu không cần.
- Khi có dấu hiệu nghi ngờ: tạm tăng tần số và giữ cửa sổ dữ liệu trước/sau sự kiện.
- Vị trí không lấy liên tục ở độ chính xác cao; chỉ tăng ưu tiên khi có nguy cơ hoặc người dùng yêu cầu.
- Ghi nhận `phoneBatteryPercent`, `isPhoneCharging` và mức tiêu thụ theo giờ để đánh giá thực nghiệm.

---

## 5. Logic nhận biết nguy cơ

### 5.1. Không dùng một ngưỡng duy nhất

Một cú va đập mạnh không đồng nghĩa chắc chắn bị ngã; ngược lại, trượt từ ghế hoặc choáng rồi tựa vào tường có thể không tạo va đập lớn. Do đó cần kết hợp các bằng chứng:

- Gia tốc tổng hợp và khoảng thời gian giảm trọng lực tương đối.
- Đỉnh va đập.
- Tốc độ quay và thay đổi tư thế.
- Thay đổi độ cao tương đối từ cảm biến áp suất, nếu thiết bị có cảm biến này.
- Mức chuyển động sau sự kiện.
- Thời gian nằm hoặc giữ tư thế bất thường.
- Mẫu mất thăng bằng lặp lại.
- Nút SOS và phản hồi của người dùng.

Mỗi đặc trưng trên có thể đến từ điện thoại, ESP32 hoặc cả hai. `DetectionEngine` không được mặc định rằng dữ liệu chuyển động luôn đến từ ESP32.

### 5.2. Các nhóm sự kiện đề xuất

**Ngã mạnh:** có pha rơi/giảm tải, va đập, đổi tư thế và ít chuyển động sau đó.

**Ngã từ tư thế ngồi:** độ cao thay đổi ít hơn nhưng có đổi tư thế nhanh, va đập vừa và bất động sau đó. Không loại bỏ sự kiện chỉ vì quãng rơi ngắn.

**Choáng váng hoặc loạng choạng:** dao động tư thế bất thường, bước đi không ổn định hoặc nhiều lần nghiêng nhanh nhưng không có va đập lớn. Sự kiện này nên tạo mức “cần chú ý” và hỏi thăm, không tự động coi là té ngã chắc chắn.

**Hoạt động bình thường:** ngồi xuống, nằm xuống, nhảy, chạy hoặc tháo thiết bị thường có ngữ cảnh và diễn biến khác; dữ liệu thử nghiệm phải bao gồm các hoạt động này để đo báo động giả.

### 5.3. Điểm nguy cơ minh bạch

`riskScore` nằm trong khoảng 0–100 và được tính từ các thành phần có thể giải thích:

- `impactScore`: mức va đập.
- `orientationScore`: mức thay đổi tư thế.
- `heightDropScore`: mức thay đổi độ cao tương đối.
- `inactivityScore`: mức bất động sau sự kiện.
- `instabilityScore`: mức mất thăng bằng trước/sau sự kiện.
- `signalQualityScore`: chất lượng dữ liệu; chất lượng thấp không được làm tăng độ chắc chắn.

Ngưỡng ban đầu chỉ là giả thuyết kỹ thuật và phải hiệu chỉnh bằng thực nghiệm. Mọi quyết định cần lưu `triggerReasons` để biết hệ thống cảnh báo vì yếu tố nào.

---

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

## 7. Kết hợp vị trí và gửi cảnh báo

Thứ tự chọn vị trí:

1. Vị trí mới, đủ chính xác từ điện thoại.
2. Vị trí GNSS của thiết bị đeo nếu có và tốt hơn.
3. Vị trí cuối cùng kèm thời gian và sai số.
4. Nếu không có tọa độ, vẫn gửi cảnh báo cùng thời điểm, trạng thái kết nối và lời nhắn “chưa xác định được vị trí”.

Không được trì hoãn cảnh báo khẩn cấp chỉ để chờ GPS. Nội dung gửi đi nên gồm:

- Tên người dùng.
- Loại nguy cơ.
- Thời điểm.
- Liên kết bản đồ nếu có.
- Độ chính xác và tuổi của vị trí.
- Pin thiết bị đeo và pin điện thoại.
- Số điện thoại gọi lại.

Ứng dụng phải xin quyền Android theo từng tính năng và giải thích dễ hiểu. Cần kiểm thử riêng giới hạn SMS, cuộc gọi, thông báo, vị trí nền và chạy nền trên từng hãng điện thoại.

---

## 8. Bảo mật và độ tin cậy

- Chỉ kết nối thiết bị đã ghép cặp và lưu định danh.
- Không dựa vào tên BLE để xác thực thiết bị.
- Dữ liệu nhạy cảm lưu cục bộ cần được bảo vệ bằng cơ chế Android phù hợp.
- Không ghi số điện thoại, vị trí đầy đủ hoặc khóa bí mật vào log gỡ lỗi.
- Kiểm tra giới hạn độ dài, kiểu dữ liệu và `protocolVersion` trước khi giải mã.
- Gói sai cấu trúc không được làm ứng dụng dừng.
- Ghi lại mất kết nối, gói bị bỏ, thời gian trễ và lỗi cảm biến.
- Foreground Service phải hiển thị thông báo giám sát rõ ràng; không tìm cách che hoạt động nền.
- Cảnh báo mất kết nối chỉ phát sau một khoảng trễ hợp lý để tránh làm phiền vì nhiễu BLE ngắn.

---

## 9. Dữ liệu cần lưu

Không lưu vô hạn toàn bộ dữ liệu thô. Chia thành ba mức:

- **Bộ đệm thời gian thực:** giữ một khoảng ngắn trước và sau hiện tại.
- **Cửa sổ sự kiện:** lưu dữ liệu quanh sự kiện để phân tích nghiên cứu.
- **Nhật ký dài hạn:** chỉ lưu tóm tắt, kết quả xác minh, lỗi và thống kê.

Mỗi sự kiện Android nên có các biến chính:

```kotlin
data class DetectionEvent(
    val eventId: String,
    val deviceId: String,
    val startedAtMs: Long,
    val endedAtMs: Long?,
    val detectionType: String,
    val riskScore: Int,
    val triggerReasons: List<String>,
    val userResponse: String?,
    val alertStatus: String,
    val latitude: Double?,
    val longitude: Double?,
    val locationAccuracyM: Float?,
    val locationTimestampMs: Long?
)
```

Giá trị `userResponse`: `SAFE`, `NEED_HELP`, `NO_RESPONSE`, `UNKNOWN`.

Giá trị `alertStatus`: `NOT_REQUIRED`, `COUNTDOWN`, `SENDING`, `SENT`, `FAILED`, `ACKNOWLEDGED`.

---

## 10. Kiểm thử bắt buộc

### 10.1. Kiểm thử kỹ thuật

- Chạy hoàn toàn bằng cảm biến điện thoại khi chưa ghép nối ESP32.
- Kiểm tra từng cảm biến Android có/không có trên nhiều mẫu điện thoại.
- Kiểm tra sai lệch thời gian giữa điện thoại và ESP32.
- Chuyển tự động giữa `PHONE_ONLY`, `HYBRID` và `ESP32_ONLY_INPUT`.
- Kết nối lại sau khi ra khỏi phạm vi BLE.
- Android khóa màn hình, tắt màn hình và ứng dụng chạy nền.
- ESP32 khởi động lại giữa phiên.
- Gói trùng, mất gói, sai phiên bản và sai kiểu dữ liệu.
- Mất Internet nhưng BLE còn hoạt động.
- GPS yếu hoặc không có GPS.
- Pin yếu, đang sạc và rút sạc.
- Nút SOS, hủy cảnh báo và hết thời gian xác minh.

### 10.2. Kiểm thử hoạt động

- Đi bộ, chạy chậm, lên xuống cầu thang.
- Ngồi xuống nhanh, nằm xuống và trở mình.
- Làm rơi thiết bị khi không đeo.
- Trượt khỏi ghế ở độ cao thấp trong điều kiện mô phỏng an toàn.
- Mất thăng bằng rồi bám vào bàn/tường.
- Ngã mô phỏng có đệm và người giám sát.

Mỗi kịch bản phải lặp lại ít nhất theo ba cấu hình: chỉ điện thoại, chỉ dữ liệu đeo ESP32 và kết hợp hai nguồn. Với điện thoại, cần lặp theo vị trí đặt như túi quần, túi áo, cầm tay và đặt trên bàn để chứng minh rõ giới hạn của phương án.

Không thử ngã thật với người cao tuổi. Các thử nghiệm va chạm phải dùng hình nộm, túi cát hoặc người trẻ có bảo hộ và giám sát phù hợp.

### 10.3. Chỉ số đánh giá

- Tỷ lệ phát hiện đúng.
- Tỷ lệ bỏ sót.
- Số báo động giả trên mỗi giờ/ngày sử dụng.
- Thời gian từ sự kiện đến màn hình xác minh.
- Thời gian từ sự kiện đến khi gửi cảnh báo.
- Tỷ lệ kết nối BLE ổn định.
- Mức tiêu thụ pin của điện thoại và thiết bị đeo.

---

## 11. Lộ trình triển khai Android

### Giai đoạn 1 – Ứng dụng khung

- Dựng giao diện bốn mục.
- Tích hợp gia tốc kế, con quay, vectơ xoay, khí áp kế và vị trí có sẵn trên điện thoại.
- Tạo `PhoneSensorPacket`, bộ đệm và màn hình kiểm tra cảm biến điện thoại.
- Tạo dữ liệu giả và hoàn thiện máy trạng thái.
- Mô phỏng đếm ngược, SOS và nhật ký sự kiện.

### Giai đoạn 2 – Kết nối ESP32

- Quét, ghép nối, tự kết nối lại BLE.
- Nhận `Esp32SensorPacket`, `Esp32EventPacket`, `Esp32DeviceStatus`.
- Gửi `Esp32Command` và kiểm tra `Esp32CommandAck`.
- Hiển thị chế độ chẩn đoán dữ liệu trực tiếp.
- Xây dựng `SensorFusionEngine` và chuyển đổi ổn định giữa ba chế độ vận hành.

### Giai đoạn 3 – Phát hiện theo luật

- Tính đặc trưng chuyển động và điểm nguy cơ.
- Phân biệt ngã mạnh, ngã thấp, mất thăng bằng và hoạt động thường ngày.
- Lưu cửa sổ dữ liệu quanh sự kiện và lý do kích hoạt.

### Giai đoạn 4 – Cảnh báo thực tế

- Hoàn thiện quyền Android, vị trí, người liên hệ và nội dung cảnh báo.
- Thử khi khóa màn hình, mất mạng và chạy nền lâu.
- Kiểm tra trên nhiều hãng điện thoại Android.

### Giai đoạn 5 – Thực nghiệm và hiệu chỉnh

- Xây kịch bản thử có nhãn.
- Đo độ nhạy, báo động giả, độ trễ và pin.
- Chỉnh ngưỡng dựa trên dữ liệu, không chỉnh theo cảm giác.
- Chỉ cân nhắc mô hình học máy khi bộ dữ liệu đủ tin cậy.

---

## 12. Tiêu chí hoàn thành phiên bản đầu

Phiên bản Android đầu tiên được xem là đạt khi:

- Giao diện chính có thể sử dụng mà không cần hiểu thông số cảm biến.
- Ứng dụng đo và tạo đặc trưng trực tiếp từ cảm biến điện thoại khi không có ESP32.
- Trang chủ hiển thị đúng nguồn đang hoạt động và cảnh báo khi điện thoại có thể không được mang theo.
- BLE tự kết nối lại và báo rõ khi mất thiết bị.
- Nhận đúng dữ liệu theo tên biến và UUID đã thống nhất.
- Nút SOS từ Android và ESP32 đều kích hoạt quy trình cảnh báo.
- Luồng `MONITORING → SUSPECTED → VERIFYING → ALERTING` hoạt động ổn định.
- Người dùng có thể hủy báo động giả trong thời gian xác minh.
- Sự kiện lưu được nguyên nhân, thời gian, phản hồi và vị trí khả dụng.
- Phát hiện vẫn chạy khi tắt màn hình và không phụ thuộc Internet.
- Có bộ kiểm thử cho bộ giải mã gói tin, máy trạng thái và các luật cốt lõi.

---

## 13. Kết luận phương án

Ứng dụng Android vừa là **thiết bị đo độc lập bằng cảm biến của điện thoại**, vừa là trung tâm điều phối dữ liệu từ ESP32. Android đánh giá nhiều dấu hiệu, hợp nhất hai nguồn khi có thể, hỏi lại người dùng và gửi cảnh báo. Thiết kế frontend được giữ gọn, rõ trạng thái và thuận tiện cho người lớn tuổi; phần chuyên sâu được tách riêng cho người chăm sóc và nhóm nghiên cứu. Backend chỉ gồm các mô-đun cần thiết, có luồng dữ liệu một chiều và máy trạng thái dễ kiểm thử.

Chuẩn API trong tài liệu này là hợp đồng chung giữa nhóm Android và nhóm ESP32. Hai nhóm phải dùng đúng tên biến, kiểu dữ liệu, đơn vị, UUID và `protocolVersion`; mọi thay đổi cần được ghi lại trước khi cập nhật mã nguồn hai phía.
