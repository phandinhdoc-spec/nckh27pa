// ms5611.cpp — register-level MS5611 driver with a non-blocking conversion cycle.
// Math, commands and timings are exactly T13 section 5.2 (datasheet mode 0, double math).
#include "ms5611.h"

namespace esp_node {
namespace {

inline uint32_t Be24(uint8_t hi, uint8_t mid, uint8_t lo) {
  return (static_cast<uint32_t>(hi) << 16) | (static_cast<uint32_t>(mid) << 8) |
         static_cast<uint32_t>(lo);
}

inline uint16_t Be16(uint8_t hi, uint8_t lo) {
  return static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8) | static_cast<uint16_t>(lo));
}

}  // namespace

Ms5611::Ms5611(I2cBus& bus) : bus_(bus) {}

const char* Ms5611::errorText() const { return error_text_; }

const char* Ms5611::stateText() const {
  switch (state_) {
    case State::kUninit:
      return "uninit";
    case State::kResetWait:
      return "reset_wait";
    case State::kIdle:
      return "idle";
    case State::kD1Wait:
      return "d1_wait";
    case State::kD2Wait:
      return "d2_wait";
    case State::kError:
      return "error";
  }
  return "unknown";
}

bool Ms5611::pressureInBand(double pressure_pa) {
  return pressure_pa >= kPressureMinPa && pressure_pa <= kPressureMaxPa;
}

void Ms5611::setError(const char* text) {
  state_ = State::kError;
  error_text_ = text;
}

uint16_t Ms5611::promWord(uint8_t index) const {
  if (index >= ms5611_reg::kPromWordCount) {
    return 0;
  }
  return prom_[index];
}

uint16_t Ms5611::c(uint8_t index) const {
  if (index < 1 || index > 6) {
    return 0;
  }
  return prom_[index];
}

uint8_t Ms5611::promCrc() const {
  return static_cast<uint8_t>(prom_[7] & 0x000F);
}

uint8_t Ms5611::crc4(const uint16_t* prom_words) {
  // Datasheet AN520 CRC-4 (polynomial x^4 + x + 1 via 0x3000 shift), 16 bytes of
  // PROM words 0..7 with the CRC nibble cleared from word 7.
  uint16_t rem = 0;
  for (uint8_t i = 0; i < 16; ++i) {
    if ((i % 2) == 1) {
      rem = static_cast<uint16_t>(rem ^ static_cast<uint16_t>(prom_words[i >> 1] & 0x00FF));
    } else {
      rem = static_cast<uint16_t>(rem ^ static_cast<uint16_t>(prom_words[i >> 1] >> 8));
    }
    for (uint8_t bit = 8; bit > 0; --bit) {
      if ((rem & 0x8000) != 0) {
        rem = static_cast<uint16_t>(static_cast<uint16_t>(rem << 1) ^ 0x3000);
      } else {
        rem = static_cast<uint16_t>(rem << 1);
      }
    }
  }
  return static_cast<uint8_t>((rem >> 12) & 0x000F);
}

uint8_t Ms5611::computedCrc() const { return computed_crc_; }

bool Ms5611::crcOk() const { return prom_read_ && computed_crc_ == promCrc(); }

bool Ms5611::probeAddress() {
  const uint8_t alternate = (config_.address == kAddrPrimary) ? kAddrAlt : kAddrPrimary;
  uint8_t word[2] = {};
  if (bus_.readReg(config_.address, ms5611_reg::kCmdPromBase, word, sizeof(word))) {
    address_ = config_.address;
    return true;
  }
  if (bus_.readReg(alternate, ms5611_reg::kCmdPromBase, word, sizeof(word))) {
    address_ = alternate;
    return true;
  }
  return false;
}

bool Ms5611::begin(const Ms5611Config& config, uint64_t now_us) {
  config_ = config;
  if (config_.osr_index >= ms5611_reg::kOsrCount) {
    config_.osr_index = ms5611_reg::kOsrCount - 1;
  }
  state_ = State::kUninit;
  error_text_ = "none";
  address_ = 0;
  for (uint8_t i = 0; i < ms5611_reg::kPromWordCount; ++i) {
    prom_[i] = 0;
  }
  prom_read_ = false;
  computed_crc_ = 0;
  deadline_us_ = 0;
  last_start_us_ = 0;
  started_once_ = false;
  d1_ = 0;
  d2_ = 0;
  sample_count_ = 0;
  last_sample_ = Ms5611Sample{};

  if (!probeAddress()) {
    setError("not_detected");
    return false;
  }
  if (!issueConversion(ms5611_reg::kCmdReset)) {
    setError("reset_write_failed");
    return false;
  }
  deadline_us_ = now_us + ms5611_reg::kResetWaitUs;  // poll() finishes the init
  state_ = State::kResetWait;
  return true;
}

