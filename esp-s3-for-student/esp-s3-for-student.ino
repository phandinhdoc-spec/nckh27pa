/**
 * ============================================================================
 * NCKH27PA — Firmware phát hiện té ngã cho ESP32-S3 Super Mini
 * Phần cứng đích: ESP32-S3 Super Mini (esp32:esp32:esp32s3)
 * Cảm biến: MPU6050 (GY-521, I2C0) và MS5611 (GY-63, I2C1)
 * Giao thức: BLE GATT (FALLSAFE-xxxx), dòng lệnh Serial và dữ liệu đo từ xa
 * Tham chiếu kế hoạch: esp/esp32-plan.md (§5, §6, §7, §8) và android/android-plan.md (§6)
 * ============================================================================
 */

#include <Arduino.h> // Nạp các API nền tảng Arduino.
#include <Wire.h> // Nạp driver giao tiếp I2C.
#include <WiFi.h> // Nạp dịch vụ kết nối WiFi.
#include <math.h> // Nạp các hàm toán học dùng cho cảm biến.
#include <esp_task_wdt.h> // Nạp bộ giám sát tác vụ của ESP32.
#include <esp_timer.h> // Nạp bộ định thời độ phân giải cao.
#include <esp_system.h> // Nạp API hệ thống ESP32.
#include <esp_mac.h> // Nạp API đọc địa chỉ MAC.
#include <esp_random.h> // Nạp bộ sinh số ngẫu nhiên phần cứng.

#define CO_BLE 1 // Bật tính năng BLE của firmware.

#if CO_BLE // Chỉ biên dịch dịch vụ BLE khi tính năng được bật.
#include <BLEDevice.h> // Nạp lớp thiết bị BLE của thư viện.
#include <BLEServer.h> // Nạp lớp máy chủ BLE của thư viện.
#include <BLEUtils.h> // Nạp các tiện ích BLE dùng chung.
#include <BLE2902.h> // Nạp bộ mô tả thông báo BLE chuẩn 2902.
#endif // Kết thúc khối biên dịch có điều kiện BLE.

// ============================================================================
// 0. KIỂU NGƯỜI DÙNG KHAI BÁO TRƯỚC (ràng buộc thứ tự nguyên mẫu tự động của Arduino)
// ============================================================================
// Trình dựng Arduino tự sinh nguyên mẫu cho mọi hàm và chèn chúng trước khai báo người dùng đầu tiên.
// Kiểu xuất hiện trong chữ kí hàm phải được khai báo trước các nguyên mẫu tự sinh đó.
// Vì vậy enum này phải nằm gần đầu bản phác thảo để trình biên dịch nhận diện kiểu.
// Nếu di chuyển enum xuống dưới, nguyên mẫu tự sinh sẽ không biên dịch được.
// Giữ enum tại đây; `tenTrangThaiThietBi()` và các nơi sử dụng nằm ở phía dưới.
enum TrangThaiThietBi { // Khai báo sớm để nguyên mẫu tự sinh của Arduino nhận diện được kiểu này.
    STATE_BOOT_SELF_TEST, // Trạng thái tự kiểm tra khi khởi động.
    STATE_CALIBRATING, // Trạng thái đang hiệu chuẩn cảm biến.
    STATE_MONITORING, // Trạng thái giám sát bình thường.
    STATE_SUSPECTED, // Trạng thái vừa phát hiện dấu hiệu nghi ngờ.
    STATE_VERIFYING, // Trạng thái đang xác minh sự kiện.
    STATE_LOCAL_ALERTING, // Trạng thái đang cảnh báo tại thiết bị.
    STATE_DEGRADED // Trạng thái suy giảm khi phần cứng gặp lỗi.
};

// ============================================================================
// 1. SƠ ĐỒ CHÂN VÀ ĐỊNH NGHĨA PHẦN CỨNG
// ============================================================================
// Các chân phần cứng đã được xác nhận từ node_config.h của ESP32-S3.
static const int CHAN_I2C0_SDA = 8; // Chân SDA của MPU6050 trên bus 0 [Nguồn: node_config.h].
static const int CHAN_I2C0_SCL = 9; // Chân SCL của MPU6050 trên bus 0 [Nguồn: node_config.h].
static const int CHAN_I2C1_SDA = 7; // Chân SDA của MS5611 trên bus 1 [Nguồn: node_config.h].
static const int CHAN_I2C1_SCL = 6; // Chân SCL của MS5611 trên bus 1 [Nguồn: node_config.h].

// Các ngoại vi chưa có sơ đồ chính thức được đánh dấu TODO(HW).
static const int CHAN_NUT_SOS = 4; // Nút SOS tác động mức thấp và giả định dùng điện trở kéo lên nội [Nguồn: TODO(HW)].
static const int CHAN_NUT_HUY = 5; // Nút hủy tác động mức thấp và giả định dùng điện trở kéo lên nội [Nguồn: TODO(HW)].
static const int CHAN_CANH_BAO = 1; // Chân điều khiển còi PWM chủ động hoặc thụ động [Nguồn: TODO(HW)].
static const int CHAN_LED_TRANG_THAI = 8; // Chân LED trạng thái trên bo mạch [Nguồn: TODO(HW)].
static const int CHAN_LED_PIN_1 = 9; // Chân LED thứ nhất của thang báo pin [Nguồn: TODO(HW)].
static const int CHAN_LED_PIN_2 = 10; // Chân LED thứ hai của thang báo pin [Nguồn: TODO(HW)].
static const int CHAN_LED_PIN_3 = 11; // Chân LED thứ ba của thang báo pin [Nguồn: TODO(HW)].
static const int CHAN_ADC_PIN = 12; // Chân ADC đọc cầu chia điện áp pin [Nguồn: TODO(HW)].

// Các cờ khả năng phần cứng; phần chưa lắp được báo là không khả dụng.
#define CO_DONG_HO_NHIEN_LIEU 0 // Chưa lắp MAX17048; FIX 6 yêu cầu báo -1 và BATTERY_READ_FAILED.
#define CO_GNSS 0 // Phần cứng hiện chưa lắp bộ thu GNSS.
#define CO_4G 0 // Phần cứng hiện chưa lắp mô-đun 4G.

// ============================================================================
// 2. ĐẶC TẢ GIAO THỨC VÀ UUID (Kế hoạch §8)
// ============================================================================
#define PROTOCOL_VERSION 1 // Phiên bản giao thức mà thiết bị công bố theo §8.
#define FIRMWARE_VERSION "esp-s3 1.0.0" // Chuỗi phiên bản firmware gửi cho ứng dụng.

#define UUID_SERVICE "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001" // UUID dịch vụ BLE chính theo hợp đồng §8.
#define UUID_CHAR_STREAM "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001" // Đặc trưng thông báo luồng cảm biến.
#define UUID_CHAR_EVENT "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001" // Đặc trưng chỉ báo sự kiện cần xác nhận.
#define UUID_CHAR_STATUS "7d2a0004-6f45-4c2b-9a1e-38a8f5c10001" // Đặc trưng đọc và thông báo trạng thái.
#define UUID_CHAR_COMMAND "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001" // Đặc trưng nhận lệnh ghi từ điện thoại.
#define UUID_CHAR_ACK "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001" // Đặc trưng thông báo xác nhận lệnh.

// Thông tin trạm WiFi; kết nối chạy song song với BLE và không chặn vòng lặp.
#define WIFI_SSID "Pdmq" // Tên mạng WiFi giữ nguyên theo cấu hình triển khai.
#define WIFI_PASS "12345678" // Mật khẩu WiFi giữ nguyên theo cấu hình triển khai.

// Danh tính thiết bị và bộ đếm thứ tự toàn cục dùng chung cho mẫu và sự kiện theo §8.6.
static char tenThietBiBle[24] = "FALLSAFE-0000"; // Lưu tên quảng bá BLE của thiết bị.
static uint32_t soThuTuToanCuc = 0; // Đếm số thứ tự dùng chung theo §8.6.

// Trạng thái đồng bộ dấu thời gian theo §8.6.
static bool daDongBoThoiGian = false; // Cho biết thiết bị đã nhận mốc thời gian thực hay chưa.
static uint64_t thoiGianEpochThietBiMs = 0; // Lưu mốc epoch đã đồng bộ theo mili giây.
static uint32_t thoiDiemDongBoCuoiMs = 0; // Lưu thời điểm millis tại lần đồng bộ gần nhất.

static uint64_t layThoiGianChayMs() { // Trả thời gian đơn điệu kể từ lúc khởi động.
    return (uint64_t)(esp_timer_get_time() / 1000ULL); // Đổi micro giây của bộ định thời sang mili giây.
}

static uint64_t layThoiGianHienTaiMs() { // Chọn epoch đã đồng bộ hoặc thời gian chạy làm dấu thời gian.
    if (daDongBoThoiGian) { // Chỉ cộng độ lệch cục bộ khi đã có mốc epoch hợp lệ.
        return thoiGianEpochThietBiMs + (uint64_t)(millis() - thoiDiemDongBoCuoiMs); // Suy ra epoch hiện tại từ thời gian đã trôi.
    }
    return layThoiGianChayMs(); // Dùng thời gian đơn điệu khi chưa đồng bộ epoch.
}

// ============================================================================
// 3. CẤU HÌNH PHÁT HIỆN TÉ NGÃ (tương ứng Android FallDetectionConfig.DEFAULT)
// ============================================================================
struct CauHinhPhatHienNga { // Gom các ngưỡng phát hiện ngã tương ứng cấu hình Android.
    // Tương ứng với cấu hình trong Android FallDetectionProfiles.kt.
    float giaTocVaChamMs2; // Ngưỡng 25.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    float nguongDungYenMs2; // Ngưỡng 9.81 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    float nguongSaiSoDungYenMs2; // Ngưỡng 1.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    uint32_t cuaSoSauVaChamMs; // Ngưỡng 3000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    uint32_t thoiGianDungYenSauVaChamMs; // Ngưỡng 1000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    uint32_t minimumStillnessSamples; // Ngưỡng 6 mẫu [Nguồn: Android FallDetectionConfig.DEFAULT].
    uint32_t maximumSampleGapMs; // Ngưỡng 250 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    float nguongTangApSuatPinPa; // Ngưỡng 12.0 Pa [Nguồn: Android, cờ áp suất tắt].
    bool suDungBoLocApSuat; // Tắt bộ lọc áp suất [Nguồn: Android, cờ áp suất tắt].

    // Các ngưỡng phụ suy ra bằng kinh nghiệm, chỉ dùng cho lí do kích hoạt và bản ghi phụ.
    float nguongTuDoMs2; // Ngưỡng 3.0 m/s^2 [Nguồn: suy luận hệ thống - logging].
    float vanTocGocCaoDps; // Ngưỡng 200.0 dps [Nguồn: suy luận hệ thống - logging].
    float nguongThayDoiTuTheDo; // Ngưỡng 45.0 độ [Nguồn: suy luận hệ thống - logging].
};

static const CauHinhPhatHienNga NGUONG_PHAT_HIEN_TE_NGA = { // Khởi tạo bộ ngưỡng mặc định dùng thống nhất.
    .giaTocVaChamMs2 = 25.0f, // Ngưỡng 25.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    .nguongDungYenMs2 = 9.81f, // Ngưỡng 9.81 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    .nguongSaiSoDungYenMs2 = 1.0f, // Ngưỡng 1.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
    .cuaSoSauVaChamMs = 3000, // Ngưỡng 3000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    .thoiGianDungYenSauVaChamMs = 1000, // Ngưỡng 1000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    .minimumStillnessSamples = 6, // Ngưỡng 6 mẫu [Nguồn: Android FallDetectionConfig.DEFAULT].
    .maximumSampleGapMs = 250, // Ngưỡng 250 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    .nguongTangApSuatPinPa = 12.0f, // Ngưỡng 12.0 Pa [Nguồn: Android, cờ áp suất tắt].
    .suDungBoLocApSuat = false, // Tắt bộ lọc [Nguồn: Android, cờ áp suất tắt].
    .nguongTuDoMs2 = 3.0f, // Ngưỡng 3.0 m/s^2 [Nguồn: suy luận hệ thống - logging].
    .vanTocGocCaoDps = 200.0f, // Ngưỡng 200.0 dps [Nguồn: suy luận hệ thống - logging].
    .nguongThayDoiTuTheDo = 45.0f // Ngưỡng 45.0 độ [Nguồn: suy luận hệ thống - logging].
};

// ============================================================================
// 4. KIỂU DỮ LIỆU VÀ TRẠNG THÁI HỆ THỐNG
// ============================================================================
// LƯU Ý: `enum TrangThaiThietBi` được chủ ý khai báo ở ĐẦU bản phác thảo (mục 0):
// trình dựng Arduino chèn các nguyên mẫu hàm được tạo ngay trước phần khai báo đầu tiên của người dùng,
// vì vậy mọi kiểu do người dùng định nghĩa xuất hiện trong chữ kí hàm phải được nhìn thấy trước
// các nguyên mẫu đó; nếu không, GCC sẽ báo kiểu chưa được khai báo trong phạm vi này.

static const char* tenTrangThaiThietBi(TrangThaiThietBi s) { // Đổi trạng thái số thành chuỗi hợp đồng.
    switch(s) { // Chọn chuỗi trạng thái tương ứng.
        case STATE_BOOT_SELF_TEST: return "BOOT_SELF_TEST"; // Giữ nguyên giá trị trạng thái tự kiểm tra khi khởi động.
        case STATE_CALIBRATING:    return "CALIBRATING"; // Giữ nguyên giá trị trạng thái hiệu chuẩn.
        case STATE_MONITORING:     return "MONITORING"; // Giữ nguyên giá trị trạng thái giám sát.
        case STATE_SUSPECTED:      return "SUSPECTED"; // Giữ nguyên giá trị trạng thái nghi ngờ.
        case STATE_VERIFYING:      return "VERIFYING"; // Giữ nguyên giá trị trạng thái xác minh.
        case STATE_LOCAL_ALERTING: return "LOCAL_ALERTING"; // Giữ nguyên giá trị trạng thái cảnh báo cục bộ.
        case STATE_DEGRADED:       return "DEGRADED"; // Giữ nguyên giá trị trạng thái suy giảm.
        default:                   return "UNKNOWN"; // Giữ nguyên giá trị trạng thái không xác định.
    }
}

// Bản ghi mẫu IMU và khí áp có kích thước cố định cho bộ đệm vòng.
struct MauCamBien { // Lưu một mẫu cảm biến trong bộ đệm vòng.
    uint32_t thoiGianMs; // Dấu thời gian lấy mẫu theo mili giây.
    float ax, ay, az; // Gia tốc ba trục, đơn vị m/s^2.
    float gx, gy, gz; // Vận tốc góc ba trục, đơn vị độ/giây.
    float doLonGiaToc; // Độ lớn vectơ gia tốc theo m/s^2.
    float apSuatPa; // Áp suất khí quyển theo Pa.
    float chenhLechDoCaoM; // Chênh lệch độ cao tương đối theo mét (FIX 3).
};

// ============================================================================
// BỘ ĐỆM VÒNG (Kế hoạch §5.2, FIX 7)
// ============================================================================
// - Bộ đệm trước sự kiện: 10 giây ở 100 Hz = 1000 mẫu [Nguồn: Kế hoạch §5.2].
// - Kiểm tra bộ nhớ: sizeof(MauCamBien) là 40 byte (1 uint32_t + 9 float).
//   1000 mẫu * 40 byte = 40.000 byte (xấp xỉ 39,1 KB) [Nguồn: Kế hoạch §5.2].
//   Kích thước này phù hợp với SRAM nội 512 KB của ESP32-S3, còn hơn 300 KB vùng nhớ tự do.
// - Chính sách cửa sổ sau sự kiện: khi một phiên mở ở SUSPECTED hoặc VERIFYING,
//   thiết bị tiếp tục ghi liên tục vào bộ đệm vòng này và truyền dữ liệu
//   tối đa 20 giây sau sự kiện cho đến khi xác nhận, hết thời gian hoặc hủy.
// - Chính sách khi đầy bộ đệm hoặc tắc nghẽn: ưu tiên tuyệt đối việc giữ lại và
//   truyền bản ghi sự kiện (Esp32EventPacket); các mẫu luồng thông thường bị bỏ
//   nếu hàng đợi BLE hoặc xử lí bị bão hòa, đồng thời tăng biến đếm mẫu bị bỏ.
#define SO_LUONG_BO_DEM_VONG 1000 // Giữ 1000 mẫu [Nguồn: Kế hoạch §5.2].
static MauCamBien boDemVong[SO_LUONG_BO_DEM_VONG]; // Cấp phát bộ đệm vòng mẫu cảm biến.
static uint16_t viTriGhiBoDemVong = 0; // Theo dõi vị trí ghi kế tiếp.
static uint16_t soMauTrongBoDem = 0; // Theo dõi số mẫu hợp lệ hiện có.
static uint32_t soMauBiBo = 0; // Đếm mẫu bị bỏ khi hệ thống nghẽn.

// Điều khiển động tần số lấy mẫu (FIX 3: hỗ trợ lệnh SET_SAMPLE_RATE ở 100 Hz và 50 Hz).
static uint32_t tanSoLayMauImuHz = 100; // Tần số lấy mẫu mặc định 100 Hz [Nguồn: FIX 3].
static uint32_t chuKyLayMauImuUs = 10000; // Chu kỳ 10000 us tương ứng 100 Hz [Nguồn: FIX 3].

// Mốc áp suất tham chiếu (Kế hoạch §6.2, FIX 3).
static float apSuatThamChieuPa = 101325.0f; // Áp suất mốc khí quyển tiêu chuẩn [Nguồn: Kế hoạch §6.2].

// Bộ mã lỗi gần nhất (Kế hoạch §8.3, FIX 4, FIX 6):
// Các giá trị gồm "IMU_READ_FAILED", "BAROMETER_READ_FAILED" và "BATTERY_READ_FAILED".
// "TIME_NOT_SYNCED", "BUFFER_OVERFLOW", hoặc nullptr nếu không có lỗi.
#if CO_DONG_HO_NHIEN_LIEU // Chọn mã lỗi khởi tạo theo khả năng phần cứng đo pin.
static const char* maLoiCuoiCung = "TIME_NOT_SYNCED"; // Báo chưa đồng bộ thời gian khi có đồng hồ pin.
#else // Dùng nhánh không có đồng hồ nhiên liệu.
static const char* maLoiCuoiCung = "BATTERY_READ_FAILED"; // Báo thiếu phần cứng đọc pin theo FIX 6.
#endif // Kết thúc lựa chọn mã lỗi theo phần cứng đo pin.

// Trạng thái pin (Kế hoạch §3.4, §8.2, FIX 6), được khai báo TẠI ĐÂY trước các hàm tạo gói BLE
// định dạng chúng, vì biến toàn cục phải được khai báo trước hàm đọc biến đó:
//   CO_DONG_HO_NHIEN_LIEU == 0  ->  phanTramPin = -1 (lính canh "không có phần cứng", không giả lập 100%).
//                                      điện áp pin = null, mã lỗi cuối = BATTERY_READ_FAILED.
static int phanTramPin = -1; // -1 là giá trị lính canh cho phần cứng đo pin chưa lắp [Nguồn: FIX 6].
static bool dangSacPin = false; // Theo dõi trạng thái sạc pin hiện tại.
static bool daCanhBaoPinYeu = false; // Tránh lặp cảnh báo pin yếu.
static bool daCanhBaoPinKiet = false; // Tránh lặp cảnh báo pin cạn.

// ============================================================================
// 5. DRIVER MPU6050 Ở MỨC THANH GHI (bus 0: Wire, GPIO 8 và GPIO 9)
// ============================================================================
#define MPU6050_ADDR_A 0x68 // Địa chỉ I2C thứ nhất của MPU6050 [Nguồn: bảng dữ liệu MPU6050].
#define MPU6050_ADDR_B 0x69 // Địa chỉ I2C thứ hai của MPU6050 [Nguồn: bảng dữ liệu MPU6050].
#define MPU6050_SMPLRT_DIV 0x19 // Thanh ghi bộ chia tần số lấy mẫu [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_CONFIG 0x1A // Thanh ghi cấu hình bộ lọc số [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_GYRO_CONFIG 0x1B // Thanh ghi chọn thang con quay [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_ACCEL_CONFIG 0x1C // Thanh ghi chọn thang gia tốc [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_ACCEL_XOUT_H 0x3B // Thanh ghi đầu của khối dữ liệu đo [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_PWR_MGMT_1 0x6B // Thanh ghi quản lý nguồn [Nguồn: bảng thanh ghi MPU6050].
#define MPU6050_WHO_AM_I 0x75 // Thanh ghi nhận dạng chip [Nguồn: bảng thanh ghi MPU6050].

struct TrangThaiCamBienMpu { // Gom cấu hình và trạng thái hoạt động của MPU6050.
    uint8_t diaChi; // Lưu địa chỉ I2C đang phản hồi.
    bool dangHoatDong; // Cho biết cảm biến có giao tiếp được hay không.
    bool biBaoHoa; // Ghi nhận ADC gia tốc đã chạm giới hạn đo.
    float saiSoVanTocGocX; // Lưu độ lệch con quay trục X.
    float saiSoVanTocGocY; // Lưu độ lệch con quay trục Y.
    float saiSoVanTocGocZ; // Lưu độ lệch con quay trục Z.
    float trongLucGocX; // Lưu thành phần trọng lực gốc trên trục X.
    float trongLucGocY; // Lưu thành phần trọng lực gốc trên trục Y.
    float trongLucGocZ; // Lưu thành phần trọng lực gốc trên trục Z.
};

static TrangThaiCamBienMpu camBienMpu = { // Khởi tạo driver MPU6050 ở trạng thái chưa kết nối.
    .diaChi = MPU6050_ADDR_A, // Thử địa chỉ 0x68 trước [Nguồn: bảng dữ liệu MPU6050].
    .dangHoatDong = false, // Chưa xác nhận cảm biến trực tuyến khi khởi động.
    .biBaoHoa = false, // Chưa ghi nhận bão hòa ADC.
    .saiSoVanTocGocX = 0, .saiSoVanTocGocY = 0, .saiSoVanTocGocZ = 0, // Khởi tạo độ lệch con quay bằng không.
    .trongLucGocX = 0, .trongLucGocY = 0, .trongLucGocZ = 9.81f // Đặt trọng lực gốc theo trục Z [Nguồn: Android FallDetectionConfig.DEFAULT].
};

static bool ghiI2cMotByte(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t data) { // Ghi một byte vào thanh ghi I2C.
    wire.beginTransmission(devAddr); // Mở phiên truyền đến địa chỉ thiết bị.
    wire.write(regAddr); // Gửi địa chỉ thanh ghi đích.
    wire.write(data); // Gửi byte dữ liệu cần ghi.
    return (wire.endTransmission() == 0); // Thành công khi bus trả mã không lỗi.
}

static bool docI2cNhieuByte(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t *boDem, size_t length) { // Đọc nhiều byte liên tiếp từ thanh ghi I2C.
    wire.beginTransmission(devAddr); // Mở phiên truyền đến địa chỉ thiết bị.
    wire.write(regAddr); // Chọn thanh ghi bắt đầu đọc.
    if (wire.endTransmission(false) != 0) return false; // Giữ bus cho lần đọc lặp lại và báo lỗi nếu thiết bị không phản hồi.
    size_t count = wire.requestFrom(devAddr, (uint8_t)length); // Yêu cầu đúng số byte mà nơi gọi cần.
    if (count != length) return false; // Từ chối dữ liệu thiếu để tránh dùng mẫu hỏng.
    for (size_t i = 0; i < length; i++) { // Sao chép lần lượt toàn bộ byte nhận được.
        boDem[i] = wire.read(); // Lưu byte hiện tại vào vùng đệm đầu ra.
    }
    return true; // Báo đã đọc đủ dữ liệu.
}

