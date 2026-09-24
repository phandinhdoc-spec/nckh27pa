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
 * - Đấu dây I2C (2 bus riêng biệt):
 *     + MPU9250 (Bus 0 / Wire):  SDA = GPIO 33, SCL = GPIO 32 (non-strapping)
 *     + GY-63 / MS5611 (Bus 1 / Wire1): SDA = GPIO 26, SCL = GPIO 27
 *     Người dùng BẮT BUỘC phải xác minh nối dây vật lý trước khi cấp nguồn!
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
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================================
// 1. CẤU HÌNH PHẦN CỨNG & CHÂN KẾT NỐI (PIN DEFINITIONS)
// ============================================================================
// Hai bus I2C riêng biệt (ESP32-WROOM-32 DevKit RF 38 pin)
// Bus 0 (Wire)  — MPU9250 : SDA = GPIO 33, SCL = GPIO 32 (cùng cột với GPIO14/12,
//                           non-strapping — tránh GPIO12/15 là chân boot-strap)
// Bus 1 (Wire1) — GY-63/MS5611 : SDA = GPIO 26, SCL = GPIO 27
constexpr int PIN_IMU_I2C_SDA   = 33;   // MPU9250 SDA (GPIO 33)
constexpr int PIN_IMU_I2C_SCL   = 32;   // MPU9250 SCL (GPIO 32)
constexpr int PIN_BARO_I2C_SDA  = 26;   // GY-63 SDA  (GPIO 26)
constexpr int PIN_BARO_I2C_SCL  = 27;   // GY-63 SCL  (GPIO 27)
constexpr uint32_t I2C_CLOCK_FREQ_HZ = 400000;  // 400 kHz Fast-mode (bus IMU)
// GY-63/MS5611 chạy 100 kHz: ổn định hơn khi dùng jumper dài/breadboard,
// và giảm nhiễu xuyên trên bus riêng của baro.
constexpr uint32_t BARO_I2C_CLOCK_FREQ_HZ = 100000; // 100 kHz

// Địa chỉ I2C
constexpr uint8_t MPU9250_I2C_ADDR_PRIMARY = 0x68;
constexpr uint8_t MPU9250_I2C_ADDR_ALT     = 0x69;
constexpr uint8_t MS5611_I2C_ADDR_PRIMARY  = 0x77;
constexpr uint8_t MS5611_I2C_ADDR_ALT      = 0x76;

// MPU9250 Registers
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

// MS5611 Commands
constexpr uint8_t MS5611_CMD_RESET     = 0x1E;
constexpr uint8_t MS5611_CMD_ADC_READ  = 0x00;
constexpr uint8_t MS5611_CMD_PROM_BASE = 0xA0;
constexpr uint8_t MS5611_CMD_CONV_D1   = 0x48;  // OSR 4096 (9.04ms max)
constexpr uint8_t MS5611_CMD_CONV_D2   = 0x58;  // OSR 4096 (9.04ms max)
constexpr uint32_t MS5611_CONV_DELAY_US= 9200;  // 9.2 ms

// Hằng số tính toán
constexpr double GRAVITY_EARTH_MS2     = 9.80665;
constexpr double ACCEL_SCALE_16G       = (16.0 * GRAVITY_EARTH_MS2) / 32768.0; // m/s^2 per LSB
constexpr double GYRO_SCALE_2000DPS    = 2000.0 / 32768.0;                    // dps per LSB
constexpr double HYPSOMETRIC_SCALE     = 44330.77;
constexpr double HYPSOMETRIC_EXPONENT  = 0.190263;

// Chế độ in Serial mặc định (0 = HUMAN MODE, 1 = CSV RAW MODE)
#ifndef SERIAL_RAW_MODE
#define SERIAL_RAW_MODE 0
#endif

// ============================================================================
// 2. CẤU TRÚC DỮ LIỆU & PROFILE THUẬT TOÁN (DATA STRUCTURES & PROFILE)
// ============================================================================

// Dữ liệu đo tức thời từ cảm biến (khai báo sớm cho prototype của Arduino)
struct SensorSnapshot {
  uint32_t timestampMs = 0;
  double ax = 0.0, ay = 0.0, az = 0.0;  // m/s^2
  double gx = 0.0, gy = 0.0, gz = 0.0;  // dps
  double accelMagnitude = 0.0;          // m/s^2
  double gyroMagnitude = 0.0;           // dps
  double pressurePa = 0.0;              // Pa
  double temperatureC = 0.0;            // deg C
  double altitudeDeltaM = 0.0;          // m
  bool stationary = false;
};

// Khai báo trước hàm hiệu chuẩn
void performBootCalibration();

// ============================================================================
// MÁY TRẠNG THÁI PHÁT HIỆN NGÃ (STATE MACHINE) — khai báo sớm để prototype
// của Arduino (tự động chèn trước hàm đầu tiên) thấy được các kiểu này.
// ============================================================================
enum DetectionState {
  STATE_CALIBRATING = 0,
  STATE_NORMAL,
  STATE_POSSIBLE_FREE_FALL,
  STATE_IMPACT,
  STATE_POST_IMPACT,
  STATE_FALL_CONFIRMED
};

const char* fallStateToString(DetectionState s) {
  switch (s) {
    case STATE_CALIBRATING:        return "CALIBRATING";
    case STATE_NORMAL:             return "NORMAL";
    case STATE_POSSIBLE_FREE_FALL: return "POSSIBLE_FREE_FALL";
    case STATE_IMPACT:             return "IMPACT";
    case STATE_POST_IMPACT:        return "POST_IMPACT";
    case STATE_FALL_CONFIRMED:     return "FALL_CONFIRMED";
    default:                       return "UNKNOWN";
  }
}

// BLE transport: IF-003 frame version 1, service UUIDs from interface-contract.md.
static const char* BLE_DEVICE_NAME = "ble_server";
static const char* BLE_SERVICE_UUID = "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001";
static const char* BLE_TELEMETRY_UUID = "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001";
static const char* BLE_EVENT_UUID = "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001";
static const char* BLE_REQUEST_UUID = "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001";
static const char* BLE_PROFILE_UUID = "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001";
constexpr uint16_t IF003_DEFAULT_MTU = 23;
constexpr uint8_t IF003_KIND_SENSOR = 1, IF003_KIND_EVENT = 2, IF003_KIND_COMMAND = 4, IF003_KIND_ACK = 5;
BLEServer* bleServer = nullptr;
BLECharacteristic* telemetryNotify = nullptr;
BLECharacteristic* eventNotify = nullptr;
BLECharacteristic* requestChar = nullptr;
BLECharacteristic* profileWrite = nullptr;
bool bleConnected = false;
volatile uint16_t blePeerMtu = 23;
uint32_t bleMessageId = 1;
uint32_t bleSequence = 0;
uint32_t lastTelemetryMs = 0;

static uint32_t nextMessageId() {
  if (++bleMessageId == 0) bleMessageId = 1;
  return bleMessageId;
}

