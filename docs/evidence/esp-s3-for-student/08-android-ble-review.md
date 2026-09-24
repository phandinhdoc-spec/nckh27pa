# Báo cáo điều tra & Đánh giá mã nguồn quét BLE Android (FallSafe)

- **Ngày thực hiện:** 2026-09-25
- **Tệp báo cáo:** `docs/evidence/esp-s3-for-student/08-android-ble-review.md`
- **Phạm vi:** Điều tra mã nguồn quét BLE trên ứng dụng Android (`android/app/**`), đối chiếu với hành vi phần cứng ESP32-S3 (quảng cáo `FALLSAFE-4D4D`, service UUID `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001`).
- **Tính chất nhiệm vụ:** READ-ONLY (Chỉ đọc, rà soát và báo cáo, tuyệt đối không chỉnh sửa mã nguồn ứng dụng).

---

## 1. Bảng đối chiếu & Đánh giá 5 Giả thuyết kỹ thuật

| STT | Giả thuyết kỹ thuật | Bằng chứng trong mã nguồn (Đường dẫn tệp + Số dòng + Trích dẫn code) | KẾT LUẬN |
| :--- | :--- | :--- | :---: |
| **A1** | **Quyền hạn & Cờ `neverForLocation`:**<br>`BLUETOOTH_SCAN` có kèm `android:usesPermissionFlags="neverForLocation"` không? Trên Android 12+, nếu KHÔNG có cờ này thì hệ thống đòi quyền `ACCESS_FINE_LOCATION` và DỊCH VỤ VỊ TRÍ phải BẬT, nếu không `onScanResult` trả về rỗng không báo lỗi. | **1. `android/app/src/main/AndroidManifest.xml` (Dòng 18-19):**<br>```xml<br><uses-permission android:name="android.permission.BLUETOOTH_SCAN" /><br><uses-permission android:name="android.permission.BLUETOOTH_CONNECT" /><br>```<br>→ Hoàn toàn **KHÔNG có** thuộc tính `android:usesPermissionFlags="neverForLocation"`.<br><br>**2. `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` (Dòng 125-138):**<br>```kotlin<br>fun requestBluetoothPermissions() {<br>    if (Build.VERSION.SDK_INT >= 31) {<br>        val permissions = mutableListOf<String>()<br>        if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {<br>            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)<br>        }<br>        if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {<br>            permissions.add(Manifest.permission.BLUETOOTH_SCAN)<br>        }<br>        if (permissions.isNotEmpty()) {<br>            bluetoothPermissionRequest.launch(permissions.toTypedArray())<br>        }<br>    }<br>}<br>```<br>→ Hàm chỉ yêu cầu `BLUETOOTH_CONNECT` và `BLUETOOTH_SCAN`, không hề kiểm tra hay xin quyền `ACCESS_FINE_LOCATION` phục vụ quét BLE.<br><br>**Cơ chế Android 12+ (API 31+):** Khi thiếu `neverForLocation`, Android OS coi quét BLE là thao tác định vị vật lý. Bắt buộc: (a) Đã cấp quyền `ACCESS_FINE_LOCATION`, VÀ (b) Công tắc **Vị trí (GPS / Location Services)** trên điện thoại phải đang **BẬT**. Nếu Vị trí tắt, hệ điều hành âm thầm chặn (`silent drop`) toàn bộ kết quả quét, `onScanResult` không nhận được gì và không có lỗi nào được báo ra. | **ĐÚNG** |
| **A2** | **Bộ lọc quét (`ScanFilter`):**<br>Có khớp đúng UUID `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001` và tiền tố `FALLSAFE-` không? Có so khớp tên cũ nào không? | **1. `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/BleGattUuids.kt` (Dòng 6-7):**<br>```kotlin<br>object BleGattUuids {<br>    val SERVICE_UUID: UUID = UUID.fromString("7d2a0001-6f45-4c2b-9a1e-38a8f5c10001")<br>```<br><br>**2. `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/BleTestScreen.kt` (Dòng 187-190):**<br>```kotlin<br>val filter = ScanFilter.Builder()<br>    .setServiceUuid(ParcelUuid(BleGattUuids.SERVICE_UUID))<br>    .build()<br>```<br><br>**3. `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/FallSafeBleClient.kt` (Dòng 405-408):**<br>```kotlin<br>val filter = ScanFilter.Builder()<br>    .setServiceUuid(ParcelUuid(BleGattUuids.SERVICE_UUID))<br>    .build()<br>```<br>→ Bộ lọc `ScanFilter` chỉ lọc duy nhất theo `serviceUuid` `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001` (khớp chính xác 100% với firmware ESP32 `esp-s3/esp-s3.ino:1548`).<br>→ **Hoàn toàn KHÔNG lọc theo tên thiết bị hay tiền tố `FALLSAFE-`** (không gọi `.setDeviceName(...)`), và không có bất kỳ logic so khớp tên cũ nào gây loại trừ thiết bị. Ở `BleTestScreen.kt:159`, `dev.name` chỉ lấy để hiển thị lên thẻ UI. | **SAI**<br>(ScanFilter không bị sai và không lọc theo tên) |
| **A3** | **Phụ thuộc cache BLE điện thoại:**<br>Có phụ thuộc cache BLE của máy / chỉ quét lại sau khi bật-tắt Bluetooth không? | **1. `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/BleTestScreen.kt` (Dòng 94, 183):**<br>```kotlin<br>val discoveredDevices = remember { mutableStateListOf<DiscoveredBleDevice>() }<br>...<br>val startDeviceScan: () -> Unit = {<br>    discoveredDevices.clear()<br>```<br>→ Mã nguồn app tự xoá danh sách hiển thị khi bắt đầu quét mới, không tự lưu cache scan giữa các lần mở.<br><br>**2. `android/app/src/main/java/vn/nckh27pa/fallsafe/bluetooth/FallSafeBleClient.kt` (Dòng 620-626):**<br>```kotlin<br>private fun safeRefresh(gatt: BluetoothGatt) {<br>    try {<br>        val method = gatt.javaClass.getMethod("refresh")<br>        method.invoke(gatt)<br>    } catch (_: Exception) {}<br>}<br>```<br>→ Hàm này chỉ dùng để xoá cache bảng GATT khi gặp lỗi kết nối (ví dụ status 133 sau khi kết nối), không ảnh hưởng đến việc quét (`startScan`).<br><br>**3. Tầng OS Android:** Ở tầng hệ điều hành, daemon Bluetooth (`bt_stack`) có thể bị treo hoặc giữ tham chiếu thiết bị cũ nếu firmware nạp lại liên tục. Tuy nhiên đây là hành vi của OS, mã app không có logic nào ép buộc phải bật-tắt Bluetooth mới cho quét. | **SAI**<br>(Về phía mã app)<br>---<br>**KHÔNG XÁC ĐỊNH**<br>(Về phía OS Bluetooth stack) |
| **A4** | **Khởi động scan & Vòng đời:**<br>Scan chạy ở đâu (màn hình/luồng nào), có chạy TRƯỚC khi quyền được cấp, hoặc chỉ chạy 1 lần rồi dừng không? | **1. Màn hình khởi động:**<br>BLE scan KHÔNG chạy tự động trong `MainActivity.onCreate()`, `onResume()`, `HomeScreen`, hay `MonitoringService`. Chỉ chạy duy nhất tại `BleTestScreen.kt` khi người dùng bấm nút thủ công "Quét thiết bị" (`BleTestScreen.kt:289-294`).<br><br>**2. Race Condition - Chạy TRƯỚC khi cấp quyền:**<br>`BleTestScreen.kt` (Dòng 181-194):<br>```kotlin<br>val startDeviceScan: () -> Unit = {<br>    onRequestPermissions?.invoke()<br>    discoveredDevices.clear()<br>    val scanner = bluetoothAdapter?.bluetoothLeScanner<br>    if (scanner != null && bluetoothAdapter.isEnabled) {<br>        try {<br>            ...<br>            scanner.startScan(listOf(filter), settings, scanCallback)<br>```<br>→ `onRequestPermissions?.invoke()` bắn Intent xin quyền hệ thống bất đồng bộ, nhưng mã bên dưới **chạy ngay lập tức** `scanner.startScan(...)` mà không đợi người dùng xác nhận cấp quyền. Nếu quyền chưa có, lệnh quét văng lỗi hoặc bị bỏ qua ngay lúc đó.<br>Đồng thời tại `MainActivity.kt` (Dòng 53-55):<br>```kotlin<br>private val bluetoothPermissionRequest = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {<br>    // Bluetooth permissions handled<br>}<br>```<br>→ Khi người dùng bấm "Cho phép" ở hộp thoại hệ thống, callback để trống, **KHÔNG tự động kích hoạt quét lại**. Người dùng phải bấm nút quét lần thứ hai mới thực sự quét được.<br><br>**3. Quét một lần rồi dừng (Timeout 10s):**<br>`BleTestScreen.kt` (Dòng 195-202):<br>```kotlin<br>handler.postDelayed({<br>    if (isManualScanning) {<br>        try { scanner.stopScan(scanCallback) } catch (_: Exception) {}<br>        isManualScanning = false<br>    }<br>}, 10000L)<br>```<br>→ Quét chỉ chạy đúng 10 giây rồi dừng hẳn (`stopScan`), không tự động quét lại định kỳ. | **ĐÚNG** |
| **A5** | **Giới hạn tần suất scan (5 lần/30s) & Quản lý `stopScan`:**<br>Có bị giới hạn tần suất scan của Android (5 lần/30s) hoặc thiếu `stopScan`/khởi động lại gây treo scan không? | **1. Giới hạn tần suất 5 lần / 30 giây của Android:**<br>Từ Android 7.0+ (API 24+), Android áp dụng cơ chế *BLE Scan Throttling*: nếu ứng dụng gọi `startScan()` quá 5 lần trong 30 giây, hệ thống sẽ âm thầm chặn (`silently drop`) toàn bộ kết quả quét cho đến khi hết 30 giây.<br>Tại `BleTestScreen.kt` (Dòng 288-303), nút bấm chuyển đổi giữa "Quét thiết bị" và "Dừng quét" **hoàn toàn không có debounce hay rate limiting**. Nếu người dùng bấm thử/dừng liên tục > 5 lần trong 30s, Android OS sẽ khoá kết quả quét.<br><br>**2. Quản lý `stopScan`:**<br>App có gọi `stopScan()` ở timeout 10s (`BleTestScreen.kt:198`), khi bấm dừng (`line 214`), trong `DisposableEffect.onDispose` (`line 221`), và trước khi kết nối (`line 364`). Tuy nhiên, có **2 implementation quét riêng biệt** không đồng bộ: một trong `BleTestScreen.kt` (dòng 154-202) và một trong `FallSafeBleClient.kt` (dòng 391-443). Dù `FallSafeBleClient.startScan()` hiện chưa được UI gọi tới, sự tồn tại song song 2 scanner/callback tiềm ẩn rủi ro lệch trạng thái. | **ĐÚNG**<br>(Về nguy cơ Scan Throttling 5 lần/30s)<br>---<br>**KHÔNG XÁC ĐỊNH**<br>(Về việc thiếu `stopScan` gây treo hoàn toàn) |

