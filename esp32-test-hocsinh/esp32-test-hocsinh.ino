/**
 * ============================================================================
 * ESP32 FALL DETECTION TEST RIG — FIRMWARE THỬ NGHIỆM THUẬT TOÁN
 * ============================================================================
 * 
 * !!! CẢNH BÁO QUAN TRỌNG:
 * ĐÂY LÀ TEST_RIG THỬ NGHIỆM — ĐÂY KHÔNG PHẢI PHẦN CỨNG PRODUCTION!
 * 
 * - Phần cứng Test Rig:
 *     + Vi điều khiển: ESP32-WROOM-32 (ESP32 Dev Module)
 *     + Cảm biến IMU: MPU9250 (GY-9250) — Tạm thời cho bộ test, KHÔNG phải
 *       cảm biến sản phẩm đích (sản phẩm đích dùng MPU6050 trên ESP32-S3).
 *     + Cảm biến khí áp: GY-63 / MS5611-01BA03
 * - Đấu dây I2C mặc định:
 *     // VERIFY_TEST_WIRING: SDA = GPIO 21, SCL = GPIO 22 (Bus 0 mặc định)
 *     LƯU Ý: Repo chưa ghi nhận sơ đồ đấu dây thực tế cho bộ test WROOM-32.
 *     Người dùng BẮT BUỘC phải xác minh nối dây vật lý trước khi cấp nguồn!
 *     TUYỆT ĐỐI KHÔNG dùng chân I2C của ESP32-S3 Super Mini (GPIO 6/7) cho
 *     ESP32-WROOM-32 vì GPIO 6..11 nối chip Flash nội bộ gây crash/reboot!
 * - Từ kế AK8963 trên MPU9250: KHÔNG SỬ DỤNG vì nhiễu từ trường lớn trong nhà,
 *     không cần thiết cho phát hiện ngã và gây nghẽn bus I2C 100 Hz.
 * - Đơn vị đo lường chuẩn SI:
 *     + Gia tốc: m/s^2 (bao gồm trọng lực 9.81 m/s^2)
 *     + Tốc độ góc: độ/giây (°/s hay dps)
 *     + Áp suất: Pascal (Pa)
 *     + Độ cao tương đối: mét (m)
 *     + Thời gian: milliseconds (ms)
 * - Tốc độ Serial: 115200 baud
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// 1. CẤU HÌNH PHẦN CỨNG & CHÂN KẾT NỐI (PIN DEFINITIONS)
// ============================================================================
// [KHÁI NIỆM] `constexpr` và `#define` đều là "hằng số": ta đặt TÊN cho một con
// số để sau này chỉ cần sửa MỘT chỗ là toàn bộ chương trình đổi theo.
// Ví dụ: muốn đổi chân SDA chỉ cần sửa dòng PIN_TEST_I2C_SDA, không phải tìm
// và sửa từng chỗ dùng số 21 trong cả file.

// VERIFY_TEST_WIRING: Cặp chân I2C mặc định an toàn cho ESP32-WROOM-32
constexpr int PIN_TEST_I2C_SDA = 21;   // Chân truyền dữ liệu I2C (SDA) - KHÔNG ĐỔI
constexpr int PIN_TEST_I2C_SCL = 22;   // Chân xung nhịp I2C (SCL) - KHÔNG ĐỔI
constexpr uint32_t I2C_CLOCK_FREQ_HZ = 400000;  // 400 kHz Fast-mode - KHÔNG ĐỔI

// Địa chỉ I2C: mỗi con chip trên cùng một sợi dây I2C có một "số nhà" riêng
// để bo mạch biết đang nói chuyện với ai. Đây là số nhà ghi trong datasheet.
constexpr uint8_t MPU9250_I2C_ADDR_PRIMARY = 0x68;
constexpr uint8_t MPU9250_I2C_ADDR_ALT     = 0x69;
constexpr uint8_t MS5611_I2C_ADDR_PRIMARY  = 0x77;
constexpr uint8_t MS5611_I2C_ADDR_ALT      = 0x76;

// MPU9250 Registers (địa chỉ thanh ghi trong datasheet - GIỮ NGUYÊN TÊN gốc)
// "Thanh ghi" giống như các ô nhớ đặc biệt bên trong con chip; ghi số vào một
// ô là ra lệnh cho chip, đọc một ô là hỏi chip trả về dữ liệu.
constexpr uint8_t MPU_REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t MPU_REG_CONFIG       = 0x1A;
constexpr uint8_t MPU_REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t MPU_REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t MPU_REG_ACCEL_CONFIG2= 0x1D;
constexpr uint8_t MPU_REG_INT_PIN_CFG  = 0x37;
constexpr uint8_t MPU_REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t MPU_REG_USER_CTRL    = 0x6A;
constexpr uint8_t MPU_REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t MPU_REG_PWR_MGMT_2   = 0x6C;
constexpr uint8_t MPU_REG_WHO_AM_I     = 0x75;

// MS5611 Commands (lệnh gửi cho cảm biến khí áp - GIỮ NGUYÊN TÊN datasheet)
constexpr uint8_t MS5611_CMD_RESET     = 0x1E;
constexpr uint8_t MS5611_CMD_ADC_READ  = 0x00;
constexpr uint8_t MS5611_CMD_PROM_BASE = 0xA0;
constexpr uint8_t MS5611_CMD_CONV_D1   = 0x48;  // OSR 4096 (9.04ms max)
constexpr uint8_t MS5611_CMD_CONV_D2   = 0x58;  // OSR 4096 (9.04ms max)
constexpr uint32_t MS5611_CONV_DELAY_US= 9200;  // 9.2 ms

// Hằng số tính toán (hằng số vật lý - GIỮ NGUYÊN TÊN datasheet)
// GRAVITY_EARTH_MS2: gia tốc trọng trường Trái Đất tiêu chuẩn, đơn vị m/s^2.
// Đây là con số mà cảm biến đo được khi đặt yên trên bàn (≈ 1g).
constexpr double GRAVITY_EARTH_MS2     = 9.80665;
// ACCEL_SCALE_16G: quy đổi số đọc thô của gia tốc kế sang m/s^2.
// Thang ±16g, dữ liệu 16 bit có dải -32768..32767.
constexpr double ACCEL_SCALE_16G       = (16.0 * GRAVITY_EARTH_MS2) / 32768.0; // m/s^2 per LSB
// GYRO_SCALE_2000DPS: quy đổi số đọc thô của con quay hồi chuyển sang độ/giây.
constexpr double GYRO_SCALE_2000DPS    = 2000.0 / 32768.0;                    // dps per LSB
// HYPSOMETRIC_SCALE / HYPSOMETRIC_EXPONENT: hai hằng số của công thức khí áp
// quốc tế (barometric formula) dùng để ước lượng độ cao từ áp suất. Đây là
// công thức chuẩn đã được kiểm chứng, học sinh không cần tự suy ra, chỉ cần
// hiểu ý nghĩa: lên cao thì áp suất giảm, xuống thấp thì áp suất tăng.
constexpr double HYPSOMETRIC_SCALE     = 44330.77;
constexpr double HYPSOMETRIC_EXPONENT  = 0.190263;

// Chế độ in Serial mặc định (0 = HUMAN MODE, 1 = CSV RAW MODE)
#ifndef SERIAL_RAW_MODE
#define SERIAL_RAW_MODE 0
#endif

// ============================================================================
// 2. CẤU TRÚC DỮ LIỆU & PROFILE THUẬT TOÁN (DATA STRUCTURES & PROFILE)
// ============================================================================

// [KHÁI NIỆM] `struct` giống như MỘT CÁI HỘP gồm nhiều biến có liên quan, để
// mang cả nhóm đi cùng nhau. Thay vì phải truyền 9 biến rời rạc cho mỗi hàm,
// ta chỉ truyền MỘT "hộp" SensorSnapshot chứa đủ mọi thứ đo được trong 1 lần đọc.
// (Khai báo sớm để các hàm phía dưới có thể dùng.)

// Dữ liệu đo tức thời từ cảm biến trong MỘT lần đọc (1 "bức ảnh chụp" trạng thái)
struct SensorSnapshot {
  uint32_t timestampMs = 0;   // Thời điểm đọc mẫu, tính bằng mili-giây (ms)
  double ax = 0.0, ay = 0.0, az = 0.0;  // Gia tốc 3 trục X, Y, Z - đơn vị m/s^2
  double gx = 0.0, gy = 0.0, gz = 0.0;  // Vận tốc góc 3 trục X, Y, Z - đơn vị độ/giây (dps)
  double tongGiaToc = 0.0;          // Độ lớn (chiều dài) của vectơ gia tốc, m/s^2
  double tongVanTocGoc = 0.0;           // Độ lớn của vectơ vận tốc góc, dps
  double apSuatPa = 0.0;              // Áp suất khí quyển, đơn vị Pascal (Pa)
  double temperatureC = 0.0;            // Nhiệt độ cảm biến khí áp, độ C
  double chenhLechDoCaoM = 0.0;          // Chênh lệch độ cao so với lúc khởi động, mét (m)
  bool dangDungYen = false;              // Đúng (true) khi thiết bị đang nằm yên
};

// Khai báo trước hàm hiệu chuẩn
void hieuChuanLucKhoiDong();

#define PROFILE_NAME "TEST_RIG_EXP_V1"

// [KHÁI NIỆM] FallProfile là "bộ cấu hình ngưỡng" nhận diện té ngã: nó gom mọi
// con số quyết định "thế nào là ngã" vào một chỗ. Các giá trị bên dưới đều là
// GIÁ TRỊ THỬ NGHIỆM (chưa phải giá trị cuối cùng): đơn vị ghi rõ ở từng dòng,
// và cần ĐO THỰC TẾ trên thiết bị để chỉnh lại cho đúng người dùng thật.
struct FallProfile {
  // --- Kế thừa nguyên bản từ Android FallDetectionConfig.DEFAULT ---
  float impactAccelerationMs2 = 25.0f;           // Ngưỡng va đập ~2.55g (đơn vị m/s^2) [FallDetectionProfiles.kt:56] - THỬ NGHIỆM, cần đo thực tế
  float stillnessTargetAccelerationMs2 = 9.81f;  // Trọng trường chuẩn khi tĩnh = 1.0g (đơn vị m/s^2) - hằng số vật lý, KHÔNG cần đổi
  float stillnessToleranceMs2 = 1.0f;            // Dung sai tĩnh: |a - 9.81| <= 1.0 (đơn vị m/s^2) - THỬ NGHIỆM
  uint32_t postImpactWindowMs = 3000;            // Cửa sổ tối đa sau va đập (đơn vị ms) - THỬ NGHIỆM
  uint32_t postImpactStillnessDurationMs = 1000; // Thời lượng nằm yên tối thiểu (đơn vị ms) - THỬ NGHIỆM
  uint16_t minimumStillnessSamples = 6;          // Giữ 6 để bám đúng FallDetectionConfig.DEFAULT Android; ở 100 Hz điều kiện duration >= 1000 ms mới là ràng buộc thực tế (đơn vị: số mẫu)
  uint32_t maximumSampleGapMs = 250;             // Hủy chuỗi nếu mất mẫu > 250 ms (đơn vị ms) - THỬ NGHIỆM

  // --- Tham số thử nghiệm bổ sung cho Test Rig (bằng chứng phụ & log) ---
  float freeFallThresholdMs2 = 4.90f;            // Rơi tự do < 0.5g (đơn vị m/s^2) - THỬ NGHIỆM
  uint32_t freeFallMinDurationMs = 80;           // Duy trì rơi tự do tối thiểu 80 ms (đơn vị ms) - THỬ NGHIỆM
  float gyroTurnThresholdDps = 120.0f;           // Vận tốc góc xoay thân khi ngã (đơn vị dps) - THỬ NGHIỆM
  float pressureEvidenceMinRisePa = 12.0f;       // Tăng áp suất đối chứng, Android mặc định 12 Pa (đơn vị Pa) - THỬ NGHIỆM
  uint32_t pressureWindowMs = 5000;              // Cửa sổ tính áp suất tăng 5000 ms [Nguồn: Android DemoLogic.kt PRESSURE_WINDOW_NS] - THỬ NGHIỆM
  float altitudeDropMinM = -0.40f;               // Độ cao giảm tối thiểu đối chứng -0.40 m (đơn vị m) - THỬ NGHIỆM
  uint32_t sampleWatchdogMs = 100;               // Cảnh báo watchdog nếu trễ đọc cảm biến > 100 ms (đơn vị ms) - THỬ NGHIỆM
};

// ============================================================================
// 3. MÁY TRẠNG THÁI PHÁT HIỆN NGÃ (STATE MACHINE)
// ============================================================================
// [KHÁI NIỆM] `enum` là danh sách các trạng thái mà thiết bị có thể đang ở.
// Mỗi lúc thiết bị CHỈ ở ĐÚNG MỘT trạng thái. Bộ nhớ "đang ở trạng thái nào"
// rồi căn cứ vào đó mà quyết định bước kế tiếp được gọi là "máy trạng thái"
// (state machine). Ví dụ đời sống: một người không thể cùng lúc vừa "đang ngủ"
// vừa "đang chạy" - máy trạng thái cũng vậy, luôn ở một trạng thái duy nhất.
enum DetectionState {
  TRANG_THAI_DANG_HIEU_CHUAN = 0,   // Đang hiệu chuẩn cảm biến lúc khởi động
  TRANG_THAI_BINH_THUONG,           // Bình thường: chưa thấy dấu hiệu ngã
  TRANG_THAI_NGHI_ROI,              // Nghi đang rơi tự do (gia tốc tụt rất thấp)
  TRANG_THAI_PHAT_HIEN_VA_CHAM,     // Vừa đo được một cú va đập mạnh
  TRANG_THAI_THEO_DOI_SAU_VA_CHAM,  // Đang theo dõi xem có nằm yên sau va chạm không
  TRANG_THAI_XAC_NHAN_TE_NGA        // Đủ bằng chứng: xác nhận đã té ngã
};

const char* tenTrangThai(DetectionState s) {
  switch (s) {
    case TRANG_THAI_DANG_HIEU_CHUAN:     return "TRANG_THAI_DANG_HIEU_CHUAN";
    case TRANG_THAI_BINH_THUONG:         return "TRANG_THAI_BINH_THUONG";
    case TRANG_THAI_NGHI_ROI:            return "TRANG_THAI_NGHI_ROI";
    case TRANG_THAI_PHAT_HIEN_VA_CHAM:   return "TRANG_THAI_PHAT_HIEN_VA_CHAM";
    case TRANG_THAI_THEO_DOI_SAU_VA_CHAM: return "TRANG_THAI_THEO_DOI_SAU_VA_CHAM";
    case TRANG_THAI_XAC_NHAN_TE_NGA:     return "TRANG_THAI_XAC_NHAN_TE_NGA";
    default:                             return "KHONG_XAC_DINH";
  }
}

// ============================================================================
// 4. BIẾN TOÀN CỤC VÀ DỮ LIỆU CẢM BIẾN
// ============================================================================
FallProfile cauHinhNgua;
DetectionState trangThaiHienTai = TRANG_THAI_DANG_HIEU_CHUAN;
bool cheDoCsvRaw = (SERIAL_RAW_MODE != 0);

// Địa chỉ I2C đang hoạt động
uint8_t mpuAddress = 0;
uint8_t baroAddress = 0;
bool mpuSanSang = false;
bool khiApSanSang = false;

// Hiệu chuẩn MPU
double saiSoVanTocGocX = 0.0, saiSoVanTocGocY = 0.0, saiSoVanTocGocZ = 0.0;
bool giaTocBiBaoHoa = false;
bool vanTocGocBiBaoHoa = false;

// Hiệu chuẩn Barometer
uint16_t heSoHieuChuanMs[8] = {};  // PROM calibration coefficients C1..C6 (C0, C7 CRC)
double apSuatGocPa = 0.0;
bool apSuatGocHopLe = false;

// Đọc MS5611 không chặn (Non-blocking State Machine)
enum BaroState { BARO_IDLE, BARO_CONV_D1, BARO_CONV_D2 };
BaroState baroState = BARO_IDLE;
uint32_t henGioKhiApUs = 0;
uint32_t chuKyKhiApTruocUs = 0;
uint32_t giaTriThoD1 = 0, giaTriThoD2 = 0;

// Dữ liệu đo tức thời
SensorSnapshot mauCamBien;

// Biến theo dõi của thuật toán
uint32_t thoiDiemMauTruocMs = 0;
uint32_t thoiDiemBatDauRoiMs = 0;
uint32_t thoiDiemVaChamMs = 0;
float dinhVaChamMs2 = 0.0f;
uint32_t thoiDiemBatDauYenMs = 0;
uint16_t soMauDungYen = 0;
uint32_t thoiDiemXacNhanBaoDongMs = 0;
bool coBangChungXoayNgua = false;
bool coBangChungGiamDoCao = false;
const char* nhanSuKienCho = "NONE";

// Bộ đệm lịch sử áp suất để tính delta (cửa sổ trượt ~5000 ms)
constexpr size_t PRESSURE_HIST_CAP = 160;
struct PressureHistEntry {
  uint32_t tMs;
  double pPa;
};
PressureHistEntry pressureHist[PRESSURE_HIST_CAP];
size_t pressureHistHead = 0;
size_t pressureHistCount = 0;

// Bộ lọc IIR cho nhánh tư thế
double filtAx = 0.0, filtAy = 0.0, filtAz = 9.81;

// Bộ đếm chu kỳ hiển thị Human Log
uint32_t lastHumanLogMs = 0;
uint32_t lastImuReadUs = 0;
uint32_t soMauImu = 0;

// ============================================================================
// 5. DRIVER GIAO TIẾP MỨC THANH GHI I2C (REGISTER LEVEL DRIVERS)
// ============================================================================

bool ghiI2cMotByte(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

bool docI2cNhieuByte(uint8_t addr, uint8_t reg, uint8_t* buffer, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  size_t read = Wire.requestFrom(static_cast<uint16_t>(addr), static_cast<uint8_t>(len));
  if (read != len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

// ----------------------------------------------------------------------------
// MPU9250 Driver
// ----------------------------------------------------------------------------
bool khoiTaoMpu9250() {
  // Thử địa chỉ 0x68 trước, nếu không được thử 0x69
  uint8_t testAddrs[] = {MPU9250_I2C_ADDR_PRIMARY, MPU9250_I2C_ADDR_ALT};
  uint8_t foundAddr = 0;
  uint8_t whoAmI = 0;

  for (uint8_t addr : testAddrs) {
    if (docI2cNhieuByte(addr, MPU_REG_WHO_AM_I, &whoAmI, 1)) {
      if (whoAmI == 0x71 || whoAmI == 0x73) {
        foundAddr = addr;
        break;
      }
    }
  }

  if (foundAddr == 0) {
    Serial.printf("[ERROR] MPU9250 not found! (WHO_AM_I read: 0x%02X)\n", whoAmI);
    return false;
  }

  mpuAddress = foundAddr;

  // 1. Reset chip & đánh thức (PWR_MGMT_1 = 0x80 rồi 0x01 để chọn xung PLL)
  ghiI2cMotByte(mpuAddress, MPU_REG_PWR_MGMT_1, 0x80);
  delay(100);
  ghiI2cMotByte(mpuAddress, MPU_REG_PWR_MGMT_1, 0x01); // Auto-select best clock (PLL)
  delay(10);

  // 2. Kích hoạt toàn bộ trục Accel & Gyro
  ghiI2cMotByte(mpuAddress, MPU_REG_PWR_MGMT_2, 0x00);

  // 3. Cấu hình tốc độ lấy mẫu nội bộ: 1 kHz / (1 + 9) = 100 Hz
  ghiI2cMotByte(mpuAddress, MPU_REG_SMPLRT_DIV, 0x09);

  // 4. Cấu hình DLPF 184 Hz cho Gyro
  ghiI2cMotByte(mpuAddress, MPU_REG_CONFIG, 0x01);

  // 5. Cấu hình Full Scale Gyro: ±2000 dps (FS_SEL = 3 -> 0x18)
  ghiI2cMotByte(mpuAddress, MPU_REG_GYRO_CONFIG, 0x18);

  // 6. Cấu hình Full Scale Accel: ±16 g (AFS_SEL = 3 -> 0x18)
  ghiI2cMotByte(mpuAddress, MPU_REG_ACCEL_CONFIG, 0x18);

  // 7. Cấu hình DLPF 184 Hz cho Accel (ACCEL_CONFIG_2)
  ghiI2cMotByte(mpuAddress, MPU_REG_ACCEL_CONFIG2, 0x01);

  // 8. Tắt khối I2C Master nội bộ (không kết nối từ kế AK8963 để tránh nghẽn bus)
  ghiI2cMotByte(mpuAddress, MPU_REG_USER_CTRL, 0x00);
  ghiI2cMotByte(mpuAddress, MPU_REG_INT_PIN_CFG, 0x02); // BYPASS_EN

  Serial.printf("[OK] MPU9250 detected at 0x%02X (WHO_AM_I=0x%02X | AFS=±16g, GFS=±2000dps)\n",
                mpuAddress, whoAmI);
  mpuSanSang = true;
  return true;
}

bool docMpu9250(SensorSnapshot& s) {
  if (!mpuSanSang) return false;

  uint8_t buf[14];
  if (!docI2cNhieuByte(mpuAddress, MPU_REG_ACCEL_XOUT_H, buf, 14)) {
    return false;
  }

  int16_t rawAx = (int16_t)((buf[0] << 8) | buf[1]);
  int16_t rawAy = (int16_t)((buf[2] << 8) | buf[3]);
  int16_t rawAz = (int16_t)((buf[4] << 8) | buf[5]);
  // buf[6], buf[7] là nhiệt độ nội bộ IMU (không dùng)
  int16_t rawGx = (int16_t)((buf[8] << 8) | buf[9]);
  int16_t rawGy = (int16_t)((buf[10] << 8) | buf[11]);
  int16_t rawGz = (int16_t)((buf[12] << 8) | buf[13]);

  // Kiểm tra cờ bão hòa cảm biến (gần chạm giới hạn int16 ±32768)
  giaTocBiBaoHoa = (abs(rawAx) >= 32750 || abs(rawAy) >= 32750 || abs(rawAz) >= 32750);
  vanTocGocBiBaoHoa = (abs(rawGx) >= 32750 || abs(rawGy) >= 32750 || abs(rawGz) >= 32750);

  // Chuyển đổi đơn vị chuẩn SI
  s.ax = (double)rawAx * ACCEL_SCALE_16G;
  s.ay = (double)rawAy * ACCEL_SCALE_16G;
  s.az = (double)rawAz * ACCEL_SCALE_16G;

  s.gx = ((double)rawGx * GYRO_SCALE_2000DPS) - saiSoVanTocGocX;
  s.gy = ((double)rawGy * GYRO_SCALE_2000DPS) - saiSoVanTocGocY;
  s.gz = ((double)rawGz * GYRO_SCALE_2000DPS) - saiSoVanTocGocZ;

  // [KHÁI NIỆM] Gia tốc là gì?
  // Gia tốc cho biết thiết bị "đang chuyển động mạnh/chậm thế nào". Cảm biến
  // gia tốc đo cả trọng lực Trái Đất, nên:
  //   - Đặt yên trên bàn -> tổng gia tốc ~ 1g (9.81 m/s^2), vì chỉ có trọng lực kéo.
  //   - Rơi tự do        -> tổng gia tốc tụt xuống rất nhỏ (gần 0), vì "rơi cùng" trọng lực.
  //   - Vừa va đập       -> tổng gia tốc vọt lên rất mạnh (lớn hơn 2.5g).
  //   - Đi lại bình thường -> số đo dao động nhẹ quanh 1g.
  //
  // Cảm biến đo theo 3 trục (ax, ay, az). Công thức dưới đây tính "độ dài vectơ
  // gia tốc" tongGiaToc = sqrt(ax^2 + ay^2 + az^2). Nhờ vậy ta luôn biết độ LỚN
  // của gia tốc mà không cần quan tâm thiết bị đang nằm nghiêng hay úp ngược.
  s.tongGiaToc = sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);

  // [KHÁI NIỆM] Vận tốc góc là gì?
  // Vận tốc góc cho biết thiết bị "đang xoay nhanh thế nào", đơn vị độ/giây (dps).
  //   - Đứng yên        -> ~ 0 dps.
  //   - Nghiêng người, lật thiết bị, xoay điện thoại -> vài chục đến vài trăm dps.
  // Cũng tính độ lớn tổng từ 3 trục bằng cùng công thức căn bậc hai.
  s.tongVanTocGoc = sqrt(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);

  // Bộ lọc IIR cho tư thế tĩnh (alpha = 0.1)
  filtAx = 0.9 * filtAx + 0.1 * s.ax;
  filtAy = 0.9 * filtAy + 0.1 * s.ay;
  filtAz = 0.9 * filtAz + 0.1 * s.az;

  // Nhận diện tĩnh: gia tốc gần 9.81 và tốc độ góc nhỏ
  double filtMag = sqrt(filtAx * filtAx + filtAy * filtAy + filtAz * filtAz);
  // CHỈ dùng cho log/quan sát, KHÔNG dùng cho quyết định ngã (Android không dùng gyro trong quyết định)
  s.dangDungYen = (fabs(filtMag - cauHinhNgua.stillnessTargetAccelerationMs2) <= cauHinhNgua.stillnessToleranceMs2) &&
                 (s.tongVanTocGoc < 25.0);

  return true;
}

// ----------------------------------------------------------------------------
// MS5611 (GY-63) Driver
// ----------------------------------------------------------------------------
uint8_t tinhCrc4Ms5611(const uint16_t* prom) {
  uint16_t rem = 0;
  for (uint8_t i = 0; i < 16; ++i) {
    if ((i % 2) == 1) {
      rem ^= (prom[i >> 1] & 0x00FF);
    } else {
      rem ^= (prom[i >> 1] >> 8);
    }
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (rem & 0x8000) {
        rem = (rem << 1) ^ 0x3000;
      } else {
        rem = (rem << 1);
      }
    }
  }
  return static_cast<uint8_t>((rem >> 12) & 0x000F);
}

bool khoiTaoMs5611() {
  uint8_t testAddrs[] = {MS5611_I2C_ADDR_PRIMARY, MS5611_I2C_ADDR_ALT};
  uint8_t foundAddr = 0;

  for (uint8_t addr : testAddrs) {
    Wire.beginTransmission(addr);
    Wire.write(MS5611_CMD_PROM_BASE);
    if (Wire.endTransmission() == 0) {
      foundAddr = addr;
      break;
    }
  }

  if (foundAddr == 0) {
    Serial.println("[WARN] GY-63 (MS5611) not responding! Continuing in IMU-only mode.");
    return false;
  }

  baroAddress = foundAddr;

  // Gửi lệnh Reset
  Wire.beginTransmission(baroAddress);
  Wire.write(MS5611_CMD_RESET);
  Wire.endTransmission();
  delay(10); // Chờ nạp PROM

  // Đọc 8 từ PROM (16 byte)
  for (uint8_t i = 0; i < 8; ++i) {
    Wire.beginTransmission(baroAddress);
    Wire.write(MS5611_CMD_PROM_BASE + (i * 2));
    if (Wire.endTransmission() != 0) {
      Serial.println("[ERROR] Failed to read MS5611 PROM!");
      return false;
    }
    Wire.requestFrom(static_cast<uint16_t>(baroAddress), static_cast<uint8_t>(2));
    if (Wire.available() >= 2) {
      heSoHieuChuanMs[i] = (Wire.read() << 8) | Wire.read();
    }
  }

  // Kiểm tra CRC-4 (AN520)
  uint16_t promForCrc[8];
  for (int i = 0; i < 8; ++i) promForCrc[i] = heSoHieuChuanMs[i];
  promForCrc[7] &= 0xFF00; // Xóa nibble CRC
  uint8_t calculatedCrc = tinhCrc4Ms5611(promForCrc);
  uint8_t storedCrc = heSoHieuChuanMs[7] & 0x000F;

  if (calculatedCrc != storedCrc) {
    Serial.printf("[WARN] MS5611 CRC mismatch! calc=%u, stored=%u\n", calculatedCrc, storedCrc);
  }

  Serial.printf("[OK] GY-63 (MS5611) detected at 0x%02X (CRC OK: %s | C1=%u, C2=%u)\n",
                baroAddress, (calculatedCrc == storedCrc ? "YES" : "NO"), heSoHieuChuanMs[1], heSoHieuChuanMs[2]);
  khiApSanSang = true;
  return true;
}

// Máy trạng thái đọc MS5611 không chặn
void docMs5611KhongChan(uint32_t nowUs) {
  if (!khiApSanSang) return;

  switch (baroState) {
    case BARO_IDLE:
      // Mỗi 40 ms (25 Hz) bắt đầu một chu kỳ đo mới
      if ((nowUs - chuKyKhiApTruocUs) >= 40000) {
        chuKyKhiApTruocUs = nowUs;
        Wire.beginTransmission(baroAddress);
        Wire.write(MS5611_CMD_CONV_D1); // Gửi lệnh chuyển đổi D1 (áp suất)
        if (Wire.endTransmission() == 0) {
          henGioKhiApUs = nowUs;
          baroState = BARO_CONV_D1;
        }
      }
      break;

    case BARO_CONV_D1:
      // Chờ chuyển đổi D1 hoàn tất (~9.2 ms)
      if ((nowUs - henGioKhiApUs) >= MS5611_CONV_DELAY_US) {
        Wire.beginTransmission(baroAddress);
        Wire.write(MS5611_CMD_ADC_READ);
        if (Wire.endTransmission() == 0) {
          Wire.requestFrom(static_cast<uint16_t>(baroAddress), static_cast<uint8_t>(3));
          if (Wire.available() >= 3) {
            giaTriThoD1 = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
          }
        }
        // Gửi tiếp lệnh chuyển đổi D2 (nhiệt độ)
        Wire.beginTransmission(baroAddress);
        Wire.write(MS5611_CMD_CONV_D2);
        if (Wire.endTransmission() == 0) {
          henGioKhiApUs = nowUs;
          baroState = BARO_CONV_D2;
        } else {
          baroState = BARO_IDLE;
        }
      }
      break;

    case BARO_CONV_D2:
      // Chờ chuyển đổi D2 hoàn tất (~9.2 ms)
      if ((nowUs - henGioKhiApUs) >= MS5611_CONV_DELAY_US) {
        Wire.beginTransmission(baroAddress);
        Wire.write(MS5611_CMD_ADC_READ);
        if (Wire.endTransmission() == 0) {
          Wire.requestFrom(static_cast<uint16_t>(baroAddress), static_cast<uint8_t>(3));
          if (Wire.available() >= 3) {
            giaTriThoD2 = (Wire.read() << 16) | (Wire.read() << 8) | Wire.read();
          }
        }

        // Bù nhiệt độ bậc 2 (2nd-order temperature compensation chuẩn AN520)
        double c1 = heSoHieuChuanMs[1];
        double c2 = heSoHieuChuanMs[2];
        double c3 = heSoHieuChuanMs[3];
        double c4 = heSoHieuChuanMs[4];
        double c5 = heSoHieuChuanMs[5];
        double c6 = heSoHieuChuanMs[6];

        double dt = (double)giaTriThoD2 - c5 * 256.0;
        double temp100 = 2000.0 + dt * c6 / 8388608.0;
        double off = c2 * 65536.0 + c4 * dt / 128.0;
        double sens = c1 * 32768.0 + c3 * dt / 256.0;

        if (temp100 < 2000.0) {
          double t2 = dt * dt * 4.6566128731e-10;
          double t = (temp100 - 2000.0) * (temp100 - 2000.0);
          double off2 = 2.5 * t;
          double sens2 = 1.25 * t;
          if (temp100 < -1500.0) {
            double tLow = (temp100 + 1500.0) * (temp100 + 1500.0);
            off2 += 7.0 * tLow;
            sens2 += 5.5 * tLow;
          }
          temp100 -= t2;
          off -= off2;
          sens -= sens2;
        }

        // [KHÁI NIỆM] Áp suất là "sức nặng" của lớp không khí phía trên đè xuống.
        //   - Lên cao (đi thang máy, leo núi) -> áp suất GIẢM.
        //   - Xuống thấp                     -> áp suất TĂNG.
        // Cảm biến GY63 (MS5611) đo áp suất rồi nhờ đó ước lượng độ cao. Lưu ý:
        // áp suất MỘT MÌNH không đủ để xác nhận té ngã, vì thời tiết và gió cũng
        // làm áp suất thay đổi. Nó chỉ đóng vai trò "bằng chứng phụ" hỗ trợ.
        double pPa = ((double)giaTriThoD1 * sens / 2097152.0 - off) / 32768.0;
        double tC = temp100 * 0.01;

        if (pPa >= 30000.0 && pPa <= 110000.0) {
          mauCamBien.apSuatPa = pPa;
          mauCamBien.temperatureC = tC;

          // Cập nhật độ cao tương đối nếu baseline đã có.
          // Công thức khí áp cao độ (barometric formula) là công thức CHUẨN QUỐC TẾ
          // đã được kiểm chứng: độ cao = 44330.77 * (1 - (P/P0)^0.190263), trong đó
          // P0 là áp suất lúc hiệu chuẩn (apSuatGocPa), P là áp suất đo hiện tại.
          if (apSuatGocHopLe && apSuatGocPa > 0.0) {
            mauCamBien.chenhLechDoCaoM = HYPSOMETRIC_SCALE * (1.0 - pow(pPa / apSuatGocPa, HYPSOMETRIC_EXPONENT));
          }

          // Cập nhật bộ đệm lịch sử áp suất
          pressureHist[pressureHistHead].tMs = millis();
          pressureHist[pressureHistHead].pPa = pPa;
          pressureHistHead = (pressureHistHead + 1) % PRESSURE_HIST_CAP;
          if (pressureHistCount < PRESSURE_HIST_CAP) {
            pressureHistCount++;
          }
        }

        baroState = BARO_IDLE;
      }
      break;
  }
}

// ============================================================================
// 6. THUẬT TOÁN ĐỐI CHỨNG VÀ ĐÁNH GIÁ ĐIỂM NGUY CƠ (RISK SCORE)
// ============================================================================
// [VÌ SAO KẾT HỢP NHIỀU CẢM BIẾN?]
// Mỗi cảm biến đều có "điểm mù" - tình huống nó dễ báo nhầm:
//   - Gia tốc kế: nhảy mạnh hay vỗ tay cũng tạo va đập, dễ tưởng là ngã.
//   - Con quay hồi chuyển: chỉ biết "đang xoay", không biết người có nằm xuống không.
//   - Khí áp kế: thời tiết và gió cũng đổi áp suất, không thể dùng một mình.
// Kết hợp nhiều cảm biến giúp GIẢM BÁO SAI: một cú ngã thật thường có đủ cả ba
// dấu hiệu (va đập mạnh + xoay người + đổi độ cao), nên càng nhiều bằng chứng
// trùng khớp thì càng chắc chắn.

double tinhDoTangApSuat(uint32_t windowMs) {
  if (pressureHistCount < 2) return 0.0;
  uint32_t now = millis();
  double newestP = mauCamBien.apSuatPa;
  double oldestP = newestP;
  bool found = false;

  for (size_t i = 0; i < pressureHistCount; ++i) {
    size_t idx = (pressureHistHead + PRESSURE_HIST_CAP - 1 - i) % PRESSURE_HIST_CAP;
    if ((now - pressureHist[idx].tMs) <= windowMs) {
      oldestP = pressureHist[idx].pPa;
      found = true;
    } else {
      break;
    }
  }
  return found ? (newestP - oldestP) : 0.0;
}

int tinhDiemNguyCoTeNga() {
  switch (trangThaiHienTai) {
    case TRANG_THAI_DANG_HIEU_CHUAN:
      return 0;
    case TRANG_THAI_BINH_THUONG: {
      // Điểm nền dao động theo mức độ chuyển động hiện tại (0 - 25%)
      int score = 5;
      if (mauCamBien.tongGiaToc > 15.0) score += 10;
      if (mauCamBien.tongVanTocGoc > 80.0) score += 10;
      return score;
    }
    case TRANG_THAI_NGHI_ROI:
      return 45;
    case TRANG_THAI_PHAT_HIEN_VA_CHAM:
      return 75;
    case TRANG_THAI_THEO_DOI_SAU_VA_CHAM: {
      // Tiến trình tích lũy thời gian tĩnh (70 - 95%)
      int progress = (thoiDiemBatDauYenMs > 0) ? ((millis() - thoiDiemBatDauYenMs) * 25 / cauHinhNgua.postImpactStillnessDurationMs) : 0;
      if (progress > 25) progress = 25;
      return 70 + progress;
    }
    case TRANG_THAI_XAC_NHAN_TE_NGA:
      return 100;
    default:
      return 0;
  }
}

// ============================================================================
// 7. XỬ LÝ CHUYỂN TRẠNG THÁI THUẬT TOÁN (ALGORITHM ENGINE)
// ============================================================================
// [KHÁI NIỆM] Trong `void xuLyPhatHienTeNga(const SensorSnapshot& s)`:
//   - dấu `&` nghĩa là "truyền thẳng địa chỉ của biến gốc" thay vì sao chép cả
//     "hộp" SensorSnapshot sang bản mới. Làm vậy KHÔNG tốn bộ nhớ.
//   - chữ `const` nghĩa là "hứa sẽ không sửa" biến đó bên trong hàm.
// Tóm lại: hàm chỉ ĐỌC dữ liệu cảm biến, không vô tình làm hỏng nó.

void xuLyPhatHienTeNga(const SensorSnapshot& s) {
  uint32_t now = s.timestampMs;

  // Watchdog kiểm tra gián đoạn mẫu
  if (thoiDiemMauTruocMs > 0) {
    uint32_t gap = now - thoiDiemMauTruocMs;
    if (gap > cauHinhNgua.maximumSampleGapMs) {
      if (trangThaiHienTai == TRANG_THAI_PHAT_HIEN_VA_CHAM || trangThaiHienTai == TRANG_THAI_THEO_DOI_SAU_VA_CHAM) {
        if (!cheDoCsvRaw) {
          Serial.printf("[WARN] Sample gap (%u ms > %u ms) aborted impact episode!\n",
                        gap, cauHinhNgua.maximumSampleGapMs);
        }
        trangThaiHienTai = TRANG_THAI_BINH_THUONG;
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        thoiDiemBatDauRoiMs = 0;
        coBangChungXoayNgua = false;
        coBangChungGiamDoCao = false;
        nhanSuKienCho = "SAMPLE_GAP_RESET";
      } else {
        // FIX 3: Với NORMAL / POSSIBLE_FREE_FALL chỉ xóa mốc free-fall, giữ nguyên trạng thái
        thoiDiemBatDauRoiMs = 0;
        if (!cheDoCsvRaw) {
          Serial.printf("[WARN] Sample gap (%u ms > %u ms threshold)\n",
                        gap, cauHinhNgua.maximumSampleGapMs);
        }
      }
    } else if (gap > cauHinhNgua.sampleWatchdogMs) {
      if (!cheDoCsvRaw) {
        Serial.printf("[WARN] High sample jitter: %u ms\n", gap);
      }
    }
  }
  thoiDiemMauTruocMs = now;

  switch (trangThaiHienTai) {
    case TRANG_THAI_BINH_THUONG: {
      // Nhánh kiểm tra rơi tự do (bằng chứng phụ)
      if (s.tongGiaToc < cauHinhNgua.freeFallThresholdMs2) {
        if (thoiDiemBatDauRoiMs == 0) {
          thoiDiemBatDauRoiMs = now;
        } else if ((now - thoiDiemBatDauRoiMs) >= cauHinhNgua.freeFallMinDurationMs) {
          trangThaiHienTai = TRANG_THAI_NGHI_ROI;
          nhanSuKienCho = "FREE_FALL";
          if (!cheDoCsvRaw) {
            Serial.println(">>> PHAT HIEN DAU HIEU ROI TU DO");
            Serial.printf("[EVENT] FREE FALL detected (Acc=%.2fg | %.2f m/s^2, dur=%u ms)\n",
                          s.tongGiaToc / 9.81, s.tongGiaToc, (now - thoiDiemBatDauRoiMs));
          }
        }
      } else {
        thoiDiemBatDauRoiMs = 0;
      }

      // Nhánh kiểm tra va đập mạnh (luật cốt lõi bám sát Android)
      if (s.tongGiaToc >= cauHinhNgua.impactAccelerationMs2) {
        trangThaiHienTai = TRANG_THAI_PHAT_HIEN_VA_CHAM;
        thoiDiemVaChamMs = now;
        dinhVaChamMs2 = s.tongGiaToc;
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        coBangChungXoayNgua = (s.tongVanTocGoc >= cauHinhNgua.gyroTurnThresholdDps);
        coBangChungGiamDoCao = false;
        nhanSuKienCho = "IMPACT";
        if (!cheDoCsvRaw) {
          Serial.println(">>> PHAT HIEN VA CHAM");
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg)\n", s.tongGiaToc, s.tongGiaToc / 9.81);
        }
      }
      break;
    }

    case TRANG_THAI_NGHI_ROI: {
      // Nếu có va đập ngay sau rơi tự do
      if (s.tongGiaToc >= cauHinhNgua.impactAccelerationMs2) {
        trangThaiHienTai = TRANG_THAI_PHAT_HIEN_VA_CHAM;
        thoiDiemVaChamMs = now;
        dinhVaChamMs2 = s.tongGiaToc;
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        coBangChungXoayNgua = (s.tongVanTocGoc >= cauHinhNgua.gyroTurnThresholdDps);
        coBangChungGiamDoCao = false;
        nhanSuKienCho = "IMPACT";
        if (!cheDoCsvRaw) {
          Serial.println(">>> PHAT HIEN VA CHAM");
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg) [after Free-Fall]\n",
                        s.tongGiaToc, s.tongGiaToc / 9.81);
        }
      } else if ((now - thoiDiemBatDauRoiMs) > 600) {
        // Rơi tự do quá 600 ms mà không va đập -> kết thúc, về NORMAL
        trangThaiHienTai = TRANG_THAI_BINH_THUONG;
        thoiDiemBatDauRoiMs = 0;
      }
      break;
    }

    case TRANG_THAI_PHAT_HIEN_VA_CHAM: {
      // Ghi nhận đỉnh va đập tức thời cao nhất
      if (s.tongGiaToc > dinhVaChamMs2) {
        dinhVaChamMs2 = s.tongGiaToc;
      }
      if (s.tongVanTocGoc >= cauHinhNgua.gyroTurnThresholdDps) {
        coBangChungXoayNgua = true;
      }
      // Chuyển ngay sang bước theo dõi bất động sau va chạm
      trangThaiHienTai = TRANG_THAI_THEO_DOI_SAU_VA_CHAM;
      break;
    }

    case TRANG_THAI_THEO_DOI_SAU_VA_CHAM: {
      // FIX 4: Parity Android — Re-latch impact nếu có mẫu mới |a| >= impactThreshold
      if (s.tongGiaToc >= cauHinhNgua.impactAccelerationMs2) {
        thoiDiemVaChamMs = now;
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        if (s.tongGiaToc > dinhVaChamMs2) {
          dinhVaChamMs2 = s.tongGiaToc;
        }
        if (s.tongVanTocGoc >= cauHinhNgua.gyroTurnThresholdDps) {
          coBangChungXoayNgua = true;
        }
        nhanSuKienCho = "IMPACT";
        if (!cheDoCsvRaw) {
          Serial.println(">>> PHAT HIEN VA CHAM");
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg) [re-latched]\n",
                        s.tongGiaToc, s.tongGiaToc / 9.81);
        }
        break;
      }

      // Tiếp tục cập nhật đỉnh va đập nếu xung lực còn dội lại
      if (s.tongGiaToc > dinhVaChamMs2) {
        dinhVaChamMs2 = s.tongGiaToc;
      }

      // Kiểm tra hết hạn cửa sổ sau va đập (3000 ms)
      if ((now - thoiDiemVaChamMs) > cauHinhNgua.postImpactWindowMs) {
        trangThaiHienTai = TRANG_THAI_BINH_THUONG;
        nhanSuKienCho = "WINDOW_EXPIRED";
        if (!cheDoCsvRaw) {
          Serial.printf("[EVENT] POST-IMPACT window expired (%u ms), false alarm\n",
                        cauHinhNgua.postImpactWindowMs);
        }
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        coBangChungXoayNgua = false;
        coBangChungGiamDoCao = false;
        break;
      }

      // Kiểm tra điều kiện bất động (|magnitude - 9.81| <= 1.0)
      bool isStill = (fabs(s.tongGiaToc - cauHinhNgua.stillnessTargetAccelerationMs2) <= cauHinhNgua.stillnessToleranceMs2);

      if (!isStill) {
        // Bị gián đoạn rung lắc -> reset thời lượng tĩnh
        if (thoiDiemBatDauYenMs > 0 && !cheDoCsvRaw) {
          // Serial.printf("[DEBUG] Stillness interrupted at %u ms\n", now - thoiDiemBatDauYenMs);
        }
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
      } else {
        if (thoiDiemBatDauYenMs == 0) {
          thoiDiemBatDauYenMs = now;
          nhanSuKienCho = "POST_IMPACT_START";
          if (!cheDoCsvRaw) {
            Serial.println(">>> DANG THEO DOI SAU VA CHAM");
            Serial.println("[EVENT] POST-IMPACT low motion started");
          }
        }
        soMauDungYen++;

        uint32_t quietDuration = now - thoiDiemBatDauYenMs;

        // XÁC NHẬN NGÃ KHI ĐẠT TIÊU CHÍ (Parity Android: >= 6 mẫu & >= 1000 ms)
        if ((soMauDungYen >= cauHinhNgua.minimumStillnessSamples) &&
            (quietDuration >= cauHinhNgua.postImpactStillnessDurationMs)) {
          trangThaiHienTai = TRANG_THAI_XAC_NHAN_TE_NGA;
          thoiDiemXacNhanBaoDongMs = now;
          nhanSuKienCho = "FALL_CONFIRMED";

          // FIX 6: Tính toán bằng chứng khí áp đối chứng theo cửa sổ pressureWindowMs (5000 ms)
          double deltaP = tinhDoTangApSuat(cauHinhNgua.pressureWindowMs);
          double deltaH = s.chenhLechDoCaoM;

          // FIX 5: Đánh dấu cờ bằng chứng sụt độ cao (KHÔNG tham gia điều kiện if xác nhận ngã)
          coBangChungGiamDoCao = (deltaH <= cauHinhNgua.altitudeDropMinM);

          if (!cheDoCsvRaw) {
            Serial.println(">>> CANH BAO: CO THE DA XAY RA TE NGA");
            Serial.println("================================================================================");
            Serial.printf("[ALERT] FALL CONFIRMED (impact=%.2f m/s^2 | %.2fg, stillness=%u ms, deltaP=%+.1f Pa, deltaH=%+.2f m, gyro=%s, dH=%s)\n",
                          dinhVaChamMs2, dinhVaChamMs2 / 9.81, quietDuration, deltaP, deltaH,
                          (coBangChungXoayNgua ? "YES" : "NO"),
                          (coBangChungGiamDoCao ? "YES" : "NO"));
            if (deltaP >= cauHinhNgua.pressureEvidenceMinRisePa) {
              Serial.printf("        [CORROBORATED] Barometric pressure rise %+.1f Pa >= %.1f Pa threshold\n",
                            deltaP, cauHinhNgua.pressureEvidenceMinRisePa);
            }
            if (coBangChungXoayNgua) {
              Serial.printf("        [CORROBORATED] Gyro turn rate >= %.1f dps threshold\n",
                            cauHinhNgua.gyroTurnThresholdDps);
            }
            if (coBangChungGiamDoCao) {
              Serial.printf("        [CORROBORATED] Altitude drop %+.2f m <= %.2f m threshold\n",
                            deltaH, cauHinhNgua.altitudeDropMinM);
            }
            Serial.println("================================================================================");
          }
        }
      }
      break;
    }

    case TRANG_THAI_XAC_NHAN_TE_NGA: {
      // Giữ cảnh báo trong 3 giây debounce rồi tự động quay về NORMAL
      if ((now - thoiDiemXacNhanBaoDongMs) > 3000) {
        trangThaiHienTai = TRANG_THAI_BINH_THUONG;
        thoiDiemBatDauYenMs = 0;
        soMauDungYen = 0;
        thoiDiemBatDauRoiMs = 0;
        coBangChungXoayNgua = false;
        coBangChungGiamDoCao = false;
        nhanSuKienCho = "ALERT_HOLD_END";
        if (!cheDoCsvRaw) {
          Serial.println("[STATE] Monitoring resumed (NORMAL)");
        }
      }
      break;
    }

    default:
      break;
  }
}

// ============================================================================
// 8. LOGGING VÀ XUẤT SERIAL (HUMAN & CSV RAW MODES)
// ============================================================================

void inTieuDeCsv() {
  Serial.println("timestamp_ms,ax_ms2,ay_ms2,az_ms2,acc_mag,gx_dps,gy_dps,gz_dps,gyro_mag,p_pa,temp_c,alt_m,state,event");
}

void inDongCsv(const SensorSnapshot& s, const char* eventTag) {
  Serial.printf("%lu,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f,%s,%s\n",
                s.timestampMs, s.ax, s.ay, s.az, s.tongGiaToc,
                s.gx, s.gy, s.gz, s.tongVanTocGoc,
                s.apSuatPa, s.temperatureC, s.chenhLechDoCaoM,
                tenTrangThai(trangThaiHienTai), eventTag);
}

void inThongTinDeDoc(const SensorSnapshot& s) {
  // Khối dữ liệu thân thiện cho con người đọc (đơn vị quen thuộc):
  Serial.println("=== DU LIEU CAM BIEN ===");
  Serial.printf("Gia toc tong : %.2f g\n", s.tongGiaToc / 9.81);
  Serial.printf("Van toc goc  : %.1f do/s\n", s.tongVanTocGoc);
  Serial.printf("Ap suat      : %.2f hPa\n", s.apSuatPa / 100.0);
  Serial.printf("Chenh cao    : %+.2f m\n", s.chenhLechDoCaoM);
  Serial.printf("Trang thai   : %s\n", tenTrangThai(trangThaiHienTai));

  // Các dòng chẩn đoán gốc (giữ để kiểm tra kỹ thuật):
  Serial.printf("[OK] MPU9250 | Acc=%.2fg%s | Gyro=%.0f°/s%s | Still=%s\n",
                s.tongGiaToc / 9.81, (giaTocBiBaoHoa ? " [SAT!]" : ""),
                s.tongVanTocGoc, (vanTocGocBiBaoHoa ? " [SAT!]" : ""),
                (s.dangDungYen ? "YES" : "NO"));

  if (khiApSanSang) {
    Serial.printf("[OK] GY63   | P=%.2f hPa | ΔH=%+.2f m\n",
                  s.apSuatPa / 100.0, s.chenhLechDoCaoM);
  } else {
    Serial.println("[--] GY63   | UNAVAILABLE");
  }

  Serial.printf("[STATE] %s\n", tenTrangThai(trangThaiHienTai));
  Serial.printf("[RISK] Fall score: %d%%\n", tinhDiemNguyCoTeNga());
}

void inBangNguong() {
  Serial.println("\n--- BẢNG THAM SỐ CẤU HÌNH THỬ NGHIỆM (FALL PROFILE) ---");
  Serial.printf("Profile ID                     : %s\n", PROFILE_NAME);
  Serial.printf("impactAccelerationMs2          : %.2f m/s^2 (%.2fg) [Nguồn: Android FallDetectionProfiles.kt]\n",
                cauHinhNgua.impactAccelerationMs2, cauHinhNgua.impactAccelerationMs2 / 9.81);
  Serial.printf("stillnessTargetAccelerationMs2 : %.2f m/s^2 [Nguồn: Android]\n", cauHinhNgua.stillnessTargetAccelerationMs2);
  Serial.printf("stillnessToleranceMs2          : %.2f m/s^2 [Nguồn: Android]\n", cauHinhNgua.stillnessToleranceMs2);
  Serial.printf("postImpactWindowMs             : %u ms [Nguồn: Android]\n", cauHinhNgua.postImpactWindowMs);
  Serial.printf("postImpactStillnessDurationMs  : %u ms [Nguồn: Android]\n", cauHinhNgua.postImpactStillnessDurationMs);
  Serial.printf("minimumStillnessSamples        : %u mẫu [Nguồn: Android]\n", cauHinhNgua.minimumStillnessSamples);
  Serial.printf("maximumSampleGapMs             : %u ms [Nguồn: Android]\n", cauHinhNgua.maximumSampleGapMs);
  Serial.printf("freeFallThresholdMs2           : %.2f m/s^2 (%.2fg) [Suy luận kỹ thuật]\n",
                cauHinhNgua.freeFallThresholdMs2, cauHinhNgua.freeFallThresholdMs2 / 9.81);
  Serial.printf("freeFallMinDurationMs          : %u ms [Suy luận kỹ thuật]\n", cauHinhNgua.freeFallMinDurationMs);
  Serial.printf("gyroTurnThresholdDps           : %.1f dps [Suy luận kỹ thuật]\n", cauHinhNgua.gyroTurnThresholdDps);
  Serial.printf("pressureEvidenceMinRisePa      : %.1f Pa [Nguồn: Android]\n", cauHinhNgua.pressureEvidenceMinRisePa);
  Serial.printf("pressureWindowMs               : %u ms [Nguồn: Android DemoLogic.kt PRESSURE_WINDOW_NS]\n", cauHinhNgua.pressureWindowMs);
  Serial.printf("altitudeDropMinM               : %.2f m [Suy luận kỹ thuật]\n", cauHinhNgua.altitudeDropMinM);
  Serial.println("----------------------------------------------------------\n");
}

void xuLyLenhSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      cheDoCsvRaw = !cheDoCsvRaw;
      if (cheDoCsvRaw) {
        inTieuDeCsv();
      } else {
        Serial.println("\n[MODE] Switched to HUMAN READABLE mode (115200 baud).");
      }
    } else if (c == 't' || c == 'T') {
      inBangNguong();
    } else if (c == 'c' || c == 'C') {
      Serial.println("\n[CMD] Yêu cầu hiệu chuẩn lại (Recalibrating)...");
      trangThaiHienTai = TRANG_THAI_DANG_HIEU_CHUAN;
      // Kích hoạt hiệu chuẩn
      hieuChuanLucKhoiDong();
    } else if (c == 'h' || c == 'H') {
      Serial.println("\n=== HƯỚNG DẪN LỆNH NỐI TIẾP (SERIAL COMMANDS) ===");
      Serial.println("  'r' : Bật / tắt chế độ CSV RAW (100 Hz)");
      Serial.println("  't' : In bảng cấu hình ngưỡng FallProfile hiện tại");
      Serial.println("  'c' : Thực hiện hiệu chuẩn lại cảm biến lúc đứng yên");
      Serial.println("  'h' : In bảng hướng dẫn này");
      Serial.println("=================================================");
    }
  }
}

// ============================================================================
// 9. QUY TRÌNH KHỞI ĐỘNG VÀ HIỆU CHUẨN (BOOT & CALIBRATION)
// ============================================================================

void hieuChuanLucKhoiDong() {
  trangThaiHienTai = TRANG_THAI_DANG_HIEU_CHUAN;
  Serial.println("\n[CAL] ========================================================");
  Serial.println("[CAL] BẮT ĐẦU HIỆU CHUẨN: Giữ thiết bị đứng yên trong 3 giây...");
  Serial.println("[CAL] ========================================================");

  double sumGx = 0.0, sumGy = 0.0, sumGz = 0.0;
  size_t imuSamples = 0;
  size_t accelOutOfToleranceSamples = 0;

  double pSamples[60];
  size_t pCount = 0;

  uint32_t startCalMs = millis();
  while (millis() - startCalMs < 3000) {
    uint32_t nowUs = micros();
    docMs5611KhongChan(nowUs);

    if (nowUs - lastImuReadUs >= 10000) { // 100 Hz
      lastImuReadUs = nowUs;
      uint8_t buf[14];
      if (docI2cNhieuByte(mpuAddress, MPU_REG_ACCEL_XOUT_H, buf, 14)) {
        int16_t rawAx = (int16_t)((buf[0] << 8) | buf[1]);
        int16_t rawAy = (int16_t)((buf[2] << 8) | buf[3]);
        int16_t rawAz = (int16_t)((buf[4] << 8) | buf[5]);
        int16_t rawGx = (int16_t)((buf[8] << 8) | buf[9]);
        int16_t rawGy = (int16_t)((buf[10] << 8) | buf[11]);
        int16_t rawGz = (int16_t)((buf[12] << 8) | buf[13]);

        // FIX 7: Kiểm tra độ lệch gia tốc so với trọng trường tĩnh chuẩn
        double ax = (double)rawAx * ACCEL_SCALE_16G;
        double ay = (double)rawAy * ACCEL_SCALE_16G;
        double az = (double)rawAz * ACCEL_SCALE_16G;
        double aMag = sqrt(ax * ax + ay * ay + az * az);
        if (fabs(aMag - cauHinhNgua.stillnessTargetAccelerationMs2) > cauHinhNgua.stillnessToleranceMs2) {
          accelOutOfToleranceSamples++;
        }

        sumGx += (double)rawGx * GYRO_SCALE_2000DPS;
        sumGy += (double)rawGy * GYRO_SCALE_2000DPS;
        sumGz += (double)rawGz * GYRO_SCALE_2000DPS;
        imuSamples++;
      }
    }

    if (khiApSanSang && mauCamBien.apSuatPa >= 30000.0 && pCount < 60) {
      // Chỉ lưu nếu mẫu áp suất mới xuất hiện
      if (pCount == 0 || mauCamBien.apSuatPa != pSamples[pCount - 1]) {
        pSamples[pCount++] = mauCamBien.apSuatPa;
      }
    }
    delay(2);
  }

  // 1. Tính toán Gyro Bias & Cảnh báo rung lắc khi hiệu chuẩn
  if (imuSamples > 50) {
    saiSoVanTocGocX = sumGx / imuSamples;
    saiSoVanTocGocY = sumGy / imuSamples;
    saiSoVanTocGocZ = sumGz / imuSamples;
    Serial.printf("[CAL] Gyro Bias: X=%.2f, Y=%.2f, Z=%.2f dps (%u samples)\n",
                  saiSoVanTocGocX, saiSoVanTocGocY, saiSoVanTocGocZ, imuSamples);

    if (accelOutOfToleranceSamples > (imuSamples * 20 / 100)) {
      Serial.printf("[CAL][WARN] Device moved during calibration (%u/%u samples out of tolerance) — press 'c' to recalibrate.\n",
                    accelOutOfToleranceSamples, imuSamples);
    }
  }

  // 2. Tính toán Pressure Median Baseline
  if (pCount >= 10) {
    // Sắp xếp tìm trung vị (median)
    for (size_t i = 1; i < pCount; ++i) {
      double key = pSamples[i];
      int j = i - 1;
      while (j >= 0 && pSamples[j] > key) {
        pSamples[j + 1] = pSamples[j];
        j--;
      }
      pSamples[j + 1] = key;
    }
    apSuatGocPa = (pCount % 2 == 1) ? pSamples[pCount / 2] : (pSamples[pCount / 2 - 1] + pSamples[pCount / 2]) * 0.5;
    apSuatGocHopLe = true;
    Serial.printf("[CAL] Pressure Baseline P0: %.2f Pa (%.2f hPa, %u samples)\n",
                  apSuatGocPa, apSuatGocPa / 100.0, pCount);
  } else {
    Serial.println("[WARN] Insufficient baro samples for baseline! Using default 101325 Pa.");
    apSuatGocPa = 101325.0;
    apSuatGocHopLe = false;
  }

  Serial.println("[READY] Monitoring started. State transitioned to NORMAL.");
  trangThaiHienTai = TRANG_THAI_BINH_THUONG;
}

// ============================================================================
// 10. ARDUINO SETUP & MAIN LOOP
// ============================================================================

// [LUỒNG KHỞI ĐỘNG] `setup()` chỉ chạy ĐÚNG MỘT LẦN khi cấp nguồn. Các bước:
//   1. Mở cổng Serial (để máy tính đọc được dữ liệu in ra).
//   2. Khởi tạo dây I2C (đường truyền nối vi điều khiển với 2 cảm biến).
//   3. Khởi tạo MPU9250 (gia tốc + vận tốc góc).
//   4. Khởi tạo GY63/MS5611 (khí áp).
//   5. In bảng ngưỡng cấu hình.
//   6. Hiệu chuẩn cảm biến (cần giữ thiết bị đứng yên).
void setup() {
  // Bước 1: Mở cổng Serial ở tốc độ 115200 baud
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Chờ cổng Serial mở (tối đa 2 giây)

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32 FALL DETECTION TEST RIG — FIRMWARE TEST_RIG");
  Serial.println("========================================================");
  Serial.println("Board   : ESP32-WROOM-32 (ESP32 Dev Module)");
  Serial.println("IMU     : MPU9250 (Address 0x68/0x69)");
  Serial.println("Baro    : GY-63 / MS5611 (Address 0x77/0x76)");
  Serial.printf("Wiring  : SDA=GPIO%d, SCL=GPIO%d (%u Hz)\n",
                PIN_TEST_I2C_SDA, PIN_TEST_I2C_SCL, I2C_CLOCK_FREQ_HZ);
  Serial.println("// VERIFY_TEST_WIRING: Đấu dây chưa được xác minh trên phần cứng thật!");
  Serial.println("========================================================\n");

  // Bước 2: Khởi tạo I2C trên ESP32-WROOM-32 (SDA=21, SCL=22, 400 kHz)
  Wire.begin(PIN_TEST_I2C_SDA, PIN_TEST_I2C_SCL, I2C_CLOCK_FREQ_HZ);

  // Bước 3: Khởi tạo MPU9250; nếu thất bại thì dừng hẳn chương trình
  if (!khoiTaoMpu9250()) {
    Serial.println("[FATAL] IMU initialization failed! System halted.");
    while (true) {
      delay(1000);
    }
  }

  // Bước 4: Khởi tạo MS5611 (không bắt buộc thành công, vẫn chạy được bằng IMU)
  khoiTaoMs5611();

  // Bước 5: In bảng tham số cauHinhNgua (các ngưỡng nhận diện ngã)
  inBangNguong();

  // Bước 6: Hiệu chuẩn lúc boot - GIỮ THIẾT BỊ ĐỨNG YÊN trong 3 giây
  hieuChuanLucKhoiDong();

  if (cheDoCsvRaw) {
    inTieuDeCsv();
  }
}

// [LUỒNG CHÍNH] `loop()` chạy đi chạy lại LIÊN TỤC sau khi `setup()` xong.
// Mỗi vòng lặp làm bốn việc theo thứ tự:
//   (1) ĐỌC CẢM BIẾN -> (2) XỬ LÝ PHÁT HIỆN TÉ NGÃ -> (3) IN DỮ LIỆU -> (4) LẶP LẠI.
void loop() {
  uint32_t nowUs = micros();
  uint32_t nowMs = millis();

  // Xử lý các lệnh nhập từ Serial (gõ phím 'r', 't', 'c', 'h')
  xuLyLenhSerial();

  // Bước 1: Đọc cảm biến áp suất không chặn (25 Hz, không làm kẹt vòng lặp)
  docMs5611KhongChan(nowUs);

  // Bước 2: Chu kỳ đọc IMU 100 Hz (mỗi 10 ms = 10000 micro giây)
  if ((nowUs - lastImuReadUs) >= 10000) {
    lastImuReadUs = nowUs;
    mauCamBien.timestampMs = nowMs;

    if (docMpu9250(mauCamBien)) {
      soMauImu++;

      // Xử lý thuật toán phát hiện ngã (cập nhật nhanSuKienCho nếu có sự kiện)
      xuLyPhatHienTeNga(mauCamBien);

      // Nếu ở chế độ CSV raw, xuất đúng nhịp 100 Hz kèm event tag nếu có
      if (cheDoCsvRaw) {
        inDongCsv(mauCamBien, nhanSuKienCho);
      }
      // Reset nhanSuKienCho về "NONE" sau khi xử lý mẫu (chỉ dòng đầu tiên mang tag)
      nhanSuKienCho = "NONE";
    }
  }

  // Bước 3: Chu kỳ in HUMAN log (mỗi 1000 ms = 1 giây in một lần cho con người đọc)
  if (!cheDoCsvRaw && (nowMs - lastHumanLogMs) >= 1000) {
    lastHumanLogMs = nowMs;
    inThongTinDeDoc(mauCamBien);
  }
}