// Bộ quét I2C lúc khởi động in từng địa chỉ phản hồi theo dạng 0xNN.
// Hàm không dừng hệ thống; nơi gọi quyết định tiêu chí đạt hoặc lỗi cho từng bus.
static void quetI2c(TwoWire &wire, const char *busLabel) { // Quét các địa chỉ hợp lệ trên một bus I2C.
    Serial.printf("[I2C] Quet bus %s: ", busLabel); // In nhãn bus bằng tiếng Việt không dấu và giữ nguyên %s.
    bool timThayThietBi = false; // Theo dõi việc có ít nhất một thiết bị phản hồi.
    for (uint8_t addr = 1; addr < 127; addr++) { // Quét dải địa chỉ I2C 7 bit hợp lệ [Nguồn: đặc tả I2C].
        wire.beginTransmission(addr); // Thăm dò địa chỉ hiện tại.
        if (wire.endTransmission() == 0) { // Địa chỉ tồn tại khi thiết bị xác nhận truyền.
            Serial.printf("0x%02X ", addr); // In địa chỉ, giữ nguyên định dạng %02X.
            timThayThietBi = true; // Ghi nhận đã tìm thấy thiết bị.
        }
    }
    if (!timThayThietBi) Serial.print("(khong co)"); // Báo bus trống bằng tiếng Việt không dấu.
    Serial.println(); // Kết thúc dòng kết quả quét.
}

static bool khoiTaoMpu6050() { // Tìm, xác minh và cấu hình MPU6050.
    uint8_t maChip = 0; // Nhận giá trị WHO_AM_I đọc từ cảm biến.
    camBienMpu.diaChi = MPU6050_ADDR_A; // Thử địa chỉ 0x68 trước.
    if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_WHO_AM_I, &maChip, 1) || maChip != 0x68) { // Xác minh mã chip 0x68 [Nguồn: bảng dữ liệu MPU6050].
        camBienMpu.diaChi = MPU6050_ADDR_B; // Chuyển sang địa chỉ dự phòng 0x69.
        if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_WHO_AM_I, &maChip, 1) || maChip != 0x68) { // Xác minh lại mã chip tại địa chỉ dự phòng.
            camBienMpu.dangHoatDong = false; // Đánh dấu ngoại tuyến khi cả hai địa chỉ đều thất bại.
            return false; // Báo khởi tạo không thành công.
        }
    }

    // 1. Đánh thức cảm biến và chọn PLL theo con quay trục X.
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_PWR_MGMT_1, 0x01); // Chọn nguồn xung PLL trục X [Nguồn: bảng thanh ghi MPU6050].
    delay(10); // Chờ 10 ms để cảm biến ổn định [Nguồn: trình tự khởi tạo hiện có].
    // 2. SMPLRT_DIV = 9 cho tần số lấy mẫu nội 1 kHz / (1 + 9) = 100 Hz (FIX 8).
    // Nguồn: esp/node/firmware/esp_node/node_config.h, kSmplrtDiv = 9.
    // Khớp với lịch vòng lặp chính 100 Hz và cấu hình nút đã được kiểm chứng.
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_SMPLRT_DIV, 0x09); // Bộ chia 9 tạo 100 Hz [Nguồn: node_config.h kSmplrtDiv = 9].
    // 3. CONFIG: DLPF_CFG = 1, gia tốc 184 Hz, trễ 2,0 ms, con quay 188 Hz [Nguồn: node_config.h kDlpfCfg = 1].
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_CONFIG, 0x01); // DLPF_CFG 1 đặt băng thông đã chọn [Nguồn: node_config.h kDlpfCfg = 1].
    // 4. GYRO_CONFIG: FS_SEL = 3, dải ±2000 dps, giá trị 0x18 để tránh bão hòa khi xoay mạnh [Nguồn: bảng dữ liệu MPU6050].
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_GYRO_CONFIG, 0x18); // Chọn thang ±2000 dps [Nguồn: bảng thanh ghi MPU6050].
    // 5. ACCEL_CONFIG: AFS_SEL = 3, dải ±16 g, giá trị 0x18 để tránh bão hòa tại va chạm 25 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT và bảng dữ liệu MPU6050].
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_ACCEL_CONFIG, 0x18); // Chọn thang ±16 g để chịu va chạm 25 m/s² [Nguồn: Android FallDetectionConfig.DEFAULT].

    camBienMpu.dangHoatDong = true; // Đánh dấu cảm biến sẵn sàng sau cấu hình.
    return true; // Báo khởi tạo thành công.
}

static bool docMpu6050(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) { // Đọc một mẫu gia tốc và con quay từ MPU6050.
    if (!camBienMpu.dangHoatDong) return false; // Không truy cập bus khi cảm biến đang ngoại tuyến.
    uint8_t duLieuTho[14]; // Chứa 14 byte liên tiếp gồm gia tốc, nhiệt độ và con quay.
    if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_ACCEL_XOUT_H, duLieuTho, 14)) { // Đọc trọn khối thanh ghi đo.
        return false; // Báo lỗi nếu không nhận đủ dữ liệu.
    }

    int16_t rawAx = (int16_t)((duLieuTho[0] << 8) | duLieuTho[1]); // Ghép gia tốc thô trục X theo thứ tự byte cao trước.
    int16_t rawAy = (int16_t)((duLieuTho[2] << 8) | duLieuTho[3]); // Ghép gia tốc thô trục Y.
    int16_t rawAz = (int16_t)((duLieuTho[4] << 8) | duLieuTho[5]); // Ghép gia tốc thô trục Z.
    // Hai byte duLieuTho[6..7] chứa nhiệt độ và không dùng trong thuật toán này.
    int16_t rawGx = (int16_t)((duLieuTho[8] << 8) | duLieuTho[9]); // Ghép tốc độ góc thô trục X.
    int16_t rawGy = (int16_t)((duLieuTho[10] << 8) | duLieuTho[11]); // Ghép tốc độ góc thô trục Y.
    int16_t rawGz = (int16_t)((duLieuTho[12] << 8) | duLieuTho[13]); // Ghép tốc độ góc thô trục Z.

    // Kiểm tra bão hòa ADC ±16 g khi dữ liệu thô tiến gần ±32767 [Nguồn: bảng dữ liệu MPU6050].
    if (abs(rawAx) >= 32700 || abs(rawAy) >= 32700 || abs(rawAz) >= 32700) { // Ngưỡng 32700 phát hiện giá trị gần giới hạn ADC [Nguồn: giới hạn int16_t của MPU6050].
        camBienMpu.biBaoHoa = true; // Ghi nhận mẫu gia tốc đã bão hòa.
    } else { // Xử lí trường hợp gia tốc chưa bão hòa.
        camBienMpu.biBaoHoa = false; // Xóa cờ khi cả ba trục còn trong thang đo.
    }

    // Các hệ số sau đổi dữ liệu ADC thô sang đơn vị vật lý.
    // Thang ±16 g có 2048 LSB/g và 1 g bằng 9,80665 m/s² [Nguồn: bảng dữ liệu MPU6050 và gia tốc trọng trường chuẩn].
    const float thangDoGiaToc = 9.80665f / 2048.0f; // Hệ số đổi LSB sang m/s² [Nguồn: bảng dữ liệu MPU6050].
    ax = (float)rawAx * thangDoGiaToc; // Chuyển gia tốc trục X sang m/s².
    ay = (float)rawAy * thangDoGiaToc; // Chuyển gia tốc trục Y sang m/s².
    az = (float)rawAz * thangDoGiaToc; // Chuyển gia tốc trục Z sang m/s².

    // Thang ±2000 dps có độ nhạy 16,4 LSB/(độ/giây) [Nguồn: bảng dữ liệu MPU6050].
    const float thangDoVanTocGoc = 1.0f / 16.4f; // Hệ số đổi LSB sang độ/giây [Nguồn: bảng dữ liệu MPU6050].
    gx = ((float)rawGx * thangDoVanTocGoc) - camBienMpu.saiSoVanTocGocX; // Chuyển và bù độ lệch trục X.
    gy = ((float)rawGy * thangDoVanTocGoc) - camBienMpu.saiSoVanTocGocY; // Chuyển và bù độ lệch trục Y.
    gz = ((float)rawGz * thangDoVanTocGoc) - camBienMpu.saiSoVanTocGocZ; // Chuyển và bù độ lệch trục Z.

    return true; // Báo mẫu đã được đọc và chuyển đổi đầy đủ.
}

// ============================================================================
// 6. DRIVER MS5611 KHÔNG CHẶN Ở MỨC THANH GHI (bus 1: Wire1, GPIO 7 và GPIO 6)
// ============================================================================
#define MS5611_ADDR_A 0x77 // Địa chỉ I2C thứ nhất của MS5611 [Nguồn: bảng dữ liệu MS5611].
#define MS5611_ADDR_B 0x76 // Địa chỉ I2C thứ hai của MS5611 [Nguồn: bảng dữ liệu MS5611].
#define MS5611_CMD_RESET 0x1E // Lệnh đặt lại cảm biến [Nguồn: MS5611 AN520].
#define MS5611_CMD_PROM_RD 0xA0 // Lệnh gốc để đọc PROM [Nguồn: MS5611 AN520].
#define MS5611_CMD_CONV_D1 0x48 // Lệnh đổi áp suất D1 ở OSR 4096 [Nguồn: MS5611 AN520].
#define MS5611_CMD_CONV_D2 0x58 // Lệnh đổi nhiệt độ D2 ở OSR 4096 [Nguồn: MS5611 AN520].
#define MS5611_CMD_ADC_RD 0x00 // Lệnh đọc kết quả ADC [Nguồn: MS5611 AN520].

enum TrangThaiMs5611 { // Mô tả các pha của phép đo MS5611 không chặn.
    MS_IDLE, // Driver đang rỗi và sẵn sàng bắt đầu D1.
    MS_CONV_D1_WAIT, // Driver đang chờ chuyển đổi áp suất hoàn tất.
    MS_CONV_D2_WAIT // Driver đang chờ chuyển đổi nhiệt độ hoàn tất.
};

struct TrangThaiCamBienKhiAp { // Gom dữ liệu hiệu chuẩn và trạng thái MS5611.
    uint8_t diaChi; // Lưu địa chỉ I2C đang phản hồi.
    bool dangHoatDong; // Cho biết cảm biến có sẵn sàng đo hay không.
    bool crcHopLe; // Cho biết CRC4 của PROM có hợp lệ hay không.
    bool coLoiDoc; // Ghi nhận lỗi đọc gần nhất.
    uint16_t c[8]; // Lưu tám từ PROM hiệu chuẩn.
    TrangThaiMs5611 giaTriTrangThai; // Lưu pha hiện tại của máy trạng thái.
    uint32_t thoiDiemBatDauChuyenDoiUs; // Ghi thời điểm bắt đầu chuyển đổi ADC.
    uint32_t duLieuThoApSuat; // Lưu kết quả D1 thô.
    uint32_t duLieuThoNhietDo; // Lưu kết quả D2 thô.
    float apSuatPa; // Lưu áp suất đã bù theo Pascal.
    float nhietDoC; // Lưu nhiệt độ đã bù theo độ C.
    float chenhLechDoCaoM; // Lưu chênh lệch độ cao tương đối (FIX 3).
};

static TrangThaiCamBienKhiAp camBienKhiAp = { // Khởi tạo driver khí áp ở trạng thái chưa kết nối.
    .diaChi = MS5611_ADDR_A, // Thử địa chỉ 0x77 trước.
    .dangHoatDong = false, // Chưa xác nhận cảm biến trực tuyến.
    .crcHopLe = false, // Chưa kiểm tra CRC của PROM.
    .coLoiDoc = false, // Chưa ghi nhận lỗi đọc.
    .c = {0}, // Xóa các hệ số hiệu chuẩn trước khi đọc PROM.
    .giaTriTrangThai = MS_IDLE, // Bắt đầu ở pha rỗi.
    .thoiDiemBatDauChuyenDoiUs = 0, // Chưa có phép chuyển đổi đang chạy.
    .duLieuThoApSuat = 0, // Chưa có mẫu áp suất thô.
    .duLieuThoNhietDo = 0, // Chưa có mẫu nhiệt độ thô.
    .apSuatPa = 101325.0f, // Giá trị mốc khí quyển tiêu chuẩn [Nguồn: Kế hoạch §6.2].
    .nhietDoC = 25.0f, // Giá trị khởi tạo 25 °C [Nguồn: giá trị mặc định firmware].
    .chenhLechDoCaoM = 0.0f // Chênh lệch ban đầu bằng không so với mốc.
};

// Tính CRC4 của MS5611 theo thuật toán trong tài liệu ứng dụng AN520.
static bool kiemTraCrc4Ms5611(uint16_t prom[]) { // Xác minh tính toàn vẹn của tám từ PROM.
    uint32_t soMauConLai = 0; // Tích lũy phần dư của phép chia CRC.
    uint16_t SaoLuuProm[8]; // Giữ bản sao để không sửa PROM gốc.
    for (int i = 0; i < 8; i++) SaoLuuProm[i] = prom[i]; // Sao chép đủ tám từ PROM.
    uint16_t crcDocDuoc = SaoLuuProm[7] & 0x000F; // Tách bốn bit CRC đã lưu [Nguồn: MS5611 AN520].
    SaoLuuProm[7] = SaoLuuProm[7] & 0xFF00; // Xóa nibble CRC trước khi tính lại [Nguồn: MS5611 AN520].

    for (int cnt = 0; cnt < 16; cnt++) { // Xử lý lần lượt 16 byte PROM.
        if (cnt % 2 == 1) { // Chọn byte thấp ở lượt lẻ.
            soMauConLai ^= (uint16_t)(SaoLuuProm[cnt >> 1] & 0x00FF); // Trộn byte thấp vào phần dư.
        } else { // Chọn byte cao ở lượt chẵn.
            soMauConLai ^= (uint16_t)(SaoLuuProm[cnt >> 1] >> 8); // Trộn byte cao vào phần dư.
        }
        for (uint8_t n_bit = 8; n_bit > 0; n_bit--) { // Dịch qua tám bit của byte hiện tại.
            if (soMauConLai & 0x8000) { // Áp dụng đa thức khi bit cao đang bật.
                soMauConLai = (soMauConLai << 1) ^ 0x3000; // Dùng đa thức CRC 0x3000 [Nguồn: MS5611 AN520].
            } else { // Xử lí khi bit cao không bật.
                soMauConLai = (soMauConLai << 1); // Chỉ dịch khi bit cao không bật.
            }
        }
    }
    soMauConLai = (0x000F & (soMauConLai >> 12)); // Rút bốn bit CRC tính được.
    return (soMauConLai == crcDocDuoc); // Hợp lệ khi CRC tính lại khớp PROM.
}

static bool khoiTaoMs5611() { // Tìm cảm biến, đọc PROM và khởi tạo máy trạng thái.
    camBienKhiAp.diaChi = MS5611_ADDR_A; // Thử địa chỉ 0x77 trước.
    Wire1.beginTransmission(camBienKhiAp.diaChi); // Thăm dò địa chỉ thứ nhất.
    if (Wire1.endTransmission() != 0) { // Thử địa chỉ dự phòng nếu địa chỉ thứ nhất không phản hồi.
        camBienKhiAp.diaChi = MS5611_ADDR_B; // Chuyển sang địa chỉ dự phòng 0x76.
        Wire1.beginTransmission(camBienKhiAp.diaChi); // Thăm dò địa chỉ thứ hai.
        if (Wire1.endTransmission() != 0) { // Xác định cảm biến ngoại tuyến nếu địa chỉ dự phòng cũng thất bại.
            camBienKhiAp.dangHoatDong = false; // Đánh dấu ngoại tuyến khi cả hai địa chỉ thất bại.
            return false; // Báo không tìm thấy MS5611.
        }
    }

    // Gửi lệnh đặt lại để cảm biến nạp lại hệ số PROM.
    Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên I2C đến cảm biến đã tìm thấy.
    Wire1.write(MS5611_CMD_RESET); // Gửi lệnh đặt lại theo AN520.
    Wire1.endTransmission(); // Hoàn tất lệnh đặt lại.
    delay(10); // Chờ 10 ms để đặt lại hoàn tất [Nguồn: trình tự khởi tạo hiện có].

    // Đọc tám từ PROM chứa hệ số hiệu chuẩn và CRC.
    for (uint8_t i = 0; i < 8; i++) { // Lặp qua đủ tám từ PROM [Nguồn: MS5611 AN520].
        Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên đọc từ PROM.
        Wire1.write(MS5611_CMD_PROM_RD + (i * 2)); // Chọn địa chỉ từ PROM thứ i.
        if (Wire1.endTransmission(false) != 0) { // Báo lỗi nếu không chọn được từ PROM cần đọc.
            camBienKhiAp.dangHoatDong = false; // Đánh dấu ngoại tuyến khi chọn PROM thất bại.
            return false; // Dừng khởi tạo để không dùng hệ số thiếu.
        }
        if (Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)2) != 2) { // Yêu cầu đúng hai byte của mỗi từ PROM.
            camBienKhiAp.dangHoatDong = false; // Đánh dấu ngoại tuyến khi dữ liệu bị thiếu.
            return false; // Dừng khởi tạo để tránh hệ số hỏng.
        }
        camBienKhiAp.c[i] = (Wire1.read() << 8) | Wire1.read(); // Ghép từ PROM theo thứ tự byte cao trước.
    }

    camBienKhiAp.crcHopLe = kiemTraCrc4Ms5611(camBienKhiAp.c); // Kiểm tra CRC4 của hệ số hiệu chuẩn.
    camBienKhiAp.dangHoatDong = true; // Cho phép đo sau khi đọc đủ PROM.
    camBienKhiAp.coLoiDoc = false; // Xóa lỗi đọc cũ.
    camBienKhiAp.giaTriTrangThai = MS_IDLE; // Đưa driver về pha rỗi.
    return true; // Báo khởi tạo hoàn tất.
}

static void tinhToanMs5611() { // Bù nhiệt độ và tính áp suất từ dữ liệu ADC thô.
    int64_t dt = (int64_t)camBienKhiAp.duLieuThoNhietDo - ((int64_t)camBienKhiAp.c[5] << 8); // Tính chênh lệch nhiệt độ theo AN520.
    int64_t temp = 2000 + ((dt * (int64_t)camBienKhiAp.c[6]) >> 23); // Tính nhiệt độ bậc nhất theo đơn vị 0,01 °C [Nguồn: MS5611 AN520].

    int64_t off = ((int64_t)camBienKhiAp.c[2] << 16) + (((int64_t)camBienKhiAp.c[4] * dt) >> 7); // Tính độ lệch áp suất bậc nhất [Nguồn: MS5611 AN520].
    int64_t sens = ((int64_t)camBienKhiAp.c[1] << 15) + (((int64_t)camBienKhiAp.c[3] * dt) >> 8); // Tính độ nhạy áp suất bậc nhất [Nguồn: MS5611 AN520].

    // Bù nhiệt độ bậc hai khi dưới 20,00 °C [Nguồn: MS5611 AN520].
    if (temp < 2000) { // Ngưỡng 2000 tương ứng 20,00 °C [Nguồn: MS5611 AN520].
        int64_t t2 = (dt * dt) >> 31; // Tính thành phần bù nhiệt độ bậc hai.
        int64_t off2 = 5 * ((temp - 2000) * (temp - 2000)) >> 1; // Tính thành phần bù độ lệch [Nguồn: MS5611 AN520].
        int64_t sens2 = 5 * ((temp - 2000) * (temp - 2000)) >> 2; // Tính thành phần bù độ nhạy [Nguồn: MS5611 AN520].
        if (temp < -1500) { // Ngưỡng -1500 tương ứng -15,00 °C [Nguồn: MS5611 AN520].
            off2 += 7 * ((temp + 1500) * (temp + 1500)); // Bổ sung bù độ lệch ở nhiệt độ rất thấp.
            sens2 += 11 * ((temp + 1500) * (temp + 1500)) >> 1; // Bổ sung bù độ nhạy ở nhiệt độ rất thấp.
        }
        temp -= t2; // Áp dụng phần bù nhiệt độ.
        off -= off2; // Áp dụng phần bù độ lệch.
        sens -= sens2; // Áp dụng phần bù độ nhạy.
    }

    int64_t p = ((((int64_t)camBienKhiAp.duLieuThoApSuat * sens) >> 21) - off) >> 15; // Tính áp suất đã bù theo AN520.
    camBienKhiAp.apSuatPa = (float)p; // Kết quả là Pascal; 100000 Pa bằng 1000,00 mbar [Nguồn: MS5611 AN520].
    camBienKhiAp.nhietDoC = (float)temp / 100.0f; // Đổi đơn vị 0,01 °C sang °C.

    // Chênh lệch độ cao tương đối (Kế hoạch §6.2, FIX 3):
    // chenhLechDoCaoM = 44330 * (1 - (áp suất hiện tại / áp suất tham chiếu)^(1/5.255)).
    // Giá trị dương khi đi lên và âm khi đi xuống.
    if (camBienKhiAp.apSuatPa > 10000.0f && apSuatThamChieuPa > 10000.0f) { // Ngưỡng 10000 Pa loại dữ liệu áp suất phi thực tế [Nguồn: suy luận kiểm tra hợp lệ hệ thống].
        camBienKhiAp.chenhLechDoCaoM = 44330.0f * (1.0f - powf(camBienKhiAp.apSuatPa / apSuatThamChieuPa, 1.0f / 5.255f)); // Đổi tỷ lệ áp suất thành độ cao [Nguồn: công thức khí quyển chuẩn, Kế hoạch §6.2].
    }
}