bool Ms5611::readProm() {
  for (uint8_t i = 0; i < ms5611_reg::kPromWordCount; ++i) {
    uint8_t word[2] = {};
    const uint8_t reg = static_cast<uint8_t>(ms5611_reg::kCmdPromBase + 2 * i);
    if (!bus_.readReg(address_, reg, word, sizeof(word))) {
      return false;
    }
    prom_[i] = Be16(word[0], word[1]);
  }
  uint16_t for_crc[ms5611_reg::kPromWordCount];
  for (uint8_t i = 0; i < ms5611_reg::kPromWordCount; ++i) {
    for_crc[i] = prom_[i];
  }
  for_crc[7] = static_cast<uint16_t>(for_crc[7] & 0xFF00);  // clear the CRC nibble (AN520)
  computed_crc_ = crc4(for_crc);
  prom_read_ = true;
  return true;
}

bool Ms5611::issueConversion(uint8_t command) {
  return bus_.writeReg(address_, command, nullptr, 0);
}

bool Ms5611::readAdc(uint32_t& out) {
  uint8_t adc[3] = {};
  if (!bus_.readReg(address_, ms5611_reg::kCmdAdcRead, adc, sizeof(adc))) {
    return false;
  }
  out = Be24(adc[0], adc[1], adc[2]);
  return true;
}

uint8_t Ms5611::conversionCommand(char which) const {
  const uint8_t base = (which == '2') ? ms5611_reg::kCmdD2Base : ms5611_reg::kCmdD1Base;
  return static_cast<uint8_t>(base + 2 * config_.osr_index);
}

void Ms5611::computeSample() {
  const double c1 = static_cast<double>(c(1));
  const double c2 = static_cast<double>(c(2));
  const double c3 = static_cast<double>(c(3));
  const double c4 = static_cast<double>(c(4));
  const double c5 = static_cast<double>(c(5));
  const double c6 = static_cast<double>(c(6));

  const double dt = static_cast<double>(d2_) - c5 * 256.0;
  double temp100 = 2000.0 + dt * c6 / 8388608.0;
  double off = c2 * 65536.0 + c4 * dt / 128.0;
  double sens = c1 * 32768.0 + c3 * dt / 256.0;

  if (temp100 < 2000.0) {  // second order temperature compensation
    const double t2 = dt * dt * 4.6566128731e-10;
    double t = (temp100 - 2000.0) * (temp100 - 2000.0);
    double off2 = 2.5 * t;
    double sens2 = 1.25 * t;
    if (temp100 < -1500.0) {
      t = (temp100 + 1500.0) * (temp100 + 1500.0);
      off2 += 7.0 * t;
      sens2 += 5.5 * t;
    }
    temp100 -= t2;
    off -= off2;
    sens -= sens2;
  }

  const double pressure = (static_cast<double>(d1_) * sens / 2097152.0 - off) / 32768.0;

  Ms5611Sample sample;
  sample.d1 = d1_;
  sample.d2 = d2_;
  sample.pressure_pa = pressure;
  sample.temperature_c = temp100 * 0.01;
  sample.in_band = pressureInBand(pressure);
  last_sample_ = sample;
}

bool Ms5611::poll(uint64_t now_us) {
  switch (state_) {
    case State::kUninit:
    case State::kError:
      return false;

    case State::kResetWait:
      if (now_us < deadline_us_) {
        return false;
      }
      if (!readProm()) {
        setError("prom_read_failed");
        return false;
      }
      state_ = State::kIdle;
      return false;

    case State::kIdle:
      if (started_once_ && (now_us - last_start_us_) < config_.min_interval_us) {
        return false;
      }
      if (!issueConversion(conversionCommand('1'))) {
        setError("d1_write_failed");
        return false;
      }
      deadline_us_ = now_us + ms5611_reg::kMaxConversionWaitUs[config_.osr_index];
      last_start_us_ = now_us;
      started_once_ = true;
      state_ = State::kD1Wait;
      return false;

    case State::kD1Wait:
      if (now_us < deadline_us_) {
        return false;
      }
      if (!readAdc(d1_)) {
        setError("d1_read_failed");
        return false;
      }
      if (!issueConversion(conversionCommand('2'))) {
        setError("d2_write_failed");
        return false;
      }
      deadline_us_ = now_us + ms5611_reg::kMaxConversionWaitUs[config_.osr_index];
      state_ = State::kD2Wait;
      return false;

    case State::kD2Wait:
      if (now_us < deadline_us_) {
        return false;
      }
      if (!readAdc(d2_)) {
        setError("d2_read_failed");
        return false;
      }
      state_ = State::kIdle;
      computeSample();
      ++sample_count_;
      return true;  // exactly once per completed sample
  }
  return false;
}

}  // namespace esp_node
