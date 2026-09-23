// mpu6050.cpp — register-level MPU6050 driver, T13 section 5.1.
#include "mpu6050.h"

namespace esp_node {
namespace {

inline int16_t Be16(uint8_t hi, uint8_t lo) {
  return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | static_cast<uint16_t>(lo));
}

inline void WaitMs(DelayMsFn delay_ms, uint32_t ms) {
  if (delay_ms != nullptr) {
    delay_ms(ms);
  }
}

}  // namespace

Mpu6050::Mpu6050(I2cBus& bus) : bus_(bus) {}

const char* Mpu6050::statusText() const {
  switch (status_) {
    case MpuStatus::kOk:
      return "ok";
    case MpuStatus::kNotDetected:
      return "not_detected";
    case MpuStatus::kBusWriteFailed:
      return "bus_write_failed";
    case MpuStatus::kBusReadFailed:
      return "bus_read_failed";
    case MpuStatus::kWhoAmIMismatch:
      return "who_am_i_mismatch";
  }
  return "unknown";
}

bool Mpu6050::writeReg(uint8_t reg, uint8_t value) {
  return bus_.writeReg(address_, reg, &value, 1);
}

bool Mpu6050::readReg(uint8_t reg, uint8_t& value) {
  uint8_t byte = 0;
  if (!bus_.readReg(address_, reg, &byte, 1)) {
    return false;
  }
  value = byte;
  return true;
}

bool Mpu6050::begin(const MpuConfig& config, DelayMsFn delay_ms) {
  config_ = config;
  address_ = 0;
  last_who_am_i_ = 0;
  status_ = MpuStatus::kNotDetected;

  // 1. probe: try 0x68, then 0x69 (a device ACKs if reading WHO_AM_I succeeds).
  uint8_t who = 0;
  if (bus_.readReg(kAddrPrimary, mpu6050_reg::kWhoAmI, &who, 1)) {
    address_ = kAddrPrimary;
  } else if (bus_.readReg(kAddrAlt, mpu6050_reg::kWhoAmI, &who, 1)) {
    address_ = kAddrAlt;
  } else {
    status_ = MpuStatus::kNotDetected;
    return false;
  }

  // 2. device reset.
  if (!writeReg(mpu6050_reg::kPwrMgmt1, 0x80)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }
  WaitMs(delay_ms, config_.reset_wait_ms);

  // 3. WHO_AM_I check: must be 0x68.
  if (!readReg(mpu6050_reg::kWhoAmI, last_who_am_i_)) {
    status_ = MpuStatus::kBusReadFailed;
    return false;
  }
  if (last_who_am_i_ != kWhoAmIValue) {
    status_ = MpuStatus::kWhoAmIMismatch;
    return false;
  }

  // 4. clock source: PLL with X gyro reference.
  if (!writeReg(mpu6050_reg::kPwrMgmt1, 0x01)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  // 5. signal path reset, then all axes on.
  if (!writeReg(mpu6050_reg::kSignalPathReset, 0x07)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }
  WaitMs(delay_ms, config_.reset_wait_ms);
  if (!writeReg(mpu6050_reg::kPwrMgmt2, 0x00)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  // 6. DLPF (named constant so it can be re-tuned).
  if (!writeReg(mpu6050_reg::kConfig, static_cast<uint8_t>(config_.dlpf_cfg & 0x07))) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  // 7. sample rate divider -> 100 Hz.
  if (!writeReg(mpu6050_reg::kSmplrtDiv, config_.smplrt_div)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  // 8. gyro +-250 dps, accel +-2 g.
  if (!writeReg(mpu6050_reg::kGyroConfig, static_cast<uint8_t>(config_.gyro_fs_sel & 0x18))) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }
  if (!writeReg(mpu6050_reg::kAccelConfig, static_cast<uint8_t>(config_.accel_fs_sel & 0x18))) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  // 9. no interrupts (polled).
  if (!writeReg(mpu6050_reg::kIntEnable, 0x00)) {
    status_ = MpuStatus::kBusWriteFailed;
    return false;
  }

  status_ = MpuStatus::kOk;
  return true;
}

bool Mpu6050::readRaw(MpuRaw& out) {
  if (address_ == 0) {
    return false;
  }
  uint8_t burst[mpu6050_reg::kBurstLen] = {};
  if (!bus_.readReg(address_, mpu6050_reg::kAccelXoutH, burst, sizeof(burst))) {
    return false;  // `out` untouched: no partially updated output.
  }
  MpuRaw decoded;
  decoded.ax = Be16(burst[0], burst[1]);
  decoded.ay = Be16(burst[2], burst[3]);
  decoded.az = Be16(burst[4], burst[5]);
  decoded.temp = Be16(burst[6], burst[7]);
  decoded.gx = Be16(burst[8], burst[9]);
  decoded.gy = Be16(burst[10], burst[11]);
  decoded.gz = Be16(burst[12], burst[13]);
  out = decoded;
  return true;
}

bool Mpu6050::read(MpuSample& out) {
  MpuRaw raw;
  if (!readRaw(raw)) {
    return false;
  }
  MpuSample scaled;
  scaled.ax = accelToMps2(raw.ax);
  scaled.ay = accelToMps2(raw.ay);
  scaled.az = accelToMps2(raw.az);
  scaled.gx = gyroToDps(raw.gx);
  scaled.gy = gyroToDps(raw.gy);
  scaled.gz = gyroToDps(raw.gz);
  out = scaled;
  return true;
}

}  // namespace esp_node