static void docMs5611KhongChan(uint32_t nowUs) { // Tiến máy trạng thái đo khí áp mà không chặn vòng lặp.
    if (!camBienKhiAp.dangHoatDong) return; // Bỏ qua phép đo khi cảm biến ngoại tuyến.

    switch (camBienKhiAp.giaTriTrangThai) { // Chọn hành vi theo pha chuyển đổi hiện tại.
        case MS_IDLE: // Bắt đầu một chu kỳ đo mới khi driver rỗi.
            Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên gửi lệnh D1.
            Wire1.write(MS5611_CMD_CONV_D1); // Bắt đầu đo áp suất ở OSR 4096 [Nguồn: MS5611 AN520].
            if (Wire1.endTransmission() == 0) { // Chỉ chuyển pha khi cảm biến nhận lệnh.
                camBienKhiAp.thoiDiemBatDauChuyenDoiUs = nowUs; // Ghi mốc bắt đầu để chờ không chặn.
                camBienKhiAp.giaTriTrangThai = MS_CONV_D1_WAIT; // Chuyển sang pha chờ D1.
            } else { // Xử lí lỗi gửi lệnh D1.
                camBienKhiAp.coLoiDoc = true; // Ghi nhận lệnh đo áp suất thất bại.
                maLoiCuoiCung = "BAROMETER_READ_FAILED"; // Ghi mã lỗi chuẩn để ứng dụng biết lần đọc khí áp đã thất bại.
            }
            break; // Kết thúc nhánh rỗi.

        case MS_CONV_D1_WAIT: // Chờ và thu kết quả áp suất D1.
            // OSR 4096 cần tối đa 9,04 ms [Nguồn: MS5611 AN520].
            if (nowUs - camBienKhiAp.thoiDiemBatDauChuyenDoiUs >= 9500) { // Chờ 9500 us để có biên an toàn trên 9,04 ms [Nguồn: MS5611 AN520].
                Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên gửi lệnh đọc ADC.
                Wire1.write(MS5611_CMD_ADC_RD); // Chọn thanh ghi kết quả ADC.
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)3) == 3) { // Yêu cầu đủ ba byte kết quả D1.
                    camBienKhiAp.duLieuThoApSuat = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read(); // Ghép kết quả áp suất 24 bit.
                    // Bắt đầu chuyển đổi D2 để đo nhiệt độ.
                    Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên gửi lệnh D2.
                    Wire1.write(MS5611_CMD_CONV_D2); // Gửi lệnh bắt đầu chuyển đổi nhiệt độ D2.
                    if (Wire1.endTransmission() == 0) { // Chỉ chuyển pha khi cảm biến nhận lệnh D2.
                        camBienKhiAp.thoiDiemBatDauChuyenDoiUs = nowUs; // Ghi mốc bắt đầu D2.
                        camBienKhiAp.giaTriTrangThai = MS_CONV_D2_WAIT; // Chuyển sang pha chờ nhiệt độ.
                    } else { // Xử lí lỗi gửi lệnh D2.
                        camBienKhiAp.coLoiDoc = true; // Ghi nhận lệnh D2 thất bại.
                        maLoiCuoiCung = "BAROMETER_READ_FAILED"; // Ghi mã lỗi chuẩn khi cảm biến không nhận được lệnh bắt đầu đo nhiệt độ.
                        camBienKhiAp.giaTriTrangThai = MS_IDLE; // Trở về rỗi để thử lại chu kỳ sau.
                    }
                } else { // Xử lí lỗi đọc kết quả D1.
                    camBienKhiAp.coLoiDoc = true; // Ghi nhận lỗi đọc kết quả D1.
                    maLoiCuoiCung = "BAROMETER_READ_FAILED"; // Ghi mã lỗi chuẩn khi không đọc đủ ba byte kết quả áp suất D1.
                    camBienKhiAp.giaTriTrangThai = MS_IDLE; // Trở về rỗi sau lỗi.
                }
            }
            break; // Kết thúc nhánh chờ D1.

        case MS_CONV_D2_WAIT: // Chờ và thu kết quả nhiệt độ D2.
            if (nowUs - camBienKhiAp.thoiDiemBatDauChuyenDoiUs >= 9500) { // Chờ 9500 us để vượt thời gian tối đa 9,04 ms [Nguồn: MS5611 AN520].
                Wire1.beginTransmission(camBienKhiAp.diaChi); // Mở phiên gửi lệnh đọc ADC.
                Wire1.write(MS5611_CMD_ADC_RD); // Chọn thanh ghi kết quả ADC cho D2.
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)3) == 3) { // Yêu cầu đủ ba byte kết quả D2.
                    camBienKhiAp.duLieuThoNhietDo = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read(); // Ghép kết quả nhiệt độ 24 bit.
                    tinhToanMs5611(); // Tính nhiệt độ, áp suất và chênh lệch độ cao đã bù.
                    camBienKhiAp.coLoiDoc = false; // Xóa cờ lỗi khi chu kỳ hoàn tất.
                } else { // Xử lí lỗi đọc kết quả D2.
                    camBienKhiAp.coLoiDoc = true; // Ghi nhận lỗi đọc kết quả D2.
                    maLoiCuoiCung = "BAROMETER_READ_FAILED"; // Ghi mã lỗi chuẩn khi không đọc đủ ba byte kết quả nhiệt độ D2.
                }
                camBienKhiAp.giaTriTrangThai = MS_IDLE; // Trở về rỗi để sẵn sàng cho chu kỳ tiếp theo.
            }
            break; // Kết thúc nhánh chờ D2.
    }
}

// ============================================================================
// 7. TRẠNG THÁI HỆ THỐNG, BỘ ĐẾM VÀ MÁY TRẠNG THÁI
// ============================================================================
struct BoDemHeThong { // Gom các bộ đếm dùng để theo dõi hoạt động của hệ thống.
    uint32_t soMauImuDaDoc; // Đếm số mẫu IMU đã đọc.
    uint32_t soMauKhiApDaDoc; // Đếm số mẫu khí áp đã đọc.
    uint32_t soLanTreChuKy; // Đếm số chu kỳ xử lí bị trễ.
    uint32_t suKienNgaDaXacMinh; // Đếm số sự kiện ngã đã xác minh.
    uint32_t lanBamSos; // Đếm số lần kích hoạt SOS.
    uint32_t goiBleBiBo; // Đếm số gói BLE bị loại.
};

static BoDemHeThong boDemHeThong = {0}; // Khởi tạo mọi bộ đếm bằng 0 [Nguồn: trạng thái khởi động firmware].
static TrangThaiThietBi trangThaiThietBi = STATE_BOOT_SELF_TEST; // Bắt đầu ở trạng thái tự kiểm tra [Nguồn: máy trạng thái firmware].
static TrangThaiThietBi trangThaiTruocDo = STATE_BOOT_SELF_TEST; // Ghi nhận trạng thái trước đó khi khởi động [Nguồn: máy trạng thái firmware].
static bool nhpTimBat = true; // Bật nhịp tim Serial mặc định để học sinh luôn thấy firmware còn hoạt động.
static uint32_t thoiDiemNhipTimTruoc = 0; // Giữ mốc nhịp tim qua các vòng lặp để lập lịch không chặn bằng millis().
static void inMenuSerial() { // Gom đủ tám phím vào một menu ngắn để học sinh tra cứu từ Serial.
    Serial.println(F("===== MENU =====")); // In dấu đầu khối để menu nổi bật giữa luồng dữ liệu.
    Serial.println(F("r : Bat/tat che do in du lieu tho (CSV)")); // Giải thích phím r điều khiển dữ liệu thô.
    Serial.println(F("t : In bang nguong phat hien nga")); // Giải thích phím t hiển thị các ngưỡng phát hiện ngã.
    Serial.println(F("c : Chay lai hieu chuan cam bien")); // Giải thích phím c chạy lại hiệu chuẩn cảm biến.
    Serial.println(F("s : In trang thai he thong")); // Giải thích phím s hiển thị trạng thái hệ thống.
    Serial.println(F("h : In tro giup (menu nay)")); // Giải thích phím h vẫn là lệnh trợ giúp cũ.
    Serial.println(F("m : In MENU nay")); // Giải thích phím m in menu đầy đủ này.
    Serial.println(F("p : In khoi TIEN TRINH (6 buoc)")); // Giải thích phím p hiển thị sáu bước vận hành.
    Serial.println(F("d : Bat/tat nhip tim (5 giay mot lan)")); // Giải thích phím d điều khiển bản tin sống mỗi 5000 ms.
    Serial.println(F("=================")); // In dấu cuối khối để phân cách menu với dữ liệu tiếp theo.
} // Kết thúc hàm in menu Serial đầy đủ.
static void inDongTienTrinh(uint8_t buoc, uint8_t buocHienTai, const char* tenBuoc) { // Dùng chung định dạng để sáu dòng tiến trình có dấu nhất quán.
    const char* dauBuoc = (buoc < buocHienTai) ? "x" : ((buoc == buocHienTai) ? ">" : " "); // Chọn đã xong, hiện tại hoặc chưa tới theo số bước.
    Serial.printf(" [%s] %u. %-17s%s\n", dauBuoc, buoc, tenBuoc, (buoc == buocHienTai) ? " <== dang o buoc nay" : ""); // In đúng một dòng và chỉ chú thích bước hiện tại.
} // Kết thúc hàm định dạng một dòng tiến trình.
static void inKhoiTienTrinh() { // Chuyển trạng thái máy thành khối sáu bước chỉ dành cho giao diện Serial.
    uint8_t buocHienTai = 0; // Dùng 0 cho trạng thái suy giảm để không giả nhận một bước vận hành đang chạy.
    switch (trangThaiThietBi) { // Ánh xạ trực tiếp từng trạng thái giao thức sang số bước hiển thị.
        case STATE_BOOT_SELF_TEST: buocHienTai = 1; break; // Trạng thái tự kiểm tra tương ứng bước khởi động.
        case STATE_CALIBRATING: buocHienTai = 2; break; // Trạng thái hiệu chuẩn tương ứng bước hai.
        case STATE_MONITORING: buocHienTai = 3; break; // Trạng thái theo dõi tương ứng bước ba.
        case STATE_SUSPECTED: buocHienTai = 4; break; // Trạng thái nghi ngờ ngã tương ứng bước bốn.
        case STATE_VERIFYING: buocHienTai = 5; break; // Trạng thái xác minh ngã tương ứng bước năm.
        case STATE_LOCAL_ALERTING: buocHienTai = 6; break; // Trạng thái báo động cục bộ tương ứng bước sáu.
        case STATE_DEGRADED: buocHienTai = 0; break; // Trạng thái suy giảm đứng ngoài sáu bước và có cảnh báo riêng.
        default: buocHienTai = 0; break; // Trạng thái lạ không được đánh dấu nhầm là một bước hợp lệ.
    } // Kết thúc ánh xạ trạng thái sang bước tiến trình.
    Serial.println(F("========== KHOI TIEN TRINH ==========")); // In dấu đầu khối để dễ nhận biết trên Serial Monitor.
    inDongTienTrinh(1, buocHienTai, "BOOT / TU KIEM TRA"); // In bước một và dấu tiến độ phù hợp.
    inDongTienTrinh(2, buocHienTai, "HIEU CHUAN"); // In bước hai và dấu tiến độ phù hợp.
    inDongTienTrinh(3, buocHienTai, "THEO DOI"); // In bước ba và dấu tiến độ phù hợp.
    inDongTienTrinh(4, buocHienTai, "NGHI NGA"); // In bước bốn và dấu tiến độ phù hợp.
    inDongTienTrinh(5, buocHienTai, "XAC MINH"); // In bước năm và dấu tiến độ phù hợp.
    inDongTienTrinh(6, buocHienTai, "BAO DONG"); // In bước sáu và dấu tiến độ phù hợp.
    Serial.println(F("=====================================")); // In dấu cuối khối để tách khỏi bản tin tiếp theo.
    if (trangThaiThietBi == STATE_DEGRADED) Serial.println(F(" [!] CANH BAO: thiet bi dang SUY GIAM (thieu cam bien)")); // Cảnh báo riêng vì suy giảm không thuộc sáu bước bình thường.
} // Kết thúc hàm in khối tiến trình.
static void inNhipTim(uint32_t thoiGianHienTaiMs) { // In một bản tin sống từ dữ liệu sẵn có mà không thay đổi cảm biến hay giao thức.
    Serial.printf("[NHIP TIM] Thoi gian chay: %.1f s | Trang thai: %s | Mau trong bo dem: %u | Mau bi bo: %u | ", thoiGianHienTaiMs / 1000.0f, tenTrangThaiThietBi(trangThaiThietBi), soMauTrongBoDem, soMauBiBo); // In thời gian, trạng thái và hai bộ đếm mẫu đúng ý nghĩa.
    if (phanTramPin < 0) Serial.print(F("Pin: chua co pin | ")); else Serial.printf("Pin: %d%% | ", phanTramPin); // Phân biệt rõ phần cứng pin chưa có với phần trăm pin hợp lệ.
    if (WiFi.status() == WL_CONNECTED) Serial.printf("WiFi RSSI: %d dBm\n", WiFi.RSSI()); else Serial.println(F("WiFi: chua ket noi")); // Chỉ đọc RSSI khi WiFi đang kết nối để tránh số tín hiệu vô nghĩa.
} // Kết thúc hàm in nhịp tim Serial.

// Các biến của máy trạng thái phát hiện ngã, đồng nhất với Android.
struct TrangThaiXacMinhNga { // Lưu dữ liệu trong cửa sổ xác minh một lần ngã.
    uint32_t thoiDiemVaChamMs; // Lưu thời điểm phát hiện va chạm theo mili giây.
    float giaTocVaChamCucDai; // Lưu gia tốc va chạm cực đại.
    float giaTocNhoNhatTruocVaCham; // Lưu gia tốc nhỏ nhất trước va chạm.
    uint32_t thoiDiemBatDauDungYenMs; // Lưu thời điểm bắt đầu đứng yên.
    uint32_t soMauDungYen; // Đếm số mẫu đứng yên liên tiếp.
    uint32_t mauCuoiCungMs; // Lưu thời điểm mẫu gần nhất.
    bool dangTrongCuaSoXacMinh; // Cho biết đang xác minh sự kiện ngã.
    float trongLucTruocVaCham[3]; // Lưu vectơ trọng lực ba trục trước va chạm [Nguồn: mô hình phát hiện ngã].
    float trongLucSauVaCham[3]; // Lưu vectơ trọng lực ba trục sau va chạm [Nguồn: mô hình phát hiện ngã].
    float doCaoMoc; // Lưu độ cao mốc khi bắt đầu xác minh.
    char nguyenNhaKichHoat[96]; // Lưu chuỗi nguyên nhân kích hoạt tối đa 95 kí tự [Nguồn: kích thước bộ đệm giao thức].
};

static TrangThaiXacMinhNga trangThaiPhatHienNga = {0}; // Xóa trạng thái xác minh lúc khởi động [Nguồn: trạng thái khởi động firmware].

// Bộ định thời cảnh báo và còi.
static uint32_t thoiDiemBatDauCanhBaoMs = 0; // Chưa có cảnh báo khi khởi động [Nguồn: trạng thái khởi động firmware].
static uint32_t hanChoTruongCoiMs = 0; // Chưa đặt hạn phát mẫu còi khi khởi động [Nguồn: trạng thái khởi động firmware].
static uint16_t cheDoCanhBao = 0; // Chưa chọn mẫu còi khi khởi động [Nguồn: trạng thái khởi động firmware].

static bool ketNoiBle = false; // BLE chưa kết nối khi khởi động [Nguồn: trạng thái khởi động firmware].
static bool truyenBle = false; // Chưa truyền luồng BLE khi khởi động [Nguồn: trạng thái khởi động firmware].
static volatile uint16_t mtuBle = 23; // Dùng MTU BLE mặc định 23 byte [Nguồn: Bluetooth Core Specification].

// ============================================================================
// 8. TRIỂN KHAI MÁY CHỦ BLE GATT (Kế hoạch §8)
// ============================================================================
#if CO_BLE // Chỉ biên dịch phần máy chủ BLE khi cấu hình CO_BLE được bật.
static BLEServer *mayChuBle = nullptr; // Giữ con trỏ tới máy chủ BLE; nullptr nghĩa là máy chủ chưa được tạo.
static BLECharacteristic *dacTruTruyenDuLieu = nullptr; // Giữ con trỏ tới đặc trưng gửi định kỳ dữ liệu cảm biến.
static BLECharacteristic *dacTruSuKien = nullptr; // Giữ con trỏ tới đặc trưng phát các sự kiện như ngã hoặc SOS.
static BLECharacteristic *dacTruTrangThai = nullptr; // Giữ con trỏ tới đặc trưng gửi trạng thái hiện tại của thiết bị.
static BLECharacteristic *dacTruLenh = nullptr; // Giữ con trỏ tới đặc trưng nhận lệnh do ứng dụng ghi xuống.
static BLECharacteristic *dacTruXacNhan = nullptr; // Giữ con trỏ tới đặc trưng trả kết quả xác nhận lệnh cho ứng dụng.

static char lenhCuoiCung[37] = ""; // Dành 37 byte cho mã lệnh cuối: tối đa 36 kí tự và một byte kết thúc chuỗi.
static bool choXuLyLen = false; // Đánh dấu có lệnh BLE mới đang chờ vòng lặp chính xử lí.
static char lenhBleChoXuLy[128] = ""; // Dành 128 byte chứa lệnh BLE nhận được, gồm cả byte kết thúc chuỗi.

// Theo dõi eventId cố định khi truyền lại (§8.4).
static uint32_t boDemSuKien = 0; // Đếm số sự kiện đã tạo để góp phần làm mỗi mã sự kiện khác nhau.
static char maSuKienHienTai[32] = ""; // Dành 32 byte lưu mã sự kiện hiện tại để lần truyền lại vẫn dùng cùng mã.
static uint32_t thuTuSuKienHienTai = 0; // Lưu số thứ tự của sự kiện hiện tại để tái sử dụng khi truyền lại.