---

## 2. Kết luận: Nguyên nhân khả dĩ nhất giải thích CẢ HAI lần (Thấy và Không thấy)

Dựa trên bằng chứng thực nghiệm thu thập từ phần cứng ESP32-S3, công cụ quét Mac CoreBluetooth (`docs/evidence/esp-s3-for-student/00-baseline/ket-luan.md`) và mã nguồn ứng dụng Android:

### A. Lần "Không thấy" (Trước đó):
Sự cố không dò thấy trước đây được giải thích hoàn toàn bởi sự kết hợp của 2 nguyên nhân độc lập:
1. **Nguyên nhân phần cứng/firmware (Đã xác minh bằng log thực tế):**
   - Khi cắm board nạp firmware hoặc khi cổng Serial bị Arduino IDE Serial Monitor giữ (`/dev/cu.usbmodem1101`), tín hiệu RTS/DTR làm ESP32 rơi vào chế độ nạp bootloader: `boot:0x23 (DOWNLOAD(USB/UART0))` / `waiting for download`.
   - Ở chế độ này, CPU ESP32 dừng chạy ứng dụng, radio Bluetooth không được khởi tạo và **hoàn toàn không phát bất kỳ gói tin quảng cáo nào**. Cả máy Mac và điện thoại Android quét cùng lúc đều không thể thấy thiết bị.
