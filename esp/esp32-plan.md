# KẾ HOẠCH PHÁT TRIỂN THIẾT BỊ ESP32

## Thiết bị đeo hỗ trợ phát hiện té ngã, mất thăng bằng và cảnh báo khẩn cấp

**Dự án:** nckh27pa  
**Ngày tổng hợp:** 14/09/2026  
**Tài liệu đối chiếu:** `android-plan.md`, phiên bản 2.  
**Phạm vi:** Phần cứng, trải nghiệm sử dụng thiết bị, firmware ESP32, giao tiếp với Android, thử nghiệm và lộ trình. Đây là kế hoạch triển khai, chưa phải báo cáo về một thiết bị đã được kiểm chứng.

## 1. Mục tiêu và những nguyên tắc đã thống nhất

Thiết kế thiết bị nhỏ gọn đeo ở thắt lưng, đo chuyển động sát cơ thể, bổ sung thay đổi độ cao tương đối, có nút SOS vật lý và hiển thị pin. ESP32 đo liên tục, xử lý cơ bản, gửi dữ liệu cho Android và duy trì báo động tại chỗ khi mất kết nối.

Android đồng thời sử dụng cảm biến điện thoại, hợp nhất hai nguồn khi thích hợp và điều phối liên hệ người thân. Không coi điện thoại chỉ là màn hình nhận dữ liệu ESP32.

Các yêu cầu cốt lõi:

- Xét cả ngã mạnh, trượt khỏi ghế, ngã thấp và mất thăng bằng không có va đập lớn.
- Không bắt người bị nạn bấm nút để cảnh báo được gửi. Nút xác nhận an toàn chỉ để hủy báo động giả; không phản hồi sẽ dẫn đến cảnh báo tự động khi đã vào bước xác minh nguy cơ.
- SOS và nguy cơ khẩn cấp không phải chờ hết đếm ngược.
- Không chờ GPS mới cảnh báo.
- Hướng đến pin 48–72 giờ, nhưng phải đo trên toàn bộ thiết bị mới kết luận đạt.
- Dùng BLE làm kết nối chính; Wi-Fi chỉ bật phục vụ thử nghiệm, cấu hình hoặc cập nhật.
- Không gọi kết quả cảm biến là chẩn đoán đột quỵ. Thiết bị phát hiện dấu hiệu vận động bất thường; có thể bỏ sót tình huống mất ý thức mà không có thay đổi vận động rõ.

## 2. Phạm vi phiên bản đầu và hướng mở rộng

| Hạng mục | Phiên bản đầu | Hướng mở rộng |
|---|---|---|
| ESP32, IMU, nút SOS, còi/rung, pin | Bắt buộc | Tối ưu kích thước và năng lượng |
| Khí áp kế | Tích hợp khi có linh kiện; thiết bị vẫn đo chuyển động khi thiếu | So sánh hiệu quả có/không có khí áp kế |
| BLE đến Android | Kết nối chính | Tối ưu truyền theo lô |
| Phát hiện cơ bản tại ESP32 | Luật đơn giản, lý do rõ ràng | Hiệu chỉnh theo dữ liệu nghiên cứu |
| Phân tích hợp nhất hai nguồn | Android thực hiện | Chỉ bổ sung mô hình sau khi có dữ liệu |
| Vị trí gửi cảnh báo | Android cung cấp | GNSS u-blox M10 trên thiết bị |
| Liên hệ người thân | Qua Android và các kênh đã cấu hình | Modem 4G để hoạt động độc lập |
| Wi-Fi/HTTP | Chế độ kỹ thuật có thời hạn | Cập nhật có kiểm tra và khôi phục |

**Phân biệt hai cấu hình:** Bản ESP32 + BLE có thể đo và kêu/rung khi không có điện thoại, nhưng không tự gửi cảnh báo từ xa nếu không có đường truyền độc lập. Bản có modem 4G là nhánh mở rộng cần nguồn, SIM, anten và giao thức máy chủ riêng. Không ghi bản BLE là thiết bị cứu hộ độc lập hoàn toàn.

## 3. Thiết kế phần cứng

### 3.1. Các khối chức năng

| Khối | Nhiệm vụ | Giao tiếp dự kiến |
|---|---|---|
| Vi điều khiển ESP32 có BLE | Thu thập, xử lý, quản lý trạng thái và truyền dữ liệu | BLE, I²C/SPI, GPIO, UART |
| IMU gồm gia tốc kế và con quay | Gia tốc ba trục, tốc độ góc, hỗ trợ tư thế và bất động | I²C hoặc SPI; ngắt dữ liệu nếu có |
| Khí áp kế | Áp suất và thay đổi độ cao tương đối | I²C hoặc SPI |
| Bộ đo dung lượng pin | Ước lượng phần trăm và điện áp pin | I²C |
| Mạch sạc và bảo vệ pin | Sạc, bảo vệ, cấp nguồn phù hợp | Chân trạng thái sạc và nguồn |
| Nút SOS và nút xác nhận | Cầu cứu và xác nhận an toàn | GPIO có chống dội |
| Còi, motor rung, đèn trạng thái | Phản hồi tại chỗ | GPIO/PWM qua mạch điều khiển phù hợp |
| Dải LED mức pin | Cho người dùng xem nhanh năng lượng còn lại | GPIO hoặc IC điều khiển |
| GNSS tùy chọn | Tọa độ ngoài trời và chất lượng định vị | UART |
| Modem 4G tùy chọn | Đường truyền cảnh báo độc lập | UART và nguồn riêng đáp ứng tải xung |

### 3.2. Chọn ESP32

Bo vi điều khiển cho sản phẩm đích chính thức (**TARGET_PRODUCT**) được chọn là **ESP32-S3 (Super Mini)** tích hợp Wi-Fi 2,4 GHz và Bluetooth 5 (LE) theo [datasheet Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf), đáp ứng yêu cầu kích thước nhỏ gọn để đeo ở thắt lưng. Trong giai đoạn thử nghiệm thuật toán hiện tại, bộ test (**TEST_RIG**) sử dụng bo **ESP32-WROOM-32** sẵn có để kiểm tra sớm logic phát hiện ngã và thu thập dữ liệu qua cổng nối tiếp.

Trước khi cố định sơ đồ chân, ghi rõ mã module, mã bo, dung lượng bộ nhớ, sơ đồ nguồn và chân đã bị ngoại vi trên bo sử dụng. Tuyệt đối không áp dụng một bảng GPIO cho mọi loại ESP32. Đặc biệt, sơ đồ chân của ESP32-S3 Super Mini không được dùng cho ESP32-WROOM-32 (xem mục 3.6). Khả năng tiết kiệm điện của chip không đại diện cho dòng tiêu thụ của cả bo phát triển.

### 3.3. IMU và khí áp kế không thay thế nhau

- IMU là nguồn chuyển động chính: Bo sản phẩm đích (**TARGET_PRODUCT**) sử dụng **MPU6050** (GY-521); trong khi bộ test thuật toán hiện tại (**TEST_RIG**) sử dụng **MPU9250** (tận dụng linh kiện sẵn có để kiểm tra logic và thu dữ liệu; MPU9250 không phải cảm biến sản phẩm cuối).
- Khí áp kế thực tế được tích hợp trên phần cứng hiện nay là **GY-63 / MS5611-01BA03** (sử dụng cho cả node thử nghiệm và bộ test). Khí áp kế BMP390 trước đây là hướng đã trao đổi, nay chỉ còn là phương án thay thế tiềm năng trong tương lai (cùng BMP388 hoặc DPS310), không phải lựa chọn chính của đợt triển khai hiện tại.
- Không dùng MS5611 hay BMP390/DPS310 để thay chức năng gia tốc kế và con quay của MPU.
- Khi thay linh kiện, driver phải đưa ra cùng đơn vị chuẩn; ghi lại mã cảm biến, dải đo, tần số thực, bộ lọc và kết quả hiệu chuẩn.
- Với IMU thay thế, yêu cầu có cả gia tốc và tốc độ góc, đáp ứng 50–100 Hz, có tài liệu rõ và driver kiểm chứng được. Dải đo phải đủ tránh bão hòa trong các thử nghiệm va chạm; ghi cờ bão hòa thay vì xem số bị cắt là đỉnh thật.