static void putLe(uint8_t* p, uint32_t v, uint8_t n) {
  for (uint8_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

// IF-003 encoder: FS/version/kind/id/index/count/total/offset + UTF-8 bytes.
static void notifyFramed(BLECharacteristic* characteristic, uint8_t kind, const String& payload) {
  if (!bleConnected || characteristic == nullptr || payload.length() == 0 || payload.length() > 1024) return;
  size_t chunk = (blePeerMtu >= 27) ? (static_cast<size_t>(blePeerMtu) - 19) : 4; // ATT hdr 3 + frame hdr 16.
  if (chunk > 200) chunk = 200; // Cap ATT cho stack NimBLE cu.
  const size_t total = payload.length();
  const uint16_t count = static_cast<uint16_t>((total - 1) / chunk + 1);
  const uint32_t id = nextMessageId();
  for (uint16_t index = 0; index < count; ++index) {
    if (count > 20 && index >= 20) return; // MTU thap: khong chan loop qua lau.
    const size_t offset = index * chunk;
    const size_t length = min(chunk, total - offset);
    std::vector<uint8_t> frame(16 + length);
    frame[0] = 0x46; frame[1] = 0x53; frame[2] = 1; frame[3] = kind;
    putLe(frame.data() + 4, id, 4); putLe(frame.data() + 8, index, 2);
    putLe(frame.data() + 10, count, 2); putLe(frame.data() + 12, total, 2);
    putLe(frame.data() + 14, offset, 2);
    memcpy(frame.data() + 16, payload.c_str() + offset, length);
    characteristic->setValue(frame.data(), frame.size());
    characteristic->notify();
    delay(1);
  }
}

#if 0
static bool jsonNumber(const String& json, const char* key, double& out) {
  int p = json.indexOf(String("\"") + key + "\"");
  if (p < 0) return false;
  p = json.indexOf(':', p); if (p < 0) return false; ++p;
  while (p < json.length() && (json[p] == ' ' || json[p] == '\t')) ++p;
  char* end = nullptr; out = strtod(json.c_str() + p, &end);
  return end != json.c_str() + p;
}

static bool applyProfileJson(const String& json, String& error) {
  const char* keys[] = {"impactAccelerationMs2", "stillnessTargetAccelerationMs2", "stillnessToleranceMs2",
    "postImpactWindowMs", "postImpactStillnessDurationMs", "minimumStillnessSamples", "maximumSampleGapMs",
    "freeFallThresholdMs2", "freeFallMinDurationMs", "gyroTurnThresholdDps", "pressureEvidenceMinRisePa",
    "pressureWindowMs", "altitudeDropMinM", "sampleWatchdogMs"};
  double v[14];
  for (uint8_t i = 0; i < 14; ++i) if (!jsonNumber(json, keys[i], v[i])) { error = String("missing ") + keys[i]; return false; }
  if (v[0] <= 0 || v[1] <= 0 || v[2] <= 0 || v[3] < 1 || v[4] < 1 || v[5] < 1 || v[5] > 65535 ||
      v[6] < 1 || v[7] <= 0 || v[8] < 1 || v[9] < 0 || v[10] < 0 || v[11] < 1 || v[12] >= 0 || v[13] < 1) {
    error = "profile value out of range"; return false;
  }
  profile.impactAccelerationMs2=v[0]; profile.stillnessTargetAccelerationMs2=v[1]; profile.stillnessToleranceMs2=v[2];
  profile.postImpactWindowMs=v[3]; profile.postImpactStillnessDurationMs=v[4]; profile.minimumStillnessSamples=v[5];
  profile.maximumSampleGapMs=v[6]; profile.freeFallThresholdMs2=v[7]; profile.freeFallMinDurationMs=v[8];
  profile.gyroTurnThresholdDps=v[9]; profile.pressureEvidenceMinRisePa=v[10]; profile.pressureWindowMs=v[11];
  profile.altitudeDropMinM=v[12]; profile.sampleWatchdogMs=v[13];
  return true;
}

static String telemetryJson() {
  String s = "{\"protocolVersion\":1,\"sensorSource\":\"ESP32\",\"deviceId\":\"ble_server\",\"sequenceNumber\":";
  s += String(++bleSequence); s += ",\"timestampMs\":"; s += String(currentSample.timestampMs);
  s += ",\"accelXMs2\":"; s += String(currentSample.ax, 3); s += ",\"accelYMs2\":"; s += String(currentSample.ay, 3);
  s += ",\"accelZMs2\":"; s += String(currentSample.az, 3); s += ",\"gyroXDps\":"; s += String(currentSample.gx, 3);
  s += ",\"gyroYDps\":"; s += String(currentSample.gy, 3); s += ",\"gyroZDps\":"; s += String(currentSample.gz, 3);
  s += ",\"pressurePa\":"; s += String(currentSample.pressurePa, 2); s += ",\"temperatureC\":"; s += String(currentSample.temperatureC, 2);
  s += ",\"altitudeDeltaM\":"; s += String(currentSample.altitudeDeltaM, 3); s += ",\"sensorQuality\":100}"; return s;
}

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { bleConnected = true; }
  void onDisconnect(BLEServer* server) override { bleConnected = false; server->getAdvertising()->start(); }
};
class RequestCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override { String token = c->getValue().c_str(); String ack = "{\"ok\":true,\"token\":\"" + token + "\"}"; notifyFramed(requestChar, IF003_KIND_ACK, ack); }
};
class ProfileCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override { String error; String json = c->getValue().c_str(); bool ok = applyProfileJson(json, error); String ack = ok ? "{\"ok\":true,\"profile\":\"updated\"}" : (String("{\"ok\":false,\"error\":\"") + error + "\"}"); notifyFramed(requestChar, IF003_KIND_ACK, ack); }
};