2. **Nguyên nhân phần mềm Android (Giải thích trường hợp chip đã chạy nhưng app vẫn không thấy):**
   - **Thiếu cờ `neverForLocation` trong `AndroidManifest.xml` (A1):** Điện thoại chạy Android 12+ bắt buộc phải BẬT "Dịch vụ Vị trí" (Location toggle trên thanh trạng thái). Nếu lúc thử nghiệm trước đó điện thoại đang tắt GPS/Vị trí, hệ điều hành Android chặn toàn bộ kết quả quét của app mà không báo lỗi.
   - **Hiện tượng Race Condition khi cấp quyền lần đầu (A4):** Người dùng bấm "Quét thiết bị", app gọi `startScan` trước khi hộp thoại quyền được cấp, dẫn đến lần quét đó thất bại ngay.
   - **Cơ chế timeout 10 giây (A4):** ESP32 cần khoảng 3–5 giây sau reset để hoàn tất khởi tạo cảm biến I2C trước khi bắt đầu quảng cáo BLE. Nếu bấm quét trên điện thoại trước khi ESP32 kịp phát quảng cáo, sau 10 giây app tự dừng quét (`stopScan`), dẫn đến kết quả rỗng.

### B. Lần "Thấy" (Nghiệm thu thành công lúc 01:40 ngày 2026-09-25):
- **Phần cứng ESP32-S3:** Board đã được khởi động bình thường (`boot:0x13 (SPI_FAST_FLASH_BOOT)`), chạy firmware chính thức `esp-s3.ino`, phát gói tin quảng cáo chứa Service UUID `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001` (CoreBluetooth trên Mac thu được RSSI -52 đến -59 dBm).
- **Phía điện thoại Android:**
  - Chủ dự án **bật Hotspot cá nhân `Pdmq`**: Trên hệ điều hành Android, việc kích hoạt tính năng Mobile Hotspot (Phát Wi-Fi) kích hoạt ngầm hoặc đòi hỏi Dịch vụ Vị trí (Location Services) phải hoạt động (để quản lý kênh phát sóng Wi-Fi theo quy chuẩn vùng địa lý).
  - Khi Dịch vụ Vị trí đã được bật + Quyền đã được cấp đầy đủ + ESP32 đang phát quảng cáo ổn định: Bộ lọc `ScanFilter` với UUID `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001` trong `BleTestScreen.kt` khớp chính xác 100% và phát hiện ngay thiết bị `FALLSAFE-4D4D`.