BMP390 đo áp suất tuyệt đối, hỗ trợ theo dõi độ cao (phương án dự phòng tương lai). Bosch công bố độ chính xác tương đối điển hình ±0,03 hPa, tương đương khoảng ±25 cm trong điều kiện chỉ định; mức nhiễu rất thấp được công bố ở cấu hình băng thông thấp nhất. Không suy diễn các số này thành bảo đảm đo chính xác quãng rơi vài cm lúc ngã. Xem [thông số BMP390 của Bosch](https://www.bosch-sensortec.com/en/products/environmental-sensors/pressure-sensors/bmp390).

### 3.4. Pin, sạc và hiển thị năng lượng

MAX17048 là ứng viên đã trao đổi để ước lượng dung lượng pin Li-ion một cell. Đây là bộ đo pin, không phải IC sạc hoặc mạch bảo vệ. Xem [tài liệu Analog Devices](https://www.analog.com/en/products/max17048.html).

- Dùng pin có bảo vệ và mạch sạc phù hợp; nếu vừa hoạt động vừa sạc, thiết kế đường cấp nguồn hỗ trợ tình huống này.
- Lấy `isCharging` từ trạng thái mạch sạc; không suy ra chỉ từ việc cắm USB hoặc thấy điện áp tăng.
- LED mức năng lượng sáng ngắn khi bấm xem, lúc đổi trạng thái sạc hoặc khi pin yếu. Không để toàn bộ LED sáng liên tục.
- Đề xuất thử cảnh báo pin yếu ở 20%, mức rất thấp ở 10%, có độ trễ và ngưỡng phục hồi để tránh báo lặp. Đây là cấu hình thử, chưa phải ngưỡng đã kiểm chứng.
- Còi/motor đi qua transistor hoặc driver phù hợp; không cấp tải trực tiếp bằng chân GPIO nếu vượt khả năng chân.
- Phát hiện sụt áp, ghi nguyên nhân khởi động lại và ưu tiên duy trì đo/cảnh báo khi pin thấp.

### 3.5. Quy ước tên chân, chưa gán số GPIO

| Hằng số | Chức năng |
|---|---|
| `PIN_I2C_SDA`, `PIN_I2C_SCL` | Bus cảm biến |
| `PIN_IMU_INT` | Ngắt báo mẫu/chuyển động |
| `PIN_SOS_BUTTON`, `PIN_SAFE_BUTTON` | Hai nút vật lý |
| `PIN_BUZZER`, `PIN_VIBRATION` | Báo động |
| `PIN_STATUS_LED` | Trạng thái |
| `PIN_BATTERY_LED_1` … `PIN_BATTERY_LED_4` | Dải mức pin |
| `PIN_CHARGER_STATUS`, `PIN_USB_PRESENT` | Sạc và có nguồn ngoài |
| `PIN_GNSS_RX`, `PIN_GNSS_TX` | GNSS tùy chọn |
| `PIN_MODEM_RX`, `PIN_MODEM_TX`, `PIN_MODEM_PWRKEY` | 4G tùy chọn |

Chỉ gán chân sau khi kiểm tra sơ đồ bo, chân khởi động, USB, flash/PSRAM, điện áp logic và xung đột địa chỉ I²C. Sơ đồ nguồn phải hoàn thành trước khi gắn pin hoặc modem.

### 3.6. Hai cấu hình phần cứng: sản phẩm đích và bộ test

Dự án phân định rõ ràng hai cấu hình phần cứng phục vụ hai mục đích khác nhau:

| Tiêu chí | Cấu hình sản phẩm đích (TARGET_PRODUCT) | Cấu hình bộ test thử nghiệm (TEST_RIG) |
|---|---|---|
| **Mục đích** | Thiết bị đeo thắt lưng hoàn chỉnh, tiết kiệm điện, kết nối BLE tới Android | Kiểm thử sớm thuật toán ngã, thu thập dữ liệu, tinh chỉnh ngưỡng qua Serial |
| **Vi điều khiển** | ESP32-S3 (Super Mini) | ESP32-WROOM-32 (NodeMCU-32S / Dev Module) |
| **Cảm biến IMU** | MPU6050 (GY-521), WHO_AM_I = `0x68` | MPU9250 (GY-9250), WHO_AM_I = `0x71` (không dùng AK8963) |
| **Cảm biến khí áp** | GY-63 / MS5611-01BA03 | GY-63 / MS5611-01BA03 |
| **Giao tiếp I2C** | 2 bus độc lập: Bus 0 (SDA=7, SCL=6), Bus 1 (SDA=3, SCL=2) | 1 bus chung an toàn: SDA=GPIO21, SCL=GPIO22 |
| **Năng lượng & BLE** | Tối ưu pin 48–72h, BLE GATT Server hoạt động liên tục | Cấp nguồn qua cáp USB, không bật BLE/Wi-Fi, chưa tối ưu pin |
| **Giao diện người dùng** | Nút SOS, nút an toàn, còi buzzer, LED trạng thái/pin | Cổng nối tiếp Serial Monitor (chế độ HUMAN và chế độ CSV) |

> **CẢNH BÁO AN TOÀN PHẦN CỨNG — TUYỆT ĐỐI KHÔNG TRỘN LẪN:**
> 1. **Chân I2C:** Trên ESP32-WROOM-32, các chân GPIO 6 đến 11 được nối trực tiếp với chip SPI Flash tích hợp bên trong module. Gán GPIO 6 hoặc 7 làm I2C trên WROOM-32 sẽ làm chip gặp lỗi bộ nhớ (Crash / Panic / Reboot loop) ngay lập tức. Sơ đồ chân của ESP32-S3 Super Mini tuyệt đối không được sao chép sang ESP32-WROOM-32.
> 2. **Cảm biến IMU:** MPU9250 chỉ là giải pháp tạm thời cho bộ test do linh kiện sẵn có; không phải cảm biến của sản phẩm đích. Driver MPU9250 dùng địa chỉ WHO_AM_I `0x71` và dải đo `±16g` / `±2000 dps` để tránh bão hòa khi thử va đập.
> 3. **Từ kế AK8963:** Khối từ kế tích hợp bên trong MPU9250 không được kích hoạt vì nhiễu từ trường trong nhà và thiết bị đeo rất lớn, không cần thiết cho phát hiện ngã và gây nghẽn bus I2C 100 Hz.

## 4. Trải nghiệm sử dụng gọn và thuận tiện

ESP32 không cần một frontend nhiều màn hình. Giao diện của thiết bị là nút, đèn, rung và âm thanh; cấu hình chi tiết thực hiện trên Android.

- Vỏ bo góc, kẹp thắt lưng chắc và có dấu chỉ chiều đeo để tư thế cảm biến nhất quán.
- Nút SOS lớn, dễ tìm bằng tay; đề xuất giữ 2 giây, phản hồi rung ngay khi được nhận.
- Nút xác nhận an toàn riêng, dễ phân biệt bằng hình dạng; chỉ có tác dụng hủy khi đang có cảnh báo. Có thể dùng nút này xem pin khi bình thường.
- Không dùng cùng thao tác SOS để bật/tắt nguồn, tránh vô tình tắt lúc cần giúp.
- Không yêu cầu người dùng thực hiện chuỗi thao tác phức tạp khi đang choáng.
- Cửa thông áp có giải pháp hạn chế nước và bụi; không bọc kín khí áp kế rồi kỳ vọng đo độ cao nhanh. Kiểm tra độ trễ sau khi lắp vỏ.
- Tách anten khỏi vùng bị pin/kim loại che chắn theo thiết kế module; thử BLE khi đeo thực tế.

| Tình huống | Phản hồi đề xuất |
|---|---|
| Đo bình thường | Đèn xanh nháy ngắn, thưa |
| Mất Android kéo dài | Đèn vàng theo nhịp riêng, rung ngắn một lần; không kêu liên tục |
| Đang xác minh | Rung và còi theo nhịp nhanh dần |
| Khẩn cấp/SOS | Âm và rung rõ, LED đỏ |
| Pin yếu | Dải pin và tín hiệu nhắc có giới hạn |
| Đang sạc | Chỉ báo sạc riêng, không đồng nghĩa đã kết nối Android |
| Cảm biến chuyển động lỗi | Tín hiệu lỗi khác cảnh báo ngã |

Màu phải đi cùng nhịp/rung hoặc ký hiệu; không dựa riêng vào màu. Không phát tín hiệu “người thân đã nhận” chỉ vì một gói BLE đã được xác nhận.

## 5. Firmware rành mạch và tinh giản

### 5.1. Cách tổ chức

Đề xuất dùng ESP-IDF và C/C++ cho bản firmware chính; khóa phiên bản SDK và thư viện sau khi chạy được mẫu cơ bản. Nếu nhóm đã có mã Arduino, có thể dùng để thử linh kiện trước nhưng giữ nguyên hợp đồng dữ liệu.

| Mô-đun | Nhiệm vụ duy nhất |
|---|---|
| `board` | Chân, nguồn, cấu hình bo |
| `sensors` | Driver IMU, khí áp, pin; chuẩn hóa đơn vị |
| `features` | Lọc cơ bản, đặc trưng trong cửa sổ thời gian |
| `detection` | Luật dự phòng và trạng thái cảnh báo cục bộ |
| `protocol` | UUID, kiểu dữ liệu, đóng/giải gói, kiểm tra lệnh |
| `transport` | BLE; HTTP kỹ thuật tùy chọn |
| `interaction` | Nút, còi, rung, đèn |
| `storage` | Cấu hình, bộ đệm, hàng đợi sự kiện |
| `diagnostics` | Tự kiểm tra, lỗi và số liệu vận hành |

Luồng thực hiện: đọc cảm biến → chuẩn hóa và ghi bộ đệm → tính đặc trưng → đánh giá cơ bản → phát sự kiện và cập nhật phản hồi; bộ truyền lấy gói từ hàng đợi riêng.

Không đặt toàn bộ xử lý trong callback BLE. Không ghi flash hoặc chờ mạng trong tác vụ lấy mẫu. Khởi đầu chỉ cần tác vụ lấy mẫu, tác vụ xử lý/điều khiển và tác vụ truyền; không tạo một tác vụ cho từng đèn hay từng trường dữ liệu.

### 5.2. Tần số và bộ đệm đề xuất

| Thành phần | Cấu hình khởi đầu | Lưu ý |
|---|---:|---|
| IMU | 50 Hz; thử 100 Hz khi đánh giá | Lấy timestamp lúc thu mẫu |
| Khí áp kế | 25 Hz nếu cấu hình cảm biến đáp ứng | Không nhân bản mẫu cũ thành mẫu mới |
| Pin | Mỗi 10–30 giây | Thay đổi sạc báo ngay |
| Trạng thái thiết bị | Mỗi 5 giây và khi thay đổi | Không lấn hàng đợi sự kiện |
| BLE dữ liệu | Mẫu chuẩn hoặc gói nhiều mẫu | Đo băng thông thực trước khi chốt |
| Bộ đệm RAM | 10 giây trước, 20 giây sau sự kiện | Lưu kiểu nhị phân cố định |

Ví dụ tính bộ nhớ: nếu một bản ghi nội bộ là 48 byte, 100 Hz trong 30 giây cần 144.000 byte, chưa tính hàng đợi và bộ nhớ BLE. Đây là ví dụ ngân sách, không phải kích thước gói API đã chốt. Ưu tiên bộ đệm vòng và giới hạn số sự kiện; khi đầy, bỏ mẫu thường trước, giữ sự kiện khẩn và ghi số mẫu mất.

## 6. Xử lý cảm biến và giới hạn suy luận

### 6.1. Chuẩn hóa và hiệu chuẩn

- Gia tốc gửi bằng m/s², gồm trọng lực như gia tốc kế điện thoại; tốc độ góc bằng độ/giây.
- Công bố hệ trục phải theo quy tắc bàn tay phải và gắn nhãn trên vỏ. Không ghép trực tiếp từng trục với điện thoại vì hai thiết bị có thể xoay khác nhau.
- Hiệu chuẩn độ lệch con quay lúc đứng yên; giữ cấu hình hiệu chuẩn theo mã cảm biến và phiên bản firmware.
- Kiểm tra cảm biến không phản hồi, mẫu đứng yên bất thường, bão hòa, mất mẫu và tốc độ lấy mẫu thực.
- Giữ đặc trưng đỉnh va đập từ dữ liệu chưa bị làm mượt quá mức; bộ lọc tư thế và bộ phát hiện đỉnh có thể dùng hai nhánh riêng.
- Cảm biến lỗi phải giảm chất lượng và báo lỗi; không điền toàn số 0 rồi diễn giải thành rơi tự do.

### 6.2. Độ cao tương đối

`altitudeDeltaM` là độ cao so với mốc áp suất gần nhất, dương khi lên, âm khi xuống. Mốc chỉ đổi khi có lệnh hoặc điều kiện ổn định đã định nghĩa; không âm thầm đổi giữa một cửa sổ nghi ngờ ngã.

Để phân tích một sự kiện, lấy chênh lệch độ cao trước/sau trong cùng hệ mốc, không so giá trị từ hai lần đặt mốc khác nhau. Lưu mã mốc và thời điểm mốc ở nhật ký nội bộ.

Không dùng độ cao làm điều kiện bắt buộc vì người dùng có thể ngã từ ghế hoặc khuỵu xuống rất ít. Áp suất chịu ảnh hưởng môi trường, gió, cửa đóng mở và vỏ thiết bị. Giá trị nhiệt độ khí áp kế là nhiệt độ cảm biến phục vụ bù đo, không phải thân nhiệt.

### 6.3. Gia tốc, vận tốc và quãng rơi

Có thể nghiên cứu tích phân gia tốc trong cửa sổ ngắn sau khi ước lượng hướng và loại trọng lực. Tuy nhiên sai số tư thế và độ lệch cảm biến gây trôi nhanh. Không dùng tích phân hai lần làm thước đo quãng rơi chính xác duy nhất; không dùng tốc độ GPS để suy ra quãng rơi ngắn.

Các đặc trưng nội bộ đề xuất: `accelerationMagnitudeMs2`, `peakAccelerationMs2`, `minimumAccelerationMs2`, `angularSpeedDps`, `orientationChangeDeg`, `inactivityDurationMs`, `altitudeDeltaM`, `signalAgeMs`, `sampleDropCount` và `triggerReasons`. Chúng chưa tự động là trường API mới.

## 7. Phát hiện và cảnh báo tự động

### 7.1. Phân chia quyết định

Khi kết nối tốt, Android là bộ điều phối cảnh báo chính. ESP32 phát sự kiện sớm, giữ dữ liệu và chấp hành lệnh còi/rung. ESP32 vẫn có luật dự phòng tối thiểu để báo động tại chỗ nếu Android không phản hồi.

| Dấu hiệu | Cách xử lý đề xuất |
|---|---|
| Một đỉnh va đập | `IMPACT_DETECTED`; Android đánh giá thêm, không khẳng định ngã |
| Giảm tải + đổi tư thế + va đập + bất động | Chuỗi có nguy cơ cao; có thể nâng khẩn theo luật đã hiệu chỉnh |
| Ngã thấp không có giảm tải rõ | Xét đổi tư thế, va chạm vừa và diễn biến sau đó |
| Loạng choạng/bám vật | Sự kiện mất ổn định, mức chú ý; không mặc định là đột quỵ |
| Chỉ bất động | Cần ngữ cảnh; có thể đang ngủ hoặc đã tháo thiết bị |
| Giữ SOS đủ thời gian | Khẩn cấp ngay, không đếm ngược trước khi gửi |

`eventConfidence` là điểm luật 0–100, không phải xác suất y khoa đã hiệu chuẩn. Không dùng mức chất lượng thấp để tăng độ chắc chắn.

### 7.2. Máy trạng thái cục bộ

| Trạng thái nội bộ | Hành vi | Chuyển trạng thái |
|---|---|---|
| `BOOT_SELF_TEST` | Kiểm tra nguồn, cảm biến, cấu hình | Đạt → `MONITORING`; lỗi thiết yếu → `DEGRADED` |
| `MONITORING` | Đo, lưu bộ đệm, truyền | Có dấu hiệu → `SUSPECTED` |
| `SUSPECTED` | Giữ cửa sổ dữ liệu, phát sự kiện | Đủ nguy cơ → `VERIFYING`; hết dấu hiệu → theo dõi |
| `VERIFYING` | Còi/rung và hạn chót xác minh | Xác nhận an toàn → theo dõi; hết hạn → `LOCAL_ALERTING` |
| `LOCAL_ALERTING` | Báo động tại chỗ, giữ sự kiện để truyền lại | Có xác nhận hợp lệ → kết thúc và ghi nhật ký |
| `DEGRADED` | Báo lỗi, giữ các chức năng còn dùng được | Khôi phục cảm biến → theo dõi |

SOS hoặc chuỗi đủ điều kiện khẩn được phép đi trực tiếp tới `LOCAL_ALERTING` từ bất kỳ trạng thái nào. Trạng thái kết nối và sạc là các cờ riêng, không biến thành hàng chục trạng thái kết hợp.

### 7.3. Tránh chờ hai lần và tránh báo trùng

- Khi liên kết hoạt động, Android quản lý một hạn chót xác minh; lệnh điều khiển cục bộ gắn cùng `eventId`.
- Đề xuất `TRIGGER_BUZZER` nhận `eventId`, `pattern` và `remainingMs` để ESP32 phản hồi theo thời gian còn lại. Đây là quy ước tham số bổ sung, phải triển khai ở cả hai nhóm trước khi dùng.
- Hạn chót ESP32 không được lùi về sau vì gói lệnh đến chậm hoặc bị gửi lặp. Mất BLE giữa đếm ngược vẫn chạy tới hạn cục bộ.
- Với `HIGH_RISK`, thời gian 10 giây trong bản Android là cấu hình thử cho bước xác minh, không phải tổng độ trễ cam kết. Phải đo từ thời điểm sự kiện đến khi người thân nhận thông báo.
- Với `CRITICAL_RISK`/SOS, phát sự kiện và cảnh báo ban đầu ngay khi đủ điều kiện; không chờ người dùng trả lời.
- `ACK_EVENT` chỉ cho biết Android đã tiếp nhận sự kiện; không được tắt báo động chỉ vì ACK. `CANCEL_ALERT` mới thực hiện hủy có chủ đích.
- `STOP_BUZZER` chỉ tắt âm, không xóa sự kiện hoặc vô hiệu hóa cảnh báo đang chờ.
- Sau khi đã gửi cảnh báo từ xa, xác nhận an toàn tạo bản cập nhật; không xóa lịch sử cảnh báo đã gửi.

Để gửi khẩn theo hợp đồng hiện có, Android phải kiểm thử ánh xạ `eventSeverity=CRITICAL` đến đường cảnh báo khẩn, đặc biệt khi điện thoại không mang theo người. Không tạo tên sự kiện mới rồi kỳ vọng decoder Android tự hiểu.

### 7.4. Trường hợp mất kết nối

- BLE mất: giữ đo, nút SOS, xử lý cơ bản, còi/rung và bộ đệm.
- Tự quảng bá lại với nhịp hợp lý; không quét/kết nối bận liên tục làm cạn pin.
- Sự kiện chưa được Android xác nhận được lưu vào hàng đợi bền vững có giới hạn; khi kết nối lại gửi sự kiện trước dữ liệu thông thường.
- Bản BLE không có đường truyền khác phải hiển thị rõ chưa chuyển được cảnh báo đến điện thoại. Còi không tương đương liên lạc người thân.
- Mất Internet nhưng BLE còn: Android quyết định kênh khả dụng; ESP32 không tự coi mạng đã hoạt động chỉ vì BLE kết nối.

## 8. Hợp đồng API giữ nguyên với Android

> **LƯU Ý:** Bộ test thuật toán (`esp32-test`) chỉ phục vụ thử nghiệm cảm biến, quan sát trạng thái và tinh chỉnh tham số cục bộ qua cổng nối tiếp Serial; bộ test **KHÔNG** triển khai hợp đồng BLE/HTTP dưới đây. Các hợp đồng này áp dụng cho firmware sản phẩm đích (`esp/node` và phiên bản hoàn chỉnh).

### 8.1. Định danh BLE

`PROTOCOL_VERSION = 1`; `ESP32_DEVICE_NAME_PREFIX = "FALLSAFE-"`.

| Hằng số | UUID | Thuộc tính |
|---|---|---|
| `ESP32_SERVICE_UUID` | `7d2a0001-6f45-4c2b-9a1e-38a8f5c10001` | Service |
| `SENSOR_STREAM_CHARACTERISTIC_UUID` | `7d2a0002-6f45-4c2b-9a1e-38a8f5c10001` | Notify |
| `EVENT_CHARACTERISTIC_UUID` | `7d2a0003-6f45-4c2b-9a1e-38a8f5c10001` | Indicate |
| `DEVICE_STATUS_CHARACTERISTIC_UUID` | `7d2a0004-6f45-4c2b-9a1e-38a8f5c10001` | Read/Notify |
| `COMMAND_CHARACTERISTIC_UUID` | `7d2a0005-6f45-4c2b-9a1e-38a8f5c10001` | Write |
| `COMMAND_ACK_CHARACTERISTIC_UUID` | `7d2a0006-6f45-4c2b-9a1e-38a8f5c10001` | Notify |

Không thay UUID hoặc tên trường tùy tiện. ESP32 là GATT server, Android là client. Indication có xác nhận ở tầng GATT nhưng không chứng minh ứng dụng đã lưu sự kiện hoặc người thân đã nhận; xem [GATT Server API của Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/esp_gatts.html).

### 8.2. `Esp32SensorPacket`

| Trường | Kiểu | Quy ước |
|---|---|---|
| `protocolVersion` | int | 1 |
| `deviceId` | string | Định danh ổn định của thiết bị |
| `sequenceNumber` | long | Số thứ tự gói |
| `timestampMs` | long | Unix milliseconds sau đồng bộ |
| `accelXMs2`, `accelYMs2`, `accelZMs2` | float | m/s², có trọng lực |
| `gyroXDps`, `gyroYDps`, `gyroZDps` | float | độ/giây |
| `pressurePa` | float/null | Pa |
| `temperatureC` | float/null | Nhiệt độ cảm biến khí áp, °C |
| `altitudeDeltaM` | float/null | m, so mốc gần nhất |
| `batteryPercent` | int | 0–100 |
| `batteryVoltageMv` | int/null | mV |
| `isCharging` | boolean | Từ mạch sạc |
| `sosButtonPressed` | boolean | Trạng thái nút |
| `sensorQuality` | int | 0–100, điểm chất lượng kỹ thuật |

Trường tùy chọn không có dữ liệu dùng `null`, không dùng số 0 thay dữ liệu thiếu. Hợp đồng v1 bắt buộc IMU và phần trăm pin: khi IMU hỏng, ngừng phát gói mẫu giả và phát trạng thái lỗi; khi đo pin hỏng, báo lỗi và ngừng gói đòi phần trăm mới cho tới khi khôi phục. Muốn tiếp tục mẫu với các trường này nullable phải sửa hợp đồng hai phía, không tự đổi riêng firmware.

Ví dụ gói mẫu:

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

### 8.3. `Esp32DeviceStatus`

Các trường giữ nguyên: `protocolVersion: int`, `deviceId: string`, `timestampMs: long`, `firmwareVersion: string`, `uptimeSeconds: long`, `batteryPercent: int`, `batteryVoltageMv: int/null`, `isCharging: boolean`, `imuStatus: string`, `barometerStatus: string`, `gnssStatus: string`, `bufferUsagePercent: int`, `lastErrorCode: string/null`.

Trạng thái cảm biến: `OK`, `CALIBRATING`, `UNAVAILABLE`, `ERROR`. Không có GNSS thì `gnssStatus=UNAVAILABLE`. Không có khí áp kế thì `barometerStatus=UNAVAILABLE` và các trường liên quan trong mẫu là `null`.

Đề xuất mã `lastErrorCode`: `IMU_READ_FAILED`, `BAROMETER_READ_FAILED`, `BATTERY_READ_FAILED`, `TIME_NOT_SYNCED`, `BUFFER_OVERFLOW`. Đây là từ vựng lỗi bổ sung cần hai nhóm ghi vào decoder/hiển thị; không phải các mã đã có sẵn trong bản Android.

Nếu pin lỗi nhưng có giá trị đo cũ, trạng thái có thể giữ giá trị cuối kèm `BATTERY_READ_FAILED`; Android bắt buộc trình bày là giá trị cũ. Nếu chưa từng có giá trị hợp lệ, v1 chưa biểu diễn đầy đủ: dùng `SENSOR_ERROR` để báo trước, không xuất trạng thái pin 0% giả.

### 8.4. `Esp32EventPacket`

Trường bắt buộc: `protocolVersion: int`, `eventId: string`, `deviceId: string`, `sequenceNumber: long`, `timestampMs: long`, `eventType: string`, `eventSeverity: string`, `sosButtonPressed: boolean`, `eventConfidence: int`.

Trường nullable: `peakAccelerationMs2: float/null`, `orientationChangeDeg: float/null`, `altitudeDeltaM: float/null`, `checksum: string/null`.

`eventType` chỉ dùng danh sách đã thống nhất:

- `IMPACT_DETECTED`
- `FREE_FALL_SUSPECTED`
- `POSTURE_CHANGED`
- `INSTABILITY_DETECTED`
- `INACTIVITY_DETECTED`
- `SOS_PRESSED`
- `SOS_CANCELLED`
- `LOW_BATTERY`
- `SENSOR_ERROR`

`eventSeverity`: `INFO`, `WARNING`, `CRITICAL`. Một sự kiện được truyền lại phải giữ nguyên `eventId`, payload và `sequenceNumber`; sự kiện mới có ID mới. Android gom các dấu hiệu liên quan thành một vụ việc để tránh nhiều cuộc gọi cho cùng một lần ngã.

Hợp đồng v1 chưa có `relatedEventId` cho `SOS_CANCELLED`. Trong bản đầu chỉ cho một cảnh báo cục bộ hoạt động tại một thời điểm; hủy từ nút áp dụng cho cảnh báo đó. Nếu cần nhiều cảnh báo đồng thời hoặc phát lại hủy sau mất kết nối, phải bổ sung liên kết sự kiện trong phiên bản giao thức kế tiếp, không suy đoán nhầm từ thời gian.

### 8.5. `Esp32Command` và xác nhận

Lệnh gồm `protocolVersion: int`, `commandId: string`, `timestampMs: long`, `commandType: string`, `parameters: Map<String,String>`. Giá trị bên trong `parameters` vẫn là chuỗi theo bản Android.

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

Giữ `ESP32_HTTP_PORT=80`, `ESP32_HTTP_BASE_URL="http://192.168.4.1"` trong chế độ điểm truy cập thử nghiệm.

| Hằng số Android | Đường dẫn | Phương thức đề xuất |
|---|---|---|
| `API_SENSOR_LATEST` | `/api/v1/sensors/latest` | GET |
| `API_DEVICE_STATUS` | `/api/v1/device/status` | GET |
| `API_DEVICE_COMMAND` | `/api/v1/device/command` | POST |
| `API_EVENT_STREAM` | `/api/v1/events` | GET, SSE cho chế độ thử |

Phương thức và cách truyền SSE là đề xuất bổ sung. HTTP không phải đường truyền mặc định của thiết bị đeo; tắt tự động sau phiên kỹ thuật, chỉ mở sau thao tác vật lý và xác thực phù hợp. Không đưa cổng cấu hình ra Internet.

## 9. GNSS và 4G: giữ hướng nghiên cứu, triển khai sau lõi

u-blox M10 là hướng GNSS đã trao đổi; mã module, anten và bo chuyển tiếp còn phải xác định. Tham khảo [dòng MAX-M10 của u-blox](https://www.u-blox.com/en/product/max-m10-series). Không lấy tên M10 làm bảo đảm bắt vị trí trong nhà như điện thoại.

Firmware GNSS phải ghi tọa độ, thời điểm fix, tuổi vị trí và sai số khả dụng; không hiển thị vị trí cũ là vị trí hiện tại. V1 mới có `gnssStatus`, chưa có gói tọa độ ESP32. Khi bật GNSS cần đặc tả gói vị trí phiên bản tiếp theo, đề xuất tên `latitude`, `longitude`, `locationAccuracyM`, `locationTimestampMs`, `locationSource`; chưa gửi các trường này như một phần hợp đồng v1 đã chốt.

Nhánh 4G phải kiểm chứng riêng: loại mạng/modem thực tế, SIM, tín hiệu, dòng đỉnh, sụt nguồn, gửi lại, xác nhận máy chủ, chống trùng với Android và nội dung đính chính khi người dùng báo an toàn. Không chọn modem chỉ theo chữ “4G” hoặc coi mọi modem đều hỗ trợ cuộc gọi giống nhau. Chưa chốt phần cứng và giao thức máy chủ trong kế hoạch này.

## 10. Kế hoạch năng lượng

Đo dòng trung bình của cả thiết bị trong từng chế độ, gồm bo ESP32, cảm biến, LED, bộ nguồn, pin monitor và đường truyền. Công thức ước lượng theo dung lượng quy đổi cùng phía điện áp: thời gian giờ ≈ dung lượng hữu dụng mAh / dòng trung bình mA.

Ví dụ thuần tính toán: pin danh định 1.000 mAh, giả sử dung lượng hữu dụng 80%, thì muốn đạt 48 giờ cần dòng trung bình không quá khoảng 16,7 mA; 72 giờ cần khoảng 11,1 mA. Đây không phải dự báo pin của mẫu chưa đo.

- Tắt Wi-Fi và màn/LED không cần thiết; truyền BLE theo lô khi đã kiểm chứng.
- Giữ lấy mẫu đủ nhanh ở chế độ giám sát; không dùng deep sleep dài làm bỏ mất phần đầu sự kiện.
- Có thể nghiên cứu ngắt chuyển động từ IMU cho chế độ tiết kiệm, nhưng phải đo tác động đến độ nhạy và độ trễ.
- GNSS và 4G lập ngân sách riêng; đo cả giai đoạn tìm mạng/tìm vệ tinh và các lần gửi lại.
- Thử pin liên tục 48–72 giờ với cấu hình sử dụng mô tả rõ, nhiều sự kiện mô phỏng và mất kết nối; báo dung lượng pin, dòng, thời gian, không chỉ ghi “pin 3 ngày”.

## 11. Độ tin cậy và bảo mật thiết bị

- Ghép nối có chủ đích bằng thao tác vật lý; chỉ cho thiết bị đã được ủy quyền gửi lệnh cấu hình/hủy. Tên `FALLSAFE-...` không phải xác thực.
- Dùng mã hóa BLE và bonding phù hợp khả năng thiết bị; không tuyên bố chống giả mạo đầy đủ nếu dùng phương thức ghép nối không xác minh được hai phía.
- Mỗi lệnh kiểm tra phiên bản, độ dài, loại, giá trị và trạng thái hiện tại; từ chối thay mốc hoặc self-test gây gián đoạn khi đang cảnh báo.
- Watchdog phát hiện treo; thời gian chờ bus cảm biến có giới hạn. Một cảm biến hỏng không được chặn nút SOS.
- Giới hạn ghi flash, giữ nhật ký sự kiện quan trọng có khả năng phục hồi sau mất điện; không lưu vô hạn mẫu thô.
- Kiểm tra cập nhật firmware, giữ bản khôi phục; không cập nhật giữa cảnh báo hoặc lúc pin không đủ.
- Log kỹ thuật không ghi khóa, số điện thoại và vị trí đầy đủ nếu không cần cho thử nghiệm có cho phép.

## 12. Kế hoạch kiểm thử và nghiên cứu khoa học

### 12.1. Câu hỏi nghiên cứu

1. Thêm khí áp kế vào IMU có cải thiện phát hiện ngã thấp, hay chủ yếu làm tăng nhiễu?
2. Thiết bị ở thắt lưng có giảm báo giả khi điện thoại đặt trên bàn hoặc bị đánh rơi không?
3. Luật nhiều giai đoạn có tốt hơn chỉ dùng ngưỡng gia tốc không?
4. Giảm tần số hoặc truyền theo lô ảnh hưởng thế nào đến pin, mất mẫu và thời gian cảnh báo?
5. Cảnh báo tự động có hoạt động khi không có phản hồi, mất BLE hoặc chưa có vị trí không?
6. Những tình huống mất thăng bằng nào có dấu hiệu đo được, và những tình huống nào thiết bị không phân biệt được?

Tính mới dự kiến nằm ở phương án hợp nhất, xử lý ngã thấp, cơ chế dự phòng và bằng chứng thực nghiệm. Việc ghép ESP32 + IMU + khí áp kế tự nó chưa chứng minh tính mới; không tuyên bố “đầu tiên” khi chưa khảo sát tài liệu liên quan.

### 12.2. Ma trận thử nghiệm

| Nhóm | Kịch bản | Điều cần quan sát |
|---|---|---|
| Đọc cảm biến | Nằm yên, xoay theo trục, tăng/giảm độ cao đã biết | Đơn vị, hướng trục, nhiễu, trễ |
| Hoạt động thường | Đi, ngồi nhanh, nằm, trở mình, cầu thang, cúi người | Báo động giả mỗi giờ |
| Ngã mô phỏng | Ngã mạnh có đệm, trượt thấp, khuỵu, bám vật | Độ nhạy và lý do kích hoạt |
| Không đeo | Tháo đặt bàn, rơi thiết bị | Khả năng phân biệt và giới hạn |
| Không phản hồi | Không bấm sau khi đã vào xác minh | Tự cảnh báo đúng hạn |
| SOS | Bình thường, đang lỗi cảm biến, mất BLE | Không bị chặn bởi tác vụ khác |
| BLE | Ra khỏi phạm vi, khóa điện thoại, kết nối lại | Độ trễ, giữ sự kiện, chống trùng |
| Giao thức | MTU nhỏ, thiếu mảnh, gói lặp, sai phiên bản | Không treo, không phát dữ liệu giả |
| Thời gian | Chưa đồng bộ, chỉnh giờ, reboot | Đếm ngược không bị kéo dài |
| Lệnh | ACK mất, hủy lặp, hủy sai ID, STOP_STREAM | Không thực hiện sai tác dụng |
| Nguồn | Pin thấp, cắm/rút sạc, tải còi và modem | Sụt áp, reset, trạng thái sạc |
| Vỏ | Cửa thông áp, kẹp lệch, anten bị che | Độ trễ áp suất và chất lượng BLE |

Không thử ngã thật với người cao tuổi hoặc yêu cầu học sinh tự ngã nguy hiểm. Dùng hình nộm/vật mô phỏng có đệm; các thao tác người tham gia chỉ thực hiện trong phạm vi an toàn và có giám sát.

### 12.3. Thiết kế đối chứng và chỉ số

So sánh cùng bộ kịch bản: IMU; IMU + khí áp; điện thoại; thiết bị đeo + điện thoại. Tách dữ liệu hiệu chỉnh và đánh giá, ưu tiên tách theo người/phiên để tránh cùng một lần đo xuất hiện ở cả hai tập.

- Độ nhạy = số sự kiện phát hiện đúng / tổng sự kiện mục tiêu.
- Tỷ lệ bỏ sót = số sự kiện bỏ sót / tổng sự kiện mục tiêu.
- Độ chính xác cảnh báo = cảnh báo đúng / tổng cảnh báo phát.
- Báo động giả theo giờ sử dụng, kèm tổng số giờ và hoạt động.
- Độ trễ đến sự kiện BLE, báo động tại chỗ, Android gửi và người nhận thực sự nhận; báo trung vị, phân vị 95% và trường hợp xấu nhất quan sát được.
- Tỷ lệ mất gói, kết nối lại, sự kiện gửi lại thành công, mức tiêu thụ pin.
- Báo số lần thử, loại người/vật mô phỏng và giới hạn suy rộng. Không dùng một tỷ lệ chung để khẳng định thiết bị phát hiện đột quỵ.

## 13. Lộ trình triển khai và điều kiện hoàn thành

| Giai đoạn | Sản phẩm cụ thể | Điều kiện chuyển tiếp |
|---|---|---|
| 1. Chốt bo và nguồn | Danh sách linh kiện, sơ đồ nguồn/chân | Điện áp đúng, tải còi không làm reset |
| 2. Đo cảm biến | Driver, hiệu chuẩn, log có thời gian | Dữ liệu đúng đơn vị, tốc độ ổn định, lỗi được phát hiện |
| 3. BLE và hợp đồng | GATT, decoder/encoder, frame, bộ gói mẫu | Android nhận/ra lệnh đúng, qua thử MTU nhỏ và lệnh lặp |
| 4. SOS và cảnh báo | Nút, đèn, còi/rung, máy trạng thái | Không phản hồi vẫn cảnh báo; SOS không chờ; mất BLE vẫn báo tại chỗ |
| 5. Luật phát hiện | Đặc trưng, cửa sổ sự kiện, lý do kích hoạt | Phân tích được ngã thấp và hoạt động thường qua dữ liệu |
| 6. Vỏ và năng lượng | Mẫu đeo, đo dòng, log chạy dài | Có số đo thực 48–72 giờ hoặc nêu rõ mức chưa đạt |
| 7. Đối chứng | Dữ liệu có nhãn, bảng chỉ số | Kết quả lặp được; nêu được trường hợp bỏ sót |
| 8. Mở rộng | GNSS/4G và protocol tiếp theo | Chỉ triển khai sau khi lõi BLE ổn định |

Không đặt lịch cố định khi chưa biết linh kiện nhóm đang có. Ưu tiên một nguyên mẫu chạy trọn luồng SOS → ESP32 → Android → người nhận thử nghiệm trước, sau đó mới tăng độ phức tạp thuật toán.

## 14. Bộ bàn giao cho nhóm ESP32

- Firmware build được với phiên bản SDK/thư viện ghi rõ.
- Danh sách linh kiện thực tế và sơ đồ kết nối, nguồn, chiều lắp cảm biến.
- Cấu hình GPIO và dải đo/tần số cho đúng bo.
- Hợp đồng giao thức dùng chung với Android, gồm phần frame BLE còn cần hoàn thiện.
- Gói mẫu hợp lệ và lỗi để hai phía kiểm thử decoder; kịch bản SOS, hủy, mất kết nối và reboot.
- Dữ liệu trước/sau sự kiện cùng cấu hình thuật toán, mã cảm biến và nhãn hoạt động.
- Báo cáo pin và khả năng hoạt động khi Android vắng mặt.
- Hướng dẫn đeo, sạc, SOS, xác nhận an toàn và nhận biết mất kết nối bằng ngôn ngữ người dùng.

**Tiêu chí trọng tâm:** ESP32 đo được, giao tiếp đúng hợp đồng, cảnh báo không phụ thuộc thao tác của người mất khả năng phản hồi, và biểu thị trung thực giới hạn của bản BLE. Các mục về frame, thời gian chưa đồng bộ, pin lỗi và tham số điều khiển là khoảng trống tích hợp đã được chỉ rõ để hai nhóm hoàn thiện trước khi tuyên bố tương thích.

## 15. Bộ test thuật toán phát hiện té ngã (esp32-test)

Nhằm xác minh sớm thuật toán phát hiện ngã trước khi hoàn thiện bo mạch sản phẩm đích, dự án thiết lập một bộ thử nghiệm độc lập đặt tại thư mục `esp32-test/`. Bộ test sử dụng phần cứng sẵn có (**TEST_RIG**: ESP32-WROOM-32 + MPU9250 + GY-63/MS5611) tập trung vào khâu cảm biến và thuật toán.

### 15.1. Mục tiêu thử nghiệm
Bộ test giải quyết 4 mục tiêu tuần tự:
1. **Kiểm tra phần cứng (Hardware bring-up):** Xác nhận giao tiếp I2C ổn định với cả MPU9250 và MS5611 trên cùng một bus, không bị treo bus hoặc tràn bộ nhớ.
2. **Thu thập dữ liệu thực nghiệm (Data collection):** Xuất luồng dữ liệu cảm biến chuẩn hóa thời gian thực qua cổng Serial (chế độ CSV raw có nhãn thời gian) để xây dựng tập dataset đối chứng.
3. **Quan sát hành vi thuật toán (Algorithm observation):** Theo dõi chuỗi chuyển trạng thái của máy trạng thái phát hiện ngã qua cổng Serial (chế độ HUMAN đọc được) dưới các tác động vận động mô phỏng.
4. **Hiệu chỉnh ngưỡng (Threshold tuning):** Tinh chỉnh các tham số trong `FallProfile` để cân bằng giữa độ nhạy (Sensitivity) và khả năng chống báo giả (False Positive Rejection).
*Ghi chú:* Bộ test này chưa đặt mục tiêu tối ưu hóa tiêu thụ năng lượng hay kết nối không dây (không bật BLE/Wi-Fi).

### 15.2. Bảng ánh xạ (Mapping) tham số Android ↔ ESP
Kế thừa từ phân tích đối chiếu mã nguồn Android (`vn.nckh27pa.fallsafe.DemoLogic` và `FallDetectionProfiles.kt`):

| Đại lượng | Nguồn sự thật Android | Giá trị Android | Đơn vị Android | Quy ước trên ESP (TEST_RIG) | Đơn vị ESP | Nguồn gốc & Lý do kỹ thuật |
|---|---|---|---|---|---|---|
| `impactAccelerationMs2` | `FallDetectionProfiles.kt:56` | `25.0f` | $m/s^2$ | `25.0` ($\approx 2.55 g$) | $m/s^2$ | Kế thừa Android. Ngưỡng gia tốc tổng nhận diện va chạm mạnh. |
| `stillnessTargetAccelerationMs2` | `FallDetectionProfiles.kt:56` | `9.81f` | $m/s^2$ | `9.81` ($1.0 g$) | $m/s^2$ | Kế thừa Android. Trọng lực Trái Đất khi đứng/nằm yên. |
| `stillnessToleranceMs2` | `FallDetectionProfiles.kt:56` | `1.0f` | $m/s^2$ | `1.0` | $m/s^2$ | Kế thừa Android. Dung sai kiểm tra tĩnh ($|a - 9.81| \le 1.0$). |
| `postImpactWindowMs` | `FallDetectionProfiles.kt:56` | `3000L` | ms | `3000` | ms | Kế thừa Android. Cửa sổ tối đa sau va đập để xác nhận bất động. |
| `postImpactStillnessDurationMs` | `FallDetectionProfiles.kt:56` | `1000L` | ms | `1000` | ms | Kế thừa Android. Thời lượng bất động liên tục tối thiểu. |
| `minimumStillnessSamples` | `FallDetectionProfiles.kt:56` | `6` | mẫu | `6` (tối thiểu) | mẫu | Kế thừa Android (ở 100 Hz, điều kiện 1000 ms tương đương ~100 mẫu, tự chi phối độ tin cậy). |
| `maximumSampleGapMs` | `FallDetectionProfiles.kt:56` | `250L` | ms | `250` (hủy chuỗi) | ms | Kế thừa Android. ESP bổ sung watchdog 100 ms cảnh báo bus trễ. |
| `phonePressureEvidenceEnabled` | `FallDetectionProfiles.kt:23` | `false` | boolean | `false` (không bắt buộc) | boolean | Kế thừa Android. Áp suất là bằng chứng phụ/ghi log, KHÔNG phủ quyết ngã. |
| `phonePressureMinimumRisePa` | `FallDetectionProfiles.kt:54` | `12.0f` | Pa | `12.0` (ngưỡng log) | Pa | Kế thừa Android ($12 Pa \approx 1 m$ độ cao tại mực nước biển). |
| `freeFallThresholdMs2` | `NOT_FOUND` (Android không có) | Không có | - | `4.90` ($0.5 g$) | $m/s^2$ | Suy luận kỹ thuật (bằng chứng phụ/log, không bắt buộc để chốt ngã). |
| `freeFallMinDurationMs` | `NOT_FOUND` | Không có | - | `80` | ms | Suy luận kỹ thuật (tránh rung lắc nhẹ tức thời làm kích hoạt nhầm). |
| `gyroTurnThresholdDps` | `NOT_FOUND` (Android không dùng) | Không có | dps | `120.0` | °/s | Suy luận kỹ thuật (tốc độ góc xoay thân lúc ngã; bằng chứng phụ). |
| `altitudeDeltaM` | `DemoLogic.kt:75` (không dùng chốt) | Tính độ cao | m | Tính theo công thức khí áp | m | Âm khi ngã xuống thấp; chỉ dùng làm bằng chứng đối chứng. |

### 15.3. Pipeline đọc cảm biến và xử lý lỗi
- **Bus I2C:** Sử dụng 1 bus I2C duy nhất (`TwoWire Wire`), cấu hình chân an toàn cho ESP32-WROOM-32: `SDA = GPIO21`, `SCL = GPIO22`, tần số chuẩn 400 kHz (`Wire.setClock(400000)`).
- **Địa chỉ cảm biến:**
  - MPU9250: Địa chỉ chính `0x68` (AD0 nối GND), địa chỉ dự phòng `0x69` (AD0 nối VCC).
  - MS5611: Địa chỉ chính `0x77` (CSB nối GND), địa chỉ dự phòng `0x76` (CSB nối VCC). Hai cảm biến khác địa chỉ nên hoạt động trơn tru trên cùng bus.
- **Trình tự chu kỳ đọc:**
  1. *Đọc IMU (100 Hz - mỗi 10 ms):* Đọc burst 14 byte qua I2C bắt đầu từ thanh ghi `0x3B` (`ACCEL_XOUT_H`) để thu thập đồng thời 3 trục gia tốc, nhiệt độ và 3 trục con quay.
  2. *Đọc Khí áp kế (25 Hz - máy trạng thái không chặn):* Điều khiển MS5611 theo chu kỳ OSR 4096 (yêu cầu chờ chuyển đổi tối đa 9.1 ms). Máy trạng thái chuyển tiếp: Gửi lệnh D1 → Chờ 9.1 ms không chặn (`micros()`) → Đọc ADC D1 → Gửi lệnh D2 → Chờ 9.1 ms không chặn → Đọc ADC D2 → Bù nhiệt độ bậc 2 thu được Pa và °C.
- **Xử lý lỗi đọc:**
  - Nếu đọc MPU9250 thất bại: Ghi nhận cờ lỗi `IMU_READ_FAILED`, tăng biến đếm lỗi, giữ nguyên trạng thái hoặc hủy chuỗi va đập dở dang nếu mất mẫu > 100 ms. Tuyệt đối không điền giá trị 0 giả tạo (tránh hiểu lầm là rơi tự do). Nếu lỗi kéo dài lúc khởi động, thiết bị dừng ở trạng thái lỗi `[ERROR]`.
  - Nếu đọc MS5611 thất bại: Ghi nhận cờ cảnh báo `BARO_READ_FAILED`, giữ giá trị áp suất cũ hoặc để trống, thuật toán phát hiện ngã vẫn tiếp tục dựa trên IMU.

### 15.4. Tần số lấy mẫu (Sampling Rate)
- **IMU:** Cố định **100 Hz** (chu kỳ 10 ms). Lý do: Cú va đập cơ học khi người chạm sàn có thời gian đỉnh xung lực (impact spike) rất hẹp (khoảng 20–50 ms). Tần số 100 Hz đảm bảo bắt trọn vẹn đỉnh gia tốc $> 25 m/s^2$ mà không bị hiện tượng lướt đỉnh (undersampling).
- **Khí áp kế:** Cố định **25 Hz** (chu kỳ 40 ms) với OSR 4096. Lý do: MS5611 ở OSR 4096 có độ phân giải cao nhất (~0.012 hPa ≈ 10 cm), thời gian chuyển đổi kép D1+D2 mất ~18.2 ms. Chu kỳ 40 ms để lại đủ băng thông cho bus I2C phục vụ luồng IMU 100 Hz mà không gây xung đột hay trễ ngắt.

### 15.5. Chiến lược lọc dữ liệu (Filtering: Hai nhánh độc lập)
Firmware phân tách rõ hai luồng xử lý tín hiệu nhằm tránh làm méo đặc tính chuyển động:
1. **Nhánh đỉnh va đập (Impact Peak Branch - Raw):**
   - Sử dụng trực tiếp giá trị gia tốc thô từ thanh ghi cảm biến, chỉ qua bộ lọc phần cứng DLPF 184 Hz nội bộ của MPU.
   - Tính toán độ lớn gia tốc tức thời: $a_{mag} = \sqrt{a_x^2 + a_y^2 + a_z^2}$.
   - Không áp dụng bất kỳ bộ lọc trung bình trượt (Moving Average) hay làm mượt nào tại nhánh này để bảo toàn đỉnh va đập $a_{mag} \ge 25 m/s^2$.
2. **Nhánh tư thế và bất động (Posture & Stillness Branch - Filtered):**
   - Áp dụng bộ lọc thông thấp (IIR Filter) hoặc cửa sổ trượt trung bình ngắn ($N=5$ mẫu) để triệt tiêu các rung động vi mô.
   - Dùng để xác định hướng vector trọng lực tĩnh $\vec{g}$ và kiểm tra điều kiện bất động: $|a_{filtered} - 9.81| \le 1.0 m/s^2$ kết hợp với độ lớn vận tốc góc $|\vec{\omega}| < 20^\circ/s$.

### 15.6. Đường chuẩn áp suất (Pressure Baseline)
- **Thu thập mốc khởi động:** Khi khởi động, sau khi IMU sẵn sàng, thiết bị giữ yên và đọc liên tục các mẫu MS5611 hợp lệ trong 3 giây.
- **Tính toán mốc $P_0$:** Lấy trung vị (median) của tập mẫu thu thập được (tối thiểu 20 mẫu, tối đa 40 mẫu) để loại bỏ nhiễu đột biến, sau đó "đóng băng" giá trị này làm `baselinePa`.
- **Nguyên tắc bất biến:** Mốc $P_0$ tuyệt đối không được tự động cập nhật hoặc trôi dạt trong suốt phiên hoạt động, đặc biệt là khi đang trong cửa sổ nghi ngờ có va đập/ngã (tránh triệt tiêu độ chênh áp khi người hạ thấp độ cao).

### 15.7. Tính toán chênh lệch độ cao (Altitude Delta)
- Công thức hypsometric chuẩn quốc tế (chuẩn hóa giống `node_math.cpp`):
  $$\Delta H = 44330.77 \times \left(1 - \left(\frac{P_{hiện\_tại}}{P_{baseline}}\right)^{0.190263}\right) \quad (\text{mét})$$
- **Quy ước dấu:** Dương ($+$) khi lên cao (áp suất giảm), Âm ($-$) khi hạ thấp xuống sàn (áp suất tăng). Khi ngã xuống đất từ thắt lưng, $\Delta H$ dự kiến giảm từ $-0.40$ m đến $-0.80$ m (áp suất tăng tương ứng $\approx 5 - 10$ Pa).

### 15.8. Trích xuất đặc trưng (Feature Extraction)
Mỗi chu kỳ tính toán, firmware trích xuất các đặc trưng:
- `accelMagnitudeMs2`: Độ lớn gia tốc tổng $\sqrt{a_x^2 + a_y^2 + a_z^2}$ ($m/s^2$).
- `gyroMagnitudeDps`: Độ lớn tốc độ góc tổng $\sqrt{g_x^2 + g_y^2 + g_z^2}$ ($^\circ/s$).
- `impactPeakMs2`: Đỉnh gia tốc lớn nhất ghi nhận được trong pha va đập ($m/s^2$).
- `altitudeDeltaM`: Độ cao tương đối so với baseline khởi động (m).
- `pressureDeltaPa`: Độ chênh áp suất so với đầu cửa sổ quan sát (Pa).
- `isStationary` (motionIndicator): Cờ boolean báo hiệu tĩnh, đạt khi $|accelMagnitudeMs2 - 9.81| \le 1.0$ và $gyroMagnitudeDps < 20^\circ/s$.
- `stillnessDurationMs`: Thời lượng duy trì trạng thái tĩnh liên tục (ms).
- `freeFallDurationMs`: Thời lượng gia tốc tổng giảm sâu $< 4.9 m/s^2$ (ms).

### 15.9. Máy trạng thái phát hiện ngã (Fall Detection State Machine)
Máy trạng thái vận hành trên ESP32 gồm các pha nối tiếp:

```
[BOOT] ──> [CALIBRATING] ──(Hiệu chuẩn xong 3s)──> [NORMAL]
                                                      │
                       ┌──────────────────────────────┴──────────────────────────────┐
                       │ (a < 4.9 m/s² liên tục ≥ 80ms)                             │ (a ≥ 25 m/s²)
                       ▼                                                             ▼
             [POSSIBLE_FREE_FALL] ──(a ≥ 25 m/s²)───────────────────────────> [IMPACT]
                       │                                                             │
                       │ (hết 500ms không va đập)                                    │ (mẫu kế tiếp)
                       ▼                                                             ▼
                   [NORMAL] <────(Hết hạn cửa sổ 3000ms hoặc Gap > 250ms)──── [POST_IMPACT]
                                                                                     │
                                                                                     │ (Tĩnh liên tục ≥ 1000ms
                                                                                     │  và số mẫu tĩnh ≥ 6)
                                                                                     ▼
                                                                             [FALL_CONFIRMED]
                                                                                     │
                                                                                     │ (Sau báo động / lệnh reset)
                                                                                     ▼
                                                                                 [NORMAL]
```

**Chi tiết điều kiện chuyển trạng thái:**
1. `NORMAL → POSSIBLE_FREE_FALL`: Khi $accelMagnitudeMs2 < 4.90 m/s^2$ duy trì liên tục $\ge 80 ms$. Ghi log `[EVENT] FREE FALL detected`. Tăng điểm rủi ro. Nếu sau 500 ms không xảy ra va đập, tự động quay về `NORMAL`.
2. `NORMAL → IMPACT` hoặc `POSSIBLE_FREE_FALL → IMPACT`: Khi $accelMagnitudeMs2 \ge 25.0 m/s^2$. Đánh dấu thời điểm `impactTimeMs`, reset bộ đếm mẫu tĩnh và chuyển sang `POST_IMPACT`. Ghi log `[EVENT] IMPACT ...`.
3. `IMPACT → POST_IMPACT`: Bước đệm theo dõi hành vi sau va đập.
4. `POST_IMPACT → NORMAL` (Hủy bỏ / Báo giả):
   - Nếu $t - impactTimeMs > 3000 ms$ (hết hạn `postImpactWindowMs`) mà chưa đạt tiêu chí tĩnh.
   - Hoặc khoảng cách giữa hai mẫu liên tiếp $> 250 ms$ (`maximumSampleGapMs`).
   Ghi log `[EVENT] POST-IMPACT window expired, false alarm`.
5. `POST_IMPACT → FALL_CONFIRMED` (XÁC NHẬN NGÃ):
   - Mẫu thỏa mãn $|accelMagnitudeMs2 - 9.81| \le 1.0 m/s^2$.
   - Thời gian duy trì liên tục đạt $t_{quiet} \ge 1000 ms$ (`postImpactStillnessDurationMs`).
   - Số lượng mẫu tĩnh tích lũy liên tục $\ge 6$ mẫu (`minimumStillnessSamples`).
   - **QUY TẮC CỐT LÕI:** Bám sát tuyệt đối logic Android. Sự kiện rơi tự do và áp suất chỉ là bằng chứng củng cố (corroboration) in ra log, **KHÔNG** bắt buộc phải có để chuyển sang `FALL_CONFIRMED`.
   - Ghi log cảnh báo mức cao: `[ALERT] FALL CONFIRMED`.

### 15.10. Định dạng Serial Logging
Hỗ trợ hai chế độ xuất dữ liệu qua cổng nối tiếp (tốc độ baud: **115200**):
1. **HUMAN MODE (Mặc định):**
   - Tần số in 2–5 dòng/giây (mỗi 250–500 ms in một nhịp trạng thái) giúp người kiểm thử dễ đọc màn hình:
     ```
     [OK] MPU9250 | Acc=1.02g | Gyro=12°/s
     [OK] GY63   | P=1008.42 hPa | ΔH=+0.08 m
     [STATE] NORMAL
     [RISK] Fall score: 12%
     ```
   - Khi có sự kiện xảy ra, in ngay lập tức một dòng sự kiện chuyên biệt:
     - `[EVENT] FREE FALL detected (Acc=3.21 m/s^2, dur=90 ms)`
     - `[EVENT] IMPACT 27.50 m/s^2 (2.80g)`
     - `[EVENT] POST-IMPACT low motion (count=10, dur=100 ms)`
     - `[ALERT] FALL CONFIRMED (impact=27.50 m/s^2, stillness=1050 ms, deltaP=+14.2 Pa, deltaH=-0.72 m)`
2. **CSV RAW MODE (Thu thập dữ liệu nghiên cứu):**
   - Xuất dữ liệu ở đúng tốc độ lấy mẫu (100 Hz).
   - Dòng đầu tiên là tiêu đề cột (Header):
     `timestamp_ms,ax_ms2,ay_ms2,az_ms2,acc_mag,gx_dps,gy_dps,gz_dps,gyro_mag,p_pa,temp_c,alt_m,state,event`
   - Khi bật chế độ CSV, tuyệt đối không in xen kẽ các dòng chữ văn bản để thuận tiện cho các script Python/MATLAB phân tích trực tiếp.
   - Điều khiển chế độ qua Serial Command: gõ phím `r` để bật/tắt chế độ Raw CSV, `c` để hiệu chuẩn lại, `t` để in cấu hình profile, `h` để xem trợ giúp.

### 15.11. Bảng cấu hình ngưỡng thử nghiệm (FallProfile)
Toàn bộ tham số thuật toán được gom vào một cấu trúc `struct FallProfile`:

| Trường tham số | Kiểu dữ liệu | Giá trị khởi điểm | Đơn vị | Ý nghĩa & Nguồn gốc |
|---|---|---|---|---|
| `impactAccelerationMs2` | `float` | `25.0f` | $m/s^2$ | Ngưỡng gia tốc va đập (sao chép từ Android `FallDetectionConfig.DEFAULT`) |
| `stillnessTargetAccelerationMs2` | `float` | `9.81f` | $m/s^2$ | Mốc trọng lực tĩnh (sao chép từ Android) |
| `stillnessToleranceMs2` | `float` | `1.0f` | $m/s^2$ | Biên dung sai tĩnh (sao chép từ Android) |
| `postImpactWindowMs` | `uint32_t` | `3000` | ms | Cửa sổ quan sát sau va đập (sao chép từ Android) |
| `postImpactStillnessDurationMs` | `uint32_t` | `1000` | ms | Thời lượng tĩnh yêu cầu (sao chép từ Android) |
| `minimumStillnessSamples` | `uint16_t` | `6` | mẫu | Số mẫu tĩnh tối thiểu (sao chép từ Android) |
| `maximumSampleGapMs` | `uint32_t` | `250` | ms | Ngưỡng đứt đoạn mẫu để reset (sao chép từ Android) |
| `freeFallThresholdMs2` | `float` | `4.90f` | $m/s^2$ | Ngưỡng rơi tự do ($0.5 g$, suy luận kỹ thuật) |
| `freeFallMinDurationMs` | `uint32_t` | `80` | ms | Thời lượng rơi tự do tối thiểu (suy luận kỹ thuật) |
| `gyroTurnThresholdDps` | `float` | `120.0f` | °/s | Ngưỡng xoay thân mạnh (suy luận kỹ thuật) |
| `pressureEvidenceMinRisePa` | `float` | `12.0f` | Pa | Ngưỡng đối chứng tăng áp suất (sao chép từ Android) |
| `altitudeDropMinM` | `float` | `-0.40f` | m | Ngưỡng đối chứng giảm độ cao (suy luận kỹ thuật) |

### 15.12. Quy trình hiệu chuẩn lúc khởi động (Boot Calibration)
Trình tự thực thi tuần tự trong `setup()`:
1. Thiết lập cổng nối tiếp Serial ở baud rate 115200, in thông tin phiên bản firmware và cấu hình phần cứng.
2. Khởi tạo bus I2C (SDA=21, SCL=22, 400 kHz).
3. Thử kết nối và kiểm tra ID của MPU9250 (đọc thanh ghi `WHO_AM_I` = `0x71`). Nếu thất bại, thử lại địa chỉ phụ `0x69`. Nếu cả hai đều thất bại, in `[ERROR] MPU9250 not found` và dừng chương trình trong vòng lặp cảnh báo.
4. Thử kết nối và đọc PROM của MS5611 (địa chỉ `0x77` hoặc `0x76`), tính toán và kiểm tra CRC-4. Nếu lỗi, in cảnh báo nhưng cho phép tiếp tục chạy ở chế độ giảm cấp (chỉ IMU).
5. In thông báo: `[CAL] Keep device still... (3 seconds)`.
6. Giữ yên thiết bị trong 3 giây:
   - Thu thập ~300 mẫu gyro để tính trung bình độ lệch tĩnh (bias offset: `gx_offset`, `gy_offset`, `gz_offset`).
   - Thu thập ~75 mẫu áp suất MS5611 hợp lệ, tính trung vị (median) đóng băng làm mốc `baselinePa`.
7. In thông báo xác nhận: `[READY] Monitoring started`. Thiết bị chính thức chuyển sang trạng thái `NORMAL`.

### 15.13. Quy trình kiểm thử nghiệm thu (Testing Protocol)
Kế hoạch thử nghiệm chi tiết 13 kịch bản vận động (T0 đến T12) được trình bày độc lập tại tài liệu:
`esp32-test/TEST_PLAN.md`
Các ca kiểm thử bao gồm kiểm tra tĩnh, hoạt động sinh hoạt thường ngày (ADL) như đi bộ, ngồi mạnh, nằm xuống giường, cúi nhặt đồ, nhảy rung lắc, đi thang máy (chống báo giả); và các thử nghiệm ngã mô phỏng có đệm mút bảo hộ như ngã trước, ngã sau, ngã nghiêng, trượt ngã từ ghế ngồi, bám vật trượt chân.

### 15.14. Hướng dẫn chuyển đổi sang sản phẩm thật (TARGET_PRODUCT)
Khi chuyển thuật toán đã kiểm chứng từ bộ test (`esp32-test`) sang firmware sản phẩm chính thức (`esp/node` và phiên bản hoàn thiện trên ESP32-S3 Super Mini), nhóm phát triển bắt buộc phải thực hiện các thay đổi kỹ thuật sau:

1. **Thay đổi driver IMU (MPU9250 → MPU6050):**
   - Thanh ghi `WHO_AM_I` (`0x75`): MPU9250 trả về `0x71` (hoặc `0x73`), trong khi MPU6050 trả về `0x68`. Cần đổi giá trị kiểm tra sang `0x68`.
   - Vô hiệu hóa mã xử lý AK8963: Loại bỏ hoàn toàn các đoạn code cấu hình I2C Bypass hoặc thanh ghi liên quan đến từ kế nội bộ của MPU9250.
   - Thang đo gia tốc và hệ số đổi đơn vị: Đảm bảo cấu hình thanh ghi `ACCEL_CONFIG` trên MPU6050 đạt dải đo tối thiểu $\pm 8g$ hoặc $\pm 16g$ (thay vì mặc định $\pm 2g$) để không bị bão hòa khi va đập; đồng thời chia lại hệ số raw tương ứng.
2. **Thay đổi cấu hình chân GPIO (WROOM-32 → ESP32-S3 Super Mini):**
   - Tuyệt đối không dùng cặp chân `SDA=21, SCL=22` trên ESP32-S3 Super Mini.
   - Chuyển sang cấu hình 2 bus I2C riêng biệt của bo S3:
     - Bus 0 (MPU6050): `SDA = GPIO7`, `SCL = GPIO6`.
     - Bus 1 (MS5611): `SDA = GPIO3`, `SCL = GPIO2`.
3. **Tích hợp các giao thức sản phẩm:**
   - Chuyển cấu trúc `FallProfile` vào lưu trữ NVS (`Preferences`) để có thể cập nhật động từ ứng dụng Android thông qua BLE.
   - Bổ sung các service GATT BLE và cấu trúc gói tin `Esp32EventPacket` / `Esp32SensorPacket` theo đúng quy định tại Mục 8 của tài liệu này.

