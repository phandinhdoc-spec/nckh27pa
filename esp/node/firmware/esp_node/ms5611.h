// ms5611.h — register-level MS5611 (GY-63) driver, NON-BLOCKING state machine.
//
// Pure C++17: no Arduino.h / Wire.h / ESP headers, no timing header, no delay()/sleep,
// no dynamic allocation. Time enters only through the caller-supplied `now_us`
// (microseconds from micros() on the device, a fake clock in the host tests).
//
// Usage (device and host identical):
//   Ms5611 ms(bus);
//   if (!ms.begin(now_us)) { ... }          // probe + issue reset, never blocks
//   // every loop iteration:
//   if (ms.poll(now_us)) { const Ms5611Sample& s = ms.lastSample(); ... }
//
// poll() returns true exactly once per completed sample (D1 read -> D2 read -> math).
// A conversion cycle is started at most once every config.min_interval_us (default
// 40 ms = 25 Hz); while a cycle is pending, poll() returns false and the caller is free
// to keep sampling the MPU6050.
#pragma once

#include <cstdint>

#include "i2c_bus.h"

namespace esp_node {

// T13 section 5.2 commands and timings.
namespace ms5611_reg {
constexpr uint8_t kCmdReset = 0x1E;
constexpr uint8_t kCmdAdcRead = 0x00;   // then 3 bytes big-endian (24-bit ADC)
constexpr uint8_t kCmdPromBase = 0xA0;  // PROM word i: 0xA0 + 2*i, 2 bytes big-endian
constexpr uint8_t kCmdD1Base = 0x40;    // D1 (pressure):    0x40 + 2*osr_index
constexpr uint8_t kCmdD2Base = 0x50;    // D2 (temperature): 0x50 + 2*osr_index
constexpr uint32_t kResetWaitUs = 3000; // >= 3000 us after reset

constexpr uint8_t kOsrCount = 5;
constexpr uint8_t kOsr4096 = 4;  // OSR 256/512/1024/2048/4096
constexpr uint32_t kMaxConversionWaitUs[kOsrCount] = {600, 1200, 2300, 4600, 9100};

constexpr uint8_t kPromWordCount = 8;  // word 0 = manufacturer data, 1..6 = C1..C6, 7 = CRC
}  // namespace ms5611_reg

struct Ms5611Config {
  uint8_t address = 0x77;            // 0x77 (CSB low) preferred, 0x76 as fallback
  uint8_t osr_index = ms5611_reg::kOsr4096;
  uint32_t min_interval_us = 40000;  // start a cycle at most once per 40 ms (25 Hz)
};

struct Ms5611Sample {
  uint32_t d1 = 0;                // raw pressure ADC word
  uint32_t d2 = 0;                // raw temperature ADC word
  double pressure_pa = 0.0;       // Pa (datasheet mode 0 compensation, double math)
  double temperature_c = 0.0;     // deg C
  bool in_band = false;           // pressure inside the plausibility band
};

class Ms5611 {
 public:
  static constexpr uint8_t kAddrPrimary = 0x77;
  static constexpr uint8_t kAddrAlt = 0x76;

  // Wearable-node plausibility band (T13 section 5.2).
  static constexpr double kPressureMinPa = 30000.0;
  static constexpr double kPressureMaxPa = 110000.0;

  enum class State {
    kUninit,     // no device yet
    kResetWait,  // reset issued, PROM not read yet
    kIdle,       // PROM present, no conversion pending (poll may start one)
    kD1Wait,     // D1 conversion pending
    kD2Wait,     // D2 conversion pending
    kError,      // terminal until begin() is called again
  };

  explicit Ms5611(I2cBus& bus);

  // Probes 0x77 then 0x76, issues the reset command and arms the non-blocking init.
  // Never blocks. Returns false (state kError) when no device ACKs.
  bool begin(const Ms5611Config& config, uint64_t now_us);
  bool begin(uint64_t now_us) { return begin(Ms5611Config{}, now_us); }

  // Advances the state machine. Returns true exactly once per completed sample.
  bool poll(uint64_t now_us);

  State state() const { return state_; }
  bool busy() const { return state_ == State::kResetWait || state_ == State::kD1Wait || state_ == State::kD2Wait; }
  bool initialized() const { return prom_read_ && state_ != State::kError; }
  bool hasError() const { return state_ == State::kError; }
  const char* errorText() const;
  const char* stateText() const;

  uint8_t address() const { return address_; }

  // PROM: word 0 = manufacturer data, 1..6 = C1..C6 (raw uint16), 7 = CRC word.
  bool promRead() const { return prom_read_; }
  uint16_t promWord(uint8_t index) const;
  uint16_t c(uint8_t index) const;  // C1..C6 for index 1..6, 0 otherwise
  uint8_t promCrc() const;          // CRC-4 stored in the low nibble of PROM word 7
  uint8_t computedCrc() const;      // CRC-4 computed over PROM words 0..6 (word 7 zeroed)
  bool crcOk() const;
  // Datasheet (AN520) CRC-4 over 8 PROM words; words[7] must have its CRC nibble cleared
  // (high byte preserved, low byte zeroed). Exposed for the startup report.
  static uint8_t crc4(const uint16_t* prom_words);

  uint32_t sampleCount() const { return sample_count_; }
  bool hasSample() const { return sample_count_ > 0; }
  uint64_t lastStartUs() const { return last_start_us_; }
  const Ms5611Sample& lastSample() const { return last_sample_; }

  static bool pressureInBand(double pressure_pa);

 private:
  bool probeAddress();
  bool readProm();
  bool issueConversion(uint8_t command);
  bool readAdc(uint32_t& out);
  void computeSample();
  void setError(const char* text);
  uint8_t conversionCommand(char which) const;  // '1' -> D1, '2' -> D2

  I2cBus& bus_;
  Ms5611Config config_{};
  State state_ = State::kUninit;
  const char* error_text_ = "none";
  uint8_t address_ = 0;

  uint16_t prom_[ms5611_reg::kPromWordCount] = {};
  bool prom_read_ = false;
  uint8_t computed_crc_ = 0;

  uint64_t deadline_us_ = 0;
  uint64_t last_start_us_ = 0;
  bool started_once_ = false;

  uint32_t d1_ = 0;
  uint32_t d2_ = 0;
  uint32_t sample_count_ = 0;
  Ms5611Sample last_sample_{};
};

}  // namespace esp_node