---

## 3. Rủi ro còn tồn tại & Đề xuất giải pháp (Đề xuất kỹ thuật, KHÔNG tự sửa mã nguồn)

> [!WARNING]
> Dưới đây là các đề xuất kiến trúc và mã nguồn nhằm khắc phục triệt để các rủi ro đã phát hiện. Theo quy định READ-ONLY, các đề xuất này được lập ra để chủ dự án xem xét, **tuyệt đối không tự động chỉnh sửa vào thư mục `android/app/**`**.

### Rủi ro 1: Quét BLE phụ thuộc vào công tắc Bật/Tắt Vị trí hệ thống (Nghiêm trọng)
- **Hiện trạng:** `AndroidManifest.xml` (dòng 18) khai báo `<uses-permission android:name="android.permission.BLUETOOTH_SCAN" />` mà không có cờ `neverForLocation`. Nếu người dùng tắt GPS để tiết kiệm pin, app sẽ lập tức mất khả năng dò tìm ESP32.
- **Đề xuất khắc phục:**
  Cập nhật thẻ quyền trong `android/app/src/main/AndroidManifest.xml`:
  ```xml
  <uses-permission
      android:name="android.permission.BLUETOOTH_SCAN"
      android:usesPermissionFlags="neverForLocation"
      tools:targetApi="s" />
  ```
  *(Lưu ý: App vẫn giữ thẻ `ACCESS_FINE_LOCATION` riêng biệt phục vụ định vị khẩn cấp cho tính năng SOS, hai mục đích này hoàn toàn độc lập).*