static void setupBle() {
  BLEDevice::init(BLE_DEVICE_NAME); bleServer = BLEDevice::createServer(); bleServer->setCallbacks(new BleServerCallbacks());
  BLEService* service = bleServer->createService(BLE_SERVICE_UUID);
  telemetryNotify = service->createCharacteristic(BLE_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  eventNotify = service->createCharacteristic(BLE_EVENT_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  requestChar = service->createCharacteristic(BLE_REQUEST_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  profileWrite = service->createCharacteristic(BLE_PROFILE_UUID, BLECharacteristic::PROPERTY_WRITE);
  telemetryNotify->addDescriptor(new BLE2902()); eventNotify->addDescriptor(new BLE2902()); requestChar->addDescriptor(new BLE2902());
  requestChar->setCallbacks(new RequestCallbacks()); profileWrite->setCallbacks(new ProfileCallbacks()); service->start();
  bleServer->getAdvertising()->addServiceUUID(BLE_SERVICE_UUID); bleServer->getAdvertising()->start();
}
#endif

#define PROFILE_NAME "TEST_RIG_EXP_V1"

struct FallProfile {
  // --- Kế thừa nguyên bản từ Android FallDetectionConfig.DEFAULT ---
  float impactAccelerationMs2 = 25.0f;           // Ngưỡng va đập (~2.55g) [FallDetectionProfiles.kt:56]
  float stillnessTargetAccelerationMs2 = 9.81f;  // Trọng trường chuẩn khi tĩnh (1.0g)
  float stillnessToleranceMs2 = 1.0f;            // Dung sai tĩnh (|a - 9.81| <= 1.0 m/s^2)
  uint32_t postImpactWindowMs = 3000;            // Cửa sổ tối đa sau va đập (3000 ms)
  uint32_t postImpactStillnessDurationMs = 1000; // Thời lượng tĩnh tối thiểu (1000 ms)
  uint16_t minimumStillnessSamples = 6;          // Giữ 6 để bám đúng FallDetectionConfig.DEFAULT Android; ở 100 Hz điều kiện duration >= 1000 ms mới là ràng buộc thực tế
  uint32_t maximumSampleGapMs = 250;             // Hủy chuỗi nếu mất mẫu > 250 ms

  // --- Tham số thử nghiệm bổ sung cho Test Rig (bằng chứng phụ & log) ---
  float freeFallThresholdMs2 = 4.90f;            // Rơi tự do (< 0.5g)
  uint32_t freeFallMinDurationMs = 80;           // Duy trì rơi tự do tối thiểu 80 ms
  float gyroTurnThresholdDps = 120.0f;           // Vận tốc góc xoay thân ngã
  float pressureEvidenceMinRisePa = 12.0f;       // Tăng áp suất đối chứng (Android mặc định 12 Pa)
  uint32_t pressureWindowMs = 5000;              // Cửa sổ tính áp suất tăng (5000 ms) [Nguồn: Android DemoLogic.kt PRESSURE_WINDOW_NS]
  float altitudeDropMinM = -0.40f;               // Độ cao giảm tối thiểu đối chứng (-0.40 m)
  uint32_t sampleWatchdogMs = 100;               // Cảnh báo watchdog nếu trễ đọc cảm biến > 100 ms
};

// ============================================================================
// 3. MÁY TRẠNG THÁI PHÁT HIỆN NGÃ (STATE MACHINE)
// ============================================================================
// enum DetectionState + fallStateToString được khai báo sớm ở đầu file
// (trước phần BLE transport) để prototype tự động của Arduino thấy kiểu.

// ============================================================================
// 4. BIẾN TOÀN CỤC VÀ DỮ LIỆU CẢM BIẾN
// ============================================================================
FallProfile profile;
DetectionState currentState = STATE_CALIBRATING;
bool rawCsvMode = (SERIAL_RAW_MODE != 0);

// Địa chỉ I2C đang hoạt động
uint8_t mpuAddress = 0;
uint8_t baroAddress = 0;
bool mpuAvailable = false;
bool baroAvailable = false;

// Hiệu chuẩn MPU
double gyroBiasX = 0.0, gyroBiasY = 0.0, gyroBiasZ = 0.0;
bool accelSaturated = false;
bool gyroSaturated = false;

// Hiệu chuẩn Barometer
uint16_t msC[8] = {};  // PROM calibration coefficients C1..C6 (C0, C7 CRC)
double baselinePressurePa = 0.0;
bool baselineValid = false;

// Đọc MS5611 không chặn (Non-blocking State Machine)
enum BaroState { BARO_IDLE, BARO_CONV_D1, BARO_CONV_D2 };
BaroState baroState = BARO_IDLE;
uint32_t baroTimerUs = 0;
uint32_t lastBaroCycleUs = 0;
uint32_t rawD1 = 0, rawD2 = 0;

// Dữ liệu đo tức thời
SensorSnapshot currentSample;

static bool jsonNumber(const String& json, const char* key, double& out) {
  int p = json.indexOf(String("\"") + key + "\""); if (p < 0) return false;
  p = json.indexOf(':', p); if (p < 0) return false; ++p;
  while (p < json.length() && (json[p] == ' ' || json[p] == '\t')) ++p;
  char* end = nullptr; out = strtod(json.c_str() + p, &end); return end != json.c_str() + p;
}
static bool applyProfileJson(const String& json, String& error) {
  const char* k[] = {"impactAccelerationMs2","stillnessTargetAccelerationMs2","stillnessToleranceMs2","postImpactWindowMs","postImpactStillnessDurationMs","minimumStillnessSamples","maximumSampleGapMs","freeFallThresholdMs2","freeFallMinDurationMs","gyroTurnThresholdDps","pressureEvidenceMinRisePa","pressureWindowMs","altitudeDropMinM","sampleWatchdogMs"};
  double v[14]; bool present[14] = {};
  for (uint8_t i=0;i<14;++i) present[i] = jsonNumber(json,k[i],v[i]);
  if (!present[0] && !present[1] && !present[2] && !present[3] && !present[4] && !present[5] && !present[6] && !present[7] && !present[8] && !present[9] && !present[10] && !present[11] && !present[12] && !present[13]) { error="no profile fields"; return false; }
  if (!present[0]) v[0]=profile.impactAccelerationMs2; if (!present[1]) v[1]=profile.stillnessTargetAccelerationMs2; if (!present[2]) v[2]=profile.stillnessToleranceMs2; if (!present[3]) v[3]=profile.postImpactWindowMs; if (!present[4]) v[4]=profile.postImpactStillnessDurationMs; if (!present[5]) v[5]=profile.minimumStillnessSamples; if (!present[6]) v[6]=profile.maximumSampleGapMs; if (!present[7]) v[7]=profile.freeFallThresholdMs2; if (!present[8]) v[8]=profile.freeFallMinDurationMs; if (!present[9]) v[9]=profile.gyroTurnThresholdDps; if (!present[10]) v[10]=profile.pressureEvidenceMinRisePa; if (!present[11]) v[11]=profile.pressureWindowMs; if (!present[12]) v[12]=profile.altitudeDropMinM; if (!present[13]) v[13]=profile.sampleWatchdogMs;
  if (v[0]<=0||v[1]<=0||v[2]<=0||v[3]<1||v[4]<1||v[5]<1||v[5]>65535||v[6]<1||v[7]<=0||v[8]<1||v[9]<0||v[10]<0||v[11]<1||v[12]>=0||v[13]<1) { error="profile value out of range"; return false; }
  profile.impactAccelerationMs2=v[0]; profile.stillnessTargetAccelerationMs2=v[1]; profile.stillnessToleranceMs2=v[2]; profile.postImpactWindowMs=v[3]; profile.postImpactStillnessDurationMs=v[4]; profile.minimumStillnessSamples=v[5]; profile.maximumSampleGapMs=v[6]; profile.freeFallThresholdMs2=v[7]; profile.freeFallMinDurationMs=v[8]; profile.gyroTurnThresholdDps=v[9]; profile.pressureEvidenceMinRisePa=v[10]; profile.pressureWindowMs=v[11]; profile.altitudeDropMinM=v[12]; profile.sampleWatchdogMs=v[13]; return true;
}
static String telemetryJson() {
  String s="{\"protocolVersion\":1,\"sensorSource\":\"ESP32\",\"deviceId\":\"ble_server\",\"sequenceNumber\":"; s+=String(++bleSequence); s+=",\"timestampMs\":"; s+=String(currentSample.timestampMs); s+=",\"accelXMs2\":"; s+=String(currentSample.ax,3); s+=",\"accelYMs2\":"; s+=String(currentSample.ay,3); s+=",\"accelZMs2\":"; s+=String(currentSample.az,3); s+=",\"gyroXDps\":"; s+=String(currentSample.gx,3); s+=",\"gyroYDps\":"; s+=String(currentSample.gy,3); s+=",\"gyroZDps\":"; s+=String(currentSample.gz,3); s+=",\"pressurePa\":"; s+=String(currentSample.pressurePa,2); s+=",\"temperatureC\":"; s+=String(currentSample.temperatureC,2); s+=",\"altitudeDeltaM\":"; s+=String(currentSample.altitudeDeltaM,3); s+=",\"batteryPercent\":100,\"batteryVoltageMv\":3300,\"isCharging\":true,\"sosButtonPressed\":false,\"sensorQuality\":100}"; return s;
}
class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleConnected = true;
    uint16_t connId = pServer->getConnId();
    uint16_t mtu = pServer->getPeerMTU(connId);
    if (mtu < 23) mtu = 23; else if (mtu > 517) mtu = 517;
    blePeerMtu = mtu;
  }
  void onDisconnect(BLEServer* s) override { bleConnected = false; blePeerMtu = 23; s->getAdvertising()->start(); }
};
class RequestCallbacks : public BLECharacteristicCallbacks { void onWrite(BLECharacteristic*) override { notifyFramed(telemetryNotify,IF003_KIND_SENSOR,telemetryJson()); } };
class ProfileCallbacks : public BLECharacteristicCallbacks { void onWrite(BLECharacteristic* c) override { String e,j=c->getValue().c_str(); bool ok=applyProfileJson(j,e); notifyFramed(telemetryNotify,IF003_KIND_ACK,ok?"{\"status\":\"OK\",\"updated\":true}":String("{\"status\":\"ERROR\",\"updated\":false,\"error\":\"")+e+"\"}"); } };
static void setupBle() {
  BLEDevice::init(BLE_DEVICE_NAME); BLEDevice::setMTU(517); bleServer=BLEDevice::createServer(); bleServer->setCallbacks(new BleServerCallbacks()); BLEService* svc=bleServer->createService(BLE_SERVICE_UUID);
  telemetryNotify=svc->createCharacteristic(BLE_TELEMETRY_UUID,BLECharacteristic::PROPERTY_NOTIFY); eventNotify=svc->createCharacteristic(BLE_EVENT_UUID,BLECharacteristic::PROPERTY_NOTIFY); requestChar=svc->createCharacteristic(BLE_REQUEST_UUID,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_NOTIFY); profileWrite=svc->createCharacteristic(BLE_PROFILE_UUID,BLECharacteristic::PROPERTY_WRITE);
  telemetryNotify->addDescriptor(new BLE2902()); eventNotify->addDescriptor(new BLE2902()); requestChar->addDescriptor(new BLE2902()); requestChar->setCallbacks(new RequestCallbacks()); profileWrite->setCallbacks(new ProfileCallbacks()); svc->start(); bleServer->getAdvertising()->addServiceUUID(BLE_SERVICE_UUID); bleServer->getAdvertising()->start();
}

// Biến theo dõi của thuật toán
uint32_t lastSampleTimeMs = 0;
uint32_t freeFallStartTimeMs = 0;
uint32_t impactTimeMs = 0;
float impactPeakMs2 = 0.0f;
uint32_t quietStartTimeMs = 0;
uint16_t stillnessSampleCount = 0;
uint32_t confirmedAlertTimeMs = 0;
bool gyroTurnEvidence = false;
bool altitudeDropEvidence = false;
const char* pendingEventTag = "NONE";

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
uint32_t lastBaroRetryMs = 0;
uint32_t lastImuReadUs = 0;
uint32_t imuSampleCount = 0;

// ============================================================================
// 5. DRIVER GIAO TIẾP MỨC THANH GHI I2C (REGISTER LEVEL DRIVERS)
// ============================================================================

bool i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  return (Wire.endTransmission() == 0);
}

bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buffer, size_t len) {
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

// Baro (MS5611) nằm trên Bus 1 (Wire1): SDA = GPIO 26, SCL = GPIO 27
bool baroWriteByte(uint8_t addr, uint8_t reg) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  return (Wire1.endTransmission() == 0);
}

bool baroReadBytes(uint8_t addr, uint8_t reg, uint8_t* buffer, size_t len) {
  Wire1.beginTransmission(addr);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) {
    return false;
  }
  size_t read = Wire1.requestFrom(static_cast<uint16_t>(addr), static_cast<uint8_t>(len));
  if (read != len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    buffer[i] = Wire1.read();
  }
  return true;
}

// Đọc ADC 24-bit của MS5611 (sensor trả MSB trước → ráp đúng thứ tự)
bool baroRead24(uint8_t addr, uint8_t reg, uint32_t& out) {
  uint8_t buf[3];
  if (!baroReadBytes(addr, reg, buf, 3)) {
    return false;
  }
  out = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[2];
  return true;
}

// ----------------------------------------------------------------------------
// MPU9250 Driver (Bus 0 / Wire: SDA=14, SCL=12)
// ----------------------------------------------------------------------------
bool initMPU9250() {
  // Thử địa chỉ 0x68 trước, nếu không được thử 0x69
  uint8_t testAddrs[] = {MPU9250_I2C_ADDR_PRIMARY, MPU9250_I2C_ADDR_ALT};
  uint8_t foundAddr = 0;
  uint8_t whoAmI = 0;

  for (uint8_t addr : testAddrs) {
    if (i2cReadBytes(addr, MPU_REG_WHO_AM_I, &whoAmI, 1)) {
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
  i2cWriteByte(mpuAddress, MPU_REG_PWR_MGMT_1, 0x80);
  delay(100);
  i2cWriteByte(mpuAddress, MPU_REG_PWR_MGMT_1, 0x01); // Auto-select best clock (PLL)
  delay(10);

  // 2. Kích hoạt toàn bộ trục Accel & Gyro
  i2cWriteByte(mpuAddress, MPU_REG_PWR_MGMT_2, 0x00);

  // 3. Cấu hình tốc độ lấy mẫu nội bộ: 1 kHz / (1 + 9) = 100 Hz
  i2cWriteByte(mpuAddress, MPU_REG_SMPLRT_DIV, 0x09);

  // 4. Cấu hình DLPF 184 Hz cho Gyro
  i2cWriteByte(mpuAddress, MPU_REG_CONFIG, 0x01);

  // 5. Cấu hình Full Scale Gyro: ±2000 dps (FS_SEL = 3 -> 0x18)
  i2cWriteByte(mpuAddress, MPU_REG_GYRO_CONFIG, 0x18);

  // 6. Cấu hình Full Scale Accel: ±16 g (AFS_SEL = 3 -> 0x18)
  i2cWriteByte(mpuAddress, MPU_REG_ACCEL_CONFIG, 0x18);

  // 7. Cấu hình DLPF 184 Hz cho Accel (ACCEL_CONFIG_2)
  i2cWriteByte(mpuAddress, MPU_REG_ACCEL_CONFIG2, 0x01);

  // 8. Tắt khối I2C Master nội bộ (không kết nối từ kế AK8963 để tránh nghẽn bus)
  i2cWriteByte(mpuAddress, MPU_REG_USER_CTRL, 0x00);
  i2cWriteByte(mpuAddress, MPU_REG_INT_PIN_CFG, 0x02); // BYPASS_EN

  Serial.printf("[OK] MPU9250 detected at 0x%02X (WHO_AM_I=0x%02X | AFS=±16g, GFS=±2000dps)\n",
                mpuAddress, whoAmI);
  mpuAvailable = true;
  return true;
}

bool readMPU9250(SensorSnapshot& s) {
  if (!mpuAvailable) return false;

  uint8_t buf[14];
  if (!i2cReadBytes(mpuAddress, MPU_REG_ACCEL_XOUT_H, buf, 14)) {
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
  accelSaturated = (abs(rawAx) >= 32750 || abs(rawAy) >= 32750 || abs(rawAz) >= 32750);
  gyroSaturated = (abs(rawGx) >= 32750 || abs(rawGy) >= 32750 || abs(rawGz) >= 32750);

  // Chuyển đổi đơn vị chuẩn SI
  s.ax = (double)rawAx * ACCEL_SCALE_16G;
  s.ay = (double)rawAy * ACCEL_SCALE_16G;
  s.az = (double)rawAz * ACCEL_SCALE_16G;

  s.gx = ((double)rawGx * GYRO_SCALE_2000DPS) - gyroBiasX;
  s.gy = ((double)rawGy * GYRO_SCALE_2000DPS) - gyroBiasY;
  s.gz = ((double)rawGz * GYRO_SCALE_2000DPS) - gyroBiasZ;

  // Tính Magnitude
  s.accelMagnitude = sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  s.gyroMagnitude = sqrt(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);

  // Bộ lọc IIR cho tư thế tĩnh (alpha = 0.1)
  filtAx = 0.9 * filtAx + 0.1 * s.ax;
  filtAy = 0.9 * filtAy + 0.1 * s.ay;
  filtAz = 0.9 * filtAz + 0.1 * s.az;

  // Nhận diện tĩnh: gia tốc gần 9.81 và tốc độ góc nhỏ
  double filtMag = sqrt(filtAx * filtAx + filtAy * filtAy + filtAz * filtAz);
  // CHỈ dùng cho log/quan sát, KHÔNG dùng cho quyết định ngã (Android không dùng gyro trong quyết định)
  s.stationary = (fabs(filtMag - profile.stillnessTargetAccelerationMs2) <= profile.stillnessToleranceMs2) &&
                 (s.gyroMagnitude < 25.0);

  return true;
}

// ----------------------------------------------------------------------------
// MS5611 (GY-63) Driver
// ----------------------------------------------------------------------------
uint8_t computeMs5611Crc4(const uint16_t* prom) {
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

bool initMS5611() {
  uint8_t testAddrs[] = {MS5611_I2C_ADDR_PRIMARY, MS5611_I2C_ADDR_ALT};
  uint8_t foundAddr = 0;

  for (uint8_t addr : testAddrs) {
    if (baroWriteByte(addr, MS5611_CMD_PROM_BASE)) {
      foundAddr = addr;
      break;
    }
  }

  if (foundAddr == 0) {
    Serial.println("[WARN] GY-63 (MS5611) not responding on Wire1! Scanning bus...");
    scanBaroBus();
    Serial.println("[WARN] Continuing in IMU-only mode (retry every ~2s in loop).");
    return false;
  }

  baroAddress = foundAddr;

  // Gửi lệnh Reset
  baroWriteByte(baroAddress, MS5611_CMD_RESET);
  delay(10); // Chờ nạp PROM

  // Đọc 8 từ PROM (16 byte) — MS5611 trả MSB trước
  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t promBuf[2];
    if (!baroReadBytes(baroAddress, MS5611_CMD_PROM_BASE + (i * 2), promBuf, 2)) {
      Serial.println("[ERROR] Failed to read MS5611 PROM!");
      return false;
    }
    msC[i] = ((uint16_t)promBuf[0] << 8) | (uint16_t)promBuf[1];
  }

  // Kiểm tra CRC-4 (AN520)
  uint16_t promForCrc[8];
  for (int i = 0; i < 8; ++i) promForCrc[i] = msC[i];
  promForCrc[7] &= 0xFF00; // Xóa nibble CRC
  uint8_t calculatedCrc = computeMs5611Crc4(promForCrc);
  uint8_t storedCrc = msC[7] & 0x000F;

  if (calculatedCrc != storedCrc) {
    Serial.printf("[WARN] MS5611 CRC mismatch! calc=%u, stored=%u\n", calculatedCrc, storedCrc);
  }

  Serial.printf("[OK] GY-63 (MS5611) detected at 0x%02X (CRC OK: %s | C1=%u, C2=%u)\n",
                baroAddress, (calculatedCrc == storedCrc ? "YES" : "NO"), msC[1], msC[2]);
  baroAvailable = true;
  return true;
}

// Quét toàn bộ bus Wire1 (là địa chỉ 7-bit thường, bỏ các địa chỉ dành riêng)
void scanBaroBus() {
  Serial.println("[DIAG] Scanning Wire1 (GPIO26=SDA, GPIO27=SCL)...");
  bool any = false;
  for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() == 0) {
      Serial.printf("[DIAG]   ACK at 0x%02X\n", addr);
      any = true;
    }
  }
  if (!any) {
    Serial.println("[DIAG]   No device ACKs on Wire1. → Kiểm tra: nguồn 3V3/GND, đấu SDA/SCL 26/27, pull-up.");
  }
}

// Máy trạng thái đọc MS5611 không chặn
void pollMS5611(uint32_t nowUs) {
  if (!baroAvailable) return;

  switch (baroState) {
    case BARO_IDLE:
      // Mỗi 40 ms (25 Hz) bắt đầu một chu kỳ đo mới
      if ((nowUs - lastBaroCycleUs) >= 40000) {
        lastBaroCycleUs = nowUs;
        // Gửi lệnh chuyển đổi D1 (áp suất)
        if (baroWriteByte(baroAddress, MS5611_CMD_CONV_D1)) {
          baroTimerUs = nowUs;
          baroState = BARO_CONV_D1;
        }
      }
      break;

    case BARO_CONV_D1:
      // Chờ chuyển đổi D1 hoàn tất (~9.2 ms)
      if ((nowUs - baroTimerUs) >= MS5611_CONV_DELAY_US) {
        baroRead24(baroAddress, MS5611_CMD_ADC_READ, rawD1);
        // Gửi tiếp lệnh chuyển đổi D2 (nhiệt độ)
        if (baroWriteByte(baroAddress, MS5611_CMD_CONV_D2)) {
          baroTimerUs = nowUs;
          baroState = BARO_CONV_D2;
        } else {
          baroState = BARO_IDLE;
        }
      }
      break;

    case BARO_CONV_D2:
      // Chờ chuyển đổi D2 hoàn tất (~9.2 ms)
      if ((nowUs - baroTimerUs) >= MS5611_CONV_DELAY_US) {
        baroRead24(baroAddress, MS5611_CMD_ADC_READ, rawD2);

        // Bù nhiệt độ bậc 2 (2nd-order temperature compensation chuẩn AN520)
        double c1 = msC[1];
        double c2 = msC[2];
        double c3 = msC[3];
        double c4 = msC[4];
        double c5 = msC[5];
        double c6 = msC[6];

        double dt = (double)rawD2 - c5 * 256.0;
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

        double pPa = ((double)rawD1 * sens / 2097152.0 - off) / 32768.0;
        double tC = temp100 * 0.01;

        if (pPa >= 30000.0 && pPa <= 110000.0) {
          currentSample.pressurePa = pPa;
          currentSample.temperatureC = tC;

          // Cập nhật độ cao tương đối nếu baseline đã có
          if (baselineValid && baselinePressurePa > 0.0) {
            currentSample.altitudeDeltaM = HYPSOMETRIC_SCALE * (1.0 - pow(pPa / baselinePressurePa, HYPSOMETRIC_EXPONENT));
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

double calculatePressureRise(uint32_t windowMs) {
  if (pressureHistCount < 2) return 0.0;
  uint32_t now = millis();
  double newestP = currentSample.pressurePa;
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

int calculateFallRiskScore() {
  switch (currentState) {
    case STATE_CALIBRATING:
      return 0;
    case STATE_NORMAL: {
      // Điểm nền dao động theo mức độ chuyển động hiện tại (0 - 25%)
      int score = 5;
      if (currentSample.accelMagnitude > 15.0) score += 10;
      if (currentSample.gyroMagnitude > 80.0) score += 10;
      return score;
    }
    case STATE_POSSIBLE_FREE_FALL:
      return 45;
    case STATE_IMPACT:
      return 75;
    case STATE_POST_IMPACT: {
      // Tiến trình tích lũy thời gian tĩnh (70 - 95%)
      int progress = (quietStartTimeMs > 0) ? ((millis() - quietStartTimeMs) * 25 / profile.postImpactStillnessDurationMs) : 0;
      if (progress > 25) progress = 25;
      return 70 + progress;
    }
    case STATE_FALL_CONFIRMED:
      return 100;
    default:
      return 0;
  }
}

// ============================================================================
// 7. XỬ LÝ CHUYỂN TRẠNG THÁI THUẬT TOÁN (ALGORITHM ENGINE)
// ============================================================================

void processFallDetection(const SensorSnapshot& s) {
  uint32_t now = s.timestampMs;

  // Watchdog kiểm tra gián đoạn mẫu
  if (lastSampleTimeMs > 0) {
    uint32_t gap = now - lastSampleTimeMs;
    if (gap > profile.maximumSampleGapMs) {
      if (currentState == STATE_IMPACT || currentState == STATE_POST_IMPACT) {
        if (!rawCsvMode) {
          Serial.printf("[WARN] Sample gap (%u ms > %u ms) aborted impact episode!\n",
                        gap, profile.maximumSampleGapMs);
        }
        currentState = STATE_NORMAL;
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        freeFallStartTimeMs = 0;
        gyroTurnEvidence = false;
        altitudeDropEvidence = false;
        pendingEventTag = "SAMPLE_GAP_RESET";
      } else {
        // FIX 3: Với NORMAL / POSSIBLE_FREE_FALL chỉ xóa mốc free-fall, giữ nguyên trạng thái
        freeFallStartTimeMs = 0;
        if (!rawCsvMode) {
          Serial.printf("[WARN] Sample gap (%u ms > %u ms threshold)\n",
                        gap, profile.maximumSampleGapMs);
        }
      }
    } else if (gap > profile.sampleWatchdogMs) {
      if (!rawCsvMode) {
        Serial.printf("[WARN] High sample jitter: %u ms\n", gap);
      }
    }
  }
  lastSampleTimeMs = now;

  switch (currentState) {
    case STATE_NORMAL: {
      // Nhánh kiểm tra rơi tự do (bằng chứng phụ)
      if (s.accelMagnitude < profile.freeFallThresholdMs2) {
        if (freeFallStartTimeMs == 0) {
          freeFallStartTimeMs = now;
        } else if ((now - freeFallStartTimeMs) >= profile.freeFallMinDurationMs) {
          currentState = STATE_POSSIBLE_FREE_FALL;
          pendingEventTag = "FREE_FALL";
          if (!rawCsvMode) {
            Serial.printf("[EVENT] FREE FALL detected (Acc=%.2fg | %.2f m/s^2, dur=%u ms)\n",
                          s.accelMagnitude / 9.81, s.accelMagnitude, (now - freeFallStartTimeMs));
          }
        }
      } else {
        freeFallStartTimeMs = 0;
      }

      // Nhánh kiểm tra va đập mạnh (luật cốt lõi bám sát Android)
      if (s.accelMagnitude >= profile.impactAccelerationMs2) {
        currentState = STATE_IMPACT;
        impactTimeMs = now;
        impactPeakMs2 = s.accelMagnitude;
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        gyroTurnEvidence = (s.gyroMagnitude >= profile.gyroTurnThresholdDps);
        altitudeDropEvidence = false;
        pendingEventTag = "IMPACT";
        if (!rawCsvMode) {
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg)\n", s.accelMagnitude, s.accelMagnitude / 9.81);
        }
      }
      break;
    }

    case STATE_POSSIBLE_FREE_FALL: {
      // Nếu có va đập ngay sau rơi tự do
      if (s.accelMagnitude >= profile.impactAccelerationMs2) {
        currentState = STATE_IMPACT;
        impactTimeMs = now;
        impactPeakMs2 = s.accelMagnitude;
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        gyroTurnEvidence = (s.gyroMagnitude >= profile.gyroTurnThresholdDps);
        altitudeDropEvidence = false;
        pendingEventTag = "IMPACT";
        if (!rawCsvMode) {
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg) [after Free-Fall]\n",
                        s.accelMagnitude, s.accelMagnitude / 9.81);
        }
      } else if ((now - freeFallStartTimeMs) > 600) {
        // Rơi tự do quá 600 ms mà không va đập -> kết thúc, về NORMAL
        currentState = STATE_NORMAL;
        freeFallStartTimeMs = 0;
      }
      break;
    }

    case STATE_IMPACT: {
      // Ghi nhận đỉnh va đập tức thời cao nhất
      if (s.accelMagnitude > impactPeakMs2) {
        impactPeakMs2 = s.accelMagnitude;
      }
      if (s.gyroMagnitude >= profile.gyroTurnThresholdDps) {
        gyroTurnEvidence = true;
      }
      // Chuyển ngay sang bước theo dõi bất động sau va chạm
      currentState = STATE_POST_IMPACT;
      break;
    }

    case STATE_POST_IMPACT: {
      // FIX 4: Parity Android — Re-latch impact nếu có mẫu mới |a| >= impactThreshold
      if (s.accelMagnitude >= profile.impactAccelerationMs2) {
        impactTimeMs = now;
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        if (s.accelMagnitude > impactPeakMs2) {
          impactPeakMs2 = s.accelMagnitude;
        }
        if (s.gyroMagnitude >= profile.gyroTurnThresholdDps) {
          gyroTurnEvidence = true;
        }
        pendingEventTag = "IMPACT";
        if (!rawCsvMode) {
          Serial.printf("[EVENT] IMPACT %.2f m/s^2 (%.2fg) [re-latched]\n",
                        s.accelMagnitude, s.accelMagnitude / 9.81);
        }
        break;
      }

      // Tiếp tục cập nhật đỉnh va đập nếu xung lực còn dội lại
      if (s.accelMagnitude > impactPeakMs2) {
        impactPeakMs2 = s.accelMagnitude;
      }

      // Kiểm tra hết hạn cửa sổ sau va đập (3000 ms)
      if ((now - impactTimeMs) > profile.postImpactWindowMs) {
        currentState = STATE_NORMAL;
        pendingEventTag = "WINDOW_EXPIRED";
        if (!rawCsvMode) {
          Serial.printf("[EVENT] POST-IMPACT window expired (%u ms), false alarm\n",
                        profile.postImpactWindowMs);
        }
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        gyroTurnEvidence = false;
        altitudeDropEvidence = false;
        break;
      }

      // Kiểm tra điều kiện bất động (|magnitude - 9.81| <= 1.0)
      bool isStill = (fabs(s.accelMagnitude - profile.stillnessTargetAccelerationMs2) <= profile.stillnessToleranceMs2);

      if (!isStill) {
        // Bị gián đoạn rung lắc -> reset thời lượng tĩnh
        if (quietStartTimeMs > 0 && !rawCsvMode) {
          // Serial.printf("[DEBUG] Stillness interrupted at %u ms\n", now - quietStartTimeMs);
        }
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
      } else {
        if (quietStartTimeMs == 0) {
          quietStartTimeMs = now;
          pendingEventTag = "POST_IMPACT_START";
          if (!rawCsvMode) {
            Serial.println("[EVENT] POST-IMPACT low motion started");
          }
        }
        stillnessSampleCount++;

        uint32_t quietDuration = now - quietStartTimeMs;

        // XÁC NHẬN NGÃ KHI ĐẠT TIÊU CHÍ (Parity Android: >= 6 mẫu & >= 1000 ms)
        if ((stillnessSampleCount >= profile.minimumStillnessSamples) &&
            (quietDuration >= profile.postImpactStillnessDurationMs)) {
          currentState = STATE_FALL_CONFIRMED;
          confirmedAlertTimeMs = now;
          pendingEventTag = "FALL_CONFIRMED";

          // FIX 6: Tính toán bằng chứng khí áp đối chứng theo cửa sổ pressureWindowMs (5000 ms)
          double deltaP = calculatePressureRise(profile.pressureWindowMs);
          double deltaH = s.altitudeDeltaM;

          // FIX 5: Đánh dấu cờ bằng chứng sụt độ cao (KHÔNG tham gia điều kiện if xác nhận ngã)
          altitudeDropEvidence = (deltaH <= profile.altitudeDropMinM);

          if (!rawCsvMode) {
            Serial.println("================================================================================");
            Serial.printf("[ALERT] FALL CONFIRMED (impact=%.2f m/s^2 | %.2fg, stillness=%u ms, deltaP=%+.1f Pa, deltaH=%+.2f m, gyro=%s, dH=%s)\n",
                          impactPeakMs2, impactPeakMs2 / 9.81, quietDuration, deltaP, deltaH,
                          (gyroTurnEvidence ? "YES" : "NO"),
                          (altitudeDropEvidence ? "YES" : "NO"));
            if (deltaP >= profile.pressureEvidenceMinRisePa) {
              Serial.printf("        [CORROBORATED] Barometric pressure rise %+.1f Pa >= %.1f Pa threshold\n",
                            deltaP, profile.pressureEvidenceMinRisePa);
            }
            if (gyroTurnEvidence) {
              Serial.printf("        [CORROBORATED] Gyro turn rate >= %.1f dps threshold\n",
                            profile.gyroTurnThresholdDps);
            }
            if (altitudeDropEvidence) {
              Serial.printf("        [CORROBORATED] Altitude drop %+.2f m <= %.2f m threshold\n",
                            deltaH, profile.altitudeDropMinM);
            }
            Serial.println("================================================================================");
          }
        }
      }
      break;
    }

    case STATE_FALL_CONFIRMED: {
      // Giữ cảnh báo trong 3 giây debounce rồi tự động quay về NORMAL
      if ((now - confirmedAlertTimeMs) > 3000) {
        currentState = STATE_NORMAL;
        quietStartTimeMs = 0;
        stillnessSampleCount = 0;
        freeFallStartTimeMs = 0;
        gyroTurnEvidence = false;
        altitudeDropEvidence = false;
        pendingEventTag = "ALERT_HOLD_END";
        if (!rawCsvMode) {
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

void printCsvHeader() {
  Serial.println("timestamp_ms,ax_ms2,ay_ms2,az_ms2,acc_mag,gx_dps,gy_dps,gz_dps,gyro_mag,p_pa,temp_c,alt_m,state,event");
}

void outputCsvRow(const SensorSnapshot& s, const char* eventTag) {
  Serial.printf("%lu,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f,%s,%s\n",
                s.timestampMs, s.ax, s.ay, s.az, s.accelMagnitude,
                s.gx, s.gy, s.gz, s.gyroMagnitude,
                s.pressurePa, s.temperatureC, s.altitudeDeltaM,
                fallStateToString(currentState), eventTag);
}

void outputHumanLog(const SensorSnapshot& s) {
  Serial.printf("[OK] MPU9250 | Acc=%.2fg%s | Gyro=%.0f°/s%s | Still=%s\n",
                s.accelMagnitude / 9.81, (accelSaturated ? " [SAT!]" : ""),
                s.gyroMagnitude, (gyroSaturated ? " [SAT!]" : ""),
                (s.stationary ? "YES" : "NO"));

  if (baroAvailable) {
    Serial.printf("[OK] GY63   | P=%.2f hPa | ΔH=%+.2f m\n",
                  s.pressurePa / 100.0, s.altitudeDeltaM);
  } else {
    Serial.println("[--] GY63   | UNAVAILABLE");
  }

  Serial.printf("[STATE] %s\n", fallStateToString(currentState));
  Serial.printf("[RISK] Fall score: %d%%\n", calculateFallRiskScore());
}

void printProfileTable() {
  Serial.println("\n--- BẢNG THAM SỐ CẤU HÌNH THỬ NGHIỆM (FALL PROFILE) ---");
  Serial.printf("Profile ID                     : %s\n", PROFILE_NAME);
  Serial.printf("impactAccelerationMs2          : %.2f m/s^2 (%.2fg) [Nguồn: Android FallDetectionProfiles.kt]\n",
                profile.impactAccelerationMs2, profile.impactAccelerationMs2 / 9.81);
  Serial.printf("stillnessTargetAccelerationMs2 : %.2f m/s^2 [Nguồn: Android]\n", profile.stillnessTargetAccelerationMs2);
  Serial.printf("stillnessToleranceMs2          : %.2f m/s^2 [Nguồn: Android]\n", profile.stillnessToleranceMs2);
  Serial.printf("postImpactWindowMs             : %u ms [Nguồn: Android]\n", profile.postImpactWindowMs);
  Serial.printf("postImpactStillnessDurationMs  : %u ms [Nguồn: Android]\n", profile.postImpactStillnessDurationMs);
  Serial.printf("minimumStillnessSamples        : %u mẫu [Nguồn: Android]\n", profile.minimumStillnessSamples);
  Serial.printf("maximumSampleGapMs             : %u ms [Nguồn: Android]\n", profile.maximumSampleGapMs);
  Serial.printf("freeFallThresholdMs2           : %.2f m/s^2 (%.2fg) [Suy luận kỹ thuật]\n",
                profile.freeFallThresholdMs2, profile.freeFallThresholdMs2 / 9.81);
  Serial.printf("freeFallMinDurationMs          : %u ms [Suy luận kỹ thuật]\n", profile.freeFallMinDurationMs);
  Serial.printf("gyroTurnThresholdDps           : %.1f dps [Suy luận kỹ thuật]\n", profile.gyroTurnThresholdDps);
  Serial.printf("pressureEvidenceMinRisePa      : %.1f Pa [Nguồn: Android]\n", profile.pressureEvidenceMinRisePa);
  Serial.printf("pressureWindowMs               : %u ms [Nguồn: Android DemoLogic.kt PRESSURE_WINDOW_NS]\n", profile.pressureWindowMs);
  Serial.printf("altitudeDropMinM               : %.2f m [Suy luận kỹ thuật]\n", profile.altitudeDropMinM);
  Serial.println("----------------------------------------------------------\n");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      rawCsvMode = !rawCsvMode;
      if (rawCsvMode) {
        printCsvHeader();
      } else {
        Serial.println("\n[MODE] Switched to HUMAN READABLE mode (115200 baud).");
      }
    } else if (c == 't' || c == 'T') {
      printProfileTable();
    } else if (c == 'c' || c == 'C') {
      Serial.println("\n[CMD] Yêu cầu hiệu chuẩn lại (Recalibrating)...");
      currentState = STATE_CALIBRATING;
      // Kích hoạt hiệu chuẩn
      performBootCalibration();
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

void performBootCalibration() {
  currentState = STATE_CALIBRATING;
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
    pollMS5611(nowUs);

    if (nowUs - lastImuReadUs >= 10000) { // 100 Hz
      lastImuReadUs = nowUs;
      uint8_t buf[14];
      if (i2cReadBytes(mpuAddress, MPU_REG_ACCEL_XOUT_H, buf, 14)) {
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
        if (fabs(aMag - profile.stillnessTargetAccelerationMs2) > profile.stillnessToleranceMs2) {
          accelOutOfToleranceSamples++;
        }

        sumGx += (double)rawGx * GYRO_SCALE_2000DPS;
        sumGy += (double)rawGy * GYRO_SCALE_2000DPS;
        sumGz += (double)rawGz * GYRO_SCALE_2000DPS;
        imuSamples++;
      }
    }

    if (baroAvailable && currentSample.pressurePa >= 30000.0 && pCount < 60) {
      // Chỉ lưu nếu mẫu áp suất mới xuất hiện
      if (pCount == 0 || currentSample.pressurePa != pSamples[pCount - 1]) {
        pSamples[pCount++] = currentSample.pressurePa;
      }
    }
    delay(2);
  }

  // 1. Tính toán Gyro Bias & Cảnh báo rung lắc khi hiệu chuẩn
  if (imuSamples > 50) {
    gyroBiasX = sumGx / imuSamples;
    gyroBiasY = sumGy / imuSamples;
    gyroBiasZ = sumGz / imuSamples;
    Serial.printf("[CAL] Gyro Bias: X=%.2f, Y=%.2f, Z=%.2f dps (%u samples)\n",
                  gyroBiasX, gyroBiasY, gyroBiasZ, imuSamples);

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
    baselinePressurePa = (pCount % 2 == 1) ? pSamples[pCount / 2] : (pSamples[pCount / 2 - 1] + pSamples[pCount / 2]) * 0.5;
    baselineValid = true;
    Serial.printf("[CAL] Pressure Baseline P0: %.2f Pa (%.2f hPa, %u samples)\n",
                  baselinePressurePa, baselinePressurePa / 100.0, pCount);
  } else {
    Serial.println("[WARN] Insufficient baro samples for baseline! Using default 101325 Pa.");
    baselinePressurePa = 101325.0;
    baselineValid = false;
  }

  Serial.println("[READY] Monitoring started. State transitioned to NORMAL.");
  currentState = STATE_NORMAL;
}

// ============================================================================
// 10. ARDUINO SETUP & MAIN LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Chờ cổng Serial mở

  Serial.println("\n\n========================================================");
  Serial.println("  ESP32 FALL DETECTION TEST RIG — FIRMWARE TEST_RIG");
  Serial.println("========================================================");
  Serial.println("Board   : ESP32-WROOM-32 (ESP32 Dev Module)");
  Serial.println("IMU     : MPU9250 (Address 0x68/0x69)");
  Serial.println("Baro    : GY-63 / MS5611 (Address 0x77/0x76)");
  Serial.printf("Wiring  : MPU  (Bus0/Wire)  SDA=GPIO%d SCL=GPIO%d | Baro (Bus1/Wire1) SDA=GPIO%d SCL=GPIO%d (%u Hz)\n",
                PIN_IMU_I2C_SDA, PIN_IMU_I2C_SCL, PIN_BARO_I2C_SDA, PIN_BARO_I2C_SCL, I2C_CLOCK_FREQ_HZ);
  Serial.println("// VERIFY_TEST_WIRING: Xác minh nối dây vật lý khớp esp.jpeg trước khi cấp nguồn!");
  Serial.println("========================================================\n");

  // Khởi tạo 2 bus I2C trên ESP32-WROOM-32 (DevKit RF 38 pin)
  // Bus 0 (Wire)  = MPU9250  : SDA=GPIO33, SCL=GPIO32
  // Bus 1 (Wire1) = GY-63    : SDA=GPIO26, SCL=GPIO27
  Wire.begin(PIN_IMU_I2C_SDA, PIN_IMU_I2C_SCL, I2C_CLOCK_FREQ_HZ);
  Wire1.begin(PIN_BARO_I2C_SDA, PIN_BARO_I2C_SCL, BARO_I2C_CLOCK_FREQ_HZ);

  // Khởi tạo MPU9250
  if (!initMPU9250()) {
    Serial.println("[FATAL] IMU initialization failed! System halted.");
    while (true) {
      delay(1000);
    }
  }

  // Khởi tạo MS5611
  initMS5611();

  setupBle();

  // In bảng tham số profile
  printProfileTable();

  // Hiệu chuẩn lúc boot
  performBootCalibration();

  if (rawCsvMode) {
    printCsvHeader();
  }
}

void loop() {
  uint32_t nowUs = micros();
  uint32_t nowMs = millis();

  // Xử lý các lệnh nhập từ Serial
  handleSerialCommands();

  // 0. Nếu GY-63 chưa lên, tự thử lại mỗi ~2 s (vào dây muộn / nguồn trễ)
  if (!baroAvailable && (nowMs - lastBaroRetryMs) >= 2000) {
    lastBaroRetryMs = nowMs;
    Serial.println("[WARN] Retrying GY-63 (MS5611) init...");
    initMS5611();
  }

  // 1. Đọc cảm biến áp suất không chặn
  pollMS5611(nowUs);

  // 2. Chu kỳ đọc IMU 100 Hz (mỗi 10 ms)
  if ((nowUs - lastImuReadUs) >= 10000) {
    lastImuReadUs = nowUs;
    currentSample.timestampMs = nowMs;

    if (readMPU9250(currentSample)) {
      imuSampleCount++;

      // Xử lý thuật toán phát hiện ngã (cập nhật pendingEventTag nếu có sự kiện)
      processFallDetection(currentSample);

      if (bleConnected && pendingEventTag[0] != 'N') {
        String event = "{\"protocolVersion\":1,\"eventId\":\"evt-" + String(currentSample.timestampMs) + "\",\"deviceId\":\"ble_server\",\"sequenceNumber\":";
        event += String(bleSequence + 1); event += ",\"timestampMs\":"; event += String(currentSample.timestampMs);
        event += ",\"eventType\":\""; event += pendingEventTag; event += "\",\"severity\":\"CRITICAL\",\"alertState\":\"";
        event += fallStateToString(currentState); event += "\",\"peakAccelerationMs2\":"; event += String(impactPeakMs2, 3);
        event += ",\"altitudeDeltaM\":"; event += String(currentSample.altitudeDeltaM, 3); event += "}";
        notifyFramed(eventNotify, IF003_KIND_EVENT, event);
      }
      if (bleConnected && (nowMs - lastTelemetryMs) >= 100) {
        lastTelemetryMs = nowMs;
        notifyFramed(telemetryNotify, IF003_KIND_SENSOR, telemetryJson());
      }

      // Nếu ở chế độ CSV raw, xuất đúng nhịp 100 Hz kèm event tag nếu có
      if (rawCsvMode) {
        outputCsvRow(currentSample, pendingEventTag);
      }
      // Reset pendingEventTag về "NONE" sau khi xử lý mẫu (chỉ dòng đầu tiên mang tag)
      pendingEventTag = "NONE";
    }
  }

  // 3. Chu kỳ in HUMAN log (mỗi 1000 ms = 1 chu kỳ 4 dòng/giây, thỏa yêu cầu 2–5 dòng/giây)
  if (!rawCsvMode && (nowMs - lastHumanLogMs) >= 1000) {
    lastHumanLogMs = nowMs;
    outputHumanLog(currentSample);
  }
}