class ServerCallbacks : public BLEServerCallbacks { // Khai báo lớp xử lí hai thời điểm máy khách BLE kết nối và ngắt kết nối.
    void onConnect(BLEServer* mayChu) override { // Hàm này tự chạy khi một máy khách vừa kết nối tới máy chủ BLE.
        ketNoiBle = true; // Đánh dấu thiết bị đang có kết nối BLE.
        uint16_t mtu = mayChu->getPeerMTU(mayChu->getConnId()); // Đọc MTU mà máy khách đã thương lượng cho đúng kết nối hiện tại.
        if (mtu < 23) mtu = 23; // Nâng giá trị bất thường thấp hơn 23 lên MTU tối thiểu mặc định của BLE.
        if (mtu > 517) mtu = 517; // Giới hạn MTU ở 517 byte, mức lớn nhất mà giao thức ATT cho phép.
        mtuBle = mtu; // Lưu MTU hợp lệ để kiểm tra kích thước các gói gửi sau này.
        Serial.printf("[BLE] Da ket noi (connId=%u, peerMTU=%u)\n", // Bắt đầu in ra Serial mã kết nối và MTU của máy khách để người vận hành theo dõi.
                      mayChu->getConnId(), mtuBle); // Cung cấp mã kết nối và MTU cho hai vị trí định dạng trong dòng thông báo.
    }
    void onDisconnect(BLEServer* mayChu) override { // Hàm này tự chạy khi máy khách ngắt kết nối khỏi máy chủ BLE.
        ketNoiBle = false; // Đánh dấu thiết bị không còn kết nối BLE.
        truyenBle = false; // Tắt chế độ truyền liên tục vì bên nhận đã ngắt kết nối.
        mtuBle = 23; // Đưa MTU về 23 byte, giá trị mặc định trước khi có lần thương lượng mới.
        BLEDevice::startAdvertising(); // Khởi động quảng bá lại để điện thoại khác có thể tìm và kết nối thiết bị.
        Serial.println(F("[BLE] Da ngat ket noi, khoi dong lai quang ba (MTU dat lai 23)")); // In ra Serial rằng BLE đã ngắt, quảng bá được bật lại và MTU trở về 23.
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks { // Khai báo lớp xử lí dữ liệu mà ứng dụng ghi vào đặc trưng lệnh BLE.
    void onWrite(BLECharacteristic *dacTru) override { // Hàm này tự chạy mỗi khi ứng dụng ghi một giá trị vào đặc trưng lệnh.
        String giaTriNhan = dacTru->getValue(); // Đọc toàn bộ chuỗi lệnh vừa được ứng dụng gửi qua BLE.
        if (giaTriNhan.length() > 0 && giaTriNhan.length() < sizeof(lenhBleChoXuLy)) { // Chỉ nhận lệnh không rỗng và ngắn hơn bộ đệm 128 byte để tránh tràn.
            strncpy(lenhBleChoXuLy, giaTriNhan.c_str(), sizeof(lenhBleChoXuLy) - 1); // Chép tối đa 127 kí tự lệnh vào bộ đệm, chừa một byte kết thúc chuỗi.
            lenhBleChoXuLy[sizeof(lenhBleChoXuLy) - 1] = '\0'; // Đặt byte cuối bằng 0 để chuỗi luôn kết thúc an toàn dù đầu vào quá dài.
            choXuLyLen = true; // Báo cho vòng lặp chính biết bộ đệm đang chứa một lệnh cần xử lí.
        }
    }
};

// SỬA 2: Esp32CommandAck (§8.5)
// Các trường: protocolVersion · commandId · deviceId · timestampMs · commandStatus · errorCode · message
// commandStatus phải thuộc một trong các giá trị: ACCEPTED, COMPLETED, REJECTED, FAILED
static void guiBleAck(const char* cmdId, const char* trangThaiLenh, const char* maLoi, const char* thongDiep) { // Tạo và gửi gói JSON xác nhận kết quả thực hiện một lệnh BLE.
    if (!ketNoiBle || dacTruXacNhan == nullptr) return; // Không gửi xác nhận nếu chưa kết nối hoặc đặc trưng xác nhận chưa được tạo.
    char boDem[256]; // Dành 256 byte để ghép toàn bộ gói JSON xác nhận lệnh.
    char boDemLoi[48]; // Dành 48 byte để biểu diễn mã lỗi dưới dạng chuỗi JSON hoặc null.
    char boDemTinNhan[96]; // Dành 96 byte để biểu diễn thông điệp dưới dạng chuỗi JSON hoặc null.

    if (maLoi != nullptr) { // Kiểm tra bên gọi có cung cấp mã lỗi hay không.
        snprintf(boDemLoi, sizeof(boDemLoi), "\"%s\"", maLoi); // Nếu có mã lỗi, đặt mã đó trong dấu nháy để tạo một giá trị chuỗi JSON.
    } else { // Rẽ sang trường hợp không có mã lỗi.
        snprintf(boDemLoi, sizeof(boDemLoi), "null"); // Ghi giá trị null của JSON để biểu thị lệnh không có lỗi.
    }

    if (thongDiep != nullptr) { // Kiểm tra bên gọi có cung cấp thông điệp giải thích hay không.
        snprintf(boDemTinNhan, sizeof(boDemTinNhan), "\"%s\"", thongDiep); // Nếu có thông điệp, đặt nội dung trong dấu nháy để tạo chuỗi JSON.
    } else { // Rẽ sang trường hợp không có thông điệp.
        snprintf(boDemTinNhan, sizeof(boDemTinNhan), "null"); // Ghi giá trị null của JSON để biểu thị không có thông điệp kèm theo.
    }

    snprintf(boDem, sizeof(boDem), // Ghép các trường bên dưới thành một gói JSON xác nhận trong giới hạn bộ đệm.
             "{\"protocolVersion\":1,\"commandId\":\"%s\",\"deviceId\":\"%s\",\"timestampMs\":%llu," // Mở gói JSON phiên bản 1 và thêm mã lệnh, mã thiết bị cùng thời điểm gửi.
             "\"commandStatus\":\"%s\",\"errorCode\":%s,\"message\":%s}", // Thêm trạng thái lệnh, mã lỗi và thông điệp rồi đóng gói JSON xác nhận.
             cmdId ? cmdId : "", // Đưa mã lệnh vào JSON; dùng chuỗi rỗng nếu con trỏ mã lệnh là null.
             tenThietBiBle, // Đưa tên BLE của thiết bị vào trường deviceId.
             layThoiGianHienTaiMs(), // Đưa thời gian hiện tại theo mili giây vào trường timestampMs.
             trangThaiLenh ? trangThaiLenh : "COMPLETED", // Đưa trạng thái lệnh vào JSON; mặc định là COMPLETED nếu không được cung cấp.
             boDemLoi, // Chèn giá trị mã lỗi đã được định dạng sẵn vào JSON.
             boDemTinNhan); // Chèn thông điệp đã được định dạng sẵn và hoàn tất chuỗi JSON.

    if (strlen(boDem) > 512) { // Loại gói nếu dài quá 512 byte, giới hạn tải mà phần BLE này chấp nhận.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói BLE bị loại để có thể theo dõi lỗi kích thước.
        return; // Thoát hàm vì gói vượt giới hạn 512 byte nên không thể gửi.
    }
    if (strlen(boDem) > (size_t)(mtuBle - 3)) { // Kiểm tra gói có vừa phần dữ liệu ATT hay không; 3 byte MTU dành cho tiêu đề ATT.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói bị loại khi gói không vừa MTU đã thương lượng.
        return; // Thoát hàm để không gửi một gói lớn hơn sức chứa của kết nối.
    }
    dacTruXacNhan->setValue((uint8_t*)boDem, strlen(boDem)); // Nạp chuỗi JSON và đúng số byte của nó vào đặc trưng xác nhận.
    dacTruXacNhan->notify(); // Gửi thông báo notify của đặc trưng xác nhận tới ứng dụng.
}

// SỬA 5: Esp32EventPacket (§8.4)
// Các trường: protocolVersion · eventId · deviceId · sequenceNumber · timestampMs ·
// eventType · eventSeverity · sosButtonPressed · eventConfidence · peakAccelerationMs2 ·
// orientationChangeDeg · altitudeDeltaM · inactivityDurationMs · checksum · triggerReasons
static void guiBleSuKien(const char* loaiSuKien, const char* mucDo, int doTinCay, // Khai báo hàm gửi sự kiện với loại, mức độ và độ tin cậy do bên gọi cung cấp.
                         float giaTocCucDai, float gocHuongDo, float chenhLechDoCaoM, // Nhận thêm gia tốc cực đại, góc đổi hướng và chênh lệch độ cao của sự kiện.
                         uint32_t thoiGianKhongHoatDongMs, const char* nguyenNhan, // Nhận thời gian bất động theo mili giây và chuỗi mô tả nguyên nhân kích hoạt.
                         bool coGiaTocCucDai = true, bool coHuong = false, bool coChenhLechDoCao = false, // Cho biết ba đại lượng đo tùy chọn nào thực sự có dữ liệu để đưa vào gói.
                         bool laTruyenLai = false) { // Cho biết đây là lần gửi mới hay lần truyền lại cùng một sự kiện.
    if (!ketNoiBle || dacTruSuKien == nullptr) return; // Không tạo gói nếu chưa kết nối hoặc đặc trưng sự kiện chưa được tạo.

    if (!laTruyenLai || strlen(maSuKienHienTai) == 0) { // Tạo mã mới cho sự kiện mới, hoặc khi lần truyền lại chưa có mã cũ để dùng.
        snprintf(maSuKienHienTai, sizeof(maSuKienHienTai), "evt-%04X-%04u", // Ghép mã sự kiện từ bốn chữ số hex ngẫu nhiên và bốn chữ số đếm tăng dần.
                 (uint16_t)(esp_random() & 0xFFFF), ++boDemSuKien); // Lấy 16 bit ngẫu nhiên và tăng bộ đếm để giảm khả năng trùng mã sự kiện.
        thuTuSuKienHienTai = ++soThuTuToanCuc; // Tăng số thứ tự toàn cục và lưu nó cho sự kiện hiện tại.
    }

    uint64_t thoiGianMs = layThoiGianHienTaiMs(); // Lấy thời gian hiện tại theo mili giây để đóng dấu thời gian cho sự kiện.
    bool nutSosDangBam = (digitalRead(CHAN_NUT_SOS) == LOW); // Đọc chân nút SOS; mức LOW nghĩa là nút đang được nhấn.

    char boDemGiaTriCucDai[24]; // Dành 24 byte để đổi gia tốc cực đại thành văn bản JSON.
    if (coGiaTocCucDai) snprintf(boDemGiaTriCucDai, sizeof(boDemGiaTriCucDai), "%.2f", giaTocCucDai); // Nếu có số đo gia tốc cực đại, định dạng nó với hai chữ số thập phân.
    else snprintf(boDemGiaTriCucDai, sizeof(boDemGiaTriCucDai), "null"); // Nếu không có số đo gia tốc cực đại, ghi null vào trường JSON.

    char boDemHuong[24]; // Dành 24 byte để đổi góc thay đổi hướng thành văn bản JSON.
    if (coHuong) snprintf(boDemHuong, sizeof(boDemHuong), "%.1f", gocHuongDo); // Nếu có số đo hướng, định dạng góc với một chữ số thập phân.
    else snprintf(boDemHuong, sizeof(boDemHuong), "null"); // Nếu không có số đo hướng, ghi null vào trường JSON.

    char boDemDoCao[24]; // Dành 24 byte để đổi chênh lệch độ cao thành văn bản JSON.
    if (coChenhLechDoCao) snprintf(boDemDoCao, sizeof(boDemDoCao), "%.2f", chenhLechDoCaoM); // Nếu có số đo độ cao, định dạng chênh lệch với hai chữ số thập phân.
    else snprintf(boDemDoCao, sizeof(boDemDoCao), "null"); // Nếu không có số đo độ cao, ghi null vào trường JSON.

    char boDemKhongHoatDong[24]; // Dành 24 byte để đổi thời gian bất động thành văn bản JSON.
    if (thoiGianKhongHoatDongMs > 0) snprintf(boDemKhongHoatDong, sizeof(boDemKhongHoatDong), "%u", thoiGianKhongHoatDongMs); // Nếu thời gian bất động lớn hơn 0, ghi số mili giây vào trường JSON.
    else snprintf(boDemKhongHoatDong, sizeof(boDemKhongHoatDong), "null"); // Nếu không ghi nhận bất động, dùng null thay cho giá trị 0 trong JSON.

    // Lưu ý (§8.4 / §8.7): checksum là null trong hợp đồng v1.
    // triggerReasons là trường mở rộng ngoài hợp đồng v1, giữ lại để gỡ lỗi và đo từ xa.
    char boDem[384]; // Dành 384 byte để ghép toàn bộ gói JSON mô tả sự kiện.
    snprintf(boDem, sizeof(boDem), // Ghép các trường sự kiện bên dưới vào bộ đệm JSON.
             "{\"protocolVersion\":1,\"eventId\":\"%s\",\"deviceId\":\"%s\",\"sequenceNumber\":%u," // Mở JSON phiên bản 1 và thêm mã sự kiện, mã thiết bị, số thứ tự.
             "\"timestampMs\":%llu,\"eventType\":\"%s\",\"eventSeverity\":\"%s\"," // Thêm thời điểm, loại sự kiện và mức độ nghiêm trọng vào JSON.
             "\"sosButtonPressed\":%s,\"eventConfidence\":%d," // Thêm trạng thái nút SOS và điểm tin cậy của sự kiện.
             "\"peakAccelerationMs2\":%s,\"orientationChangeDeg\":%s,\"altitudeDeltaM\":%s," // Thêm gia tốc cực đại, góc đổi hướng và chênh lệch độ cao.
             "\"inactivityDurationMs\":%s,\"checksum\":null,\"triggerReasons\":[\"%s\"]}", // Thêm thời gian bất động, checksum chưa dùng và danh sách nguyên nhân rồi đóng JSON.
             maSuKienHienTai, // Chèn mã của sự kiện hiện tại vào trường eventId.
             tenThietBiBle, // Chèn tên BLE của thiết bị vào trường deviceId.
             thuTuSuKienHienTai, // Chèn số thứ tự đã dành cho sự kiện hiện tại.
             thoiGianMs, // Chèn thời điểm tạo gói theo mili giây.
             loaiSuKien, // Chèn loại sự kiện, chẳng hạn FALL hoặc SOS.
             mucDo, // Chèn mức độ nghiêm trọng của sự kiện.
             nutSosDangBam ? "true" : "false", // Chuyển trạng thái nút SOS thành chữ true hoặc false hợp lệ của JSON.
             doTinCay, // Chèn điểm tin cậy của thuật toán phát hiện sự kiện.
             boDemGiaTriCucDai, // Chèn gia tốc cực đại đã định dạng hoặc null.
             boDemHuong, // Chèn góc thay đổi hướng đã định dạng hoặc null.
             boDemDoCao, // Chèn chênh lệch độ cao đã định dạng hoặc null.
             boDemKhongHoatDong, // Chèn thời gian bất động đã định dạng hoặc null.
             nguyenNhan ? nguyenNhan : ""); // Chèn nguyên nhân kích hoạt; dùng chuỗi rỗng nếu không có nguyên nhân.

    // Kiểm tra an toàn chống tràn MTU (§8.7).
    if (strlen(boDem) > 512) { // Loại gói sự kiện nếu dài quá 512 byte, giới hạn tải của phần BLE này.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói BLE bị loại vì vượt giới hạn 512 byte.
        return; // Thoát hàm vì gói sự kiện quá dài nên không thể gửi.
    }
    if (strlen(boDem) > (size_t)(mtuBle - 3)) { // Kiểm tra gói sự kiện có vừa MTU sau khi trừ 3 byte tiêu đề ATT hay không.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói bị loại khi sự kiện không vừa MTU.
        return; // Thoát hàm để không gửi gói sự kiện lớn hơn sức chứa kết nối.
    }
    dacTruSuKien->setValue((uint8_t*)boDem, strlen(boDem)); // Nạp chuỗi JSON sự kiện và đúng độ dài của nó vào đặc trưng sự kiện.
    dacTruSuKien->indicate(); // Gửi sự kiện bằng indicate để ứng dụng xác nhận đã nhận ở tầng BLE.
}

// SỬA 4: Esp32DeviceStatus (§8.3)
// Các trường: protocolVersion · deviceId · timestampMs · firmwareVersion · uptimeSeconds ·
// batteryPercent · batteryVoltageMv · isCharging · imuStatus · barometerStatus · gnssStatus ·
// bufferUsagePercent · lastErrorCode
static void guiBleTrangThaiThietBi() { // Tạo và gửi một gói JSON mô tả trạng thái hiện tại của thiết bị.
    if (!ketNoiBle || dacTruTrangThai == nullptr) return; // Không gửi trạng thái nếu chưa kết nối hoặc đặc trưng trạng thái chưa được tạo.

    const char* chuoiTrangThaiImu = "OK"; // Khởi tạo trạng thái IMU là OK trước khi xét các trường hợp đặc biệt.
    if (trangThaiThietBi == STATE_CALIBRATING) chuoiTrangThaiImu = "CALIBRATING"; // Báo IMU đang CALIBRATING khi toàn thiết bị ở bước hiệu chuẩn.
    else if (!camBienMpu.dangHoatDong) chuoiTrangThaiImu = "ERROR"; // Báo IMU lỗi nếu cảm biến MPU không hoạt động.
    else chuoiTrangThaiImu = "OK"; // Giữ trạng thái IMU là OK khi cảm biến hoạt động và không hiệu chuẩn.

    const char* chuoiTrangThaiKhiAp = "UNAVAILABLE"; // Khởi tạo trạng thái khí áp là UNAVAILABLE trước khi kiểm tra cảm biến.
    if (trangThaiThietBi == STATE_CALIBRATING) chuoiTrangThaiKhiAp = "CALIBRATING"; // Báo cảm biến khí áp đang CALIBRATING trong bước hiệu chuẩn.
    else if (camBienKhiAp.coLoiDoc) chuoiTrangThaiKhiAp = "ERROR"; // Báo ERROR nếu lần đọc cảm biến khí áp gặp lỗi.
    else if (camBienKhiAp.dangHoatDong) chuoiTrangThaiKhiAp = "OK"; // Báo OK nếu cảm biến khí áp đang hoạt động và không có lỗi đọc.
    else chuoiTrangThaiKhiAp = "UNAVAILABLE"; // Báo UNAVAILABLE khi cảm biến khí áp không hoạt động.

    int mucSuDungBoDem = (int)((soMauTrongBoDem * 100UL) / SO_LUONG_BO_DEM_VONG); // Tính phần trăm số chỗ trong bộ đệm vòng đang được sử dụng.
    if (mucSuDungBoDem > 100) mucSuDungBoDem = 100; // Chặn phần trăm ở 100 để trạng thái không báo một giá trị vượt giới hạn.

    char boDemLoi[48]; // Dành 48 byte để biểu diễn mã lỗi gần nhất dưới dạng chuỗi JSON hoặc null.
    if (maLoiCuoiCung != nullptr && strlen(maLoiCuoiCung) > 0) { // Kiểm tra mã lỗi gần nhất có tồn tại và không phải chuỗi rỗng.
        snprintf(boDemLoi, sizeof(boDemLoi), "\"%s\"", maLoiCuoiCung); // Nếu có lỗi, đặt mã lỗi trong dấu nháy để tạo chuỗi JSON.
    } else { // Rẽ sang trường hợp thiết bị chưa ghi nhận mã lỗi nào.
        snprintf(boDemLoi, sizeof(boDemLoi), "null"); // Ghi null của JSON để biểu thị chưa có lỗi gần nhất.
    }

    char boDem[320]; // Dành 320 byte để ghép toàn bộ gói JSON trạng thái thiết bị.
    snprintf(boDem, sizeof(boDem), // Ghép các trường trạng thái bên dưới vào bộ đệm JSON.
             "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"timestampMs\":%llu," // Mở JSON phiên bản 1 và thêm mã thiết bị cùng thời điểm gửi.
             "\"firmwareVersion\":\"%s\",\"uptimeSeconds\":%lu,\"batteryPercent\":%d," // Thêm phiên bản firmware, số giây đã chạy và phần trăm pin.
             "\"batteryVoltageMv\":null,\"isCharging\":%s,\"imuStatus\":\"%s\"," // Để điện áp pin là null, rồi thêm trạng thái sạc và trạng thái IMU.
             "\"barometerStatus\":\"%s\",\"gnssStatus\":\"UNAVAILABLE\"," // Thêm trạng thái khí áp; GNSS là UNAVAILABLE vì firmware chưa có cảm biến này.
             "\"bufferUsagePercent\":%d,\"lastErrorCode\":%s}", // Thêm phần trăm dùng bộ đệm và mã lỗi gần nhất rồi đóng JSON.
             tenThietBiBle, // Chèn tên BLE của thiết bị vào trường deviceId.
             layThoiGianHienTaiMs(), // Chèn thời gian hiện tại theo mili giây vào trường timestampMs.
             FIRMWARE_VERSION, // Chèn chuỗi phiên bản firmware vào trường firmwareVersion.
             (unsigned long)(esp_timer_get_time() / 1000000ULL), // Đổi thời gian chạy từ micro giây sang giây bằng cách chia cho 1.000.000.
             phanTramPin, // Chèn phần trăm pin hiện tại vào gói trạng thái.
             dangSacPin ? "true" : "false", // Chuyển trạng thái sạc thành chữ true hoặc false hợp lệ của JSON.
             chuoiTrangThaiImu, // Chèn chuỗi trạng thái IMU đã xác định ở trên.
             chuoiTrangThaiKhiAp, // Chèn chuỗi trạng thái cảm biến khí áp đã xác định ở trên.
             mucSuDungBoDem, // Chèn phần trăm bộ đệm vòng đang được sử dụng.
             boDemLoi); // Chèn mã lỗi đã định dạng và hoàn tất chuỗi JSON.

    if (strlen(boDem) > 512) { // Loại gói trạng thái nếu dài quá 512 byte, giới hạn tải của phần BLE này.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói BLE bị loại vì gói trạng thái quá dài.
        return; // Thoát hàm vì gói trạng thái vượt giới hạn 512 byte.
    }
    if (strlen(boDem) > (size_t)(mtuBle - 3)) { // Kiểm tra gói trạng thái có vừa MTU sau khi trừ 3 byte tiêu đề ATT hay không.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói bị loại khi trạng thái không vừa MTU.
        return; // Thoát hàm để không gửi gói trạng thái lớn hơn sức chứa kết nối.
    }
    dacTruTrangThai->setValue((uint8_t*)boDem, strlen(boDem)); // Nạp chuỗi JSON trạng thái và đúng độ dài của nó vào đặc trưng trạng thái.
    dacTruTrangThai->notify(); // Gửi notify để ứng dụng nhận ngay trạng thái mới.
}

// SỬA 1: Esp32SensorPacket (§8.2)
// Các trường: protocolVersion · deviceId · sequenceNumber · timestampMs ·
// accelXMs2, accelYMs2, accelZMs2 · gyroXDps, gyroYDps, gyroZDps ·
// pressurePa · temperatureC · altitudeDeltaM · batteryPercent · batteryVoltageMv ·
// isCharging · sosButtonPressed · sensorQuality
static void guiBleDuLieuCamBien(float ax, float ay, float az, float gx, float gy, float gz, // Khai báo hàm gửi sáu giá trị gia tốc và con quay hồi chuyển qua BLE.
                                float apSuat, float chenhLechDoCao, float nhietDoC) { // Nhận thêm áp suất, chênh lệch độ cao và nhiệt độ để đưa vào gói cảm biến.
    if (!ketNoiBle || !truyenBle || dacTruTruyenDuLieu == nullptr) return; // Không gửi nếu chưa kết nối, chưa bật luồng hoặc chưa tạo đặc trưng dữ liệu.

    uint32_t thuTu = ++soThuTuToanCuc; // Tăng số thứ tự toàn cục để mỗi gói cảm biến có thứ tự kế tiếp.
    uint64_t thoiGianMs = layThoiGianHienTaiMs(); // Lấy thời gian hiện tại theo mili giây để đóng dấu cho gói cảm biến.
    bool nutSosDangBam = (digitalRead(CHAN_NUT_SOS) == LOW); // Đọc chân nút SOS; mức LOW nghĩa là nút đang được nhấn.

    // Tính điểm chất lượng cảm biến từ 0 đến 100 [Nguồn: quy ước chất lượng firmware].
    int chatLuong = 100; // Bắt đầu với điểm chất lượng 100 khi các cảm biến đều hoạt động tốt.
    if (!camBienMpu.dangHoatDong) chatLuong = 0; // Hạ chất lượng xuống 0 khi IMU không hoạt động vì thiếu dữ liệu chuyển động.
    else if (camBienMpu.biBaoHoa) chatLuong = 40; // Hạ chất lượng xuống 40 khi IMU bão hòa vì số đo chuyển động kém tin cậy.
    else if (!camBienKhiAp.dangHoatDong || camBienKhiAp.coLoiDoc) chatLuong = 80; // Hạ chất lượng xuống 80 khi khí áp không dùng được nhưng IMU vẫn có dữ liệu.

    char boDem[384]; // Dành 384 byte để ghép toàn bộ gói JSON dữ liệu cảm biến.
    if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc) { // Chọn gói đầy đủ khi cảm biến khí áp đang hoạt động và đọc không lỗi.
        snprintf(boDem, sizeof(boDem), // Bắt đầu ghép gói JSON có cả dữ liệu IMU và khí áp.
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu," // Mở JSON phiên bản 1 và thêm mã thiết bị, số thứ tự, thời điểm đo.
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f," // Thêm ba thành phần gia tốc, mỗi giá trị có hai chữ số thập phân.
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f," // Thêm ba tốc độ góc, mỗi giá trị có một chữ số thập phân.
                 "\"pressurePa\":%.1f,\"temperatureC\":%.1f,\"altitudeDeltaM\":%.2f," // Thêm áp suất, nhiệt độ và chênh lệch độ cao đo được.
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s," // Thêm phần trăm pin, điện áp chưa đo là null và trạng thái sạc.
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}", // Thêm trạng thái nút SOS và điểm chất lượng rồi đóng JSON.
                 tenThietBiBle, thuTu, thoiGianMs, // Chèn mã thiết bị, số thứ tự gói và thời điểm đo.
                 ax, ay, az, // Chèn gia tốc theo ba trục X, Y, Z.
                 gx, gy, gz, // Chèn tốc độ góc theo ba trục X, Y, Z.
                 apSuat, nhietDoC, chenhLechDoCao, // Chèn áp suất, nhiệt độ và chênh lệch độ cao theo đúng thứ tự định dạng.
                 phanTramPin, // Chèn phần trăm pin hiện tại.
                 dangSacPin ? "true" : "false", // Chuyển trạng thái sạc thành chữ true hoặc false hợp lệ của JSON.
                 nutSosDangBam ? "true" : "false", // Chuyển trạng thái nút SOS thành chữ true hoặc false hợp lệ của JSON.
                 chatLuong); // Chèn điểm chất lượng cảm biến và hoàn tất gói JSON.
    } else { // Rẽ sang gói rút gọn khi dữ liệu khí áp không dùng được.
        snprintf(boDem, sizeof(boDem), // Bắt đầu ghép gói JSON chỉ có dữ liệu IMU và trạng thái hệ thống.
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu," // Mở JSON phiên bản 1 và thêm mã thiết bị, số thứ tự, thời điểm đo.
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f," // Thêm ba thành phần gia tốc, mỗi giá trị có hai chữ số thập phân.
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f," // Thêm ba tốc độ góc, mỗi giá trị có một chữ số thập phân.
                 "\"pressurePa\":null,\"temperatureC\":null,\"altitudeDeltaM\":null," // Đặt áp suất, nhiệt độ và độ cao là null vì dữ liệu khí áp không hợp lệ.
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s," // Thêm phần trăm pin, điện áp chưa đo là null và trạng thái sạc.
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}", // Thêm trạng thái nút SOS và điểm chất lượng rồi đóng JSON.
                 tenThietBiBle, thuTu, thoiGianMs, // Chèn mã thiết bị, số thứ tự gói và thời điểm đo.
                 ax, ay, az, // Chèn gia tốc theo ba trục X, Y, Z.
                 gx, gy, gz, // Chèn tốc độ góc theo ba trục X, Y, Z.
                 phanTramPin, // Chèn phần trăm pin hiện tại.
                 dangSacPin ? "true" : "false", // Chuyển trạng thái sạc thành chữ true hoặc false hợp lệ của JSON.
                 nutSosDangBam ? "true" : "false", // Chuyển trạng thái nút SOS thành chữ true hoặc false hợp lệ của JSON.
                 chatLuong); // Chèn điểm chất lượng cảm biến và hoàn tất gói JSON rút gọn.
    }

    // Kiểm tra tràn MTU (§8.7): v1 chưa hỗ trợ phân mảnh nhị phân.
    // Nếu gói vượt dung lượng MTU thì tính là bị loại để không làm hỏng luồng.
    if (strlen(boDem) > 512) { // Loại gói cảm biến nếu dài quá 512 byte, giới hạn tải của phần BLE này.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói BLE bị loại vì gói cảm biến quá dài.
        return; // Thoát hàm vì gói cảm biến vượt giới hạn 512 byte.
    }
    if (strlen(boDem) > (size_t)(mtuBle - 3)) { // Kiểm tra gói cảm biến có vừa MTU sau khi trừ 3 byte tiêu đề ATT hay không.
        boDemHeThong.goiBleBiBo++; // Tăng bộ đếm gói bị loại khi dữ liệu cảm biến không vừa MTU.
        return; // Thoát hàm để không gửi gói cảm biến lớn hơn sức chứa kết nối.
    }
    dacTruTruyenDuLieu->setValue((uint8_t*)boDem, strlen(boDem)); // Nạp chuỗi JSON cảm biến và đúng độ dài của nó vào đặc trưng dữ liệu.
    dacTruTruyenDuLieu->notify(); // Gửi notify để ứng dụng nhận gói cảm biến mà không cần xác nhận tầng BLE.
}
#endif // CO_BLE

// ============================================================================
// 9. XUẤT SERIAL VÀ BỘ PHÂN TÍCH LỆNH (§7)
// ============================================================================
#define CHE_DO_SERIAL_THO 0 // 0: chế độ dễ đọc, mặc định khoảng 4 Hz; 1: chế độ CSV [Nguồn: thiết kế xuất Serial].
static bool cheDoCsvTho = (CHE_DO_SERIAL_THO == 1); // Chọn chế độ CSV khi hằng CHE_DO_SERIAL_THO bằng 1; giá trị 0 chọn dạng dễ đọc.
static uint32_t thoiDiemInDeDocCuoiMs = 0; // Lưu thời điểm gần nhất đã in dạng dễ đọc để giới hạn tần suất xuất Serial.