### Rủi ro 2: Lỗi bất đồng bộ khi xin quyền Bluetooth lần đầu (Race Condition)
- **Hiện trạng:** `BleTestScreen.kt` (dòng 182-194) gọi `onRequestPermissions?.invoke()` và ngay lập tức gọi `scanner.startScan()`. Trong khi đó `bluetoothPermissionRequest` ở `MainActivity.kt` (dòng 53-55) không có logic gọi quét lại khi quyền được cấp thành công.
- **Đề xuất khắc phục:**
  - Trước khi gọi `startScan`, kiểm tra `context.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PERMISSION_GRANTED`.
  - Nếu chưa có quyền: Chỉ kích hoạt yêu cầu quyền và thông báo "Vui lòng cấp quyền Bluetooth để quét thiết bị".
  - Trong callback `bluetoothPermissionRequest` của `MainActivity`, gửi tín hiệu (Event/State) để giao diện tự động khởi chạy lại lệnh quét nếu người dùng vừa cấp quyền thành công.

### Rủi ro 3: Bị hệ điều hành Android chặn quét do bấm nút liên tục (Scan Throttling 5 lần / 30s)
- **Hiện trạng:** Nút "Quét thiết bị" / "Dừng quét" trong `BleTestScreen.kt` (dòng 289-301) cho phép người dùng nhấn liên tục không giới hạn.
- **Đề xuất khắc phục:**
  - Thêm cơ chế Debounce / Cooldown cho nút bấm: Sau khi nhấn "Quét thiết bị", làm mờ (disable) nút trong tối thiểu 3-5 giây.
  - Hiển thị thanh tiến trình đếm ngược thời gian quét 10 giây rõ ràng để người dùng không nôn nóng bấm lại nhiều lần.

### Rủi ro 4: Phân mảnh kiến trúc quét BLE (Dual Scanner Implementation)
- **Hiện trạng:** Trong dự án tồn tại 2 nơi triển khai quét:
  1. `BleTestScreen.kt` (dòng 154-202): Tự tạo scanner, tự quản lý `scanCallback` và danh sách thiết bị.
  2. `FallSafeBleClient.kt` (dòng 391-443): Có hàm `startScan()`, `stopScan()`, `scanCallback` riêng biệt và cơ chế kết nối tự động.
- **Đề xuất khắc phục:**
  - Hợp nhất logic quét về duy nhất một nơi là `FallSafeBleClient`.
  - Màn hình `BleTestScreen` chỉ đóng vai trò View, quan sát luồng dữ liệu `bleClient.discoveredDevices` và `bleClient.connectionState`.

---

## 4. Danh sách các bước kiểm chứng Chủ dự án có thể thực hiện trên điện thoại thật

Chủ dự án có thể dùng điện thoại thật (đã cài đặt app FallSafe) và board ESP32-S3 đang chạy để kiểm chứng thực tế các kết luận trên:

