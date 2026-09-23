// i2c_bus.h — abstract register-level I2C interface for the T13 sensor node.
//
// Pure C++17. Must NOT include Arduino.h / Wire.h / any ESP header: the device-side
// adapter (arduino_i2c_bus.*) and the host fake (tests/fake_i2c.h) both implement this
// interface, so the drivers are testable on the host with a scripted register file.
#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_node {

class I2cBus {
 public:
  virtual ~I2cBus() = default;

  // Writes `len` bytes to the register/command byte `reg` of device `addr`.
  // len == 0 issues the command byte alone (used by the MS5611 conversion commands).
  // Returns false when the transfer was not acknowledged / failed.
  virtual bool writeReg(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) = 0;

  // Reads `len` bytes starting at the register/command byte `reg` of device `addr`
  // (sequential register read, as required by the MPU6050 14-byte burst and the
  // MS5611 ADC/PROM reads). Returns false when the transfer failed.
  virtual bool readReg(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) = 0;
};

}  // namespace esp_node
