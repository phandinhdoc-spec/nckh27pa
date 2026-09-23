// arduino_i2c_bus.h — device-side I2cBus adapter on top of the ESP32 Arduino Wire library.
//
// This header and its .cpp are the ONLY place (next to esp_node.ino) that pulls in
// Arduino.h / Wire.h; every sensor, math and formatting module stays pure C++17 and is
// testable on the host with tests/fake_i2c.h.
//
// Two objects of this class wrap two *independent* TwoWire controllers, so the MPU6050
// (bus 0) and the MS5611 (bus 1) never arbitrate for the same peripheral.
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "i2c_bus.h"

namespace esp_node {

class ArduinoI2cBus : public I2cBus {
 public:
  explicit ArduinoI2cBus(TwoWire& wire) : wire_(wire) {}

  // Starts the controller on the given pins. Returns false when the board rejected the
  // pin/clock combination; the adapter then fails every transfer instead of pretending.
  bool begin(int sda_pin, int scl_pin, uint32_t frequency_hz);

  bool started() const { return started_; }

  // Sequential register read: START + addr+W + reg, repeated START + addr+R + len bytes.
  // Returns false on a NACK, a short read or a non-started bus; `data` is only written
  // when the whole read succeeded.
  bool readReg(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) override;

  // Register write: START + addr+W + reg + len data bytes + STOP.
  // len == 0 issues the register byte alone (the MS5611 conversion commands).
  // Returns false when any byte was not acknowledged (endTransmission() != 0).
  bool writeReg(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len) override;

 private:
  TwoWire& wire_;
  bool started_ = false;
};

}  // namespace esp_node