### Bước 1: Kiểm chứng ảnh hưởng của Dịch vụ Vị trí (Giả thuyết A1)
1. Giữ board ESP32-S3 cắm nguồn, hoạt động bình thường (đèn sáng, đang phát quảng cáo `FALLSAFE-4D4D`).
2. Trên điện thoại Android: Vuốt mở thanh cài đặt nhanh (Quick Settings), **TẮT mục "Vị trí" (Location / GPS)**.
3. Mở app FallSafe > Chuyển sang tab **Cài đặt** > Nhấn **MỞ BẢNG ĐIỀU KHIỂN & KIỂM THỬ BLE**.
4. Nhấn nút **Quét thiết bị**.
   - *Kết quả quan sát kỳ vọng:* App hiển thị "Đang quét thiết bị có UUID..." nhưng danh sách thiết bị hoàn toàn **TRỐNG**, không tìm thấy ESP32 (do Android OS chặn vì thiếu cờ `neverForLocation`).
5. Vuốt mở thanh cài đặt nhanh, **BẬT lại mục "Vị trí" (Location / GPS)**.
6. Nhấn nút **Quét thiết bị** một lần nữa.
   - *Kết quả quan sát kỳ vọng:* Thiết bị `FALLSAFE-4D4D` **xuất hiện ngay lập tức** trong danh sách kèm địa chỉ MAC và chỉ số RSSI.

### Bước 2: Kiểm chứng hiện tượng Race Condition khi cấp quyền lần đầu (Giả thuyết A4)
1. Trên điện thoại: Vào **Cài đặt máy** > **Ứng dụng** > **FallSafe** > **Quyền** > Chọn mục **Thiết bị ở gần (Nearby Devices / Bluetooth)** > Chọn **Không cho phép (Don't allow)**.
2. Mở lại app FallSafe > Vào màn hình **Kiểm thử BLE**.
3. Nhấn nút **Quét thiết bị**.
   - *Kết quả quan sát kỳ vọng:* Hộp thoại hệ thống hỏi "Cho phép FallSafe tìm, kết nối... các thiết bị ở gần?" xuất hiện.
4. Nhấn **Cho phép (Allow)** trên hộp thoại hệ thống.
   - *Kết quả quan sát kỳ vọng:* Sau khi nhấn Cho phép, app **vẫn không tìm thấy thiết bị** vì lệnh quét trước đó đã thất bại ngay lập tức khi quyền chưa sẵn sàng.
5. Nhấn nút **Quét thiết bị** lần thứ hai.
   - *Kết quả quan sát kỳ vọng:* Lúc này thiết bị `FALLSAFE-4D4D` mới được tìm thấy và hiển thị lên danh sách.

### Bước 3: Kiểm chứng cơ chế giới hạn quét của Android (Giả thuyết A5)
1. Trong màn hình Kiểm thử BLE, nhấn **Quét thiết bị** rồi nhấn ngay **Dừng quét**, lặp lại liên tục thao tác này khoảng 6–7 lần trong vòng 15 giây.
2. Nhấn **Quét thiết bị** lần tiếp theo và chờ.
   - *Kết quả quan sát kỳ vọng:* App không tìm thấy bất kỳ thiết bị nào dù ESP32 đang phát sóng cạnh bên, do hệ điều hành Android đã kích hoạt cơ chế phạt tiết kiệm pin (Scan Throttling). Sau khi chờ khoảng 30–45 giây không thao tác, bấm quét lại thì thiết bị mới xuất hiện bình thường.

### Bước 4: Kiểm chứng xoá bộ nhớ đệm Bluetooth hệ thống (Nếu nghi ngờ OS đơ cache - Giả thuyết A3)
1. Vào **Cài đặt** trên điện thoại > **Ứng dụng** > Chọn menu dấu 3 chấm góc trên chọn **Hiển thị ứng dụng hệ thống**.
2. Tìm ứng dụng tên **Bluetooth** (hoặc **Bộ nhớ chia sẻ Bluetooth**).
3. Chọn **Lưu trữ (Storage)** > Nhấn **Xoá bộ nhớ đệm (Clear Cache)**.
4. Tắt Bluetooth trên máy và bật lại. Thao tác này giúp giải phóng hoàn toàn các socket BLE bị treo ở tầng hệ điều hành Android nếu trước đó đã kết nối nhiều lần với ESP32 mà chưa ngắt kết nối sạch sẽ.