static void inBangNguong() { // Định nghĩa hàm in toàn bộ ngưỡng phát hiện té ngã ra màn hình Serial.
    Serial.println(F("\n=======================================================")); // In đường phân cách mở đầu để bảng ngưỡng dễ nhận biết trên Serial Monitor.
    Serial.println(F("CAU HINH PHAT HIEN TE NGA NCKH27PA ESP32-S3")); // In tiêu đề cho biết đây là cấu hình phát hiện té ngã của thiết bị ESP32-S3.
    Serial.println(F("LUU Y: NGUONG THU NGHIEM - CHUA DUOC KIEM CHUNG Y KHOA")); // Cảnh báo người đọc rằng các ngưỡng chỉ phục vụ thử nghiệm, chưa được kiểm chứng y khoa.
    Serial.println(F("=======================================================")); // In đường phân cách giữa phần tiêu đề và các giá trị ngưỡng.
    Serial.printf("  Gia toc va cham:                  %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.giaTocVaChamMs2); // In ngưỡng độ lớn gia tốc được coi là va chạm, đơn vị m/s².
    Serial.printf("  Gia toc dung yen muc tieu:        %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.nguongDungYenMs2); // In gia tốc mục tiêu khi đứng yên, gần gia tốc trọng trường.
    Serial.printf("  Sai so dung yen:                  %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.nguongSaiSoDungYenMs2); // In sai số cho phép quanh gia tốc mục tiêu để nhận biết trạng thái đứng yên.
    Serial.printf("  Cua so sau va cham:               %u ms       [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.cuaSoSauVaChamMs); // In thời gian theo dõi sau va chạm, tính bằng mili giây, để tìm dấu hiệu đứng yên.
    Serial.printf("  Dung yen sau va cham:             %u ms       [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.thoiGianDungYenSauVaChamMs); // In số mili giây phải đứng yên liên tục sau va chạm để xác nhận té ngã.
    Serial.printf("  So mau dung yen toi thieu:          %u mau  [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.minimumStillnessSamples); // In số mẫu đứng yên tối thiểu cần thu được trước khi xác nhận sự kiện.
    Serial.printf("  Khoang cach mau toi da:           %u ms       [Android FallDetectionConfig.DEFAULT]\n", NGUONG_PHAT_HIEN_TE_NGA.maximumSampleGapMs); // In khoảng cách thời gian lớn nhất giữa hai mẫu để chuỗi mẫu vẫn được coi là liên tục.
    Serial.printf("  Tang ap suat toi thieu:           %.1f Pa     [Android - bo loc dang tat]\n", NGUONG_PHAT_HIEN_TE_NGA.nguongTangApSuatPinPa); // In mức tăng áp suất tối thiểu dùng làm dấu hiệu hạ độ cao, dù bộ lọc Android hiện đang tắt.
    Serial.printf("  Nguong roi tu do:                 %.2f m/s^2   [Suy luan - chi ghi nhat ky]\n", NGUONG_PHAT_HIEN_TE_NGA.nguongTuDoMs2); // In ngưỡng gia tốc nhỏ dùng nhận biết rơi tự do để ghi nhật ký.
    Serial.printf("  Van toc goc cao:                  %.1f dps    [Suy luan - chi ghi nhat ky]\n", NGUONG_PHAT_HIEN_TE_NGA.vanTocGocCaoDps); // In ngưỡng vận tốc góc cao, đơn vị độ mỗi giây, dùng để ghi nhật ký chuyển động mạnh.
    Serial.println(F("=======================================================\n")); // In đường phân cách kết thúc bảng ngưỡng và thêm một dòng trống.
}

static void inTrangThaiHeThong() { // Định nghĩa hàm tổng hợp trạng thái cảm biến, kết nối và bộ đếm để chẩn đoán qua Serial.
    Serial.println(F("\n--- TRANG THAI HE THONG THIET BI ---")); // In tiêu đề mở đầu bản báo cáo trạng thái hệ thống.
    Serial.printf("Ma thiet bi: %s | Phan mem: %s\n", tenThietBiBle, FIRMWARE_VERSION); // In tên BLE của thiết bị và phiên bản firmware cho người vận hành kiểm tra.
    Serial.printf("Trang thai: %s (Truoc do: %s)\n", tenTrangThaiThietBi(trangThaiThietBi), tenTrangThaiThietBi(trangThaiTruocDo)); // In trạng thái hiện tại và trạng thái ngay trước đó của máy trạng thái.
    Serial.printf("IMU MPU6050: %s (dia chi: 0x%02X, bao hoa: %s, tan so: %u Hz)\n", // Chuẩn bị in trạng thái, địa chỉ I2C, cờ bão hòa và tần số lấy mẫu của MPU6050.
                  camBienMpu.dangHoatDong ? "ONLINE" : "OFFLINE", camBienMpu.diaChi, // Chọn chữ ONLINE/OFFLINE theo cờ hoạt động rồi truyền địa chỉ I2C cho hai ô định dạng đầu.
                  camBienMpu.biBaoHoa ? "YES" : "NO", tanSoLayMauImuHz); // Chọn YES/NO cho cờ bão hòa và in tần số lấy mẫu IMU theo héc.
    Serial.printf("Khi ap ke MS5611: %s (dia chi: 0x%02X, CRC: %s, moc: %.1f Pa)\n", // Chuẩn bị in trạng thái, địa chỉ, kết quả CRC và áp suất mốc của MS5611.
                  camBienKhiAp.dangHoatDong ? (camBienKhiAp.coLoiDoc ? "ERROR" : "ONLINE") : "UNAVAILABLE", // Hiển thị ERROR nếu cảm biến có lỗi đọc, ONLINE nếu tốt, hoặc UNAVAILABLE nếu không hoạt động.
                  camBienKhiAp.diaChi, camBienKhiAp.crcHopLe ? "VALID" : "INVALID", apSuatThamChieuPa); // Điền địa chỉ I2C, tính hợp lệ CRC và áp suất tham chiếu theo pascal vào bản báo cáo.
    Serial.printf("BLE: %s (Dang truyen: %s) | MTU: %u\n", ketNoiBle ? "DA KET NOI" : "DA NGAT KET NOI", truyenBle ? "BAT" : "TAT", mtuBle); // In trạng thái kết nối BLE, cờ truyền luồng và kích thước MTU đang dùng.
    if (WiFi.status() == WL_CONNECTED) { // Giữ nguyên hằng API trạng thái WiFi của thư viện.
        Serial.printf("WiFi: DA KET NOI (SSID: %s, RSSI: %d dBm, IP: %s)\n", WIFI_SSID, WiFi.RSSI(), WiFi.localIP().toString().c_str()); // Khi WiFi đã nối, in tên mạng, cường độ tín hiệu dBm và địa chỉ IP cho người vận hành.
    } else { // Nếu WiFi chưa ở trạng thái kết nối thì chuyển sang nhánh báo ngắt kết nối.
        Serial.printf("WiFi: DA NGAT KET NOI (SSID: %s)\n", WIFI_SSID); // In tên mạng mà thiết bị đang cố kết nối để hỗ trợ chẩn đoán.
    }
    Serial.printf("Bo dem: %u/%u mau (%d%%) | Bi bo: %u\n", // Chuẩn bị in mức sử dụng bộ đệm vòng và tổng số mẫu IMU đã bị bỏ.
                  soMauTrongBoDem, SO_LUONG_BO_DEM_VONG, (int)((soMauTrongBoDem * 100UL) / SO_LUONG_BO_DEM_VONG), soMauBiBo); // Điền số mẫu, sức chứa, phần trăm đã dùng và số mẫu bị bỏ vào dòng thống kê.
    Serial.printf("Bo dem: IMU=%u, KhiAp=%u, Tre=%u, Nga=%u, SOS=%u, BLE bi bo=%u\n", // Chuẩn bị in sáu bộ đếm hoạt động và sự kiện của hệ thống.
                  boDemHeThong.soMauImuDaDoc, boDemHeThong.soMauKhiApDaDoc, boDemHeThong.soLanTreChuKy, // Điền số mẫu IMU, mẫu khí áp và số chu kì lấy mẫu bị trễ.
                  boDemHeThong.suKienNgaDaXacMinh, boDemHeThong.lanBamSos, boDemHeThong.goiBleBiBo); // Điền số lần xác nhận ngã, nhấn SOS và gói BLE bị bỏ.
    Serial.printf("Pin: %d%% (chua lap = -1), Heap trong: %u byte\n", phanTramPin, ESP.getFreeHeap()); // In phần trăm pin; giá trị -1 báo chưa có bộ đo pin, cùng lượng heap còn trống theo byte.
    Serial.printf("Ma loi cuoi: %s\n", maLoiCuoiCung ? maLoiCuoiCung : "NONE"); // In mã lỗi gần nhất, hoặc NONE nếu chưa ghi nhận lỗi nào.
    Serial.println(F("-----------------------------\n")); // In đường kết thúc bản báo cáo trạng thái và thêm một dòng trống.
}

static void inTieuDeCsv() { // Định nghĩa hàm in tên các cột trước khi xuất dữ liệu cảm biến dạng CSV.
    // SỬA 3: Dùng nhất quán tên trường altitudeDeltaM của giao thức.
    Serial.println(F("timestamp_ms,ax,ay,az,mag,gx,gy,gz,pressure_pa,altitudeDeltaM,state,event")); // In hàng tiêu đề CSV để phần mềm đọc đúng thứ tự thời gian, cảm biến, trạng thái và sự kiện.
}

// ============================================================================
// 10. HIỆU CHUẨN VÀ TỰ KIỂM TRA (§7.1)
// ============================================================================
static void tuKiemTraVaHieuChuan() { // Định nghĩa quy trình tự kiểm tra cảm biến và tính các giá trị mốc khi khởi động.
    trangThaiThietBi = STATE_BOOT_SELF_TEST; // Chuyển máy trạng thái sang bước tự kiểm tra phần cứng lúc khởi động.
    Serial.println(F("[BOOT] Bat dau tu kiem tra phan cung...")); // Báo trên Serial rằng quá trình tự kiểm tra phần cứng đã bắt đầu.

    bool imuDat = khoiTaoMpu6050(); // Khởi tạo MPU6050 và lưu kết quả thành công hay thất bại để quyết định có thể tiếp tục hay không.
    Serial.printf("[BOOT] IMU MPU6050 tren I2C0 (SDA 8, SCL 9): %s (addr: 0x%02X)\n", // Chuẩn bị in kết quả tự kiểm tra IMU cùng chân SDA 8, SCL 9 và địa chỉ I2C tìm được.
                  imuDat ? "DAT" : "LOI", camBienMpu.diaChi); // Điền chữ ĐẠT hoặc LỖI theo kết quả khởi tạo và địa chỉ MPU6050 dạng hệ mười sáu.

    bool khiApDat = khoiTaoMs5611(); // Khởi tạo khí áp kế MS5611 và lưu kết quả để báo tình trạng cảm biến.
    Serial.printf("[BOOT] Khi ap ke MS5611 tren I2C1 (SDA 7, SCL 6): %s (dia chi: 0x%02X, CRC: %s)\n", // Chuẩn bị in kết quả MS5611 trên bus I2C1 với SDA 7, SCL 6, địa chỉ và kiểm tra CRC.
                  khiApDat ? "DAT" : "KHONG CO", camBienKhiAp.diaChi, camBienKhiAp.crcHopLe ? "DAT" : "LOI/CHUA XAC MINH"); // Điền kết quả phát hiện cảm biến, địa chỉ và tính hợp lệ của CRC vào thông báo khởi động.

    // SỬA 6: Phát SENSOR_ERROR nếu cảm biến quan trọng không vượt qua tự kiểm tra.
    if (!imuDat) { // Nếu MPU6050 không khởi tạo được thì không thể hiệu chuẩn và phải chuyển sang chế độ suy giảm.
        Serial.println(F("[CRITICAL] IMU tu kiem tra loi. He thong chuyen sang che do suy giam.")); // In cảnh báo nghiêm trọng cho người theo dõi Serial biết IMU tự kiểm tra thất bại.
        trangThaiThietBi = STATE_DEGRADED; // Đánh dấu thiết bị đang hoạt động suy giảm vì thiếu dữ liệu IMU quan trọng.
        maLoiCuoiCung = "IMU_READ_FAILED"; // Ghi mã lỗi gần nhất là IMU_READ_FAILED vì MPU6050 không vượt qua bước tự kiểm tra.
#if CO_BLE // Chỉ biên dịch phần gửi lỗi cảm biến khi cấu hình firmware bật BLE.
        guiBleSuKien("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_SELF_TEST_FAILED", false, false, false); // Giữ nguyên loại sự kiện, mức độ và mã lỗi [Nguồn: hợp đồng BLE v1].
        guiBleTrangThaiThietBi(); // Gửi ngay trạng thái suy giảm mới cho ứng dụng qua BLE.
#endif // Kết thúc phần thông báo lỗi IMU dành riêng cho bản firmware có BLE.
        return; // Thoát quy trình sớm vì không có IMU để lấy mẫu hiệu chuẩn.
    }

    trangThaiThietBi = STATE_CALIBRATING; // Chuyển trạng thái sang đang hiệu chuẩn sau khi IMU vượt qua tự kiểm tra.
    Serial.println(F("[CALIB] Dang hieu chuan sai so con quay va trong luc moc (giu thiet bi DUNG YEN trong 2 giay)...")); // Yêu cầu người dùng giữ yên thiết bị trong 2 giây để phép đo mốc ít bị sai lệch.

    float tongGx = 0, tongGy = 0, tongGz = 0; // Khởi tạo tổng vận tốc góc ba trục để tính sai số trung bình của con quay.
    float tongAx = 0, tongAy = 0, tongAz = 0; // Khởi tạo tổng gia tốc ba trục để tính véc-tơ trọng lực trung bình khi thiết bị đứng yên.
    float tongApSuat = 0; // Khởi tạo tổng áp suất dùng tính mốc khí áp trung bình trong thời gian hiệu chuẩn.
    int soMauDaDoc = 0; // Đếm số mẫu IMU hợp lệ thực sự được đưa vào các phép tính trung bình.
    int canhBaoRungDong = 0; // Đếm số mẫu lệch nhiều khỏi trọng lực để phát hiện thiết bị bị rung khi hiệu chuẩn.

    uint32_t batDauHieuChuan = millis(); // Ghi thời điểm bắt đầu để giới hạn toàn bộ khoảng thu mẫu hiệu chuẩn.
    while (millis() - batDauHieuChuan < 2000) { // Thu mẫu trong 2000 ms, tức 2 giây như thông báo cho người dùng.
        float ax, ay, az, gx, gy, gz; // Khai báo nơi nhận gia tốc và vận tốc góc của ba trục trong một lần đọc.
        if (docMpu6050(ax, ay, az, gx, gy, gz)) { // Chỉ cộng mẫu vào phép hiệu chuẩn khi đọc đủ sáu trục MPU6050 thành công.
            float doLonGiaToc = sqrtf(ax * ax + ay * ay + az * az); // Tính độ lớn véc-tơ gia tốc từ ba thành phần X, Y, Z.
            if (fabsf(doLonGiaToc - 9.81f) > 2.0f) { // Xem là rung động nếu gia tốc lệch quá 2,0 m/s² so với trọng lực 9,81 m/s².
                canhBaoRungDong++; // Ghi nhận thêm một mẫu có dấu hiệu thiết bị không đứng yên.
            }
            tongAx += ax; tongAy += ay; tongAz += az; // Cộng gia tốc từng trục vào tổng để sau đó tính véc-tơ trọng lực trung bình.
            tongGx += gx; tongGy += gy; tongGz += gz; // Cộng vận tốc góc từng trục vào tổng để sau đó tính sai số con quay trung bình.
            soMauDaDoc++; // Tăng số mẫu hợp lệ dùng làm mẫu số của các phép tính trung bình.
        }
        if (camBienKhiAp.dangHoatDong) { // Nếu MS5611 có mặt thì đồng thời thu áp suất để tạo mốc độ cao.
            docMs5611KhongChan(micros()); // Tiến máy trạng thái đọc MS5611 theo thời gian micro giây mà không chặn lâu vòng hiệu chuẩn.
            tongApSuat += camBienKhiAp.apSuatPa; // Cộng giá trị áp suất mới nhất vào tổng dùng tính áp suất tham chiếu.
        }
        delay(10); // Chờ 10 ms giữa hai lượt, tương ứng nhịp lấy mẫu xấp xỉ 100 Hz và tránh đọc dồn dập.
    }

    if (soMauDaDoc > 50) { // Chỉ chấp nhận hiệu chuẩn khi có hơn 50 mẫu hợp lệ để giá trị trung bình đủ ổn định.
        camBienMpu.saiSoVanTocGocX = tongGx / soMauDaDoc; // Lấy vận tốc góc X trung bình khi đứng yên làm sai số cần bù.
        camBienMpu.saiSoVanTocGocY = tongGy / soMauDaDoc; // Lấy vận tốc góc Y trung bình khi đứng yên làm sai số cần bù.
        camBienMpu.saiSoVanTocGocZ = tongGz / soMauDaDoc; // Lấy vận tốc góc Z trung bình khi đứng yên làm sai số cần bù.
        camBienMpu.trongLucGocX = tongAx / soMauDaDoc; // Tính thành phần trọng lực trung bình trên trục X làm mốc tư thế.
        camBienMpu.trongLucGocY = tongAy / soMauDaDoc; // Tính thành phần trọng lực trung bình trên trục Y làm mốc tư thế.
        camBienMpu.trongLucGocZ = tongAz / soMauDaDoc; // Tính thành phần trọng lực trung bình trên trục Z làm mốc tư thế.
        if (camBienKhiAp.dangHoatDong) { // Chỉ thiết lập mốc áp suất và độ cao khi MS5611 đang hoạt động.
            // SỬA 3: Khởi tạo áp suất tham chiếu từ giá trị trung bình khi hiệu chuẩn.
            apSuatThamChieuPa = tongApSuat / soMauDaDoc; // Lấy áp suất tích lũy chia số mẫu IMU làm mốc áp suất ban đầu.
            camBienKhiAp.chenhLechDoCaoM = 0.0f; // Đặt chênh lệch độ cao về 0 mét tại chính mốc vừa hiệu chuẩn.
        }

        if (canhBaoRungDong > (soMauDaDoc * 0.20f)) { // Cảnh báo nếu hơn 20% số mẫu lệch xa trọng lực, cho thấy thiết bị đã chuyển động.
            Serial.println(F("[CALIB_WARNING] Phat hien thiet bi chuyen dong khi hieu chuan! Moc co the bi sai lech.")); // Báo cho người dùng rằng chuyển động có thể làm các giá trị mốc bị sai.
        } else { // Nếu không quá 20% mẫu bị rung thì coi véc-tơ trọng lực đã đủ ổn định.
            Serial.println(F("[CALIB] Hieu chuan hoan tat. Vecto trong luc da on dinh.")); // Báo trên Serial rằng quá trình hiệu chuẩn đã hoàn tất thành công.
        }
    }

    trangThaiThietBi = STATE_MONITORING; // Chuyển sang trạng thái theo dõi bình thường sau khi kết thúc hiệu chuẩn.
    Serial.println(F("[SYSTEM] Da chuyen sang STATE_MONITORING. San sang.")); // Báo cho người theo dõi Serial rằng thiết bị đã sẵn sàng giám sát.
}

// ============================================================================
// 11. THEO DÕI ĐỘ TRÔI KHÍ ÁP KẾ VÀ QUẢN LÍ PIN (§6.2, §3.4, SỬA 3, SỬA 6)
// ============================================================================
// Điều kiện (b): Chỉ tự động cập nhật mốc tham chiếu khi:
// 1) Thiết bị ở STATE_MONITORING và KHÔNG có lượt nghi ngờ ngã đang hoạt động (!trangThaiPhatHienNga.dangTrongCuaSoXacMinh).
// 2) Áp suất môi trường ổn định trong ±10 Pa (xấp xỉ ±0.8 m) ít nhất 10000 ms liên tục [Nguồn: đặc tả mục §6.2].
// Tuyệt đối không thay đổi mốc trong lượt nghi ngờ ngã hoặc cửa sổ xác minh đang hoạt động.
static uint32_t thoiDiemApSuatOnDinhMs = 0; // Lưu thời điểm bắt đầu giai đoạn áp suất ổn định.
static float apSuatKiemTraOnDinhCuoiPa = 0.0f; // Lưu áp suất của lần kiểm tra ổn định gần nhất.

static void capNhatApSuatThamChieu(uint32_t thoiGianHienTaiMs, float apSuatHienTaiPa) { // Theo dõi độ ổn định để cập nhật mốc áp suất.
    if (trangThaiThietBi != STATE_MONITORING || trangThaiPhatHienNga.dangTrongCuaSoXacMinh) { // Không cập nhật khi chưa theo dõi bình thường hoặc đang xác minh ngã.
        thoiDiemApSuatOnDinhMs = 0; // Hủy khoảng ổn định đang ghi nhận.
        return; // Kết thúc sớm mà không cập nhật mốc áp suất.
    }
    if (fabsf(apSuatHienTaiPa - apSuatKiemTraOnDinhCuoiPa) > 10.0f) { // ±10 Pa [Nguồn: đặc tả mốc áp suất §6.2].
        apSuatKiemTraOnDinhCuoiPa = apSuatHienTaiPa; // Chuyển mốc kiểm tra sang áp suất vừa thay đổi.
        thoiDiemApSuatOnDinhMs = thoiGianHienTaiMs; // Bắt đầu lại thời gian theo dõi ổn định.
    } else { // Áp suất vẫn nằm trong dải ổn định cho phép.
        if (thoiDiemApSuatOnDinhMs == 0) { // Khởi tạo cửa sổ ổn định nếu chưa có.
            thoiDiemApSuatOnDinhMs = thoiGianHienTaiMs; // Ghi thời điểm bắt đầu ổn định.
            apSuatKiemTraOnDinhCuoiPa = apSuatHienTaiPa; // Ghi áp suất làm mốc so sánh.
        } else if (thoiGianHienTaiMs - thoiDiemApSuatOnDinhMs >= 10000) { // 10000 ms [Nguồn: đặc tả mốc áp suất §6.2].
            // Áp suất duy trì trong ±10 Pa đủ ít nhất 10000 ms nên được dùng làm mốc mới.
            apSuatThamChieuPa = apSuatHienTaiPa; // Cập nhật áp suất tham chiếu bằng giá trị hiện tại.
            thoiDiemApSuatOnDinhMs = thoiGianHienTaiMs; // Bắt đầu chu kì ổn định kế tiếp.
        }
    }
}

// Quản lí pin và cảnh báo có vùng trễ (Kế hoạch §3.4, §8.2, SỬA 6):
// Các biến trạng thái (phanTramPin, dangSacPin, daCanhBaoPinYeu, daCanhBaoPinKiet)
// được khai báo ở khối toàn cục phía trên phần BLE vì bộ tạo gói tin cần đọc chúng.
// Khi không lắp đồng hồ nhiên liệu phần cứng (CO_DONG_HO_NHIEN_LIEU == 0):
// - phanTramPin = -1 là giá trị canh gác báo không có phần cứng, tránh báo giả 100%.
// - batteryVoltageMv = null là trường giao thức JSON nên giữ nguyên tên và giá trị.
// - maLoiCuoiCung = "BATTERY_READ_FAILED" giữ nguyên mã lỗi theo hợp đồng giao thức.

static void capNhatTrangThaiPin(uint32_t thoiGianHienTaiMs) { // Cập nhật phần trăm pin và cờ cảnh báo vùng trễ.
#if CO_DONG_HO_NHIEN_LIEU // Chỉ đọc pin khi bản phần cứng có đồng hồ nhiên liệu.
    // Khi MAX17048 hoặc mạch đo pin ADC được lắp trong phiên bản tương lai:
    // Đọc vi mạch đồng hồ nhiên liệu hoặc ADC tại đây.
    // Ví dụ: phanTramPin = readFuelGaugePercent();
    //
    // Ngưỡng cảnh báo vùng trễ [Nguồn: Kế hoạch §3.4]:
    // Cảnh báo pin yếu khi <= 20%, chỉ xóa khi > 25% [Nguồn: Kế hoạch §3.4].
    // Cảnh báo pin kiệt khi <= 10%, chỉ xóa khi > 15% [Nguồn: Kế hoạch §3.4].
    if (phanTramPin >= 0) { // Chỉ xử lí khi đã có số đo pin hợp lệ.
        if (phanTramPin <= 10 && !daCanhBaoPinKiet) { // 10% [Nguồn: Kế hoạch §3.4].
            daCanhBaoPinKiet = true; // Ghi nhận đã phát cảnh báo pin kiệt.
            daCanhBaoPinYeu = true; // Pin kiệt đồng thời thuộc mức pin yếu.
#if CO_BLE // Gửi cảnh báo qua BLE khi tính năng này được bật.
            guiBleSuKien("LOW_BATTERY", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_CRITICAL_LE_10PCT", false, false, false); // Giữ nguyên loại, mức và mã nguyên nhân giao thức.
            guiBleTrangThaiThietBi(); // Gửi ngay trạng thái pin mới.
#endif // Kết thúc khối gửi cảnh báo pin kiệt qua BLE.
        } else if (phanTramPin <= 20 && !daCanhBaoPinYeu) { // 20% [Nguồn: Kế hoạch §3.4].
            daCanhBaoPinYeu = true; // Ghi nhận đã phát cảnh báo pin yếu.
#if CO_BLE // Gửi cảnh báo qua BLE khi tính năng này được bật.
            guiBleSuKien("LOW_BATTERY", "WARNING", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_LOW_LE_20PCT", false, false, false); // Giữ nguyên loại, mức và mã nguyên nhân giao thức.
            guiBleTrangThaiThietBi(); // Gửi ngay trạng thái pin mới.
#endif // Kết thúc khối gửi cảnh báo pin yếu qua BLE.
        } else if (phanTramPin > 25) { // 25% [Nguồn: Kế hoạch §3.4, ngưỡng xóa cảnh báo pin yếu].
            daCanhBaoPinYeu = false; // Cho phép cảnh báo pin yếu ở lần tụt pin sau.
            daCanhBaoPinKiet = false; // Đồng thời xóa cờ pin kiệt.
        } else if (phanTramPin > 15) { // 15% [Nguồn: Kế hoạch §3.4, ngưỡng xóa cảnh báo pin kiệt].
            daCanhBaoPinKiet = false; // Cho phép cảnh báo pin kiệt ở lần tụt pin sau.
        }
    }
#else // Nhánh dành cho phần cứng không có đồng hồ nhiên liệu.
    // Không lắp đồng hồ nhiên liệu phần cứng (CO_DONG_HO_NHIEN_LIEU = 0).
    // Theo Kế hoạch §8.2, §8.3 và SỬA 6: tuyệt đối không gửi mức pin giả 100%.
    phanTramPin = -1; // -1 báo không có phần cứng đo pin [Nguồn: Kế hoạch §8.2, §8.3].
#endif // Kết thúc lựa chọn phần cứng đo pin.
}

// ============================================================================
// 12. XỬ LÍ NÚT BẤM, SOS VÀ CÒI CẢNH BÁO
// ============================================================================
static uint32_t thoiDiemBamSosMs = 0; // Lưu thời điểm bắt đầu giữ nút SOS.
static bool daKichHoatSos = false; // Ngăn kích hoạt SOS lặp lại trong cùng một lần giữ nút.
static uint32_t thoiDiemBamHuyMs = 0; // Lưu thời điểm bắt đầu giữ nút hủy.

static void capNhatNutBam(uint32_t thoiGianHienTaiMs) { // Đọc nút SOS và nút hủy, rồi cập nhật trạng thái.
    // Nút SOS tác động mức LOW nhờ điện trở kéo lên và phải được giữ ít nhất 2000 ms [Nguồn: đặc tả nút SOS].
    int giaTriNutSos = digitalRead(CHAN_NUT_SOS); // Đọc mức điện của nút SOS.
    if (giaTriNutSos == LOW) { // LOW nghĩa là nút đang được nhấn.
        if (thoiDiemBamSosMs == 0) { // Đây là mẫu đầu tiên của lần giữ nút.
            thoiDiemBamSosMs = thoiGianHienTaiMs; // Ghi thời điểm bắt đầu giữ.
        } else if (thoiGianHienTaiMs - thoiDiemBamSosMs >= 2000 && !daKichHoatSos) { // 2000 ms [Nguồn: đặc tả nút SOS].
            daKichHoatSos = true; // Đánh dấu lần giữ này đã kích hoạt SOS.
            boDemHeThong.lanBamSos++; // Tăng bộ đếm số lần kích hoạt SOS.
            trangThaiThietBi = STATE_LOCAL_ALERTING; // Chuyển thẳng sang cảnh báo tại chỗ.
            thoiDiemBatDauCanhBaoMs = thoiGianHienTaiMs; // Ghi mốc bắt đầu cảnh báo.
            Serial.println(F("[ALERT] >>> NUT SOS DA KICH HOAT! Chuyen thang sang LOCAL_ALERTING. <<<")); // Báo trên Serial rằng nút SOS đã được giữ đủ 2 giây và thiết bị bắt đầu cảnh báo tại chỗ.
#if CO_BLE // Đồng bộ sự kiện SOS qua BLE khi được bật.
            guiBleSuKien("SOS_PRESSED", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "SOS_BUTTON_2S_HOLD", false, false, false); // Giữ nguyên giá trị giao thức và mức cảnh báo.
            guiBleTrangThaiThietBi(); // Gửi trạng thái mới của thiết bị.
#endif // Kết thúc khối gửi sự kiện SOS qua BLE.
        }
    } else { // Nút SOS đã được thả.
        thoiDiemBamSosMs = 0; // Xóa mốc giữ nút để sẵn sàng cho lần sau.
        daKichHoatSos = false; // Cho phép lần nhấn SOS tiếp theo kích hoạt cảnh báo.
    }

    // Nút hủy tác động mức LOW; giữ ít nhất 300 ms sẽ hủy mọi cảnh báo đang hoạt động [Nguồn: đặc tả nút hủy].
    int giaTriNutHuy = digitalRead(CHAN_NUT_HUY); // Đọc mức điện của nút hủy.
    if (giaTriNutHuy == LOW) { // LOW nghĩa là nút đang được nhấn.
        if (thoiDiemBamHuyMs == 0) { // Đây là mẫu đầu tiên của lần giữ nút.
            thoiDiemBamHuyMs = thoiGianHienTaiMs; // Ghi thời điểm bắt đầu giữ nút hủy.
        } else if (thoiGianHienTaiMs - thoiDiemBamHuyMs >= 300) { // 300 ms [Nguồn: đặc tả nút hủy].
            if (trangThaiThietBi == STATE_LOCAL_ALERTING || trangThaiThietBi == STATE_VERIFYING || trangThaiThietBi == STATE_SUSPECTED) { // Chỉ hủy trạng thái cảnh báo hoặc xác minh đang hoạt động.
                Serial.println(F("[ACTION] Nguoi dung da huy canh bao bang nut CANCEL.")); // Báo trên Serial rằng người dùng vừa hủy cảnh báo bằng nút CANCEL vật lí.
                trangThaiThietBi = STATE_MONITORING; // Trở lại chế độ theo dõi bình thường.
                digitalWrite(CHAN_CANH_BAO, LOW); // Tắt còi ngay lập tức.
#if CO_BLE // Đồng bộ thao tác hủy qua BLE khi được bật.
                guiBleSuKien("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "USER_CANCEL_BUTTON", false, false, false); // Giữ nguyên giá trị giao thức và mã nguyên nhân.
                guiBleTrangThaiThietBi(); // Gửi trạng thái thiết bị sau khi hủy.
#endif // Kết thúc khối gửi sự kiện hủy qua BLE.
            }
        }
    } else { // Nút hủy đã được thả.
        thoiDiemBamHuyMs = 0; // Xóa mốc giữ nút để sẵn sàng cho lần sau.
    }
}

static void capNhatCoi(uint32_t thoiGianHienTaiMs) { // Điều khiển còi và đèn trạng thái theo chế độ thiết bị.
    if (trangThaiThietBi == STATE_LOCAL_ALERTING) { // Phát tín hiệu khẩn cấp khi đang cảnh báo tại chỗ.
        // Còi khẩn cấp luân phiên bật 200 ms và tắt 200 ms [Nguồn: mẫu cảnh báo firmware].
        bool dangBat = ((thoiGianHienTaiMs - thoiDiemBatDauCanhBaoMs) / 200) % 2 == 0; // Chu kì 200 ms [Nguồn: mẫu cảnh báo firmware].
        digitalWrite(CHAN_CANH_BAO, dangBat ? HIGH : LOW); // Cho còi chạy theo pha hiện tại.
        digitalWrite(CHAN_LED_TRANG_THAI, dangBat ? HIGH : LOW); // Cho đèn nháy đồng bộ với còi.
    } else if (hanChoTruongCoiMs > thoiGianHienTaiMs) { // Ưu tiên mẫu còi tùy chỉnh còn hiệu lực.
        // Mẫu còi tùy chỉnh được kích hoạt bởi lệnh BLE.
        bool dangBat = (thoiGianHienTaiMs / 150) % 2 == 0; // Đảo pha mỗi 150 ms [Nguồn: mẫu còi lệnh BLE].
        digitalWrite(CHAN_CANH_BAO, dangBat ? HIGH : LOW); // Bật hoặc tắt còi theo pha.
    } else { // Không còn cảnh báo hay mẫu còi tùy chỉnh.
        digitalWrite(CHAN_CANH_BAO, LOW); // Đảm bảo còi đã tắt.
        // Đèn trạng thái phát nhịp chậm khi thiết bị đang theo dõi.
        if (trangThaiThietBi == STATE_MONITORING) { // Hiển thị nhịp sống của chế độ theo dõi.
            digitalWrite(CHAN_LED_TRANG_THAI, (thoiGianHienTaiMs % 1000 < 50) ? HIGH : LOW); // Sáng 50 ms mỗi 1000 ms [Nguồn: mẫu đèn trạng thái firmware].
        } else if (trangThaiThietBi == STATE_DEGRADED) { // Báo thiết bị đang suy giảm chức năng.
            digitalWrite(CHAN_LED_TRANG_THAI, (thoiGianHienTaiMs % 250 < 125) ? HIGH : LOW); // Nháy lỗi nhanh 125 ms mỗi nửa chu kì 250 ms [Nguồn: mẫu đèn lỗi firmware].
        }
    }
}

// ============================================================================
// 13. BỘ MÁY PHÁT HIỆN TÉ NGÃ (BÁM SÁT NGHIÊM NGẶT ANDROID)
// ============================================================================
static void xuLyPhatHienTeNga(uint32_t thoiGianHienTaiMs, float ax, float ay, float az, float gx, float gy, float gz, // Nhận mẫu IMU hiện tại để chạy bộ máy phát hiện ngã.
                             float apSuatPa, float chenhLechDoCaoM, const char* &nhanSuKienDauRa) { // Nhận dữ liệu khí áp và trả nhãn sự kiện nếu có.
    nhanSuKienDauRa = nullptr; // Mặc định mẫu hiện tại chưa tạo sự kiện.
    float doLonGiaToc = sqrtf(ax * ax + ay * ay + az * az); // Tính độ lớn véc-tơ gia tốc tổng hợp.
    float vanTocGoc = sqrtf(gx * gx + gy * gy + gz * gz); // Tính độ lớn véc-tơ vận tốc góc.

    // Theo dõi gia tốc nhỏ nhất trước va chạm để bổ sung dấu hiệu rơi tự do.
    if (!trangThaiPhatHienNga.dangTrongCuaSoXacMinh) { // Chỉ cập nhật trước khi bước vào cửa sổ xác minh.
        if (doLonGiaToc < trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham || trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham == 0.0f) { // 0.0f là trạng thái chưa có mẫu trước va chạm.
            trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham = doLonGiaToc; // Lưu giá trị nhỏ nhất đã thấy.
        }
    }

    // Bước 1: phát hiện va chạm ≥ 25.0 m/s^2 → mở cửa sổ 3000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
    if (doLonGiaToc >= NGUONG_PHAT_HIEN_TE_NGA.giaTocVaChamMs2) { // 25.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
        // Ghi nhận va chạm mới hoặc chốt lại va chạm trong cửa sổ hiện tại.
        trangThaiPhatHienNga.thoiDiemVaChamMs = thoiGianHienTaiMs; // Lưu thời điểm va chạm làm đầu cửa sổ xác minh.
        trangThaiPhatHienNga.giaTocVaChamCucDai = doLonGiaToc; // Lưu độ lớn va chạm làm số liệu sự kiện.
        trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = 0; // Xóa mốc đứng yên của lần xác minh trước.
        trangThaiPhatHienNga.soMauDungYen = 0; // Xóa bộ đếm mẫu đứng yên.
        trangThaiPhatHienNga.dangTrongCuaSoXacMinh = true; // Mở cửa sổ xác minh sau va chạm.
        trangThaiPhatHienNga.mauCuoiCungMs = thoiGianHienTaiMs; // Ghi mốc mẫu để kiểm tra khoảng cách mẫu.
        trangThaiThietBi = STATE_SUSPECTED; // Chuyển sang trạng thái nghi ngờ ngã.
        nhanSuKienDauRa = "IMPACT_DETECTED"; // Giữ nguyên giá trị nhãn sự kiện giao thức.

        // Ghi véc-tơ trọng lực trước va chạm để tính thay đổi tư thế sau đó.
        trangThaiPhatHienNga.trongLucTruocVaCham[0] = camBienMpu.trongLucGocX; // Lưu thành phần trọng lực trục X.
        trangThaiPhatHienNga.trongLucTruocVaCham[1] = camBienMpu.trongLucGocY; // Lưu thành phần trọng lực trục Y.
        trangThaiPhatHienNga.trongLucTruocVaCham[2] = camBienMpu.trongLucGocZ; // Lưu thành phần trọng lực trục Z.

        // Tạo chuỗi nguyên nhân kích hoạt cho dữ liệu đo từ xa.
        snprintf(trangThaiPhatHienNga.nguyenNhaKichHoat, sizeof(trangThaiPhatHienNga.nguyenNhaKichHoat), // Giữ nguyên định dạng và thứ tự tham số.
                 "IMPACT_%.1f%s%s", // Giữ nguyên %.1f, %s, %s và thứ tự ba tham số theo giao thức.
                 doLonGiaToc, // %.1f là độ lớn gia tốc va chạm.
                 (trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham < NGUONG_PHAT_HIEN_TE_NGA.nguongTuDoMs2) ? ",FREE_FALL" : "", // 3.0 m/s^2 [Nguồn: suy luận hệ thống phục vụ ghi nhật kí].
                 (vanTocGoc > NGUONG_PHAT_HIEN_TE_NGA.vanTocGocCaoDps) ? ",HIGH_ROTATION" : ""); // 200.0 dps [Nguồn: suy luận hệ thống phục vụ ghi nhật kí].

        Serial.printf("[FALL_ENGINE] Phat hien va cham! |a| = %.2f m/s^2 >= %.2f. Chuyen sang SUSPECTED.\n", // Giữ nguyên hai mã %.2f và thứ tự tham số.
                      doLonGiaToc, NGUONG_PHAT_HIEN_TE_NGA.giaTocVaChamMs2); // In giá trị đo và ngưỡng va chạm.
#if CO_BLE // Gửi sự kiện va chạm qua BLE khi được bật.
        guiBleSuKien("IMPACT_DETECTED", "WARNING", 60, doLonGiaToc, 0.0f, chenhLechDoCaoM, 0, // Giữ nguyên loại, mức cảnh báo và các giá trị số giao thức.
                     trangThaiPhatHienNga.nguyenNhaKichHoat, true, false, (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc)); // Báo tình trạng dữ liệu khí áp.
#endif // Kết thúc khối gửi sự kiện va chạm qua BLE.
        return; // Kết thúc mẫu hiện tại sau khi ghi nhận va chạm.
    }

    // Bước 2: trong cửa sổ 3000 ms, cần ≥6 mẫu đứng yên kéo dài ≥1000 ms rồi mới xác nhận ngã [Nguồn: Android FallDetectionConfig.DEFAULT].
    if (trangThaiPhatHienNga.dangTrongCuaSoXacMinh) { // Tiếp tục đúng thứ tự kiểm tra của cửa sổ sau va chạm.
        uint32_t thoiGianQuaMs = thoiGianHienTaiMs - trangThaiPhatHienNga.thoiDiemVaChamMs; // Tính thời gian từ va chạm gần nhất.

        // Trước tiên kiểm tra khoảng cách mẫu tối đa 250 ms để bảo đảm chuỗi dữ liệu liên tục.
        if (thoiGianHienTaiMs - trangThaiPhatHienNga.mauCuoiCungMs > NGUONG_PHAT_HIEN_TE_NGA.maximumSampleGapMs) { // 250 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
            Serial.println(F("[FALL_ENGINE] Khoang cach mau vuot 250 ms. Dat lai cua so xac minh.")); // Báo rằng hai mẫu cách nhau quá 250 ms nên chuỗi xác minh ngã bị hủy vì không còn liên tục.
            trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false; // Đóng cửa sổ vì chuỗi mẫu bị gián đoạn.
            trangThaiThietBi = STATE_MONITORING; // Trở lại theo dõi bình thường.
            return; // Dừng xác minh vì khoảng cách mẫu vượt giới hạn.
        }
        trangThaiPhatHienNga.mauCuoiCungMs = thoiGianHienTaiMs; // Ghi nhận mẫu hợp lệ vừa xử lí.

        // Sau đó kiểm tra hết cửa sổ 3000 ms khi chưa đủ điều kiện đứng yên.
        if (thoiGianQuaMs > NGUONG_PHAT_HIEN_TE_NGA.cuaSoSauVaChamMs) { // 3000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
            Serial.println(F("[FALL_ENGINE] Cua so 3000 ms sau va cham da het ma chua dung yen lien tuc. Tro ve MONITORING.")); // Báo rằng đã hết 3000 ms sau va chạm mà chưa đủ thời gian đứng yên, nên thiết bị trở lại theo dõi.
            trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false; // Đóng cửa sổ xác minh hết hạn.
            trangThaiThietBi = STATE_MONITORING; // Trở lại theo dõi bình thường.
            return; // Dừng xác minh vì cửa sổ sau va chạm đã hết hạn.
        }

        // Tiếp theo kiểm tra đứng yên: ||a| - 9.81| ≤ 1.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
        float saiLechDungYen = fabsf(doLonGiaToc - NGUONG_PHAT_HIEN_TE_NGA.nguongDungYenMs2); // Mục tiêu 9.81 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
        if (saiLechDungYen <= NGUONG_PHAT_HIEN_TE_NGA.nguongSaiSoDungYenMs2) { // Sai số 1.0 m/s^2 [Nguồn: Android FallDetectionConfig.DEFAULT].
            if (trangThaiPhatHienNga.thoiDiemBatDauDungYenMs == 0) { // Đây là mẫu đứng yên đầu tiên trong chuỗi.
                trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = thoiGianHienTaiMs; // Bắt đầu đo thời gian đứng yên.
                trangThaiPhatHienNga.soMauDungYen = 1; // Tính mẫu đầu tiên vào chuỗi.
                trangThaiThietBi = STATE_VERIFYING; // Chuyển sang trạng thái đang xác minh.
            } else { // Chuỗi đứng yên vẫn liên tục.
                trangThaiPhatHienNga.soMauDungYen++; // Tăng số mẫu đứng yên liên tiếp.
            }

            uint32_t thoiGianDungYen = thoiGianHienTaiMs - trangThaiPhatHienNga.thoiDiemBatDauDungYenMs; // Tính độ dài chuỗi đứng yên hiện tại.

            // Cuối cùng chỉ xác nhận khi đủ ≥6 mẫu VÀ kéo dài ≥1000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].
            if (trangThaiPhatHienNga.soMauDungYen >= NGUONG_PHAT_HIEN_TE_NGA.minimumStillnessSamples && // Tối thiểu 6 mẫu [Nguồn: Android FallDetectionConfig.DEFAULT].
                thoiGianDungYen >= NGUONG_PHAT_HIEN_TE_NGA.thoiGianDungYenSauVaChamMs) { // Tối thiểu 1000 ms [Nguồn: Android FallDetectionConfig.DEFAULT].

                // Tính thay đổi tư thế từ véc-tơ trọng lực trước va chạm và gia tốc sau va chạm.
                float tichVoHuong = (trangThaiPhatHienNga.trongLucTruocVaCham[0] * ax + // Tính thành phần trục X của tích vô hướng.
                                     trangThaiPhatHienNga.trongLucTruocVaCham[1] * ay + // Cộng thành phần trục Y.
                                     trangThaiPhatHienNga.trongLucTruocVaCham[2] * az); // Cộng thành phần trục Z.
                float doLonTruocVaCham = sqrtf(trangThaiPhatHienNga.trongLucTruocVaCham[0]*trangThaiPhatHienNga.trongLucTruocVaCham[0] + // Bình phương thành phần X.
                                               trangThaiPhatHienNga.trongLucTruocVaCham[1]*trangThaiPhatHienNga.trongLucTruocVaCham[1] + // Bình phương thành phần Y.
                                               trangThaiPhatHienNga.trongLucTruocVaCham[2]*trangThaiPhatHienNga.trongLucTruocVaCham[2]); // Bình phương thành phần Z và lấy căn.
                float thayDoiGocDo = 0.0f; // Mặc định chưa ghi nhận thay đổi góc.
                if (doLonTruocVaCham > 0.1f && doLonGiaToc > 0.1f) { // 0.1f tránh chia cho véc-tơ gần bằng không [Nguồn: bảo vệ tính toán firmware].
                    float cosGoc = tichVoHuong / (doLonTruocVaCham * doLonGiaToc); // Tính cosin của góc giữa hai véc-tơ.
                    if (cosGoc > 1.0f) cosGoc = 1.0f; // Chặn sai số số thực ở biên trên 1.0f.
                    if (cosGoc < -1.0f) cosGoc = -1.0f; // Chặn sai số số thực ở biên dưới -1.0f.
                    thayDoiGocDo = acosf(cosGoc) * (180.0f / 3.14159265f); // Đổi radian sang độ bằng 180.0f/3.14159265f.
                }

                // Tính điểm tin cậy theo kinh nghiệm trong khoảng 0–100.
                int doTinCay = 85; // Điểm cơ sở 85 [Nguồn: kinh nghiệm chấm điểm của firmware].
                if (thayDoiGocDo >= NGUONG_PHAT_HIEN_TE_NGA.nguongThayDoiTuTheDo) doTinCay += 10; // 45.0 độ và cộng 10 điểm [Nguồn: suy luận hệ thống phục vụ ghi nhật kí].
                if (trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham < NGUONG_PHAT_HIEN_TE_NGA.nguongTuDoMs2) doTinCay += 5; // 3.0 m/s^2 và cộng 5 điểm [Nguồn: suy luận hệ thống phục vụ ghi nhật kí].
                if (doTinCay > 100) doTinCay = 100; // Giới hạn điểm tối đa ở 100.

                trangThaiThietBi = STATE_LOCAL_ALERTING; // Chuyển sang cảnh báo tại chỗ sau khi xác nhận ngã.
                thoiDiemBatDauCanhBaoMs = thoiGianHienTaiMs; // Ghi mốc bắt đầu cảnh báo.
                boDemHeThong.suKienNgaDaXacMinh++; // Tăng bộ đếm ngã đã xác nhận.
                trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false; // Đóng cửa sổ xác minh đã hoàn tất.
                nhanSuKienDauRa = "FALL_CONFIRMED"; // Giữ nguyên giá trị nhãn sự kiện giao thức.

                Serial.printf("[FALL_ENGINE] *** DA XAC MINH TE NGA! *** Dung yen: %u ms (%u mau), Goc: %.1f do, Tin cay: %d\n", // Giữ nguyên %u, %u, %.1f, %d và thứ tự tham số.
                              thoiGianDungYen, trangThaiPhatHienNga.soMauDungYen, thayDoiGocDo, doTinCay); // Truyền đúng bốn giá trị cho chuỗi định dạng.

#if CO_BLE // Gửi sự kiện ngã đã xác nhận qua BLE khi được bật.
                guiBleSuKien("INACTIVITY_DETECTED", "CRITICAL", doTinCay, // Giữ nguyên loại sự kiện và mức CRITICAL của giao thức.
                             trangThaiPhatHienNga.giaTocVaChamCucDai, thayDoiGocDo, chenhLechDoCaoM, // Gửi số đo va chạm, góc và độ cao.
                             thoiGianDungYen, trangThaiPhatHienNga.nguyenNhaKichHoat, true, true, (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc)); // Gửi thời gian đứng yên, nguyên nhân và cờ chất lượng.
                guiBleTrangThaiThietBi(); // Đồng bộ trạng thái cảnh báo mới.
#endif // Kết thúc khối gửi sự kiện ngã đã xác nhận qua BLE.
            }
        } else { // Mẫu hiện tại không còn thỏa điều kiện đứng yên.
            // Khi đứng yên bị gián đoạn trong cửa sổ, đặt lại mốc và số mẫu nhưng vẫn tiếp tục theo dõi cửa sổ.
            trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = 0; // Xóa mốc bắt đầu chuỗi đứng yên.
            trangThaiPhatHienNga.soMauDungYen = 0; // Xóa số mẫu đứng yên liên tiếp.
        }
    }
}

// ============================================================================
// 14. BỘ ĐIỀU PHỐI LỆNH (§8.5)
// ============================================================================
#if CO_BLE // Chỉ biên dịch bộ điều phối khi tính năng BLE được bật.
static void xuLyLenhBle(const char* noiDungLenh) { // Phân tích và thực hiện một lệnh nhận qua BLE.
    // Phân tích JSON chắc chắn với các trường lệnh chuẩn: commandType hoặc command, cùng commandId.
    char tenLenh[32] = ""; // Bộ đệm tên lệnh, tối đa 31 kí tự và một kí tự kết thúc chuỗi.
    char cmdId[37] = ""; // Giữ nguyên tên mã lệnh theo hợp đồng giao thức.

    const char* viTriTen = strstr(noiDungLenh, "\"commandType\":"); // Tìm khóa JSON commandType nhưng không đổi tên khóa.
    if (!viTriTen) viTriTen = strstr(noiDungLenh, "\"command\":"); // Chấp nhận khóa command dự phòng của giao thức.
    if (viTriTen) { // Chỉ đọc tên lệnh khi đã tìm thấy một khóa hợp lệ.
        if (strstr(viTriTen, "\"commandType\":") == viTriTen) { // Phân biệt đúng khóa commandType.
            sscanf(viTriTen, "\"commandType\":\"%31[^\"]\"", tenLenh); // Giữ nguyên khóa JSON và giới hạn 31 kí tự.
        } else { // Nếu khóa là command thì dùng định dạng tương ứng.
            sscanf(viTriTen, "\"command\":\"%31[^\"]\"", tenLenh); // Đọc tên từ khóa command dự phòng.
        }
    }
    const char* viTriMa = strstr(noiDungLenh, "\"commandId\":"); // Tìm khóa JSON commandId, giữ nguyên tên giao thức.
    if (viTriMa) { // Chỉ đọc mã lệnh khi khóa tồn tại.
        sscanf(viTriMa, "\"commandId\":\"%36[^\"]\"", cmdId); // Giới hạn mã ở 36 kí tự để tránh tràn bộ đệm.
    }

    // Kiểm tra lệnh trùng: trả REJECTED cùng mã DUPLICATE_COMMAND theo giao thức.
    if (strlen(cmdId) > 0 && strcmp(lenhCuoiCung, cmdId) == 0) { // So mã hợp lệ với lệnh đã xử lí gần nhất.
        guiBleAck(cmdId, "REJECTED", "DUPLICATE_COMMAND", "Lenh da duoc xu ly truoc do"); // Trả REJECTED với mã DUPLICATE_COMMAND để ứng dụng biết lệnh có cùng ID đã được xử lí.
        return; // Không thực hiện lại lệnh trùng.
    }
    if (strlen(cmdId) > 0) { // Chỉ lưu mã lệnh không rỗng.
        strncpy(lenhCuoiCung, cmdId, sizeof(lenhCuoiCung) - 1); // Chừa một byte kết thúc chuỗi trong bộ đệm đích.
    }

    Serial.printf("[BLE_CMD] Dieu phoi lenh '%s' (ID: %s)\n", tenLenh, cmdId); // Giữ nguyên hai mã %s và thứ tự tên, mã lệnh.

    if (strcmp(tenLenh, "PING") == 0) { // Nếu nhận PING thì kiểm tra đường truyền bằng cách trả lời PONG.
        guiBleAck(cmdId, "COMPLETED", nullptr, "PONG"); // Phản hồi hoàn tất kiểm tra kết nối.
    } else if (strcmp(tenLenh, "GET_STATUS") == 0) { // Nếu nhận GET_STATUS thì gửi ngay bản trạng thái hiện tại cho ứng dụng.
        guiBleTrangThaiThietBi(); // Gửi ngay trạng thái thiết bị hiện tại.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da bao cao trang thai"); // Xác nhận hoàn tất bằng chuỗi không dấu.
    } else if (strcmp(tenLenh, "START_STREAM") == 0) { // Nếu nhận START_STREAM thì bật luồng dữ liệu cảm biến định kì qua BLE.
        truyenBle = true; // Bật truyền dữ liệu đo từ xa.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da bat luong du lieu"); // Xác nhận đã bật luồng.
    } else if (strcmp(tenLenh, "STOP_STREAM") == 0) { // Nếu nhận STOP_STREAM thì ngừng gửi luồng dữ liệu cảm biến định kì qua BLE.
        truyenBle = false; // Tắt truyền dữ liệu đo từ xa.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da tat luong du lieu"); // Xác nhận đã tắt luồng.
    } else if (strcmp(tenLenh, "SET_SAMPLE_RATE") == 0) { // Nếu nhận SET_SAMPLE_RATE thì đọc tần số yêu cầu và chỉ chấp nhận 50 hoặc 100 Hz.
        // BẢN SỬA 3: đọc sampleRateHz và chỉ chấp nhận 100 hoặc 50 Hz.
        int tanSoYeuCau = 0; // Khởi tạo tần số yêu cầu trước khi đọc JSON.
        const char* viTriTanSo = strstr(noiDungLenh, "\"sampleRateHz\":"); // Giữ nguyên khóa JSON sampleRateHz.
        if (viTriTanSo) { // Chỉ phân tích khi trường tần số tồn tại.
            if (sscanf(viTriTanSo, "\"sampleRateHz\":\"%d\"", &tanSoYeuCau) != 1) { // Thử dạng chuỗi trước.
                sscanf(viTriTanSo, "\"sampleRateHz\":%d", &tanSoYeuCau); // Sau đó thử dạng số JSON.
            }
        }
        if (tanSoYeuCau == 100 || tanSoYeuCau == 50) { // Chỉ cho phép đúng hai tốc độ lấy mẫu hỗ trợ.
            tanSoLayMauImuHz = tanSoYeuCau; // Cập nhật tần số hoạt động.
            chuKyLayMauImuUs = 1000000UL / tanSoLayMauImuHz; // Đổi Hz thành chu kì micro giây.
            if (camBienMpu.dangHoatDong) { // Chỉ cấu hình thanh ghi khi MPU6050 đang trực tuyến.
                uint8_t heSoChiaTanSo = (tanSoYeuCau == 50) ? 19 : 9; // Giữ nguyên tỉ lệ chia 19 cho 50 Hz, 9 cho 100 Hz.
                ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_SMPLRT_DIV, heSoChiaTanSo); // Ghi hệ số chia vào MPU6050.
            }
            char thongDiep[64]; // Bộ đệm nội dung xác nhận tần số.
            snprintf(thongDiep, sizeof(thongDiep), "Da dat tan so lay mau %d Hz", tanSoYeuCau); // Giữ nguyên %d và một tham số tần số.
            guiBleAck(cmdId, "COMPLETED", nullptr, thongDiep); // Gửi xác nhận hoàn tất.
        } else { // Từ chối giá trị ngoài hai tần số được hỗ trợ.
            guiBleAck(cmdId, "REJECTED", "UNSUPPORTED_SAMPLE_RATE", "Chi ho tro 100 Hz va 50 Hz"); // Giữ nguyên trạng thái, mã lỗi và hai giá trị số.
        }
    } else if (strcmp(tenLenh, "SET_REFERENCE_ALTITUDE") == 0) { // Nếu nhận SET_REFERENCE_ALTITUDE thì lấy áp suất hiện tại làm mốc độ cao 0 mét.
        // BẢN SỬA 3: dùng áp suất ổn định hiện tại làm mốc độ cao 0 m.
        if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc && camBienKhiAp.apSuatPa > 10000.0f) { // Chỉ đặt mốc từ số đo khí áp hợp lệ.
            apSuatThamChieuPa = camBienKhiAp.apSuatPa; // Lưu áp suất hiện tại làm mốc.
            camBienKhiAp.chenhLechDoCaoM = 0.0f; // Đặt chênh lệch độ cao tại mốc về 0 m.
            guiBleAck(cmdId, "COMPLETED", nullptr, "Da dat lai do cao moc 0m theo ap suat hien tai"); // Xác nhận đặt mốc thành công.
        } else { // Báo lỗi khi cảm biến khí áp không sẵn sàng.
            guiBleAck(cmdId, "FAILED", "BAROMETER_UNAVAILABLE", "Khong the dat do cao moc: cam bien khi ap ngoai tuyen"); // Trả FAILED với mã BAROMETER_UNAVAILABLE khi không có số đo khí áp hợp lệ để đặt mốc.
        }
    } else if (strcmp(tenLenh, "TRIGGER_BUZZER") == 0) { // Nếu nhận TRIGGER_BUZZER thì bật mẫu còi trong thời lượng yêu cầu hoặc mặc định 2000 ms.
        int khoangThoiGianMs = 2000; // Dùng 2000 ms khi lệnh không cung cấp thời lượng.
        const char* viTriThoiGian = strstr(noiDungLenh, "\"remainingMs\":"); // Giữ nguyên khóa JSON remainingMs.
        if (viTriThoiGian) sscanf(viTriThoiGian, "\"remainingMs\":%d", &khoangThoiGianMs); // Đọc thời lượng còi từ JSON.
        hanChoTruongCoiMs = millis() + khoangThoiGianMs; // Tính thời điểm kết thúc mẫu còi.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da bat mau coi canh bao"); // Xác nhận còi đang hoạt động.
    } else if (strcmp(tenLenh, "STOP_BUZZER") == 0) { // Nếu nhận STOP_BUZZER thì xóa thời hạn phát và tắt còi ngay.
        hanChoTruongCoiMs = 0; // Xóa hạn phát còi.
        digitalWrite(CHAN_CANH_BAO, LOW); // Tắt ngay chân điều khiển còi.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da tat coi canh bao"); // Xác nhận còi đã tắt.
    } else if (strcmp(tenLenh, "ACK_EVENT") == 0) { // Nếu nhận ACK_EVENT thì xác nhận ứng dụng đã thấy sự kiện nhưng không tắt cảnh báo tại chỗ.
        // Theo kế hoạch §7.3, §8.5: ACK_EVENT KHÔNG tắt cảnh báo cục bộ.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da xac nhan su kien (canh bao cuc bo van bat den khi huy)"); // Xác nhận sự kiện nhưng giữ còi.
    } else if (strcmp(tenLenh, "CANCEL_ALERT") == 0) { // Nếu nhận CANCEL_ALERT thì hủy tiến trình cảnh báo và đưa thiết bị về chế độ theo dõi.
        if (trangThaiThietBi == STATE_LOCAL_ALERTING || trangThaiThietBi == STATE_VERIFYING || trangThaiThietBi == STATE_SUSPECTED) { // Chỉ hủy khi đang có tiến trình cảnh báo.
            trangThaiThietBi = STATE_MONITORING; // Trở về trạng thái theo dõi.
            digitalWrite(CHAN_CANH_BAO, LOW); // Tắt còi cục bộ.
            guiBleSuKien("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "BLE_CANCEL_COMMAND", false, false, false); // Giữ nguyên loại, mức và số liệu sự kiện.
            guiBleTrangThaiThietBi(); // Gửi trạng thái sau khi hủy.
            guiBleAck(cmdId, "COMPLETED", nullptr, "Da huy canh bao bang lenh BLE tu xa"); // Xác nhận hủy thành công.
        } else { // Hoàn tất an toàn khi không có cảnh báo cần hủy.
            guiBleAck(cmdId, "COMPLETED", nullptr, "Khong co canh bao dang hoat dong de huy"); // Không có cảnh báo vẫn hoàn tất lệnh an toàn.
        }
    } else if (strcmp(tenLenh, "START_SELF_TEST") == 0) { // Nếu nhận START_SELF_TEST thì chạy lại tự kiểm tra và hiệu chuẩn các cảm biến.
        tuKiemTraVaHieuChuan(); // Chạy tự kiểm tra và hiệu chuẩn phần cứng.
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da hoan tat tu kiem tra"); // Xác nhận tự kiểm tra hoàn tất.
    } else if (strcmp(tenLenh, "SET_DEVICE_TIME") == 0) { // Nếu nhận SET_DEVICE_TIME thì đồng bộ đồng hồ thiết bị từ thời gian Unix trong lệnh.
        // Theo kế hoạch §8.5, §8.6: đồng bộ dấu thời gian Unix theo milli giây.
        uint64_t thoiGianUnix = 0; // Khởi tạo giá trị epoch trước khi đọc JSON.
        const char* viTriThoiGianUnix = strstr(noiDungLenh, "\"unixTimeMs\":"); // Tìm khóa JSON unixTimeMs.
        if (!viTriThoiGianUnix) viTriThoiGianUnix = strstr(noiDungLenh, "\"epochTime\":"); // Chấp nhận khóa epochTime dự phòng.
        if (viTriThoiGianUnix) { // Chỉ đọc khi có một trong hai khóa thời gian.
            if (sscanf(viTriThoiGianUnix, "\"unixTimeMs\":%llu", &thoiGianUnix) == 1 || // Thử unixTimeMs dạng số.
                sscanf(viTriThoiGianUnix, "\"unixTimeMs\":\"%llu\"", &thoiGianUnix) == 1 || // Thử unixTimeMs dạng chuỗi.
                sscanf(viTriThoiGianUnix, "\"epochTime\":%llu", &thoiGianUnix) == 1) { // Thử epochTime dạng số.
                if (thoiGianUnix < 10000000000ULL) thoiGianUnix *= 1000ULL; // Đổi giây sang milli giây khi giá trị còn ngắn.
                thoiGianEpochThietBiMs = thoiGianUnix; // Lưu epoch đã chuẩn hóa.
                thoiDiemDongBoCuoiMs = millis(); // Ghi mốc thời gian chạy lúc đồng bộ.
                daDongBoThoiGian = true; // Đánh dấu đồng hồ đã đồng bộ.
                if (maLoiCuoiCung != nullptr && strcmp(maLoiCuoiCung, "TIME_NOT_SYNCED") == 0) { // Chỉ xóa đúng lỗi chưa đồng bộ.
                    maLoiCuoiCung = nullptr; // Xóa trạng thái lỗi thời gian.
                }
            }
        }
        guiBleAck(cmdId, "COMPLETED", nullptr, "Da dong bo dong ho epoch"); // Xác nhận đồng bộ thời gian.
    } else if (strcmp(tenLenh, "REBOOT_DEVICE") == 0) { // Nếu nhận REBOOT_DEVICE thì khởi động lại ESP32-S3, trừ khi thiết bị đang cảnh báo.
        // Theo kế hoạch §8.5: từ chối khởi động lại nếu thiết bị đang cảnh báo.
        if (trangThaiThietBi == STATE_LOCAL_ALERTING || trangThaiThietBi == STATE_VERIFYING) { // Bảo vệ cảnh báo đang hoạt động.
            guiBleAck(cmdId, "REJECTED", "DEVICE_ALERTING", "Khong the khoi dong lai khi dang canh bao"); // Trả REJECTED với mã DEVICE_ALERTING để tránh khởi động lại làm mất một cảnh báo đang hoạt động.
        } else { // Chỉ khởi động lại khi thiết bị không ở trạng thái cảnh báo.
            guiBleAck(cmdId, "ACCEPTED", nullptr, "Dang khoi dong lai thiet bi..."); // Báo chấp nhận trước khi khởi động lại.
            delay(100); // Chờ 100 ms để gói xác nhận kịp được gửi.
            esp_restart(); // Khởi động lại ESP32-S3 bằng hàm hệ thống.
        }
    } else { // Xử lí mọi tên lệnh không thuộc danh sách hỗ trợ.
        guiBleAck(cmdId, "REJECTED", "UNKNOWN_COMMAND", "Khong nhan dang duoc lenh"); // Trả REJECTED với mã UNKNOWN_COMMAND để ứng dụng biết tên lệnh không được firmware hỗ trợ.
    }
}
#endif // Kết thúc bộ điều phối lệnh BLE.

// ============================================================================
// 15. THIẾT LẬP VÀ KHỞI TẠO — Các enum phải khai báo ở đầu tệp để kiểu trạng thái có sẵn trước khi biến và hàm sử dụng.
// ============================================================================
void setup() { // Hàm khởi tạo chuẩn của Arduino, giữ nguyên tên hệ thống.
    Serial.begin(115200); // Mở cổng Serial ở đúng tốc độ 115200 baud.

#if ARDUINO_USB_CDC_ON_BOOT // Chờ USB CDC gốc khi cấu hình bo mạch bật tính năng này.
    // Chờ ngắn để USB-CDC gốc có thời gian kết nối nếu hiện diện.
    delay(500); // Giữ đúng 500 ms, đủ cho máy tính nhận cổng mà không làm chậm lâu.
#endif // Kết thúc phần chờ USB CDC khi khởi động.

    // Tạo tên thiết bị BLE dạng FALLSAFE-xxxx từ địa chỉ MAC Bluetooth.
    uint8_t diaChiMac[6]; // Bộ đệm sáu byte của địa chỉ MAC.
    esp_read_mac(diaChiMac, ESP_MAC_BT); // Đọc MAC Bluetooth bằng hàm hệ thống ESP.
    snprintf(tenThietBiBle, sizeof(tenThietBiBle), "FALLSAFE-%02X%02X", diaChiMac[4], diaChiMac[5]); // Giữ nguyên hai mã %02X và thứ tự hai byte cuối.

    Serial.println(F("\n\n=======================================================")); // In đường phân cách mở đầu.
    Serial.println(F("NCKH27PA - FIRMWARE CHINH THUC ESP32-S3 SUPER MINI")); // In tên firmware bằng tiếng Việt không dấu.
    Serial.printf("Ma thiet bi: %s | Ban dung: %s %s | Giao thuc: %d\n", // Giữ nguyên ba mã %s, một mã %d và thứ tự tham số.
                  tenThietBiBle, __DATE__, __TIME__, PROTOCOL_VERSION); // In tên BLE, ngày, giờ biên dịch và phiên bản giao thức.
    Serial.println(F("=======================================================")); // In đường phân cách kết thúc tiêu đề.

    // Cấu hình các chân GPIO.
    pinMode(CHAN_NUT_SOS, INPUT_PULLUP); // Đặt nút SOS là đầu vào có điện trở kéo lên.
    pinMode(CHAN_NUT_HUY, INPUT_PULLUP); // Đặt nút hủy là đầu vào có điện trở kéo lên.
    pinMode(CHAN_CANH_BAO, OUTPUT); // Đặt chân còi cảnh báo là đầu ra.
    pinMode(CHAN_LED_TRANG_THAI, OUTPUT); // Đặt LED trạng thái là đầu ra.
    pinMode(CHAN_LED_PIN_1, OUTPUT); // Đặt LED mức pin 1 là đầu ra.
    pinMode(CHAN_LED_PIN_2, OUTPUT); // Đặt LED mức pin 2 là đầu ra.
    pinMode(CHAN_LED_PIN_3, OUTPUT); // Đặt LED mức pin 3 là đầu ra.
    digitalWrite(CHAN_CANH_BAO, LOW); // Bảo đảm còi tắt khi vừa khởi động.
    digitalWrite(CHAN_LED_TRANG_THAI, LOW); // Bảo đảm LED trạng thái tắt ban đầu.

    // Khởi tạo bus I2C 0 cho MPU6050 (SDA = GPIO 8, SCL = GPIO 9, 400 kHz).
    Wire.begin(CHAN_I2C0_SDA, CHAN_I2C0_SCL, 400000); // Giữ đúng tần số I2C 400000 Hz.
    Wire.setTimeOut(25); // Thời gian chờ 25 ms ngăn bus kẹt làm chặn nút SOS.

    // Khởi tạo bus I2C 1 cho MS5611 (SDA = GPIO 7, SCL = GPIO 6, 400 kHz).
    Wire1.begin(CHAN_I2C1_SDA, CHAN_I2C1_SCL, 400000); // Giữ đúng tần số I2C 400000 Hz.
    Wire1.setTimeOut(25); // Áp dụng cùng thời gian chờ 25 ms cho bus khí áp.

    // Quét I2C trên cả hai bus lúc khởi động; chỉ ghi nhật kí và không bao giờ dừng hệ thống.
    quetI2c(Wire, "bus0/Wire"); // Quét bus 0 của MPU6050.
    quetI2c(Wire1, "bus1/Wire1"); // Quét bus 1 của MS5611.
    {
        bool timThayMpu = false, timThayGy63 = false; // Theo dõi riêng kết quả phát hiện hai cảm biến.
        for (uint8_t diaChiThu : {(uint8_t)0x68, (uint8_t)0x69}) { // Thử hai địa chỉ hợp lệ của MPU6050.
            Wire.beginTransmission(diaChiThu); // Bắt đầu thăm dò địa chỉ trên bus 0.
            if (Wire.endTransmission() == 0) timThayMpu = true; // Ghi nhận MPU6050 khi thiết bị phản hồi.
        }
        for (uint8_t diaChiThu : {(uint8_t)0x76, (uint8_t)0x77}) { // Thử hai địa chỉ hợp lệ của GY63/MS5611.
            Wire1.beginTransmission(diaChiThu); // Bắt đầu thăm dò địa chỉ trên bus 1.
            if (Wire1.endTransmission() == 0) timThayGy63 = true; // Ghi nhận GY63 khi thiết bị phản hồi.
        }
        Serial.printf("[KHOI_DONG] I2C bus0 MPU6050 (0x68/0x69): %s\n", timThayMpu ? "DAT" : "LOI (van tiep tuc, danh dau cam bien NGOAI_TUYEN neu khoi tao that bai)"); // Giữ nguyên %s và các địa chỉ.
        Serial.printf("[KHOI_DONG] I2C bus1 GY63 (0x76/0x77): %s\n", timThayGy63 ? "DAT" : "LOI (van tiep tuc, danh dau cam bien NGOAI_TUYEN neu khoi tao that bai)"); // Giữ nguyên %s và các địa chỉ.
    }

    // In cấu hình và các ngưỡng phát hiện.
    inBangNguong(); // Hiển thị bảng ngưỡng để kiểm tra lúc khởi động.

    // Tự kiểm tra và hiệu chuẩn phần cứng.
    tuKiemTraVaHieuChuan(); // Kiểm tra cảm biến trước khi bật liên lạc.

#if CO_BLE // Khởi tạo BLE khi tính năng được bật.
    BLEDevice::init(tenThietBiBle); // Khởi tạo BLE với tên vừa tạo.
    BLEDevice::setMTU(517); // Giữ đúng MTU yêu cầu 517 byte.
    mayChuBle = BLEDevice::createServer(); // Tạo máy chủ GATT.
    mayChuBle->setCallbacks(new ServerCallbacks()); // Gắn callback kết nối, giữ nguyên lớp thư viện.

    BLEService *dichVuBle = mayChuBle->createService(UUID_SERVICE); // Tạo dịch vụ bằng UUID giao thức giữ nguyên.

    dacTruTruyenDuLieu = dichVuBle->createCharacteristic( // Tạo đặc trưng truyền dữ liệu cảm biến.
        UUID_CHAR_STREAM, // Giữ nguyên UUID đặc trưng luồng dữ liệu.
        BLECharacteristic::PROPERTY_NOTIFY // Dùng notify cho dữ liệu thời gian thực.
    );
    dacTruTruyenDuLieu->addDescriptor(new BLE2902()); // Thêm mô tả bật thông báo phía máy khách.

    dacTruSuKien = dichVuBle->createCharacteristic( // Tạo đặc trưng sự kiện cảnh báo.
        UUID_CHAR_EVENT, // Giữ nguyên UUID đặc trưng sự kiện.
        BLECharacteristic::PROPERTY_INDICATE // Dùng indicate để có xác nhận nhận sự kiện.
    );
    dacTruSuKien->addDescriptor(new BLE2902()); // Thêm mô tả điều khiển indicate.

    dacTruTrangThai = dichVuBle->createCharacteristic( // Tạo đặc trưng trạng thái thiết bị.
        UUID_CHAR_STATUS, // Giữ nguyên UUID đặc trưng trạng thái.
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY // Cho phép đọc và nhận thông báo.
    );
    dacTruTrangThai->addDescriptor(new BLE2902()); // Thêm mô tả bật thông báo trạng thái.

    dacTruLenh = dichVuBle->createCharacteristic( // Tạo đặc trưng nhận lệnh.
        UUID_CHAR_COMMAND, // Giữ nguyên UUID đặc trưng lệnh.
        BLECharacteristic::PROPERTY_WRITE // Cho phép điện thoại ghi lệnh.
    );
    dacTruLenh->setCallbacks(new CommandCallbacks()); // Gắn callback nhận lệnh, giữ nguyên lớp thư viện.

    dacTruXacNhan = dichVuBle->createCharacteristic( // Tạo đặc trưng xác nhận lệnh.
        UUID_CHAR_ACK, // Giữ nguyên UUID đặc trưng xác nhận.
        BLECharacteristic::PROPERTY_NOTIFY // Gửi xác nhận bằng notify.
    );
    dacTruXacNhan->addDescriptor(new BLE2902()); // Thêm mô tả bật thông báo xác nhận.

    dichVuBle->start(); // Khởi động dịch vụ GATT sau khi tạo đủ đặc trưng.

    BLEAdvertising *quangBaBle = BLEDevice::getAdvertising(); // Lấy đối tượng quảng bá BLE.
    quangBaBle->addServiceUUID(UUID_SERVICE); // Công bố UUID dịch vụ chính.
    quangBaBle->setScanResponse(true); // Cho phép phản hồi quét để gửi thêm thông tin.
    quangBaBle->setMinPreferred(0x06); // Giá trị ưu tiên giúp xử lí vấn đề kết nối iPhone.
    quangBaBle->setMinPreferred(0x12); // Thêm giá trị ưu tiên tương thích iPhone.
    BLEDevice::startAdvertising(); // Bật quảng bá BLE trước khi bắt đầu WiFi để thiết bị sớm được tìm thấy và tránh WiFi làm chậm BLE.

    Serial.printf("[BLE] Da khoi tao may chu GATT. Ten thiet bi: %s\n", tenThietBiBle); // Giữ nguyên %s và tham số tên thiết bị.
#endif // Kết thúc phần khởi tạo BLE.

    // Luôn giữ thứ tự bật sóng BLE trước rồi mới WiFi để BLE quảng bá sớm; WiFi kết nối song song, không chặn BLE.
    WiFi.mode(WIFI_STA); // Đặt WiFi ở chế độ trạm.
    WiFi.begin(WIFI_SSID, WIFI_PASS); // Bắt đầu kết nối bằng thông tin mạng giữ nguyên.
    Serial.printf("[WIFI] Dang ket noi SSID: %s (khong chan, BLE khong bi anh huong)\n", WIFI_SSID); // Giữ nguyên %s và tham số SSID.

    // Khởi tạo bộ giám sát chương trình với thời gian chờ 5 giây.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) // Dùng API cấu hình của ESP-IDF phiên bản 5 trở lên.
    esp_task_wdt_config_t cauHinhWatchdog = { // Tạo cấu hình watchdog theo API hệ thống.
        .timeout_ms = 5000, // Giữ đúng thời gian chờ 5000 ms.
        .idle_core_mask = 0, // Không tự thêm tác vụ nhàn rỗi của lõi.
        .trigger_panic = false // Không kích hoạt panic khi hết thời gian.
    };
    esp_task_wdt_deinit(); // Gỡ cấu hình watchdog có thể đã tồn tại.
    esp_task_wdt_init(&cauHinhWatchdog); // Khởi tạo watchdog bằng cấu hình mới.
    esp_task_wdt_add(NULL); // Theo dõi chính tác vụ setup/loop hiện tại.
#else // Dùng API watchdog cũ với ESP-IDF trước phiên bản 5.
    esp_task_wdt_init(5, false); // Với API cũ, đặt thời gian chờ đúng 5 giây và không panic.
    esp_task_wdt_add(NULL); // Theo dõi tác vụ Arduino hiện tại.
#endif // Kết thúc lựa chọn API watchdog theo phiên bản ESP-IDF.

    if (cheDoCsvTho) { // Chỉ in tiêu đề CSV khi đang ở chế độ dữ liệu thô.
        inTieuDeCsv(); // In tên các cột trước dòng dữ liệu đầu tiên.
    }
    Serial.println(F("[HE_THONG] Khoi tao hoan tat. He thong dang hoat dong.")); // Báo hoàn tất bằng tiếng Việt không dấu.
}

// ============================================================================
// 16. VÒNG LẶP CHÍNH (Lịch IMU 100/50 Hz cấu hình được và tác vụ không chặn) — Trước notify() phải kiểm tra strlen(boDem) > mtuBle - 3 vì ATT giữ 3 byte tiêu đề, tránh gửi tải vượt MTU.
// ============================================================================
void loop() { // Hàm vòng lặp chuẩn của Arduino, giữ nguyên tên hệ thống.
    // Đặt lại watchdog ngay đầu loop() để mỗi vòng chứng minh tác vụ còn sống trước khi làm các khối xử lí có thể kéo dài.
    esp_task_wdt_reset(); // Nuôi bộ giám sát trước mọi công việc của vòng lặp.

    uint32_t thoiGianHienTaiMs = millis(); // Chụp thời gian milli giây dùng thống nhất trong vòng này.
    uint32_t thoiGianHienTaiUs = micros(); // Chụp thời gian micro giây để lập lịch IMU và khí áp.
    static TrangThaiThietBi trangThaiDaInTienTrinh = (TrangThaiThietBi)-1; // Nhớ trạng thái đã in qua các vòng để chỉ báo khi thực sự thay đổi.
    if (trangThaiThietBi != trangThaiDaInTienTrinh) { // So sánh mỗi vòng giúp bắt mọi nơi đổi trạng thái mà không sửa máy trạng thái hiện có.
        inKhoiTienTrinh(); // In khối tiến trình ngay khi phát hiện trạng thái mới.
        trangThaiDaInTienTrinh = trangThaiThietBi; // Ghi lại trạng thái vừa in để không lặp ở vòng kế tiếp.
    } // Kết thúc kiểm tra thay đổi trạng thái không chặn.
    if (nhpTimBat && thoiGianHienTaiMs - thoiDiemNhipTimTruoc >= 5000) { // Dùng millis() và 5000 ms bằng 5 giây để không làm trễ lịch 100/50 Hz.
        thoiDiemNhipTimTruoc = thoiGianHienTaiMs; // Chốt mốc trước khi in để giữ chu kỳ ổn định khi millis() tràn.
        inNhipTim(thoiGianHienTaiMs); // In nhịp tim từ trạng thái và bộ đếm sẵn có.
    } // Kết thúc lịch nhịp tim không dùng delay().

    // 1. Thăm dò khí áp không chặn trên bus 1.
    docMs5611KhongChan(thoiGianHienTaiUs); // Tiến một bước máy trạng thái MS5611 mà không chờ bận.

    // Theo dõi độ trôi áp suất mốc theo điều kiện (b).
    if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc) { // Chỉ cập nhật từ số đo khí áp hợp lệ.
        capNhatApSuatThamChieu(thoiGianHienTaiMs, camBienKhiAp.apSuatPa); // Điều chỉnh mốc theo áp suất ổn định.
    }

    // 2. Lấy mẫu IMU tốc độ cao trên bus 0: lịch 100 Hz = 10.000 us hoặc 50 Hz = 20.000 us.
    static uint32_t lanDocImuCuoiUs = 0; // Lưu mốc lập lịch IMU qua các vòng lặp.
    static uint8_t soLanLoiImuLienTiep = 0; // Đếm chuỗi lỗi đọc IMU liên tiếp.
    if (thoiGianHienTaiUs - lanDocImuCuoiUs >= chuKyLayMauImuUs) { // Chỉ đọc khi đã đủ một chu kì cấu hình.
        if (lanDocImuCuoiUs > 0 && (thoiGianHienTaiUs - lanDocImuCuoiUs > (chuKyLayMauImuUs * 3 / 2))) { // Phát hiện vòng lặp trễ quá 1,5 chu kì.
            boDemHeThong.soLanTreChuKy++; // Ghi nhận một khe lịch bị trễ.
        }
        lanDocImuCuoiUs = thoiGianHienTaiUs; // Chốt mốc của lần lấy mẫu này.

        float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0; // Khởi tạo sáu trục đo trước khi đọc cảm biến.
        bool docImuThanhCong = docMpu6050(ax, ay, az, gx, gy, gz); // Đọc gia tốc và vận tốc góc từ MPU6050.

        if (docImuThanhCong) { // Xử lí mẫu chỉ khi giao tiếp I2C thành công.
            soLanLoiImuLienTiep = 0; // Xóa chuỗi lỗi sau một mẫu tốt.
            boDemHeThong.soMauImuDaDoc++; // Tăng bộ đếm mẫu IMU hợp lệ.
            float doLonGiaToc = sqrtf(ax * ax + ay * ay + az * az); // Tính độ lớn véc-tơ gia tốc.

            // Lưu vào bộ đệm vòng: 1000 mẫu ở 100 Hz tạo lịch sử 10 giây trước sự kiện.
            boDemVong[viTriGhiBoDemVong].thoiGianMs = thoiGianHienTaiMs; // Lưu dấu thời gian của mẫu.
            boDemVong[viTriGhiBoDemVong].ax = ax; // Lưu gia tốc trục X.
            boDemVong[viTriGhiBoDemVong].ay = ay; // Lưu gia tốc trục Y.
            boDemVong[viTriGhiBoDemVong].az = az; // Lưu gia tốc trục Z.
            boDemVong[viTriGhiBoDemVong].gx = gx; // Lưu vận tốc góc trục X.
            boDemVong[viTriGhiBoDemVong].gy = gy; // Lưu vận tốc góc trục Y.
            boDemVong[viTriGhiBoDemVong].gz = gz; // Lưu vận tốc góc trục Z.
            boDemVong[viTriGhiBoDemVong].doLonGiaToc = doLonGiaToc; // Lưu độ lớn gia tốc tổng hợp.
            boDemVong[viTriGhiBoDemVong].apSuatPa = camBienKhiAp.apSuatPa; // Lưu áp suất cùng thời điểm.
            boDemVong[viTriGhiBoDemVong].chenhLechDoCaoM = camBienKhiAp.chenhLechDoCaoM; // Lưu chênh lệch độ cao.
            viTriGhiBoDemVong = (viTriGhiBoDemVong + 1) % SO_LUONG_BO_DEM_VONG; // Tiến vị trí ghi và quay vòng đúng kích thước.
            if (soMauTrongBoDem < SO_LUONG_BO_DEM_VONG) { // Chưa tăng số mẫu quá dung lượng bộ đệm.
                soMauTrongBoDem++; // Ghi nhận thêm một mẫu đang được lưu.
            }

            // Chạy bộ máy phát hiện té ngã.
            const char* nhanSuKien = nullptr; // Nhận nhãn sự kiện do thuật toán tạo ra.
            xuLyPhatHienTeNga(thoiGianHienTaiMs, ax, ay, az, gx, gy, gz, // Truyền thời gian và sáu trục cảm biến.
                                 camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM, nhanSuKien); // Truyền dữ liệu khí áp và nhận nhãn sự kiện.

            // Xuất luồng CSV; bản sửa 3 bổ sung chenhLechDoCaoM.
            if (cheDoCsvTho) { // Chỉ in dòng CSV trong chế độ thô.
                Serial.printf("%u,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%s,%s\n", // Giữ nguyên toàn bộ mã định dạng và thứ tự cột.
                              thoiGianHienTaiMs, ax, ay, az, doLonGiaToc, gx, gy, gz, // Các tham số thời gian, gia tốc và con quay.
                              camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM, // Các tham số áp suất và độ cao.
                              tenTrangThaiThietBi(trangThaiThietBi), // Chuỗi trạng thái thiết bị.
                              nhanSuKien ? nhanSuKien : ""); // Nhãn sự kiện hoặc chuỗi rỗng.
            }

#if CO_BLE // Gửi đo từ xa khi BLE được bật.
            // Gửi dữ liệu BLE thời gian thực khi được yêu cầu, giảm còn khoảng 25 Hz để tiết kiệm băng thông.
            static uint8_t boDemLocTruyen = 0; // Đếm mẫu giữa hai gói BLE.
            uint8_t mucGiamTanSo = (tanSoLayMauImuHz == 50) ? 2 : 4; // Giữ đúng tỉ lệ chia 2 ở 50 Hz và 4 ở 100 Hz.
            if (++boDemLocTruyen >= mucGiamTanSo) { // Chỉ gửi khi đủ số mẫu cần giảm tần.
                boDemLocTruyen = 0; // Đặt lại bộ đếm sau khi gửi.
                guiBleDuLieuCamBien(ax, ay, az, gx, gy, gz, // Gửi sáu trục IMU.
                                    camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM, camBienKhiAp.nhietDoC); // Gửi thêm áp suất, độ cao và nhiệt độ.
            }
#endif // Kết thúc phần truyền dữ liệu cảm biến qua BLE.
        } else { // Xử lí lỗi đọc IMU của mẫu hiện tại.
            soMauBiBo++; // Đếm mẫu bị bỏ do đọc IMU thất bại.
            if (++soLanLoiImuLienTiep >= 10 && trangThaiThietBi != STATE_DEGRADED) { // Suy giảm sau đúng 10 lỗi liên tiếp.
                trangThaiThietBi = STATE_DEGRADED; // Chuyển thiết bị sang trạng thái suy giảm.
                maLoiCuoiCung = "IMU_READ_FAILED"; // Giữ nguyên mã lỗi giao thức.
                camBienMpu.dangHoatDong = false; // Đánh dấu MPU6050 ngoại tuyến.
#if CO_BLE // Báo lỗi cảm biến qua BLE khi được bật.
                guiBleSuKien("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_READ_FAILED", false, false, false); // Giữ nguyên loại, mức, mã lỗi và số liệu.
                guiBleTrangThaiThietBi(); // Đồng bộ trạng thái suy giảm với máy khách.
#endif // Kết thúc phần báo lỗi IMU qua BLE.
            }
        }
    }

    // 3. Kiểm tra trạng thái pin định kì, có đánh giá độ trễ ngưỡng.
    static uint32_t lanKiemTraPinCuoiMs = 0; // Lưu mốc kiểm tra pin gần nhất.
    if (thoiGianHienTaiMs - lanKiemTraPinCuoiMs >= 10000) { // Giữ đúng chu kì kiểm tra pin 10000 ms.
        lanKiemTraPinCuoiMs = thoiGianHienTaiMs; // Chốt mốc kiểm tra hiện tại.
        capNhatTrangThaiPin(thoiGianHienTaiMs); // Đọc và cập nhật trạng thái pin.
    }

    // 4. Cập nhật nút bấm và trạng thái SOS.
    capNhatNutBam(thoiGianHienTaiMs); // Xử lí nút SOS 2000 ms và nút hủy 300 ms theo logic đã định.

    // 5. Cập nhật nhịp còi và LED.
    capNhatCoi(thoiGianHienTaiMs); // Điều khiển mẫu cảnh báo theo trạng thái hiện tại.

    // 5b. Quản lí kết nối lại WiFi không chặn, mỗi 5 giây.
    {
        static uint32_t lanKiemTraWifiCuoiMs = 0; // Lưu mốc thử kết nối lại WiFi.
        static bool wifiDaKetNoi = false; // Nhớ trạng thái kết nối của vòng trước.
        bool daKetNoiWifi = (WiFi.status() == WL_CONNECTED); // Đọc trạng thái WiFi hiện tại.
        if (daKetNoiWifi && !wifiDaKetNoi) { // Chỉ in khi vừa chuyển sang kết nối thành công.
            Serial.printf("[WIFI] Da ket noi! IP: %s (RSSI: %d dBm)\n", // Giữ nguyên %s, %d và thứ tự địa chỉ IP, RSSI.
                          WiFi.localIP().toString().c_str(), WiFi.RSSI()); // Cung cấp địa chỉ IP và cường độ tín hiệu.
        }
        wifiDaKetNoi = daKetNoiWifi; // Lưu trạng thái cho vòng kế tiếp.
        if (!daKetNoiWifi && (thoiGianHienTaiMs - lanKiemTraWifiCuoiMs >= 5000)) { // Giữ đúng chu kì thử lại 5000 ms.
            lanKiemTraWifiCuoiMs = thoiGianHienTaiMs; // Chốt mốc thử kết nối.
            WiFi.reconnect(); // Yêu cầu thư viện WiFi kết nối lại mà không chặn vòng lặp.
        }
    }

#if CO_BLE // Xử lí công việc BLE khi tính năng được bật.
    // 6. Xử lí bất đồng bộ lệnh BLE đang chờ.
    if (choXuLyLen) { // Chỉ điều phối khi callback đã đặt cờ có lệnh.
        choXuLyLen = false; // Xóa cờ trước khi thực hiện để không chạy lặp lại.
        xuLyLenhBle(lenhBleChoXuLy); // Phân tích và thực hiện bản sao nội dung lệnh.
    }

    // Gửi trạng thái thiết bị định kì qua BLE mỗi 5 giây.
    static uint32_t lanGuiTrangThaiBleCuoiMs = 0; // Lưu mốc gửi trạng thái gần nhất.
    if (thoiGianHienTaiMs - lanGuiTrangThaiBleCuoiMs >= 5000) { // Giữ đúng chu kì gửi 5000 ms.
        lanGuiTrangThaiBleCuoiMs = thoiGianHienTaiMs; // Chốt mốc gửi hiện tại.
        guiBleTrangThaiThietBi(); // Thông báo trạng thái mới nhất cho máy khách.
    }
#endif // Kết thúc công việc BLE trong vòng lặp.

    // 7. Hiển thị Serial dễ đọc cho con người, khoảng 4 dòng mỗi giây.
    if (!cheDoCsvTho && (thoiGianHienTaiMs - thoiDiemInDeDocCuoiMs >= 250)) { // Giữ đúng nhịp in 250 ms.
        thoiDiemInDeDocCuoiMs = thoiGianHienTaiMs; // Chốt mốc in hiện tại.
        float giaTocHienTaiX = 0, giaTocHienTaiY = 0, giaTocHienTaiZ = 0, vanTocGocHienTaiX = 0, vanTocGocHienTaiY = 0, vanTocGocHienTaiZ = 0; // Khởi tạo sáu trục hiển thị.
        docMpu6050(giaTocHienTaiX, giaTocHienTaiY, giaTocHienTaiZ, vanTocGocHienTaiX, vanTocGocHienTaiY, vanTocGocHienTaiZ); // Đọc mẫu mới cho màn hình.
        float doLonGiaToc = sqrtf(giaTocHienTaiX * giaTocHienTaiX + giaTocHienTaiY * giaTocHienTaiY + giaTocHienTaiZ * giaTocHienTaiZ); // Tính độ lớn gia tốc hiển thị.

        Serial.printf("[T=%06u ms] Trang thai: %-14s | |a|: %5.2f m/s^2 | Chenh cao: %6.2f m | BLE: %s\n", // Giữ nguyên năm mã định dạng và thứ tự tham số.
                      thoiGianHienTaiMs, // Tham số cho %06u.
                      tenTrangThaiThietBi(trangThaiThietBi), // Tham số cho %-14s.
                      doLonGiaToc, // Tham số cho %5.2f.
                      camBienKhiAp.chenhLechDoCaoM, // Tham số cho %6.2f.
                      ketNoiBle ? (truyenBle ? "DANG_TRUYEN" : "DA_KET_NOI") : "QUANG_BA"); // Tham số trạng thái BLE cho %s.
    }

    // 8. Các lệnh bảng điều khiển Serial tương tác.
    while (Serial.available() > 0) { // Xử lí hết kí tự đang chờ mà không chặn.
        char kyTu = (char)Serial.read(); // Đọc một kí tự lệnh.
        if (kyTu == 'r' || kyTu == 'R') { // Phím r đổi chế độ CSV thô.
            cheDoCsvTho = !cheDoCsvTho; // Đảo trạng thái chế độ xuất.
            if (cheDoCsvTho) { // Khi vừa bật CSV, in lại tiêu đề cột.
                inTieuDeCsv(); // In tiêu đề CSV.
            } else { // Khi tắt CSV, trở lại chế độ hiển thị dễ đọc.
                Serial.println(F("[SERIAL] Da chuyen sang CHE DO DO TU XA DE DOC (~4 Hz).")); // Thông báo chế độ bằng tiếng Việt không dấu.
            }
        } else if (kyTu == 't' || kyTu == 'T') { // Phím t in cấu hình ngưỡng.
            inBangNguong(); // Hiển thị cấu hình phát hiện té ngã.
        } else if (kyTu == 'c' || kyTu == 'C') { // Phím c chạy lại hiệu chuẩn.
            tuKiemTraVaHieuChuan(); // Chạy tự kiểm tra và hiệu chuẩn phần cứng.
        } else if (kyTu == 's' || kyTu == 'S') { // Phím s in trạng thái hệ thống.
            inTrangThaiHeThong(); // Hiển thị trạng thái và bộ đếm hiệu năng.
        } else if (kyTu == 'h' || kyTu == 'H') { // Phím h in trợ giúp.
            Serial.println(F("\n--- TRO GIUP CLI SERIAL ---")); // In tiêu đề trợ giúp không dấu.
            Serial.println(F("  r : Bat/tat che do CSV tho")); // Mô tả lệnh r.
            Serial.println(F("  t : In cau hinh va nguong phat hien te nga")); // Mô tả lệnh t.
            Serial.println(F("  c : Chay lai hieu chuan va tu kiem tra")); // Mô tả lệnh c.
            Serial.println(F("  s : In trang thai he thong va bo dem hieu nang")); // Mô tả lệnh s.
            Serial.println(F("  h : In tro giup nay")); // Mô tả lệnh h.
            Serial.println(F("---------------------------\n")); // In đường kết thúc trợ giúp.
        }
        else if (kyTu == 'm' || kyTu == 'M') { // Phím m in menu đầy đủ gồm cả ba lệnh mới.
            inMenuSerial(); // Hiển thị danh sách và giải thích của đủ tám phím Serial.
        } else if (kyTu == 'p' || kyTu == 'P') { // Phím p yêu cầu xem tiến trình hiện tại ngay lập tức.
            inKhoiTienTrinh(); // In sáu bước mà không thay đổi trạng thái máy hay thuật toán.
        } else if (kyTu == 'd' || kyTu == 'D') { // Phím d điều khiển riêng bản tin nhịp tim Serial.
            nhpTimBat = !nhpTimBat; // Đảo cờ nhịp tim mà không tác động BLE, cảm biến hoặc lấy mẫu.
            Serial.println(nhpTimBat ? F("[NHIP TIM] Da BAT") : F("[NHIP TIM] Da TAT")); // Xác nhận rõ trạng thái mới cho học sinh.
        } // Kết thúc xử lí ba phím Serial mới.
    }
}
