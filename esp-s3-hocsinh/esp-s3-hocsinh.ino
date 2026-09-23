/**
 * ============================================================================
 * NCKH27PA — ESP32-S3 Super Mini Fall Detection Official Firmware
 * Target Hardware: ESP32-S3 Super Mini (esp32:esp32:esp32s3)
 * Sensors: MPU6050 (GY-521, I2C0) + MS5611 (GY-63, I2C1)
 * Protocol: BLE GATT (FALLSAFE-xxxx) + Serial CLI / Telemetry
 * Plan Reference: esp/esp32-plan.md (§5, §6, §7, §8) & android/android-plan.md (§6)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_random.h>

#define ENABLE_BLE 1

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

// ============================================================================
// 0. USER TYPES DECLARED FIRST (Arduino auto-prototype ordering requirement)
// ============================================================================
// The Arduino builder auto-generates prototypes for every function and inserts them
// just before the first user declaration. A type used in a function signature must
// therefore be declared BEFORE those prototypes (i.e. near the very top of the sketch),
// otherwise the generated prototype fails to compile.
// Keep this enum here; `tenTrangThaiThietBi()` and all users live further below.
// enum la danh sach cac trang thai ma thiet bi co the dang o.
// Moi luc thiet bi chi o DUNG MOT trang thai (giong nhu mot chiec den giao thong
// chi bat dung mot mau: do / vang / xanh). Thiết bị chuyển qua lại giữa các trạng thái
// theo tín hiệu cảm biến và nút bấm.
enum DeviceState {
    TRANG_THAI_TU_KIEM_TRA,        // Vừa bật nguồn, đang tự kiểm tra phần cứng
    TRANG_THAI_DANG_HIEU_CHUAN,    // Đang đo để "làm quen" với trạng thái đứng yên ban đầu
    TRANG_THAI_BINH_THUONG,        // Hoạt động bình thường, đang theo dõi chuyển động
    TRANG_THAI_NGHI_NGA,           // Vừa thấy dấu hiệu nghi ngờ có té ngã (va chạm mạnh)
    TRANG_THAI_DANG_XAC_MINH,      // Đang kiểm tra thêm để xác nhận có phải té ngã thật không
    TRANG_THAI_BAO_DONG,           // Đã xác nhận té ngã, đang kêu còi và gửi cảnh báo
    TRANG_THAI_SUY_GIAM            // Có cảm biến bị lỗi, thiết bị vẫn chạy nhưng kém tin cậy
};

// ============================================================================
// 1. PINMAP & HARDWARE DEFINITIONS
// ============================================================================
// Từ khóa `static` ở các biến toàn cục: nghĩa là biến này chỉ "sống" bên trong file
// này và GIỮ GIÁ TRỊ suốt thời gian chạy, không bị tạo lại mỗi lần gọi hàm.
// --- Confirmed hardware pins (from node_config.h on ESP32-S3) ---
static const int PIN_I2C0_SDA = 7;     // MPU6050 SDA (Bus 0) - CONFIRMED
static const int PIN_I2C0_SCL = 6;     // MPU6050 SCL (Bus 0) - CONFIRMED
static const int PIN_I2C1_SDA = 3;     // MS5611 SDA (Bus 1)  - CONFIRMED
static const int PIN_I2C1_SCL = 2;     // MS5611 SCL (Bus 1)  - CONFIRMED

// --- Peripherals without official schematic: marked TODO(HW) ---
static const int PIN_BUTTON_SOS    = 4;   // TODO(HW): Active LOW, internal pull-up assumed
static const int PIN_BUTTON_CANCEL = 5;   // TODO(HW): Active LOW, internal pull-up assumed
static const int PIN_BUZZER        = 1;   // TODO(HW): Passive/Active PWM buzzer
static const int PIN_LED_STATUS    = 8;   // TODO(HW): Status LED (ESP32-S3 Super Mini on-board LED is often GPIO 8)
static const int PIN_LED_BAT_1     = 9;   // TODO(HW): Battery bar LED 1
static const int PIN_LED_BAT_2     = 10;  // TODO(HW): Battery bar LED 2
static const int PIN_LED_BAT_3     = 11;  // TODO(HW): Battery bar LED 3
static const int PIN_BAT_ADC       = 12;  // TODO(HW): Battery voltage divider analog input

// Hardware capability flags (unfitted hardware -> UNAVAILABLE)
#define HW_HAS_FUEL_GAUGE  0   // MAX17048 not fitted yet (FIX 6: report -1, BATTERY_READ_FAILED)
#define HW_HAS_GNSS        0   // GNSS not fitted yet
#define HW_HAS_CELLULAR    0   // 4G module not fitted yet

// ============================================================================
// 2. PROTOCOL & UUID SPECIFICATIONS (Plan §8)
// ============================================================================
#define PROTOCOL_VERSION 1
#define FIRMWARE_VERSION "esp-s3 1.0.0"

#define UUID_SERVICE         "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001"
#define UUID_CHAR_STREAM     "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001" // Notify
#define UUID_CHAR_EVENT      "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001" // Indicate
#define UUID_CHAR_STATUS     "7d2a0004-6f45-4c2b-9a1e-38a8f5c10001" // Read/Notify
#define UUID_CHAR_COMMAND    "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001" // Write
#define UUID_CHAR_ACK        "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001" // Notify

// Global device identification and sequence counter (§8.6: shared for sensor/event)
static char s_bleDeviceName[24] = "FALLSAFE-0000";
static uint32_t s_globalSequenceNumber = 0;

// Timestamp synchronization state (§8.6)
static bool s_timeSynced = false;
static uint64_t s_deviceEpochTimeMs = 0;
static uint32_t s_epochSyncLocalMs = 0;

static uint64_t layThoiGianChayMs() {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static uint64_t layThoiGianThucMs() {
    if (s_timeSynced) {
        return s_deviceEpochTimeMs + (uint64_t)(millis() - s_epochSyncLocalMs);
    }
    return layThoiGianChayMs();
}

// ============================================================================
// 3. FALL DETECTION CONFIGURATION (Parity with Android FallDetectionConfig.DEFAULT)
// ============================================================================
// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Ở đây cái hộp chứa TẤT CẢ các ngưỡng dùng để quyết định "có té ngã hay không".
// LƯU Ý QUAN TRỌNG: các con số dưới đây là GIÁ TRỊ THỬ NGHIỆM, chưa phải giá trị cuối cùng.
// Cần ĐO THỰC TẾ trên người dùng rồi chỉnh lại cho phù hợp.
struct FallDetectionProfile {
    // Parity with Android FallDetectionProfiles.kt
    // Ngưỡng gia tốc va chạm: vượt quá giá trị này thì nghi là vừa bị đập mạnh.
    // Đơn vị m/s^2. THỬ NGHIỆM - cần đo thực tế.
    float impactAccelerationMs2;            // 25.0 m/s^2 [Source: Android FallDetectionConfig.DEFAULT]
    // Mức gia tốc khi nằm/đứng yên = đúng bằng trọng lực Trái Đất (~1g).
    float stillnessTargetAccelerationMs2;   // 9.81 m/s^2 [Source: Android FallDetectionConfig.DEFAULT]
    // Cho phép lệch bao nhiêu so với 9.81 mà vẫn coi là "nằm yên".
    float stillnessToleranceMs2;            // 1.0 m/s^2  [Source: Android FallDetectionConfig.DEFAULT]
    // Sau va chạm, theo dõi tiếp trong bao lâu (ms) để xác nhận té ngã.
    float postImpactWindowMs;               // 3000 ms    [Source: Android FallDetectionConfig.DEFAULT]
    // Phải "gần như bất động" liên tục tối thiểu bao lâu (ms).
    uint32_t postImpactStillnessDurationMs; // 1000 ms    [Source: Android FallDetectionConfig.DEFAULT]
    // Cần ít nhất bao nhiêu mẫu liên tiếp "bất động".
    uint32_t minimumStillnessSamples;       // 6 samples  [Source: Android FallDetectionConfig.DEFAULT]
    // Khoảng cách tối đa giữa 2 mẫu liên tiếp (ms), quá hạn thì hủy cửa sổ theo dõi.
    uint32_t maximumSampleGapMs;            // 250 ms     [Source: Android FallDetectionConfig.DEFAULT]
    // Ngưỡng tăng áp suất (Pa). HIỆN KHÔNG DÙNG trong quyết định (xem cờ bên dưới).
    float phonePressureMinimumRisePa;       // 12.0 Pa    [Source: Android (disabled in decision)]
    // Bật/tắt bộ lọc dùng áp suất để phát hiện ngã. Đang TẮT.
    bool usePressureFallFilter;             // false      [Source: Android (pressure flag disabled)]

    // Các ngưỡng phụ (chỉ dùng để ghi chú nguyên nhân kích hoạt, không dùng quyết định).
    float freeFallThresholdMs2;             // 3.0 m/s^2  [Source: Suy luan he thong - logging only]
    float highAngularSpeedDps;              // 200.0 dps  [Source: Suy luan he thong - logging only]
    float postureChangeThresholdDeg;        // 45.0 deg   [Source: Suy luan he thong - logging only]
};

static const FallDetectionProfile PROFILE_DEFAULT = {
    .impactAccelerationMs2 = 25.0f,
    .stillnessTargetAccelerationMs2 = 9.81f,
    .stillnessToleranceMs2 = 1.0f,
    .postImpactWindowMs = 3000,
    .postImpactStillnessDurationMs = 1000,
    .minimumStillnessSamples = 6,
    .maximumSampleGapMs = 250,
    .phonePressureMinimumRisePa = 12.0f,
    .usePressureFallFilter = false,
    .freeFallThresholdMs2 = 3.0f,
    .highAngularSpeedDps = 200.0f,
    .postureChangeThresholdDeg = 45.0f
};

// ============================================================================
// 4. DATA TYPES & SYSTEM STATES
// ============================================================================
// NOTE: `enum DeviceState` is declared at the TOP of this sketch (section 0) on purpose:
// the Arduino builder inserts generated function prototypes just before the first user
// declarations, so any user type used in a function signature must be visible before
// those prototypes — otherwise GCC reports "''DeviceState'' was not declared in this scope".

// Hàm này chỉ đơn giản đổi một trạng thái (con số trong enum) thành chuỗi chữ
// để in ra màn hình cho con người đọc được.
static const char* tenTrangThaiThietBi(DeviceState s) {
    switch(s) {
        case TRANG_THAI_TU_KIEM_TRA:      return "TRANG_THAI_TU_KIEM_TRA";
        case TRANG_THAI_DANG_HIEU_CHUAN:  return "TRANG_THAI_DANG_HIEU_CHUAN";
        case TRANG_THAI_BINH_THUONG:      return "TRANG_THAI_BINH_THUONG";
        case TRANG_THAI_NGHI_NGA:         return "TRANG_THAI_NGHI_NGA";
        case TRANG_THAI_DANG_XAC_MINH:    return "TRANG_THAI_DANG_XAC_MINH";
        case TRANG_THAI_BAO_DONG:         return "TRANG_THAI_BAO_DONG";
        case TRANG_THAI_SUY_GIAM:         return "TRANG_THAI_SUY_GIAM";
        default:                          return "KHONG_BIET";
    }
}

// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Mỗi SensorSample là MỘT lần đo đầy đủ tại một thời điểm: gia tốc + vận tốc góc + áp suất.
struct SensorSample {
    uint32_t timestampMs; // Thời điểm đo (milli giây kể từ khi bật máy)
    float ax, ay, az;     // Gia tốc theo 3 trục X, Y, Z - đơn vị m/s^2
    float gx, gy, gz;     // Vận tốc góc theo 3 trục X, Y, Z - đơn vị độ/giây (dps)
    float magnitude;      // "Độ lớn" tổng hợp của vectơ gia tốc - đơn vị m/s^2
    float pressurePa;     // Áp suất không khí - đơn vị Pascal (Pa)
    float altitudeDeltaM; // Chênh lệch độ cao so với mốc - đơn vị mét (m)
};

// ============================================================================
// CIRCULAR RING BUFFER (Plan §5.2, FIX 7) — BỘ ĐỆM VÒNG
// ============================================================================
// boDemVong là một "sổ ghi vòng": khi ghi đầy chỗ thì quay lại ghi đè lên chỗ cũ nhất.
// Giống như một cuốn sổ có 1000 trang, viết hết trang 1000 thì quay về trang 1 viết đè.
// Nhờ vậy máy luôn giữ được 1000 mẫu MỚI NHẤT (đo ở 100 Hz nghĩa là ~10 giây gần đây),
// đủ để "quay ngược" xem chuyện gì xảy ra NGAY TRƯỚC khi có té ngã.
//   viTriGhi       : trang sổ đang viết vào.
//   soMauTrongBoDem: hiện có bao nhiêu mẫu trong sổ (chưa đầy thì tăng dần).
//   soMauBiBo      : số mẫu bị vứt đi vì bận quá không kịp xử lý.
#define RING_BUFFER_SIZE 1000
static SensorSample boDemVong[RING_BUFFER_SIZE];
static uint16_t viTriGhi = 0;
static uint16_t soMauTrongBoDem = 0;
static uint32_t soMauBiBo = 0;

// Dynamic sample rate control (FIX 3: SET_SAMPLE_RATE 100 Hz and 50 Hz supported)
static uint32_t tanSoLayMauHz = 100;
static uint32_t chuKyLayMauUs = 10000; // 100 Hz = 10,000 us nominal slot

// Reference pressure baseline (Plan §6.2, FIX 3)
static float apSuatThamChieuPa = 101325.0f;

// Last error code vocabulary (Plan §8.3, FIX 4, FIX 6):
// "IMU_READ_FAILED", "BAROMETER_READ_FAILED", "BATTERY_READ_FAILED",
// "TIME_NOT_SYNCED", "BUFFER_OVERFLOW", or nullptr if no error.
#if HW_HAS_FUEL_GAUGE
static const char* maLoiCuoiCung = "TIME_NOT_SYNCED";
#else
static const char* maLoiCuoiCung = "BATTERY_READ_FAILED";
#endif

// Battery state (Plan §3.4, §8.2, FIX 6) — declared HERE, before the BLE packet builders
// that format them (a global must be declared before the function that reads it):
//   HW_HAS_FUEL_GAUGE == 0  ->  batteryPercent = -1 (sentinel "no hardware", never a fake 100%)
//                               batteryVoltageMv = null, lastErrorCode = BATTERY_READ_FAILED
static int phanTramPin = -1;
static bool dangSacPin = false;
static bool daCanhBaoPinYeu = false;
static bool daCanhBaoPinKiet = false;

// ============================================================================
// 5. MPU6050 REGISTER-LEVEL DRIVER (Bus 0: Wire, GPIO 7 / GPIO 6)
// ============================================================================
#define MPU6050_ADDR_A       0x68
#define MPU6050_ADDR_B       0x69
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_WHO_AM_I     0x75

// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Cái hộp này chứa toàn bộ trạng thái của cảm biến gia tốc + con quay MPU6050.
struct Mpu6050Driver {
    uint8_t diaChi;         // Địa chỉ I2C của chip (0x68 hoặc 0x69)
    bool dangHoatDong;      // Có đang "nói chuyện" được với chip không
    bool biBaoHoa;          // Gia tốc vượt quá thang đo nên số bị "bão hòa" (không tin được)
    float saiSoVanTocGocX;  // Sai số lệch của con quay trục X (trừ đi khi đọc)
    float saiSoVanTocGocY;  // Sai số lệch của con quay trục Y
    float saiSoVanTocGocZ;  // Sai số lệch của con quay trục Z
    float trongLucGocX;     // Hướng trọng lực "gốc" trục X (đo lúc đứng yên)
    float trongLucGocY;     // Hướng trọng lực "gốc" trục Y
    float trongLucGocZ;     // Hướng trọng lực "gốc" trục Z (thường ~9.81)
};

static Mpu6050Driver camBienMpu = {
    .diaChi = MPU6050_ADDR_A,
    .dangHoatDong = false,
    .biBaoHoa = false,
    .saiSoVanTocGocX = 0, .saiSoVanTocGocY = 0, .saiSoVanTocGocZ = 0,
    .trongLucGocX = 0, .trongLucGocY = 0, .trongLucGocZ = 9.81f
};

// Hai hàm nhỏ bên dưới giúp "nói chuyện" với cảm biến qua I2C.
// Tham số `TwoWire &wire`: dấu & nghĩa là "truyền thẳng địa chỉ của biến gốc,
// không copy, nên không tốn bộ nhớ". Ta có thể dùng Wire (bus 0) hoặc Wire1 (bus 1)
// cho cùng một hàm mà không cần viết lại.
static bool ghiI2cMotByte(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t data) {
    wire.beginTransmission(devAddr);
    wire.write(regAddr);
    wire.write(data);
    return (wire.endTransmission() == 0);
}

static bool docI2cNhieuByte(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t *buffer, size_t length) {
    wire.beginTransmission(devAddr);
    wire.write(regAddr);
    if (wire.endTransmission(false) != 0) return false;
    size_t count = wire.requestFrom(devAddr, (uint8_t)length);
    if (count != length) return false;
    for (size_t i = 0; i < length; i++) {
        buffer[i] = wire.read();
    }
    return true;
}

static bool khoiTaoMpu6050() {
    uint8_t who = 0;
    camBienMpu.diaChi = MPU6050_ADDR_A;
    if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_WHO_AM_I, &who, 1) || who != 0x68) {
        camBienMpu.diaChi = MPU6050_ADDR_B;
        if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_WHO_AM_I, &who, 1) || who != 0x68) {
            camBienMpu.dangHoatDong = false;
            return false;
        }
    }

    // 1. Wake up device, clock source PLL with X gyro
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_PWR_MGMT_1, 0x01);
    delay(10);
    // 2. SMPLRT_DIV = 9 -> 1 kHz / (1 + 9) = 100 Hz internal sample rate (FIX 8)
    // [Source: esp/node/firmware/esp_node/node_config.h kSmplrtDiv = 9]
    // Matches the 100 Hz main loop scheduling and validated node configuration.
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_SMPLRT_DIV, 0x09);
    // 3. CONFIG: DLPF_CFG = 1 (accel BW 184Hz, delay 2.0ms, gyro BW 188Hz) [Source: node_config.h kDlpfCfg = 1]
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_CONFIG, 0x01);
    // 4. GYRO_CONFIG: FS_SEL = 3 (±2000 dps) -> 0x18 (avoids saturation during violent rotational falls)
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_GYRO_CONFIG, 0x18);
    // 5. ACCEL_CONFIG: AFS_SEL = 3 (±16 g) -> 0x18 (CRITICAL: avoids saturation at 25 m/s^2 impact)
    ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_ACCEL_CONFIG, 0x18);

    camBienMpu.dangHoatDong = true;
    return true;
}

static bool docMpu6050(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
    if (!camBienMpu.dangHoatDong) return false;
    uint8_t raw[14];
    if (!docI2cNhieuByte(Wire, camBienMpu.diaChi, MPU6050_ACCEL_XOUT_H, raw, 14)) {
        return false;
    }

    int16_t rawAx = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t rawAy = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t rawAz = (int16_t)((raw[4] << 8) | raw[5]);
    // raw[6..7] is temperature
    int16_t rawGx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t rawGy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t rawGz = (int16_t)((raw[12] << 8) | raw[13]);

    // Check ±16g ADC saturation limit (raw reaches ±32767 near ±16g)
    if (abs(rawAx) >= 32700 || abs(rawAy) >= 32700 || abs(rawAz) >= 32700) {
        camBienMpu.biBaoHoa = true;
    } else {
        camBienMpu.biBaoHoa = false;
    }

    // Scale factors:
    // Accel: ±16g range -> 2048 LSB/g. 1g = 9.80665 m/s^2.
    const float accelScale = 9.80665f / 2048.0f;
    ax = (float)rawAx * accelScale;
    ay = (float)rawAy * accelScale;
    az = (float)rawAz * accelScale;

    // Gyro: ±2000 dps range -> 16.4 LSB/(deg/s)
    const float gyroScale = 1.0f / 16.4f;
    gx = ((float)rawGx * gyroScale) - camBienMpu.saiSoVanTocGocX;
    gy = ((float)rawGy * gyroScale) - camBienMpu.saiSoVanTocGocY;
    gz = ((float)rawGz * gyroScale) - camBienMpu.saiSoVanTocGocZ;

    return true;
}

// ============================================================================
// 6. MS5611 REGISTER-LEVEL NON-BLOCKING DRIVER (Bus 1: Wire1, GPIO 3 / GPIO 2)
// ============================================================================
#define MS5611_ADDR_A       0x77
#define MS5611_ADDR_B       0x76
#define MS5611_CMD_RESET    0x1E
#define MS5611_CMD_PROM_RD  0xA0
#define MS5611_CMD_CONV_D1  0x48 // OSR 4096 pressure
#define MS5611_CMD_CONV_D2  0x58 // OSR 4096 temperature
#define MS5611_CMD_ADC_RD   0x00

enum Ms5611State {
    MS_IDLE,
    MS_CONV_D1_WAIT,
    MS_CONV_D2_WAIT
};

// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Ở đây mỗi biến mô tả một phần trạng thái của cảm biến khí áp MS5611 (GY-63).
struct Ms5611Driver {
    uint8_t diaChi;         // Địa chỉ I2C của chip (0x77 hoặc 0x76)
    bool dangHoatDong;      // Có đang "nói chuyện" được với chip không
    bool crcHopLe;          // Mã kiểm lỗi (CRC) đọc ra có hợp lệ không
    bool coLoiDoc;          // Lần đọc gần nhất có bị lỗi không
    uint16_t heSoHieuChuan[8]; // 8 hệ số hiệu chuẩn được nạp sẵn trong chip từ nhà máy
    Ms5611State giaiDoanDoc;   // Máy đo đang ở bước nào của chu trình đọc (xem enum Ms5611State)
    uint32_t thoiDiemBatDauChuyenDoiUs; // Thời điểm (micro giây) bắt đầu lệnh đo
    uint32_t giaTriThoD1;   // Giá trị thô của phép đo áp suất (chưa quy đổi)
    uint32_t giaTriThoD2;   // Giá trị thô của phép đo nhiệt độ (chưa quy đổi)
    float apSuatPa;         // Áp suất đã quy đổi, đơn vị Pascal (Pa)
    float nhietDoC;         // Nhiệt độ đã quy đổi, đơn vị độ C
    float chenhLechDoCaoM;  // Chênh lệch độ cao so với mốc tham chiếu, đơn vị mét (m)
};

static Ms5611Driver camBienKhiAp = {
    .diaChi = MS5611_ADDR_A,
    .dangHoatDong = false,
    .crcHopLe = false,
    .coLoiDoc = false,
    .heSoHieuChuan = {0},
    .giaiDoanDoc = MS_IDLE,
    .thoiDiemBatDauChuyenDoiUs = 0,
    .giaTriThoD1 = 0,
    .giaTriThoD2 = 0,
    .apSuatPa = 101325.0f,
    .nhietDoC = 25.0f,
    .chenhLechDoCaoM = 0.0f
};

// AN520 CRC4 calculation for MS5611
static bool kiemTraCrc4Ms5611(uint16_t prom[]) {
    uint32_t n_rem = 0;
    uint16_t promBackup[8];
    for (int i = 0; i < 8; i++) promBackup[i] = prom[i];
    uint16_t crcRead = promBackup[7] & 0x000F;
    promBackup[7] = promBackup[7] & 0xFF00;

    for (int cnt = 0; cnt < 16; cnt++) {
        if (cnt % 2 == 1) {
            n_rem ^= (uint16_t)(promBackup[cnt >> 1] & 0x00FF);
        } else {
            n_rem ^= (uint16_t)(promBackup[cnt >> 1] >> 8);
        }
        for (uint8_t n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem = (n_rem << 1);
            }
        }
    }
    n_rem = (0x000F & (n_rem >> 12));
    return (n_rem == crcRead);
}

static bool khoiTaoMs5611() {
    camBienKhiAp.diaChi = MS5611_ADDR_A;
    Wire1.beginTransmission(camBienKhiAp.diaChi);
    if (Wire1.endTransmission() != 0) {
        camBienKhiAp.diaChi = MS5611_ADDR_B;
        Wire1.beginTransmission(camBienKhiAp.diaChi);
        if (Wire1.endTransmission() != 0) {
            camBienKhiAp.dangHoatDong = false;
            return false;
        }
    }

    // Reset command
    Wire1.beginTransmission(camBienKhiAp.diaChi);
    Wire1.write(MS5611_CMD_RESET);
    Wire1.endTransmission();
    delay(10);

    // Read 8 PROM words
    for (uint8_t i = 0; i < 8; i++) {
        Wire1.beginTransmission(camBienKhiAp.diaChi);
        Wire1.write(MS5611_CMD_PROM_RD + (i * 2));
        if (Wire1.endTransmission(false) != 0) {
            camBienKhiAp.dangHoatDong = false;
            return false;
        }
        if (Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)2) != 2) {
            camBienKhiAp.dangHoatDong = false;
            return false;
        }
        camBienKhiAp.heSoHieuChuan[i] = (Wire1.read() << 8) | Wire1.read();
    }

    camBienKhiAp.crcHopLe = kiemTraCrc4Ms5611(camBienKhiAp.heSoHieuChuan);
    camBienKhiAp.dangHoatDong = true;
    camBienKhiAp.coLoiDoc = false;
    camBienKhiAp.giaiDoanDoc = MS_IDLE;
    return true;
}

// ÁP SUẤT & ĐỘ CAO: lên cao thì áp suất giảm, xuống thấp thì áp suất tăng.
// GY63 (MS5611) đo áp suất không khí, từ đó ước lượng chênh lệch độ cao.
// LƯU Ý: áp suất MỘT MÌNH không đủ xác nhận té ngã, vì thời tiết và gió cũng làm
// áp suất thay đổi. Vì vậy áp suất chỉ là cảm biến PHỤ (xem mục "kết hợp cảm biến").
static void tinhApSuatVaDoCao() {
    int64_t dt = (int64_t)camBienKhiAp.giaTriThoD2 - ((int64_t)camBienKhiAp.heSoHieuChuan[5] << 8);
    int64_t temp = 2000 + ((dt * (int64_t)camBienKhiAp.heSoHieuChuan[6]) >> 23);

    int64_t off = ((int64_t)camBienKhiAp.heSoHieuChuan[2] << 16) + (((int64_t)camBienKhiAp.heSoHieuChuan[4] * dt) >> 7);
    int64_t sens = ((int64_t)camBienKhiAp.heSoHieuChuan[1] << 15) + (((int64_t)camBienKhiAp.heSoHieuChuan[3] * dt) >> 8);

    // Second order temperature compensation
    if (temp < 2000) {
        int64_t t2 = (dt * dt) >> 31;
        int64_t off2 = 5 * ((temp - 2000) * (temp - 2000)) >> 1;
        int64_t sens2 = 5 * ((temp - 2000) * (temp - 2000)) >> 2;
        if (temp < -1500) {
            off2 += 7 * ((temp + 1500) * (temp + 1500));
            sens2 += 11 * ((temp + 1500) * (temp + 1500)) >> 1;
        }
        temp -= t2;
        off -= off2;
        sens -= sens2;
    }

    int64_t p = ((((int64_t)camBienKhiAp.giaTriThoD1 * sens) >> 21) - off) >> 15;
    camBienKhiAp.apSuatPa = (float)p; // Output is in Pascals (100000 = 1000.00 mbar)
    camBienKhiAp.nhietDoC = (float)temp / 100.0f;

    // CÔNG THỨC KHÍ ÁP CAO ĐỘ: dưới đây là CÔNG THỨC KHÍ ÁP CHUẨN QUỐC TẾ.
    // Các con số 44330.77 và 0.190263 (ở đây viết là 1/5.255) là hằng số của khí quyển
    // Trái Đất; học sinh KHÔNG cần tự suy ra, chỉ cần biết "áp suất càng thấp thì càng cao".
    //   chenhLechDoCaoM = 44330.77 * (1 - (p_now / apSuatThamChieuPa)^0.190263)
    // Kết quả DƯƠNG khi đang leo lên cao, ÂM khi đang đi xuống thấp.
    if (camBienKhiAp.apSuatPa > 10000.0f && apSuatThamChieuPa > 10000.0f) {
        camBienKhiAp.chenhLechDoCaoM = 44330.0f * (1.0f - powf(camBienKhiAp.apSuatPa / apSuatThamChieuPa, 1.0f / 5.255f));
    }
}

static void docMs5611KhongChan(uint32_t nowUs) {
    if (!camBienKhiAp.dangHoatDong) return;

    switch (camBienKhiAp.giaiDoanDoc) {
        case MS_IDLE:
            Wire1.beginTransmission(camBienKhiAp.diaChi);
            Wire1.write(MS5611_CMD_CONV_D1); // Start D1 conversion (pressure OSR 4096)
            if (Wire1.endTransmission() == 0) {
                camBienKhiAp.thoiDiemBatDauChuyenDoiUs = nowUs;
                camBienKhiAp.giaiDoanDoc = MS_CONV_D1_WAIT;
            } else {
                camBienKhiAp.coLoiDoc = true;
                maLoiCuoiCung = "BAROMETER_READ_FAILED";
            }
            break;

        case MS_CONV_D1_WAIT:
            // OSR 4096 requires max 9.04 ms
            if (nowUs - camBienKhiAp.thoiDiemBatDauChuyenDoiUs >= 9500) {
                Wire1.beginTransmission(camBienKhiAp.diaChi);
                Wire1.write(MS5611_CMD_ADC_RD);
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)3) == 3) {
                    camBienKhiAp.giaTriThoD1 = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read();
                    // Start D2 conversion (temperature)
                    Wire1.beginTransmission(camBienKhiAp.diaChi);
                    Wire1.write(MS5611_CMD_CONV_D2);
                    if (Wire1.endTransmission() == 0) {
                        camBienKhiAp.thoiDiemBatDauChuyenDoiUs = nowUs;
                        camBienKhiAp.giaiDoanDoc = MS_CONV_D2_WAIT;
                    } else {
                        camBienKhiAp.coLoiDoc = true;
                        maLoiCuoiCung = "BAROMETER_READ_FAILED";
                        camBienKhiAp.giaiDoanDoc = MS_IDLE;
                    }
                } else {
                    camBienKhiAp.coLoiDoc = true;
                    maLoiCuoiCung = "BAROMETER_READ_FAILED";
                    camBienKhiAp.giaiDoanDoc = MS_IDLE;
                }
            }
            break;

        case MS_CONV_D2_WAIT:
            if (nowUs - camBienKhiAp.thoiDiemBatDauChuyenDoiUs >= 9500) {
                Wire1.beginTransmission(camBienKhiAp.diaChi);
                Wire1.write(MS5611_CMD_ADC_RD);
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(camBienKhiAp.diaChi, (uint8_t)3) == 3) {
                    camBienKhiAp.giaTriThoD2 = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read();
                    tinhApSuatVaDoCao();
                    camBienKhiAp.coLoiDoc = false;
                } else {
                    camBienKhiAp.coLoiDoc = true;
                    maLoiCuoiCung = "BAROMETER_READ_FAILED";
                }
                camBienKhiAp.giaiDoanDoc = MS_IDLE; // Ready for next cycle
            }
            break;
    }
}

// ============================================================================
// 7. SYSTEM STATUS, COUNTERS & STATE MACHINE
// ============================================================================
// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Cái hộp này chỉ chứa các "con số đếm" để theo dõi máy chạy khỏe không.
struct BoDemHeThong {
    uint32_t soMauImuDaDoc;      // Đã đọc bao nhiêu mẫu gia tốc
    uint32_t soMauKhiApDaDoc;    // Đã đọc bao nhiêu mẫu khí áp
    uint32_t soLanTreChuKy;      // Số lần vòng lặp bị trễ so với nhịp 100 Hz
    uint32_t soLanXacNhanNga;    // Số lần đã XÁC NHẬN có té ngã thật
    uint32_t soLanBamSos;        // Số lần nút SOS được bấm
    uint32_t soGoiBleBiBo;       // Số gói tin BLE bị vứt vì quá tải
};

static BoDemHeThong boDem = {0};
static DeviceState trangThaiThietBi = TRANG_THAI_TU_KIEM_TRA;
static DeviceState trangThaiTruocDo = TRANG_THAI_TU_KIEM_TRA;

// VÌ SAO PHẢI KẾT HỢP NHIỀU CẢM BIẾN? Mỗi cảm biến đều có "điểm mù":
// gia tốc kế không phân biệt được "ngồi phịch xuống ghế" với "ngã"; con quay chỉ biết
// xoay mà không biết rơi; khí áp thì bị thời tiết làm nhiễu. Ghép nhiều cảm biến lại
// giúp giảm báo động SAI (tức là không báo nhầm khi người dùng chỉ ngồi xuống nhanh).
//
// struct giống như một cái hộp gồm nhiều biến có liên quan, để mang cả nhóm đi cùng nhau.
// Cái hộp này lưu các "mốc" cần nhớ trong lúc đang nghi ngờ có té ngã.
struct FallVerificationState {
    uint32_t thoiDiemVaChamMs;        // Lúc phát hiện cú va chạm mạnh (ms)
    float dinhGiaTocVaCham;           // Gia tốc lớn nhất ghi nhận lúc va chạm (m/s^2)
    float giaTocNhoNhatTruocVaCham;   // Gia tốc nhỏ nhất ngay TRƯỚC va chạm (để phát hiện rơi tự do)
    uint32_t thoiDiemBatDauDungYenMs; // Lúc bắt đầu thấy "gần như bất động" sau va chạm
    uint32_t soMauDungYen;            // Đếm bao nhiêu mẫu liên tiếp "bất động"
    uint32_t thoiDiemMauTruocMs;      // Thời điểm của mẫu ngay trước đó (để phát hiện mất mẫu)
    bool dangTrongCuaSoXacMinh;       // Có đang trong "cửa sổ theo dõi sau va chạm" không
    float trongLucTruocVaCham[3];     // Hướng trọng lực 3 trục ngay trước va chạm
    float trongLucSauVaCham[3];       // Hướng trọng lực 3 trục sau va chạm (so sánh xem có lật không)
    float doCaoGoc;                   // Độ cao lúc bắt đầu theo dõi (m)
    char lyDoKichHoat[96];            // Chuỗi ghi chú "vì sao máy nghi ngờ té ngã"
};

static FallVerificationState trangThaiPhatHienNga = {0};

// Alerting & buzzer timer
static uint32_t thoiDiemBatDauBaoDongMs = 0;
static uint32_t s_buzzerPatternDeadlineMs = 0;
static uint16_t s_buzzerPatternId = 0;

static bool s_bleConnected = false;
static bool s_bleStreaming = false;

// ============================================================================
// 8. BLE GATT SERVER IMPLEMENTATION (Plan §8)
// ============================================================================
// PHẦN BLE NÂNG CAO - học sinh có thể đọc lướt lần đầu.
// CON TRỎ `*`: biến `s_pServer` bắt đầu bằng `p` nghĩa là con trỏ. Con trỏ là biến
// lưu ĐỊA CHỈ của một đối tượng khác; `nullptr` nghĩa là "đang chưa trỏ vào đâu".
// Khác nhau giữa dấu chấm `.` và mũi tên `->`:
//   - `camBienMpu.dangHoatDong` : camBienMpu là BIẾN bình thường, dùng dấu chấm.
//   - `s_pServer->setCallbacks(...)` : s_pServer là CON TRỎ, dùng mũi tên `->` để
//     "đi theo địa chỉ" mà truy cập vào đối tượng.
#if ENABLE_BLE
static BLEServer *s_pServer = nullptr;
static BLECharacteristic *s_pCharStream = nullptr;
static BLECharacteristic *s_pCharEvent = nullptr;
static BLECharacteristic *s_pCharStatus = nullptr;
static BLECharacteristic *s_pCharCommand = nullptr;
static BLECharacteristic *s_pCharAck = nullptr;

static char s_lastCommandId[37] = "";
static bool s_pendingCommand = false;
static char s_pendingCmdPayload[128] = "";

// Event tracking for persistent eventId on retransmit (§8.4)
static uint32_t s_eventCounter = 0;
static char s_currentEventId[32] = "";
static uint32_t s_currentEventSeq = 0;

// class + KẾ THỪA: lớp này kế thừa lớp có sẵn của thư viện BLE để thay lại hàm xử lý
// sự kiện (có người kết nối / ngắt kết nối), giống như đăng ký một "người nghe điện
// thoại": khi có cuộc gọi đến thì hàm tương ứng tự được gọi.
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        s_bleConnected = true;
    }
    void onDisconnect(BLEServer* pServer) override {
        s_bleConnected = false;
        s_bleStreaming = false;
        BLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0 && rxValue.length() < sizeof(s_pendingCmdPayload)) {
            strncpy(s_pendingCmdPayload, rxValue.c_str(), sizeof(s_pendingCmdPayload) - 1);
            s_pendingCmdPayload[sizeof(s_pendingCmdPayload) - 1] = '\0';
            s_pendingCommand = true;
        }
    }
};

// FIX 2: Esp32CommandAck (§8.5)
// Fields: protocolVersion · commandId · deviceId · timestampMs · commandStatus · errorCode · message
// commandStatus must be one of: ACCEPTED, COMPLETED, REJECTED, FAILED
// `char buffer[...]` là một mảng ký tự (chuỗi). `snprintf` ghép chuỗi vào buffer một
// cách AN TOÀN: nó tự chặn không ghi quá kích thước buffer nên không bị tràn bộ nhớ.
// `strlen(buffer)` trả về độ dài chuỗi (số ký tự thật sự, chưa kể ký tự kết thúc).
static void guiBleAck(const char* cmdId, const char* commandStatus, const char* errorCode, const char* message) {
    if (!s_bleConnected || s_pCharAck == nullptr) return;
    char buffer[256];
    char errBuf[48];
    char msgBuf[96];

    if (errorCode != nullptr) {
        snprintf(errBuf, sizeof(errBuf), "\"%s\"", errorCode);
    } else {
        snprintf(errBuf, sizeof(errBuf), "null");
    }

    if (message != nullptr) {
        snprintf(msgBuf, sizeof(msgBuf), "\"%s\"", message);
    } else {
        snprintf(msgBuf, sizeof(msgBuf), "null");
    }

    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"commandId\":\"%s\",\"deviceId\":\"%s\",\"timestampMs\":%llu,"
             "\"commandStatus\":\"%s\",\"errorCode\":%s,\"message\":%s}",
             cmdId ? cmdId : "",
             s_bleDeviceName,
             layThoiGianThucMs(),
             commandStatus ? commandStatus : "COMPLETED",
             errBuf,
             msgBuf);

    if (strlen(buffer) > 512) {
        boDem.soGoiBleBiBo++;
        return;
    }
    s_pCharAck->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharAck->notify();
}

// FIX 5: Esp32EventPacket (§8.4)
// Fields: protocolVersion · eventId · deviceId · sequenceNumber · timestampMs ·
// eventType · eventSeverity · sosButtonPressed · eventConfidence · peakAccelerationMs2 ·
// orientationChangeDeg · altitudeDeltaM · inactivityDurationMs · checksum · triggerReasons
static void guiBleSuKien(const char* eventType, const char* severity, int confidence,
                         float peakAcc, float orientationDeg, float altDeltaM,
                         uint32_t inactivityDurationMs, const char* reasons,
                         bool hasPeakAcc = true, bool hasOrientation = false, bool hasAltDelta = false,
                         bool isRetransmit = false) {
    if (!s_bleConnected || s_pCharEvent == nullptr) return;

    if (!isRetransmit || strlen(s_currentEventId) == 0) {
        snprintf(s_currentEventId, sizeof(s_currentEventId), "evt-%04X-%04u",
                 (uint16_t)(esp_random() & 0xFFFF), ++s_eventCounter);
        s_currentEventSeq = ++s_globalSequenceNumber;
    }

    uint64_t ts = layThoiGianThucMs();
    bool sosPressed = (digitalRead(PIN_BUTTON_SOS) == LOW);

    char peakBuf[24];
    if (hasPeakAcc) snprintf(peakBuf, sizeof(peakBuf), "%.2f", peakAcc);
    else snprintf(peakBuf, sizeof(peakBuf), "null");

    char orientBuf[24];
    if (hasOrientation) snprintf(orientBuf, sizeof(orientBuf), "%.1f", orientationDeg);
    else snprintf(orientBuf, sizeof(orientBuf), "null");

    char altBuf[24];
    if (hasAltDelta) snprintf(altBuf, sizeof(altBuf), "%.2f", altDeltaM);
    else snprintf(altBuf, sizeof(altBuf), "null");

    char inactBuf[24];
    if (inactivityDurationMs > 0) snprintf(inactBuf, sizeof(inactBuf), "%u", inactivityDurationMs);
    else snprintf(inactBuf, sizeof(inactBuf), "null");

    // Note (§8.4 / §8.7): checksum is null in v1 contract.
    // triggerReasons is an extension field outside v1 contract retained for debug/telemetry.
    char buffer[384];
    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"eventId\":\"%s\",\"deviceId\":\"%s\",\"sequenceNumber\":%u,"
             "\"timestampMs\":%llu,\"eventType\":\"%s\",\"eventSeverity\":\"%s\","
             "\"sosButtonPressed\":%s,\"eventConfidence\":%d,"
             "\"peakAccelerationMs2\":%s,\"orientationChangeDeg\":%s,\"altitudeDeltaM\":%s,"
             "\"inactivityDurationMs\":%s,\"checksum\":null,\"triggerReasons\":[\"%s\"]}",
             s_currentEventId,
             s_bleDeviceName,
             s_currentEventSeq,
             ts,
             eventType,
             severity,
             sosPressed ? "true" : "false",
             confidence,
             peakBuf,
             orientBuf,
             altBuf,
             inactBuf,
             reasons ? reasons : "");

    // Safety check against MTU overflow (§8.7 MTU gap)
    if (strlen(buffer) > 512) {
        boDem.soGoiBleBiBo++;
        return;
    }
    s_pCharEvent->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharEvent->indicate();
}

// FIX 4: Esp32DeviceStatus (§8.3)
// Fields: protocolVersion · deviceId · timestampMs · firmwareVersion · uptimeSeconds ·
// batteryPercent · batteryVoltageMv · isCharging · imuStatus · barometerStatus · gnssStatus ·
// bufferUsagePercent · lastErrorCode
static void guiBleTrangThaiThietBi() {
    if (!s_bleConnected || s_pCharStatus == nullptr) return;

    const char* imuStatusStr = "OK";
    if (trangThaiThietBi == TRANG_THAI_DANG_HIEU_CHUAN) imuStatusStr = "CALIBRATING";
    else if (!camBienMpu.dangHoatDong) imuStatusStr = "ERROR";
    else imuStatusStr = "OK";

    const char* baroStatusStr = "UNAVAILABLE";
    if (trangThaiThietBi == TRANG_THAI_DANG_HIEU_CHUAN) baroStatusStr = "CALIBRATING";
    else if (camBienKhiAp.coLoiDoc) baroStatusStr = "ERROR";
    else if (camBienKhiAp.dangHoatDong) baroStatusStr = "OK";
    else baroStatusStr = "UNAVAILABLE";

    int bufferUsage = (int)((soMauTrongBoDem * 100UL) / RING_BUFFER_SIZE);
    if (bufferUsage > 100) bufferUsage = 100;

    char errBuf[48];
    if (maLoiCuoiCung != nullptr && strlen(maLoiCuoiCung) > 0) {
        snprintf(errBuf, sizeof(errBuf), "\"%s\"", maLoiCuoiCung);
    } else {
        snprintf(errBuf, sizeof(errBuf), "null");
    }

    char buffer[320];
    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"timestampMs\":%llu,"
             "\"firmwareVersion\":\"%s\",\"uptimeSeconds\":%lu,\"batteryPercent\":%d,"
             "\"batteryVoltageMv\":null,\"isCharging\":%s,\"imuStatus\":\"%s\","
             "\"barometerStatus\":\"%s\",\"gnssStatus\":\"UNAVAILABLE\","
             "\"bufferUsagePercent\":%d,\"lastErrorCode\":%s}",
             s_bleDeviceName,
             layThoiGianThucMs(),
             FIRMWARE_VERSION,
             (unsigned long)(esp_timer_get_time() / 1000000ULL),
             phanTramPin,
             dangSacPin ? "true" : "false",
             imuStatusStr,
             baroStatusStr,
             bufferUsage,
             errBuf);

    if (strlen(buffer) > 512) {
        boDem.soGoiBleBiBo++;
        return;
    }
    s_pCharStatus->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharStatus->notify();
}

// FIX 1: Esp32SensorPacket (§8.2)
// Fields: protocolVersion · deviceId · sequenceNumber · timestampMs ·
// accelXMs2, accelYMs2, accelZMs2 · gyroXDps, gyroYDps, gyroZDps ·
// pressurePa · temperatureC · altitudeDeltaM · batteryPercent · batteryVoltageMv ·
// isCharging · sosButtonPressed · sensorQuality
static void guiBleDuLieuCamBien(float ax, float ay, float az, float gx, float gy, float gz,
                                float p, float altDelta, float tempC) {
    if (!s_bleConnected || !s_bleStreaming || s_pCharStream == nullptr) return;

    uint32_t seq = ++s_globalSequenceNumber;
    uint64_t ts = layThoiGianThucMs();
    bool sosPressed = (digitalRead(PIN_BUTTON_SOS) == LOW);

    // Sensor quality score calculation (0 - 100)
    int quality = 100;
    if (!camBienMpu.dangHoatDong) quality = 0;
    else if (camBienMpu.biBaoHoa) quality = 40;
    else if (!camBienKhiAp.dangHoatDong || camBienKhiAp.coLoiDoc) quality = 80;

    char buffer[384];
    if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc) {
        snprintf(buffer, sizeof(buffer),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu,"
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f,"
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f,"
                 "\"pressurePa\":%.1f,\"temperatureC\":%.1f,\"altitudeDeltaM\":%.2f,"
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s,"
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}",
                 s_bleDeviceName, seq, ts,
                 ax, ay, az,
                 gx, gy, gz,
                 p, tempC, altDelta,
                 phanTramPin,
                 dangSacPin ? "true" : "false",
                 sosPressed ? "true" : "false",
                 quality);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu,"
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f,"
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f,"
                 "\"pressurePa\":null,\"temperatureC\":null,\"altitudeDeltaM\":null,"
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s,"
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}",
                 s_bleDeviceName, seq, ts,
                 ax, ay, az,
                 gx, gy, gz,
                 phanTramPin,
                 dangSacPin ? "true" : "false",
                 sosPressed ? "true" : "false",
                 quality);
    }

    // MTU overflow check (§8.7): v1 has no binary fragmentation yet.
    // If packet exceeds MTU budget, count as dropped rather than corrupting stream.
    if (strlen(buffer) > 512) {
        boDem.soGoiBleBiBo++;
        return;
    }
    s_pCharStream->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharStream->notify();
}
#endif // ENABLE_BLE

// ============================================================================
// 9. SERIAL OUTPUT & COMMAND PARSER (§7)
// ============================================================================
#define SERIAL_RAW_MODE 0 // 0: Human mode (default ~4 Hz), 1: CSV mode
static bool cheDoCsvRaw = (SERIAL_RAW_MODE == 1);
static uint32_t thoiDiemInDeDocCuoiMs = 0;

static void inBangNguong() {
    Serial.println(F("\n======================================================="));
    Serial.println(F("NCKH27PA ESP32-S3 PRODUCT FALL DETECTION PROFILE"));
    Serial.println(F("NOTE: EXPERIMENTAL THRESHOLDS - NOT MEDICALLY VERIFIED"));
    Serial.println(F("======================================================="));
    Serial.printf("  impactAccelerationMs2:            %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.impactAccelerationMs2);
    Serial.printf("  stillnessTargetAccelerationMs2:   %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.stillnessTargetAccelerationMs2);
    Serial.printf("  stillnessToleranceMs2:            %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.stillnessToleranceMs2);
    Serial.printf("  postImpactWindowMs:               %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.postImpactWindowMs);
    Serial.printf("  postImpactStillnessDurationMs:    %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.postImpactStillnessDurationMs);
    Serial.printf("  minimumStillnessSamples:          %u samples  [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.minimumStillnessSamples);
    Serial.printf("  maximumSampleGapMs:               %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.maximumSampleGapMs);
    Serial.printf("  phonePressureMinimumRisePa:       %.1f Pa     [Android (flag disabled in decision)]\n", PROFILE_DEFAULT.phonePressureMinimumRisePa);
    Serial.printf("  freeFallThresholdMs2:             %.2f m/s^2   [Heuristic telemetry logging only]\n", PROFILE_DEFAULT.freeFallThresholdMs2);
    Serial.printf("  highAngularSpeedDps:              %.1f dps    [Heuristic telemetry logging only]\n", PROFILE_DEFAULT.highAngularSpeedDps);
    Serial.println(F("=======================================================\n"));
}

static void inTrangThaiHeThong() {
    Serial.println(F("\n--- DEVICE SYSTEM STATUS ---"));
    Serial.printf("Device ID: %s | Firmware: %s\n", s_bleDeviceName, FIRMWARE_VERSION);
    Serial.printf("State: %s (Last: %s)\n", tenTrangThaiThietBi(trangThaiThietBi), tenTrangThaiThietBi(trangThaiTruocDo));
    Serial.printf("IMU MPU6050: %s (addr: 0x%02X, biBaoHoa: %s, rate: %u Hz)\n",
                  camBienMpu.dangHoatDong ? "ONLINE" : "OFFLINE", camBienMpu.diaChi,
                  camBienMpu.biBaoHoa ? "YES" : "NO", tanSoLayMauHz);
    Serial.printf("Barometer MS5611: %s (addr: 0x%02X, CRC: %s, ref: %.1f Pa)\n",
                  camBienKhiAp.dangHoatDong ? (camBienKhiAp.coLoiDoc ? "ERROR" : "ONLINE") : "UNAVAILABLE",
                  camBienKhiAp.diaChi, camBienKhiAp.crcHopLe ? "VALID" : "INVALID", apSuatThamChieuPa);
    Serial.printf("BLE: %s (Streaming: %s)\n", s_bleConnected ? "CONNECTED" : "DISCONNECTED", s_bleStreaming ? "ON" : "OFF");
    Serial.printf("Buffer Usage: %u/%u samples (%d%%) | Drops: %u\n",
                  soMauTrongBoDem, RING_BUFFER_SIZE, (int)((soMauTrongBoDem * 100UL) / RING_BUFFER_SIZE), soMauBiBo);
    Serial.printf("Counters: IMU=%u, Baro=%u, LateSlots=%u, Falls=%u, SOS=%u, BLE Drops=%u\n",
                  boDem.soMauImuDaDoc, boDem.soMauKhiApDaDoc, boDem.soLanTreChuKy,
                  boDem.soLanXacNhanNga, boDem.soLanBamSos, boDem.soGoiBleBiBo);
    Serial.printf("Battery: %d%% (unfitted = -1), Free Heap: %u bytes\n", phanTramPin, ESP.getFreeHeap());
    Serial.printf("Last Error Code: %s\n", maLoiCuoiCung ? maLoiCuoiCung : "NONE");
    Serial.println(F("-----------------------------\n"));
}

static void inTieuDeCsv() {
    // FIX 3: Consistent field naming with altitudeDeltaM
    Serial.println(F("timestamp_ms,ax,ay,az,mag,gx,gy,gz,pressure_pa,altitudeDeltaM,state,event"));
}

// ============================================================================
// 10. CALIBRATION & SELF TEST (§7.1)
// ============================================================================
static void tuKiemTraVaHieuChuan() {
    trangThaiThietBi = TRANG_THAI_TU_KIEM_TRA;
    Serial.println(F("[BOOT] Starting Hardware Self-Test..."));

    bool imuOk = khoiTaoMpu6050();
    Serial.printf("[BOOT] IMU MPU6050 on I2C0 (SDA 7, SCL 6): %s (addr: 0x%02X)\n",
                  imuOk ? "PASS" : "FAIL", camBienMpu.diaChi);

    bool baroOk = khoiTaoMs5611();
    Serial.printf("[BOOT] Barometer MS5611 on I2C1 (SDA 3, SCL 2): %s (addr: 0x%02X, CRC: %s)\n",
                  baroOk ? "PASS" : "UNAVAILABLE", camBienKhiAp.diaChi, camBienKhiAp.crcHopLe ? "PASS" : "FAIL/UNVERIFIED");

    // FIX 6: Emit SENSOR_ERROR if critical sensor self-test fails
    if (!imuOk) {
        Serial.println(F("[CRITICAL] IMU failed self-test. System transitioning to DEGRADED mode."));
        trangThaiThietBi = TRANG_THAI_SUY_GIAM;
        maLoiCuoiCung = "IMU_READ_FAILED";
#if ENABLE_BLE
        guiBleSuKien("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_SELF_TEST_FAILED", false, false, false);
        guiBleTrangThaiThietBi();
#endif
        return;
    }

    trangThaiThietBi = TRANG_THAI_DANG_HIEU_CHUAN;
    Serial.println(F("[CALIB] Calibrating gyro bias & baseline gravity (keep device STILL for 2 sec)..."));

    float sumGx = 0, sumGy = 0, sumGz = 0;
    float sumAx = 0, sumAy = 0, sumAz = 0;
    float sumP = 0;
    int samples = 0;
    int vibrationWarnings = 0;

    uint32_t calibStart = millis();
    while (millis() - calibStart < 2000) {
        float ax, ay, az, gx, gy, gz;
        if (docMpu6050(ax, ay, az, gx, gy, gz)) {
            float mag = sqrtf(ax * ax + ay * ay + az * az);
            if (fabsf(mag - 9.81f) > 2.0f) {
                vibrationWarnings++;
            }
            sumAx += ax; sumAy += ay; sumAz += az;
            sumGx += gx; sumGy += gy; sumGz += gz;
            samples++;
        }
        if (camBienKhiAp.dangHoatDong) {
            docMs5611KhongChan(micros());
            sumP += camBienKhiAp.apSuatPa;
        }
        delay(10);
    }

    if (samples > 50) {
        camBienMpu.saiSoVanTocGocX = sumGx / samples;
        camBienMpu.saiSoVanTocGocY = sumGy / samples;
        camBienMpu.saiSoVanTocGocZ = sumGz / samples;
        camBienMpu.trongLucGocX = sumAx / samples;
        camBienMpu.trongLucGocY = sumAy / samples;
        camBienMpu.trongLucGocZ = sumAz / samples;
        if (camBienKhiAp.dangHoatDong) {
            // FIX 3: Initialize apSuatThamChieuPa from median/average during calibration
            apSuatThamChieuPa = sumP / samples;
            camBienKhiAp.chenhLechDoCaoM = 0.0f;
        }

        if (vibrationWarnings > (samples * 0.20f)) {
            Serial.println(F("[CALIB_WARNING] Device motion detected during calibration! Baseline may have offsets."));
        } else {
            Serial.println(F("[CALIB] Calibration complete. Gravity vector stabilized."));
        }
    }

    trangThaiThietBi = TRANG_THAI_BINH_THUONG;
    Serial.println(F("[SYSTEM] Transitioned to TRANG_THAI_BINH_THUONG. Ready."));
}

// ============================================================================
// 11. BAROMETER DRIFT TRACKING & BATTERY MANAGEMENT (§6.2, §3.4, FIX 3, FIX 6)
// ============================================================================
// Condition (b): Baseline reference updates automatically ONLY when:
// 1) Device is in TRANG_THAI_BINH_THUONG and NO suspected fall episode is active (!trangThaiPhatHienNga.dangTrongCuaSoXacMinh)
// 2) Ambient pressure remains stable within ±10 Pa (approx ±0.8 m) for at least 10 consecutive seconds.
// The baseline is NEVER modified during an active suspected fall or verification window.
static uint32_t s_stablePressureStartMs = 0;
static float s_lastStableCheckPressurePa = 0.0f;

static void capNhatApSuatThamChieu(uint32_t nowMs, float currentPressurePa) {
    if (trangThaiThietBi != TRANG_THAI_BINH_THUONG || trangThaiPhatHienNga.dangTrongCuaSoXacMinh) {
        s_stablePressureStartMs = 0;
        return;
    }
    if (fabsf(currentPressurePa - s_lastStableCheckPressurePa) > 10.0f) {
        s_lastStableCheckPressurePa = currentPressurePa;
        s_stablePressureStartMs = nowMs;
    } else {
        if (s_stablePressureStartMs == 0) {
            s_stablePressureStartMs = nowMs;
            s_lastStableCheckPressurePa = currentPressurePa;
        } else if (nowMs - s_stablePressureStartMs >= 10000) {
            // Pressure remained stable within +/- 10 Pa for >= 10 seconds: update baseline
            apSuatThamChieuPa = currentPressurePa;
            s_stablePressureStartMs = nowMs;
        }
    }
}

// Battery Management & Hysteresis Alerting (Plan §3.4, §8.2, FIX 6):
// The state variables (phanTramPin, dangSacPin, daCanhBaoPinYeu, daCanhBaoPinKiet)
// are declared in the global block ABOVE the BLE section, because the packet builders read them.
// When hardware fuel gauge is NOT fitted (HW_HAS_FUEL_GAUGE == 0):
// - batteryPercent = -1 (sentinel indicating no hardware; avoids fake 100%)
// - batteryVoltageMv = null
// - maLoiCuoiCung = "BATTERY_READ_FAILED"

static void capNhatTrangThaiPin(uint32_t nowMs) {
#if HW_HAS_FUEL_GAUGE
    // When MAX17048 or ADC fuel gauge hardware is fitted in future revision:
    // Read fuel gauge IC / ADC...
    // e.g. phanTramPin = readFuelGaugePercent();
    //
    // Hysteresis alert thresholds (Plan §3.4):
    // Warning: <= 20% (clears when > 25%)
    // Critical: <= 10% (clears when > 15%)
    if (phanTramPin >= 0) {
        if (phanTramPin <= 10 && !daCanhBaoPinKiet) {
            daCanhBaoPinKiet = true;
            daCanhBaoPinYeu = true;
#if ENABLE_BLE
            guiBleSuKien("LOW_BATTERY", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_CRITICAL_LE_10PCT", false, false, false);
            guiBleTrangThaiThietBi();
#endif
        } else if (phanTramPin <= 20 && !daCanhBaoPinYeu) {
            daCanhBaoPinYeu = true;
#if ENABLE_BLE
            guiBleSuKien("LOW_BATTERY", "WARNING", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_LOW_LE_20PCT", false, false, false);
            guiBleTrangThaiThietBi();
#endif
        } else if (phanTramPin > 25) {
            daCanhBaoPinYeu = false;
            daCanhBaoPinKiet = false;
        } else if (phanTramPin > 15) {
            daCanhBaoPinKiet = false;
        }
    }
#else
    // Hardware fuel gauge NOT fitted (HW_HAS_FUEL_GAUGE = 0).
    // Per Plan §8.2, §8.3 & Task FIX 6: Never send fake 100% battery!
    phanTramPin = -1;
#endif
}

// ============================================================================
// 12. BUTTONS, SOS & BUZZER HANDLING
// ============================================================================
static uint32_t thoiDiemBatDauBamSosMs = 0;
static bool daKichHoatSos = false;
static uint32_t thoiDiemBatDauBamHuyMs = 0;

static void capNhatNutBam(uint32_t nowMs) {
    // SOS Button: Active LOW with pull-up. Must be held for >= 2000 ms
    int sosVal = digitalRead(PIN_BUTTON_SOS);
    if (sosVal == LOW) {
        if (thoiDiemBatDauBamSosMs == 0) {
            thoiDiemBatDauBamSosMs = nowMs;
        } else if (nowMs - thoiDiemBatDauBamSosMs >= 2000 && !daKichHoatSos) {
            daKichHoatSos = true;
            boDem.soLanBamSos++;
            trangThaiThietBi = TRANG_THAI_BAO_DONG;
            thoiDiemBatDauBaoDongMs = nowMs;
            Serial.println(F("[ALERT] >>> SOS BUTTON TRIGGERED! Entering LOCAL_ALERTING directly. <<<"));
#if ENABLE_BLE
            guiBleSuKien("SOS_PRESSED", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "SOS_BUTTON_2S_HOLD", false, false, false);
            guiBleTrangThaiThietBi();
#endif
        }
    } else {
        thoiDiemBatDauBamSosMs = 0;
        daKichHoatSos = false;
    }

    // Cancel Button: Active LOW. Held >= 300 ms cancels any active alert
    int cancelVal = digitalRead(PIN_BUTTON_CANCEL);
    if (cancelVal == LOW) {
        if (thoiDiemBatDauBamHuyMs == 0) {
            thoiDiemBatDauBamHuyMs = nowMs;
        } else if (nowMs - thoiDiemBatDauBamHuyMs >= 300) {
            if (trangThaiThietBi == TRANG_THAI_BAO_DONG || trangThaiThietBi == TRANG_THAI_DANG_XAC_MINH || trangThaiThietBi == TRANG_THAI_NGHI_NGA) {
                Serial.println(F("[ACTION] Alert cancelled by user via CANCEL button."));
                trangThaiThietBi = TRANG_THAI_BINH_THUONG;
                digitalWrite(PIN_BUZZER, LOW);
#if ENABLE_BLE
                guiBleSuKien("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "USER_CANCEL_BUTTON", false, false, false);
                guiBleTrangThaiThietBi();
#endif
            }
        }
    } else {
        thoiDiemBatDauBamHuyMs = 0;
    }
}

static void capNhatCoi(uint32_t nowMs) {
    if (trangThaiThietBi == TRANG_THAI_BAO_DONG) {
        // High urgency alternating beep (200ms ON / 200ms OFF)
        bool on = ((nowMs - thoiDiemBatDauBaoDongMs) / 200) % 2 == 0;
        digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
        digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
    } else if (s_buzzerPatternDeadlineMs > nowMs) {
        // Custom buzzer pattern triggered by BLE command
        bool on = (nowMs / 150) % 2 == 0;
        digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
    } else {
        digitalWrite(PIN_BUZZER, LOW);
        // Status LED heartbeat in monitoring: slow pulse
        if (trangThaiThietBi == TRANG_THAI_BINH_THUONG) {
            digitalWrite(PIN_LED_STATUS, (nowMs % 1000 < 50) ? HIGH : LOW);
        } else if (trangThaiThietBi == TRANG_THAI_SUY_GIAM) {
            digitalWrite(PIN_LED_STATUS, (nowMs % 250 < 125) ? HIGH : LOW); // Fast error blink
        }
    }
}

// ============================================================================
// 13. FALL DETECTION ENGINE (Strict Android Parity)
// ============================================================================
// Đây là "bộ não" quyết định có té ngã hay không.
// Tham số `const char* &outEventLabel`: dấu & nghĩa là "truyền thẳng địa chỉ của biến
// gốc, không copy" (không tốn bộ nhớ); `const` nghĩa là "hứa sẽ không sửa nội dung
// chuỗi, chỉ đổi chỗ trỏ tới". Nhờ & mà hàm này có thể "trả ra" tên sự kiện ra ngoài.
//
// GIA TỐC LÀ GÌ? Đặt điện thoại yên trên bàn, cảm biến vẫn đọc ~1g (9.81 m/s^2)
// vì Trái Đất luôn kéo mọi vật xuống (trọng lực). Thả rơi tự do thì số đo tổng tụt
// xuống rất nhỏ (gần 0); đập xuống đất thì số đo vọt lên rất mạnh (> 2.5g);
// đi lại bình thường thì số đo chỉ dao động nhẹ quanh 1g.
// `mag = sqrtf(ax*ax + ay*ay + az*az)` là "độ dài vectơ gia tốc": gộp 3 trục thành
// MỘT con số duy nhất, nên luôn biết gia tốc mạnh cỡ nào mà không cần biết hướng.
//
// VẬN TỐC GÓC LÀ GÌ? Là tốc độ XOAY của vật, đơn vị độ/giây (dps). Đứng yên thì ~0,
// xoay nhanh thì vài trăm dps; ngã sang bên, nghiêng người, lật thiết bị đều làm nó tăng.
// `angularSpeed = sqrtf(gx*gx + gy*gy + gz*gz)` gộp 3 trục xoay thành một con số.
static void xuLyPhatHienTeNga(uint32_t nowMs, float ax, float ay, float az, float gx, float gy, float gz,
                                 float pressurePa, float altitudeDeltaM, const char* &outEventLabel) {
    outEventLabel = nullptr;
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float angularSpeed = sqrtf(gx * gx + gy * gy + gz * gz);

    // Track pre-impact minimum acceleration
    if (!trangThaiPhatHienNga.dangTrongCuaSoXacMinh) {
        if (mag < trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham || trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham == 0.0f) {
            trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham = mag;
        }
    }

    // Bước 1: Phát hiện va chạm (gia tốc tổng >= 25.0 m/s^2)
    if (mag >= PROFILE_DEFAULT.impactAccelerationMs2) {
        // Vừa phát hiện cú va chạm mạnh (hoặc va chạm lại)
        trangThaiPhatHienNga.thoiDiemVaChamMs = nowMs;
        trangThaiPhatHienNga.dinhGiaTocVaCham = mag;
        trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = 0;
        trangThaiPhatHienNga.soMauDungYen = 0;
        trangThaiPhatHienNga.dangTrongCuaSoXacMinh = true;
        trangThaiPhatHienNga.thoiDiemMauTruocMs = nowMs;
        trangThaiThietBi = TRANG_THAI_NGHI_NGA;
        outEventLabel = "IMPACT_DETECTED";
        Serial.println(F(">>> PHAT HIEN VA CHAM"));

        // Record pre-impact gravity reference
        trangThaiPhatHienNga.trongLucTruocVaCham[0] = camBienMpu.trongLucGocX;
        trangThaiPhatHienNga.trongLucTruocVaCham[1] = camBienMpu.trongLucGocY;
        trangThaiPhatHienNga.trongLucTruocVaCham[2] = camBienMpu.trongLucGocZ;

        // Populate telemetry trigger reasons
        snprintf(trangThaiPhatHienNga.lyDoKichHoat, sizeof(trangThaiPhatHienNga.lyDoKichHoat),
                 "IMPACT_%.1f%s%s",
                 mag,
                 (trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham < PROFILE_DEFAULT.freeFallThresholdMs2) ? ",FREE_FALL" : "",
                 (angularSpeed > PROFILE_DEFAULT.highAngularSpeedDps) ? ",HIGH_ROTATION" : "");

        Serial.printf("[FALL_ENGINE] Impact detected! |a| = %.2f m/s^2 >= %.2f. Entering SUSPECTED.\n",
                      mag, PROFILE_DEFAULT.impactAccelerationMs2);
#if ENABLE_BLE
        guiBleSuKien("IMPACT_DETECTED", "WARNING", 60, mag, 0.0f, altitudeDeltaM, 0,
                     trangThaiPhatHienNga.lyDoKichHoat, true, false, (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc));
#endif
        return;
    }

    // Step 2: Post-impact window monitoring (<= 3000 ms)
    if (trangThaiPhatHienNga.dangTrongCuaSoXacMinh) {
        uint32_t elapsedMs = nowMs - trangThaiPhatHienNga.thoiDiemVaChamMs;

        // Check sample gap constraint (max 250 ms)
        if (nowMs - trangThaiPhatHienNga.thoiDiemMauTruocMs > PROFILE_DEFAULT.maximumSampleGapMs) {
            Serial.println(F("[FALL_ENGINE] Sample gap > 250 ms exceeded. Resetting verification window."));
            trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false;
            trangThaiThietBi = TRANG_THAI_BINH_THUONG;
            return;
        }
        trangThaiPhatHienNga.thoiDiemMauTruocMs = nowMs;

        // Window timeout (exceeded 3000 ms without full stillness confirmation)
        if (elapsedMs > PROFILE_DEFAULT.postImpactWindowMs) {
            Serial.println(F("[FALL_ENGINE] Post-impact 3000 ms window expired without sustained stillness. Return to MONITORING."));
            trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false;
            trangThaiThietBi = TRANG_THAI_BINH_THUONG;
            return;
        }

        // Check stillness condition: | |a| - 9.81 | <= 1.0 m/s^2
        float stillnessDiff = fabsf(mag - PROFILE_DEFAULT.stillnessTargetAccelerationMs2);
        if (stillnessDiff <= PROFILE_DEFAULT.stillnessToleranceMs2) {
            if (trangThaiPhatHienNga.thoiDiemBatDauDungYenMs == 0) {
                trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = nowMs;
                trangThaiPhatHienNga.soMauDungYen = 1;
                trangThaiThietBi = TRANG_THAI_DANG_XAC_MINH;
                Serial.println(F(">>> DANG THEO DOI SAU VA CHAM"));
            } else {
                trangThaiPhatHienNga.soMauDungYen++;
            }

            uint32_t stillDuration = nowMs - trangThaiPhatHienNga.thoiDiemBatDauDungYenMs;

            // Check confirmation threshold: count >= 6 AND duration >= 1000 ms
            if (trangThaiPhatHienNga.soMauDungYen >= PROFILE_DEFAULT.minimumStillnessSamples &&
                stillDuration >= PROFILE_DEFAULT.postImpactStillnessDurationMs) {

                // Calculate orientation change from post-impact gravity vector
                float dot = (trangThaiPhatHienNga.trongLucTruocVaCham[0] * ax +
                             trangThaiPhatHienNga.trongLucTruocVaCham[1] * ay +
                             trangThaiPhatHienNga.trongLucTruocVaCham[2] * az);
                float magPre = sqrtf(trangThaiPhatHienNga.trongLucTruocVaCham[0]*trangThaiPhatHienNga.trongLucTruocVaCham[0] +
                                     trangThaiPhatHienNga.trongLucTruocVaCham[1]*trangThaiPhatHienNga.trongLucTruocVaCham[1] +
                                     trangThaiPhatHienNga.trongLucTruocVaCham[2]*trangThaiPhatHienNga.trongLucTruocVaCham[2]);
                float angleChangeDeg = 0.0f;
                if (magPre > 0.1f && mag > 0.1f) {
                    float cosTheta = dot / (magPre * mag);
                    if (cosTheta > 1.0f) cosTheta = 1.0f;
                    if (cosTheta < -1.0f) cosTheta = -1.0f;
                    angleChangeDeg = acosf(cosTheta) * (180.0f / 3.14159265f);
                }

                // Confidence heuristic score (0 - 100)
                int confidence = 85;
                if (angleChangeDeg >= PROFILE_DEFAULT.postureChangeThresholdDeg) confidence += 10;
                if (trangThaiPhatHienNga.giaTocNhoNhatTruocVaCham < PROFILE_DEFAULT.freeFallThresholdMs2) confidence += 5;
                if (confidence > 100) confidence = 100;

                trangThaiThietBi = TRANG_THAI_BAO_DONG;
                thoiDiemBatDauBaoDongMs = nowMs;
                boDem.soLanXacNhanNga++;
                trangThaiPhatHienNga.dangTrongCuaSoXacMinh = false;
                outEventLabel = "FALL_CONFIRMED";
                Serial.println(F(">>> CANH BAO: CO THE DA XAY RA TE NGA"));

                Serial.printf("[FALL_ENGINE] *** FALL VERIFIED! *** Stillness: %u ms (%u samples), Angle: %.1f deg, Conf: %d\n",
                              stillDuration, trangThaiPhatHienNga.soMauDungYen, angleChangeDeg, confidence);

#if ENABLE_BLE
                guiBleSuKien("INACTIVITY_DETECTED", "CRITICAL", confidence,
                             trangThaiPhatHienNga.dinhGiaTocVaCham, angleChangeDeg, altitudeDeltaM,
                             stillDuration, trangThaiPhatHienNga.lyDoKichHoat, true, true, (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc));
                guiBleTrangThaiThietBi();
#endif
            }
        } else {
            // Stillness interrupted during window: reset stillness counters
            trangThaiPhatHienNga.thoiDiemBatDauDungYenMs = 0;
            trangThaiPhatHienNga.soMauDungYen = 0;
        }
    }
}

// ============================================================================
// 14. COMMAND DISPATCHER (§8.5)
// ============================================================================
#if ENABLE_BLE
static void xuLyLenhBle(const char* payload) {
    // Robust JSON parser for standard command fields: commandType or command, commandId
    char cmdName[32] = "";
    char cmdId[37] = "";

    const char* pName = strstr(payload, "\"commandType\":");
    if (!pName) pName = strstr(payload, "\"command\":");
    if (pName) {
        if (strstr(pName, "\"commandType\":") == pName) {
            sscanf(pName, "\"commandType\":\"%31[^\"]\"", cmdName);
        } else {
            sscanf(pName, "\"command\":\"%31[^\"]\"", cmdName);
        }
    }
    const char* pId = strstr(payload, "\"commandId\":");
    if (pId) {
        sscanf(pId, "\"commandId\":\"%36[^\"]\"", cmdId);
    }

    // Deduplication check: return REJECTED with DUPLICATE_COMMAND
    if (strlen(cmdId) > 0 && strcmp(s_lastCommandId, cmdId) == 0) {
        guiBleAck(cmdId, "REJECTED", "DUPLICATE_COMMAND", "Command was already processed");
        return;
    }
    if (strlen(cmdId) > 0) {
        strncpy(s_lastCommandId, cmdId, sizeof(s_lastCommandId) - 1);
    }

    Serial.printf("[BLE_CMD] Dispatching command '%s' (ID: %s)\n", cmdName, cmdId);

    if (strcmp(cmdName, "PING") == 0) {
        guiBleAck(cmdId, "COMPLETED", nullptr, "PONG");
    } else if (strcmp(cmdName, "GET_STATUS") == 0) {
        guiBleTrangThaiThietBi();
        guiBleAck(cmdId, "COMPLETED", nullptr, "Status reported");
    } else if (strcmp(cmdName, "START_STREAM") == 0) {
        s_bleStreaming = true;
        guiBleAck(cmdId, "COMPLETED", nullptr, "Telemetry stream started");
    } else if (strcmp(cmdName, "STOP_STREAM") == 0) {
        s_bleStreaming = false;
        guiBleAck(cmdId, "COMPLETED", nullptr, "Telemetry stream stopped");
    } else if (strcmp(cmdName, "SET_SAMPLE_RATE") == 0) {
        // FIX 3: Parse sampleRateHz (accept only 100 and 50 Hz)
        int requestedRate = 0;
        const char* pRate = strstr(payload, "\"sampleRateHz\":");
        if (pRate) {
            if (sscanf(pRate, "\"sampleRateHz\":\"%d\"", &requestedRate) != 1) {
                sscanf(pRate, "\"sampleRateHz\":%d", &requestedRate);
            }
        }
        if (requestedRate == 100 || requestedRate == 50) {
            tanSoLayMauHz = requestedRate;
            chuKyLayMauUs = 1000000UL / tanSoLayMauHz;
            if (camBienMpu.dangHoatDong) {
                uint8_t smplrtDiv = (requestedRate == 50) ? 19 : 9;
                ghiI2cMotByte(Wire, camBienMpu.diaChi, MPU6050_SMPLRT_DIV, smplrtDiv);
            }
            char msg[64];
            snprintf(msg, sizeof(msg), "Sample rate set to %d Hz", requestedRate);
            guiBleAck(cmdId, "COMPLETED", nullptr, msg);
        } else {
            guiBleAck(cmdId, "REJECTED", "UNSUPPORTED_SAMPLE_RATE", "Only 100 Hz and 50 Hz are supported");
        }
    } else if (strcmp(cmdName, "SET_REFERENCE_ALTITUDE") == 0) {
        // FIX 3: Set current stable pressure as 0 m reference altitude baseline
        if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc && camBienKhiAp.apSuatPa > 10000.0f) {
            apSuatThamChieuPa = camBienKhiAp.apSuatPa;
            camBienKhiAp.chenhLechDoCaoM = 0.0f;
            guiBleAck(cmdId, "COMPLETED", nullptr, "Reference altitude reset to 0m at current pressure");
        } else {
            guiBleAck(cmdId, "FAILED", "BAROMETER_UNAVAILABLE", "Cannot set reference altitude: barometer offline");
        }
    } else if (strcmp(cmdName, "TRIGGER_BUZZER") == 0) {
        int durationMs = 2000;
        const char* pMs = strstr(payload, "\"remainingMs\":");
        if (pMs) sscanf(pMs, "\"remainingMs\":%d", &durationMs);
        s_buzzerPatternDeadlineMs = millis() + durationMs;
        guiBleAck(cmdId, "COMPLETED", nullptr, "Buzzer pattern active");
    } else if (strcmp(cmdName, "STOP_BUZZER") == 0) {
        s_buzzerPatternDeadlineMs = 0;
        digitalWrite(PIN_BUZZER, LOW);
        guiBleAck(cmdId, "COMPLETED", nullptr, "Buzzer silenced");
    } else if (strcmp(cmdName, "ACK_EVENT") == 0) {
        // Plan §7.3, §8.5: ACK_EVENT does NOT silence local alarm
        guiBleAck(cmdId, "COMPLETED", nullptr, "Event acknowledged (local alarm remains active until cancel)");
    } else if (strcmp(cmdName, "CANCEL_ALERT") == 0) {
        if (trangThaiThietBi == TRANG_THAI_BAO_DONG || trangThaiThietBi == TRANG_THAI_DANG_XAC_MINH || trangThaiThietBi == TRANG_THAI_NGHI_NGA) {
            trangThaiThietBi = TRANG_THAI_BINH_THUONG;
            digitalWrite(PIN_BUZZER, LOW);
            guiBleSuKien("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "BLE_CANCEL_COMMAND", false, false, false);
            guiBleTrangThaiThietBi();
            guiBleAck(cmdId, "COMPLETED", nullptr, "Alert cancelled by remote BLE command");
        } else {
            guiBleAck(cmdId, "COMPLETED", nullptr, "No active alert to cancel");
        }
    } else if (strcmp(cmdName, "START_SELF_TEST") == 0) {
        tuKiemTraVaHieuChuan();
        guiBleAck(cmdId, "COMPLETED", nullptr, "Self test finished");
    } else if (strcmp(cmdName, "SET_DEVICE_TIME") == 0) {
        // Plan §8.5, §8.6: Synchronize Unix timestamp (milliseconds)
        uint64_t ep = 0;
        const char* pTime = strstr(payload, "\"unixTimeMs\":");
        if (!pTime) pTime = strstr(payload, "\"epochTime\":");
        if (pTime) {
            if (sscanf(pTime, "\"unixTimeMs\":%llu", &ep) == 1 ||
                sscanf(pTime, "\"unixTimeMs\":\"%llu\"", &ep) == 1 ||
                sscanf(pTime, "\"epochTime\":%llu", &ep) == 1) {
                if (ep < 10000000000ULL) ep *= 1000ULL; // Convert seconds to milliseconds
                s_deviceEpochTimeMs = ep;
                s_epochSyncLocalMs = millis();
                s_timeSynced = true;
                if (maLoiCuoiCung != nullptr && strcmp(maLoiCuoiCung, "TIME_NOT_SYNCED") == 0) {
                    maLoiCuoiCung = nullptr;
                }
            }
        }
        guiBleAck(cmdId, "COMPLETED", nullptr, "Epoch clock synchronized");
    } else if (strcmp(cmdName, "REBOOT_DEVICE") == 0) {
        // Plan §8.5: Reject reboot if device is currently in alerting state
        if (trangThaiThietBi == TRANG_THAI_BAO_DONG || trangThaiThietBi == TRANG_THAI_DANG_XAC_MINH) {
            guiBleAck(cmdId, "REJECTED", "DEVICE_ALERTING", "Cannot reboot during active alert");
        } else {
            guiBleAck(cmdId, "ACCEPTED", nullptr, "Rebooting device...");
            delay(100);
            esp_restart();
        }
    } else {
        guiBleAck(cmdId, "REJECTED", "UNKNOWN_COMMAND", "Command not recognized");
    }
}
#endif

// ============================================================================
// 15. SETUP & INITIALIZATION
// ============================================================================
void setup() {
    Serial.begin(115200);

#if ARDUINO_USB_CDC_ON_BOOT
    // Wait briefly for native USB-CDC to attach if present
    delay(500);
#endif

    // Generate BLE Device Name: FALLSAFE-xxxx from BT MAC diaChi
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_bleDeviceName, sizeof(s_bleDeviceName), "FALLSAFE-%02X%02X", mac[4], mac[5]);

    Serial.println(F("\n\n======================================================="));
    Serial.println(F("NCKH27PA - ESP32-S3 SUPER MINI OFFICIAL FIRMWARE"));
    Serial.printf("Device ID: %s | Build: %s %s | Protocol: %d\n",
                  s_bleDeviceName, __DATE__, __TIME__, PROTOCOL_VERSION);
    Serial.println(F("======================================================="));

    // Configure GPIOs
    pinMode(PIN_BUTTON_SOS, INPUT_PULLUP);
    pinMode(PIN_BUTTON_CANCEL, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_BAT_1, OUTPUT);
    pinMode(PIN_LED_BAT_2, OUTPUT);
    pinMode(PIN_LED_BAT_3, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Initialize I2C Bus 0 for MPU6050 (SDA = GPIO 7, SCL = GPIO 6, 400kHz)
    Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 400000);
    Wire.setTimeOut(25); // 25ms timeout prevents bus lockup from blocking SOS button

    // Initialize I2C Bus 1 for MS5611 (SDA = GPIO 3, SCL = GPIO 2, 400kHz)
    Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, 400000);
    Wire1.setTimeOut(25);

    // Print Profile and Thresholds
    inBangNguong();

    // Hardware Self-Test & Calibration
    tuKiemTraVaHieuChuan();

#if ENABLE_BLE
    BLEDevice::init(s_bleDeviceName);
    s_pServer = BLEDevice::createServer();
    s_pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = s_pServer->createService(UUID_SERVICE);

    s_pCharStream = pService->createCharacteristic(
        UUID_CHAR_STREAM,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharStream->addDescriptor(new BLE2902());

    s_pCharEvent = pService->createCharacteristic(
        UUID_CHAR_EVENT,
        BLECharacteristic::PROPERTY_INDICATE
    );
    s_pCharEvent->addDescriptor(new BLE2902());

    s_pCharStatus = pService->createCharacteristic(
        UUID_CHAR_STATUS,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharStatus->addDescriptor(new BLE2902());

    s_pCharCommand = pService->createCharacteristic(
        UUID_CHAR_COMMAND,
        BLECharacteristic::PROPERTY_WRITE
    );
    s_pCharCommand->setCallbacks(new CommandCallbacks());

    s_pCharAck = pService->createCharacteristic(
        UUID_CHAR_ACK,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharAck->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(UUID_SERVICE);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] GATT Server initialized. Device name: %s\n", s_bleDeviceName);
#endif

    // Watchdog Timer Initialization (5 seconds timeout)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = 0,
        .trigger_panic = false
    };
    esp_task_wdt_deinit();
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL);
#else
    esp_task_wdt_init(5, false);
    esp_task_wdt_add(NULL);
#endif

    if (cheDoCsvRaw) {
        inTieuDeCsv();
    }
    Serial.println(F("[SYSTEM] Initialization complete. System active."));
}

// ============================================================================
// 16. MAIN LOOP (Configurable 100/50 Hz IMU Scheduling & Non-blocking Tasks)
// ============================================================================
// LUỒNG CHÍNH CỦA CHƯƠNG TRÌNH (lặp đi lặp lại mãi):
//   1) đọc cảm biến khí áp  2) đọc cảm biến gia tốc  3) lưu vào bộ đệm vòng
//   4) xử lý phát hiện té ngã  5) gửi BLE  6) cập nhật nút / còi / pin
//   7) in dữ liệu ra màn hình  8) quay lại từ đầu.
void loop() {
    // WATCHDOG = "đồng hồ canh gác": nếu chương trình bị treo quá lâu mà không gọi
    // dòng này để báo "tôi vẫn còn sống", thì chip sẽ TỰ KHỞI ĐỘNG LẠI để thoát treo.
    esp_task_wdt_reset();

    uint32_t nowMs = millis();
    uint32_t nowUs = micros();

    // Bước 1: Đọc cảm biến khí áp (không chặn chương trình - "non-blocking")
    docMs5611KhongChan(nowUs);

    // Tự cập nhật mốc áp suất tham chiếu khi không khí ổn định
    if (camBienKhiAp.dangHoatDong && !camBienKhiAp.coLoiDoc) {
        capNhatApSuatThamChieu(nowMs, camBienKhiAp.apSuatPa);
    }

    // Bước 2: Đọc cảm biến gia tốc theo nhịp (100 Hz = mỗi 10.000 us, hoặc 50 Hz)
    // `static` nghĩa là biến được GIỮ LẠI giữa các lần gọi hàm, không bị tạo lại từ đầu.
    static uint32_t s_lastImuSampleUs = 0;
    static uint8_t s_imuConsecutiveFailures = 0;
    if (nowUs - s_lastImuSampleUs >= chuKyLayMauUs) {
        if (s_lastImuSampleUs > 0 && (nowUs - s_lastImuSampleUs > (chuKyLayMauUs * 3 / 2))) {
            boDem.soLanTreChuKy++;
        }
        s_lastImuSampleUs = nowUs;

        float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
        bool imuSuccess = docMpu6050(ax, ay, az, gx, gy, gz);

        if (imuSuccess) {
            s_imuConsecutiveFailures = 0;
            boDem.soMauImuDaDoc++;
            float mag = sqrtf(ax * ax + ay * ay + az * az);

            // Store in circular buffer (1000 samples @ 100 Hz = 10s pre-event buffer)
            boDemVong[viTriGhi].timestampMs = nowMs;
            boDemVong[viTriGhi].ax = ax;
            boDemVong[viTriGhi].ay = ay;
            boDemVong[viTriGhi].az = az;
            boDemVong[viTriGhi].gx = gx;
            boDemVong[viTriGhi].gy = gy;
            boDemVong[viTriGhi].gz = gz;
            boDemVong[viTriGhi].magnitude = mag;
            boDemVong[viTriGhi].pressurePa = camBienKhiAp.apSuatPa;
            boDemVong[viTriGhi].altitudeDeltaM = camBienKhiAp.chenhLechDoCaoM;
            viTriGhi = (viTriGhi + 1) % RING_BUFFER_SIZE;
            if (soMauTrongBoDem < RING_BUFFER_SIZE) {
                soMauTrongBoDem++;
            }

            // Fall Detection Engine
            const char* eventLabel = nullptr;
            xuLyPhatHienTeNga(nowMs, ax, ay, az, gx, gy, gz,
                                 camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM, eventLabel);

            // CSV output stream (FIX 3: altitudeDeltaM)
            if (cheDoCsvRaw) {
                Serial.printf("%u,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%s,%s\n",
                              nowMs, ax, ay, az, mag, gx, gy, gz,
                              camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM,
                              tenTrangThaiThietBi(trangThaiThietBi),
                              eventLabel ? eventLabel : "");
            }

#if ENABLE_BLE
            // Send BLE real-time telemetry if streaming requested (decimated to ~25Hz to save BLE bandwidth)
            static uint8_t s_streamDecimator = 0;
            uint8_t decimateTarget = (tanSoLayMauHz == 50) ? 2 : 4;
            if (++s_streamDecimator >= decimateTarget) {
                s_streamDecimator = 0;
                guiBleDuLieuCamBien(ax, ay, az, gx, gy, gz,
                                    camBienKhiAp.apSuatPa, camBienKhiAp.chenhLechDoCaoM, camBienKhiAp.nhietDoC);
            }
#endif
        } else {
            soMauBiBo++;
            if (++s_imuConsecutiveFailures >= 10 && trangThaiThietBi != TRANG_THAI_SUY_GIAM) {
                trangThaiThietBi = TRANG_THAI_SUY_GIAM;
                maLoiCuoiCung = "IMU_READ_FAILED";
                camBienMpu.dangHoatDong = false;
#if ENABLE_BLE
                guiBleSuKien("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_READ_FAILED", false, false, false);
                guiBleTrangThaiThietBi();
#endif
            }
        }
    }

    // Bước 3: Kiểm tra pin định kỳ (mỗi 10 giây)
    static uint32_t s_lastBatCheckMs = 0;
    if (nowMs - s_lastBatCheckMs >= 10000) {
        s_lastBatCheckMs = nowMs;
        capNhatTrangThaiPin(nowMs);
    }

    // Bước 4: Đọc nút bấm (SOS / Hủy báo động)
    capNhatNutBam(nowMs);

    // Bước 5: Điều khiển còi và đèn LED theo nhịp
    capNhatCoi(nowMs);

    // Bước 6: PHẦN BLE NÂNG CAO - học sinh có thể đọc lướt lần đầu.
#if ENABLE_BLE
    // Xử lý lệnh BLE nhận được (nếu có)
    if (s_pendingCommand) {
        s_pendingCommand = false;
        xuLyLenhBle(s_pendingCmdPayload);
    }

    // Gửi trạng thái thiết bị qua BLE mỗi 5 giây
    static uint32_t s_lastBleStatusMs = 0;
    if (nowMs - s_lastBleStatusMs >= 5000) {
        s_lastBleStatusMs = nowMs;
        guiBleTrangThaiThietBi();
    }
#endif

    // Bước 7: In dữ liệu cho CON NGƯỜI đọc (~4 khối/giây, dễ hiểu hơn chế độ CSV).
    if (!cheDoCsvRaw && (nowMs - thoiDiemInDeDocCuoiMs >= 250)) {
        thoiDiemInDeDocCuoiMs = nowMs;
        float curAx = 0, curAy = 0, curAz = 0, curGx = 0, curGy = 0, curGz = 0;
        docMpu6050(curAx, curAy, curAz, curGx, curGy, curGz);
        float mag = sqrtf(curAx * curAx + curAy * curAy + curAz * curAz);
        // Vận tốc góc tổng hợp (độ/giây) từ 3 trục con quay
        float vanTocGoc = sqrtf(curGx * curGx + curGy * curGy + curGz * curGz);

        Serial.println(F("=== DU LIEU CAM BIEN ==="));
        Serial.printf("Gia toc tong : %.2f g\n", mag / 9.81f);
        Serial.printf("Van toc goc  : %.1f do/s\n", vanTocGoc);
        Serial.printf("Ap suat      : %.2f hPa\n", camBienKhiAp.apSuatPa / 100.0f);
        Serial.printf("Chenh cao    : %+.2f m\n", camBienKhiAp.chenhLechDoCaoM);
        Serial.printf("Trang thai   : %s\n", tenTrangThaiThietBi(trangThaiThietBi));
    }

    // Bước 8: Đọc lệnh người dùng gõ từ bàn phím (Serial Monitor)
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();
        if (ch == 'r' || ch == 'R') {
            cheDoCsvRaw = !cheDoCsvRaw;
            if (cheDoCsvRaw) {
                inTieuDeCsv();
            } else {
                Serial.println(F("[SERIAL] Da chuyen sang che do DU LIEU DE DOC (~4 Hz)."));
            }
        } else if (ch == 't' || ch == 'T') {
            inBangNguong();
        } else if (ch == 'c' || ch == 'C') {
            tuKiemTraVaHieuChuan();
        } else if (ch == 's' || ch == 'S') {
            inTrangThaiHeThong();
        } else if (ch == 'h' || ch == 'H') {
            Serial.println(F("\n--- BANG TRO GIUP ---"));
            Serial.println(F("  r : Bat/tat che do CSV (du lieu tho cho ve do thi)"));
            Serial.println(F("  t : In bang cac nguong phat hien te nga"));
            Serial.println(F("  c : Chay lai tu kiem tra & hieu chuan cam bien"));
            Serial.println(F("  s : In trang thai he thong va cac bo dem"));
            Serial.println(F("  h : In bang tro giup nay"));
            Serial.println(F("-----------------------\n"));
        }
    }
}
