// mpu6050.h — register-level MPU6050 (GY-521) driver.
//
// Pure C++17: no Arduino.h / Wire.h / ESP headers, no dynamic allocation.
// Register map, init order, scaling and delays are exactly T13 section 5.1.
#pragma once

#include <cstdint>

#include "i2c_bus.h"

namespace esp_node {

// T13 section 5.1 register map.
namespace mpu6050_reg {
constexpr uint8_t kSmplrtDiv = 0x19;
constexpr uint8_t kConfig = 0x1A;
constexpr uint8_t kGyroConfig = 0x1B;
constexpr uint8_t kAccelConfig = 0x1C;
constexpr uint8_t kIntEnable = 0x38;
constexpr uint8_t kAccelXoutH = 0x3B;  // 14-byte burst: ax,ay,az,temp,gx,gy,gz
constexpr uint8_t kTempOutH = 0x41;
constexpr uint8_t kGyroXoutH = 0x43;
constexpr uint8_t kSignalPathReset = 0x68;
constexpr uint8_t kUserCtrl = 0x6A;
constexpr uint8_t kPwrMgmt1 = 0x6B;
constexpr uint8_t kPwrMgmt2 = 0x6C;
constexpr uint8_t kWhoAmI = 0x75;

constexpr uint8_t kBurstLen = 14;  // ax, ay, az, temp, gx, gy, gz (big-endian int16 each)
}  // namespace mpu6050_reg

// Blocking millisecond wait supplied by the caller (on the device: `delay`). The pure
// driver never includes a timing header and never calls delay/delayMicroseconds itself.
using DelayMsFn = void (*)(uint32_t milliseconds);

struct MpuConfig {
  uint8_t dlpf_cfg = 1;         // CONFIG register (0..6 -> 260/184/94/44/21/10/5 Hz accel BW)
  uint8_t smplrt_div = 9;       // 1 kHz / (1 + 9) = 100 Hz internal sample rate
  uint8_t gyro_fs_sel = 0;      // GYRO_CONFIG FS_SEL 0 -> +-250 dps
  uint8_t accel_fs_sel = 0;     // ACCEL_CONFIG AFS_SEL 0 -> +-2 g
  uint32_t reset_wait_ms = 100; // >= 100 ms after DEVICE_RESET / SIGNAL_PATH_RESET
};

enum class MpuStatus {
  kOk,
  kNotDetected,     // neither 0x68 nor 0x69 ACKed
  kBusWriteFailed,  // a configuration write failed
  kBusReadFailed,   // WHO_AM_I read failed
  kWhoAmIMismatch,  // WHO_AM_I returned something other than 0x68
};

struct MpuRaw {
  int16_t ax = 0;
  int16_t ay = 0;
  int16_t az = 0;
  int16_t temp = 0;
  int16_t gx = 0;
  int16_t gy = 0;
  int16_t gz = 0;
};

struct MpuSample {  // scaled, no filtering (T13 section 6: keep the measurements raw)
  double ax = 0.0;  // m/s^2
  double ay = 0.0;
  double az = 0.0;
  double gx = 0.0;  // deg/s
  double gy = 0.0;
  double gz = 0.0;
};

class Mpu6050 {
 public:
  static constexpr uint8_t kAddrPrimary = 0x68;
  static constexpr uint8_t kAddrAlt = 0x69;
  static constexpr uint8_t kWhoAmIValue = 0x68;
  static constexpr double kAccelLsbPerG = 16384.0;   // +-2 g full scale
  static constexpr double kGyroLsbPerDps = 131.072;  // 32768 / 250 (+-250 dps full scale)
  static constexpr double kGravityMps2 = 9.80665;

  explicit Mpu6050(I2cBus& bus);

  // Runs T13 section 5.1 steps 1..9 in order. `delay_ms` performs the blocking reset
  // waits (>= 100 ms each); it is mandatory so a device build cannot silently skip them.
  // On failure the reason is in status() and the raw WHO_AM_I byte in lastWhoAmI().
  bool begin(const MpuConfig& config, DelayMsFn delay_ms);
  bool begin(DelayMsFn delay_ms) { return begin(MpuConfig{}, delay_ms); }

  MpuStatus status() const { return status_; }
  const char* statusText() const;
  bool detected() const { return address_ != 0; }
  uint8_t address() const { return address_; }
  uint8_t lastWhoAmI() const { return last_who_am_i_; }

  // One 14-byte burst read from ACCEL_XOUT_H. On failure returns false and leaves
  // `out` completely untouched (no partially updated output).
  bool readRaw(MpuRaw& out);
  bool read(MpuSample& out);

  static double accelToMps2(int16_t raw) {
    return static_cast<double>(raw) / kAccelLsbPerG * kGravityMps2;
  }
  static double gyroToDps(int16_t raw) {
    return static_cast<double>(raw) / kGyroLsbPerDps;
  }

 private:
  bool writeReg(uint8_t reg, uint8_t value);
  bool readReg(uint8_t reg, uint8_t& value);

  I2cBus& bus_;
  MpuConfig config_{};
  uint8_t address_ = 0;
  uint8_t last_who_am_i_ = 0;
  MpuStatus status_ = MpuStatus::kNotDetected;
};

}  // namespace esp_node
