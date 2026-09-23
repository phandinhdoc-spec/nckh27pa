// arduino_i2c_bus.cpp — TwoWire implementation of the I2cBus interface (device only).
#include "arduino_i2c_bus.h"

#include "node_config.h"

namespace esp_node {

bool ArduinoI2cBus::begin(int sda_pin, int scl_pin, uint32_t frequency_hz) {
  started_ = wire_.begin(sda_pin, scl_pin, frequency_hz);
  if (started_) {
    // Bounded transaction timeout: a stuck bus must surface as a failed transfer, never as a
    // frozen sample loop.
    wire_.setTimeOut(node_config::kI2cTimeoutMs);
  }
  return started_;
}

bool ArduinoI2cBus::readReg(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
  if (!started_ || data == nullptr || len == 0 || len > 0xFF) {
    return false;
  }

  wire_.beginTransmission(addr);
  if (wire_.write(reg) != 1) {
    wire_.endTransmission(true);
    return false;
  }
  if (wire_.endTransmission(false) != 0) {  // repeated START, no STOP
    return false;
  }
  const size_t received = wire_.requestFrom(addr, static_cast<uint8_t>(len));
  if (received != len) {
    return false;  // short read: nothing is copied out, so no partially updated output
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

bool ArduinoI2cBus::writeReg(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) {
  if (!started_ || len > 0xFF) {
    return false;
  }
  if (len > 0 && data == nullptr) {
    return false;
  }

  wire_.beginTransmission(addr);
  if (wire_.write(reg) != 1) {
    wire_.endTransmission(true);
    return false;
  }
  if (len > 0) {
    const size_t written = wire_.write(data, len);
    if (written != len) {
      wire_.endTransmission(true);
      return false;
    }
  }
  return wire_.endTransmission(true) == 0;  // 0 == every byte acknowledged
}

}  // namespace esp_node
