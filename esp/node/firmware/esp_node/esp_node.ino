// esp_node.ino — T13 ESP32-S3 sensor node: MPU6050 (100 Hz) + MS5611 (25 Hz) CSV logger.
//
// DATA ACQUISITION ONLY. No fall threshold, no state machine, no classifier, no filtering,
// no smoothing, no clipping, no gyro-bias correction: the logged numbers are the raw
// measurements (T13 sections 2 and 6).
//
// Structure: sensor acquisition (mpu6050.*/ms5611.* over i2c_bus.h) / feature math
// (node_math.*) / logging (csv_row.* + this sketch) are separate. arduino_i2c_bus.cpp and
// this sketch are the only translation units that include Arduino.h / Wire.h.
//
// Wire contract with the host (T13 section 7):
//   * `#`-prefixed information lines BEFORE the logger starts (startup report),
//   * then the CSV header line exactly once,
//   * then ONLY CSV rows — no text, no decoration, no blank lines, ever.
#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "arduino_i2c_bus.h"
#include "csv_row.h"
#include "mpu6050.h"
#include "ms5611.h"
#include "node_config.h"
#include "node_math.h"

namespace cfg = esp_node::node_config;

// ---------------------------------------------------------------------------
// Hardware: two independent I2C controllers (T13 section 3)
// ---------------------------------------------------------------------------
TwoWire bus_imu(cfg::kImuBusNumber);    // ESP32 I2C controller 0 -> MPU6050 (GY-521)
TwoWire bus_baro(cfg::kBaroBusNumber);  // ESP32 I2C controller 1 -> MS5611  (GY-63)

esp_node::ArduinoI2cBus imu_bus(bus_imu);
esp_node::ArduinoI2cBus baro_bus(bus_baro);

esp_node::Mpu6050 imu(imu_bus);
esp_node::Ms5611 baro(baro_bus);
esp_node::BaroBaseline baseline;

// All values come from node_config.h (DLPF 1 / smplrt 9 / FS 0,0, OSR 4096, 40 ms cycle).
esp_node::MpuConfig mpu_config{cfg::kDlpfCfg, cfg::kSmplrtDiv, cfg::kGyroFsSel,
                               cfg::kAccelFsSel, cfg::kMpuResetWaitMs};
esp_node::Ms5611Config baro_config{cfg::kBaroAddressPrimary, cfg::kBaroOsrIndex,
                                   cfg::kBaroCycleIntervalUs};

// ---------------------------------------------------------------------------
// Logger state
// ---------------------------------------------------------------------------
bool mpu_ready = false;        // true only after a complete, verified MPU6050 init
uint32_t mpu_fail_streak = 0;  // consecutive failed burst reads; triggers bounded re-detection
uint64_t next_mpu_us = 0;      // 100 Hz deadline on the wrap-safe microsecond clock
uint64_t skipped_slots = 0;    // MPU slots lost to a late loop (tracked, never printed mid-run)
uint32_t last_mpu_retry_ms = 0;
uint32_t last_baro_retry_ms = 0;
bool imu_bus_started = false;
bool baro_bus_started = false;

// Blocking millisecond wait handed to the MPU driver: the only permitted blocking waits are
// the device reset delays inside begin() (T13 section 6).
void DelayMs(uint32_t milliseconds) { delay(milliseconds); }

// Wrap-safe monotonic microsecond clock: micros() is 32-bit and wraps after ~71.6 min, but
// the MS5611 intervals and the 100 Hz deadline must stay correct for a long session.
uint64_t NowUs() {
  static uint32_t last_raw = 0;
  static uint64_t high = 0;
  const uint32_t raw = micros();
  if (raw < last_raw) {
    high += 0x100000000ULL;
  }
  last_raw = raw;
  return high + static_cast<uint64_t>(raw);
}

// Bounded startup window: every in-band MS5611 sample goes into the baseline, which freezes
// itself after kCapacity (40) samples or when the window expires. The datum never moves again.
void CollectBaseline() {
  baseline.reset();
  const uint64_t window_end_us = NowUs() + static_cast<uint64_t>(cfg::kBaselineWindowMs) * 1000ULL;
  for (;;) {
    const uint64_t now_us = NowUs();
    if (baro.state() == esp_node::Ms5611::State::kError) {
      break;  // no device / PROM failure: no sample can ever arrive
    }
    if (baro.poll(now_us)) {
      const esp_node::Ms5611Sample& sample = baro.lastSample();
      if (sample.in_band) {
        baseline.addSample(sample.pressure_pa);  // out-of-band samples are rejected here
      }
    }
    if (baseline.full()) {
      break;  // 40 samples collected -> BaroBaseline froze itself
    }
    if (NowUs() >= window_end_us) {
      break;
    }
  }
  baseline.finalize();
}

// Startup report: `#`-prefixed, printed before the header. It names WHICH sensor failed and
// why (driver statusText()/errorText()/lastWhoAmI()), and never prints a NaN baseline.
void PrintStartupReport() {
  const bool mpu_ok = imu.status() == esp_node::MpuStatus::kOk;
  const bool baro_ok = baro.promRead() && !baro.hasError();
  const bool baseline_ok = baseline.valid();

  Serial.printf("# firmware=%s version=%s\n", cfg::kFirmwareName, cfg::kFirmwareVersion);
  Serial.printf("# board=esp32s3-super-mini\n");
  Serial.printf("# imu_bus=%u sda=%d scl=%d hz=%lu started=%s\n",
                static_cast<unsigned>(cfg::kImuBusNumber), cfg::kImuSdaPin, cfg::kImuSclPin,
                static_cast<unsigned long>(cfg::kI2cFrequencyHz), imu_bus_started ? "ok" : "FAIL");
  Serial.printf("# baro_bus=%u sda=%d scl=%d hz=%lu started=%s\n",
                static_cast<unsigned>(cfg::kBaroBusNumber), cfg::kBaroSdaPin, cfg::kBaroSclPin,
                static_cast<unsigned long>(cfg::kI2cFrequencyHz), baro_bus_started ? "ok" : "FAIL");

  if (imu.address() != 0) {
    Serial.printf("# mpu_addr=0x%02X (fallback 0x%02X)\n", static_cast<unsigned>(imu.address()),
                  static_cast<unsigned>(cfg::kMpuAddressAlt));
  } else {
    Serial.printf("# mpu_addr=none (tried 0x%02X and 0x%02X)\n",
                  static_cast<unsigned>(cfg::kMpuAddressPrimary),
                  static_cast<unsigned>(cfg::kMpuAddressAlt));
  }
  Serial.printf("# mpu_init=%s status=%s who_am_i=0x%02X (expected 0x%02X)\n",
                mpu_ok ? "ok" : "FAIL", imu.statusText(),
                static_cast<unsigned>(imu.lastWhoAmI()),
                static_cast<unsigned>(cfg::kMpuWhoAmIExpected));
  Serial.printf("# mpu_dlpf_cfg=%u smplrt_div=%u gyro_fs_sel=%u (+-%d dps) accel_fs_sel=%u (+-%d g) rate_hz=%lu\n",
                static_cast<unsigned>(cfg::kDlpfCfg), static_cast<unsigned>(cfg::kSmplrtDiv),
                static_cast<unsigned>(cfg::kGyroFsSel), cfg::kGyroFsDps,
                static_cast<unsigned>(cfg::kAccelFsSel), cfg::kAccelFsG,
                static_cast<unsigned long>(cfg::kMpuRateHz));

  if (baro.address() != 0) {
    Serial.printf("# baro_addr=0x%02X (fallback 0x%02X)\n", static_cast<unsigned>(baro.address()),
                  static_cast<unsigned>(cfg::kBaroAddressAlt));
  } else {
    Serial.printf("# baro_addr=none (tried 0x%02X and 0x%02X)\n",
                  static_cast<unsigned>(cfg::kBaroAddressPrimary),
                  static_cast<unsigned>(cfg::kBaroAddressAlt));
  }
  Serial.printf("# ms5611_init=%s state=%s error=%s osr=%u rate_hz=%lu cycle_interval_us=%lu\n",
                baro_ok ? "ok" : "FAIL", baro.stateText(), baro.errorText(),
                static_cast<unsigned>(cfg::kBaroOsrIndex),
                static_cast<unsigned long>(cfg::kBaroRateHz),
                static_cast<unsigned long>(cfg::kBaroCycleIntervalUs));

  if (baro.promRead()) {
    Serial.printf("# ms5611_prom C1=%u C2=%u C3=%u C4=%u C5=%u C6=%u\n",
                  static_cast<unsigned>(baro.c(1)), static_cast<unsigned>(baro.c(2)),
                  static_cast<unsigned>(baro.c(3)), static_cast<unsigned>(baro.c(4)),
                  static_cast<unsigned>(baro.c(5)), static_cast<unsigned>(baro.c(6)));
    Serial.printf("# ms5611_crc4 computed=%u stored=%u %s\n",
                  static_cast<unsigned>(baro.computedCrc()), static_cast<unsigned>(baro.promCrc()),
                  baro.crcOk() ? "match" : "MISMATCH (informational only, pressure data still used)");
  } else {
    Serial.printf("# ms5611_prom=unreadable (PROM words C1..C6 unknown, CRC unavailable)\n");
  }

  if (baseline_ok) {
    Serial.printf("# baseline_pa=%.1f baseline_samples=%u window_ms=%lu repeat_last_pressure=%s\n",
                  baseline.baselinePa(), static_cast<unsigned>(baseline.count()),
                  static_cast<unsigned long>(cfg::kBaselineWindowMs),
                  cfg::kRepeatLastPressure ? "true" : "false");
  } else {
    // baselinePa() is NaN when invalid: it is never printed and never replaced by 0.
    Serial.printf("# baseline_pa=invalid baseline_samples=%u window_ms=%lu repeat_last_pressure=%s\n",
                  static_cast<unsigned>(baseline.count()),
                  static_cast<unsigned long>(cfg::kBaselineWindowMs),
                  cfg::kRepeatLastPressure ? "true" : "false");
    Serial.printf("# WARNING barometric baseline INVALID (need >= %u in-band samples): relative_altitude_m stays EMPTY for this whole run and ALT_VALID is never set\n",
                  static_cast<unsigned>(cfg::kBaselineRequiredSamples));
  }

  if (!mpu_ok) {
    Serial.printf("# WARNING MPU6050 (GY-521) init FAILED on I2C bus %u (SDA=%d SCL=%d, addr 0x%02X/0x%02X): %s, who_am_i=0x%02X -- every row carries the MPU_ERR flag until it is detected\n",
                  static_cast<unsigned>(cfg::kImuBusNumber), cfg::kImuSdaPin, cfg::kImuSclPin,
                  static_cast<unsigned>(cfg::kMpuAddressPrimary),
                  static_cast<unsigned>(cfg::kMpuAddressAlt), imu.statusText(),
                  static_cast<unsigned>(imu.lastWhoAmI()));
  }
  if (!baro_ok) {
    Serial.printf("# WARNING MS5611 (GY-63) init FAILED on I2C bus %u (SDA=%d SCL=%d, addr 0x%02X/0x%02X): %s (%s) -- pressure/temperature/altitude stay EMPTY and rows carry P_ERR until it is detected\n",
                  static_cast<unsigned>(cfg::kBaroBusNumber), cfg::kBaroSdaPin, cfg::kBaroSclPin,
                  static_cast<unsigned>(cfg::kBaroAddressPrimary),
                  static_cast<unsigned>(cfg::kBaroAddressAlt), baro.errorText(), baro.stateText());
  }
}

void setup() {
  Serial.begin(cfg::kSerialBaud);
#if ARDUINO_USB_CDC_ON_BOOT
  // Native USB CDC: a write must never wait for the host (Serial0/USB-UART has no such method).
  Serial.setTxTimeoutMs(0);
#endif
  // Bounded wait for a host: 3000 ms maximum, never an infinite block.
  while (!Serial && millis() < cfg::kSerialWaitMs) {
    delay(1);
  }

  imu_bus_started = imu_bus.begin(cfg::kImuSdaPin, cfg::kImuSclPin, cfg::kI2cFrequencyHz);
  baro_bus_started = baro_bus.begin(cfg::kBaroSdaPin, cfg::kBaroSclPin, cfg::kI2cFrequencyHz);

  // MPU6050: blocking only for the two >= 100 ms device-reset waits inside begin().
  mpu_ready = imu.begin(mpu_config, DelayMs);
  // MS5611: fully non-blocking — the reset wait and the PROM read finish inside later poll().
  baro.begin(baro_config, NowUs());

  CollectBaseline();
  PrintStartupReport();

  Serial.println(esp_node::kCsvHeader);  // printed exactly once, then CSV rows only

  const uint32_t now_ms = static_cast<uint32_t>(NowUs() / 1000ULL);
  last_mpu_retry_ms = now_ms;
  last_baro_retry_ms = now_ms;
  next_mpu_us = NowUs();
}

void loop() {
  const uint64_t now_us = NowUs();

  // -------------------------------------------------------------------------
  // MS5611: polled every iteration. poll() returns true exactly once per completed
  // sample and starts a new cycle at most once every 40 ms (25 Hz).
  // -------------------------------------------------------------------------
  const bool pressure_fresh = baro.poll(now_us);

  // -------------------------------------------------------------------------
  // Bounded re-detection: a sensor that failed at startup is retried at most once every
  // 5 s. The outcome is visible through the flags column only — never as text on the wire.
  // -------------------------------------------------------------------------
  const uint32_t now_ms = static_cast<uint32_t>(now_us / 1000ULL);
  if (!mpu_ready && (now_ms - last_mpu_retry_ms) >= cfg::kRetryIntervalMs) {
    last_mpu_retry_ms = now_ms;
    mpu_ready = imu.begin(mpu_config, DelayMs);
    next_mpu_us = NowUs();  // no burst of catch-up rows after a slow re-init
  }
  if (baro.hasError() && (now_ms - last_baro_retry_ms) >= cfg::kRetryIntervalMs) {
    last_baro_retry_ms = now_ms;
    baro.begin(baro_config, NowUs());
  }

  // -------------------------------------------------------------------------
  // MPU6050: 100 Hz, non-blocking. The deadline advances by the nominal period; if more
  // than one period was missed the deadline resyncs to now and the lost slots are counted.
  // -------------------------------------------------------------------------
  if (now_us < next_mpu_us) {
    return;
  }
  next_mpu_us += cfg::kMpuPeriodUs;
  if (now_us > next_mpu_us && (now_us - next_mpu_us) >= cfg::kMpuPeriodUs) {
    skipped_slots += (now_us - next_mpu_us) / cfg::kMpuPeriodUs + 1ULL;
    next_mpu_us = now_us + cfg::kMpuPeriodUs;
  }

  esp_node::CsvRow row;
  row.timestamp_ms = static_cast<uint32_t>(now_us / 1000ULL);
  uint16_t flags = 0;

  esp_node::MpuSample mpu_sample;
  if (imu.read(mpu_sample)) {
    row.has_accel = true;
    row.ax = mpu_sample.ax;
    row.ay = mpu_sample.ay;
    row.az = mpu_sample.az;
    row.a_mag = esp_node::magnitude3(mpu_sample.ax, mpu_sample.ay, mpu_sample.az);
    row.has_gyro = true;
    row.gx = mpu_sample.gx;
    row.gy = mpu_sample.gy;
    row.gz = mpu_sample.gz;
    row.g_mag = esp_node::magnitude3(mpu_sample.gx, mpu_sample.gy, mpu_sample.gz);
    flags |= esp_node::csv_flags::kMpuOk;
    mpu_fail_streak = 0;
  } else {
    // All accel/gyro fields stay EMPTY (nothing fake, nothing zero-filled).
    flags |= esp_node::csv_flags::kMpuErr;
    ++mpu_fail_streak;
    if (mpu_ready && mpu_fail_streak >= 20) {
      // ~200 ms of sustained read failures: allow the bounded 5 s re-detection to run again.
      mpu_ready = false;
      mpu_fail_streak = 0;
    }
  }

  // Pressure columns are filled ONLY on a row where a new sample completed
  // (kRepeatLastPressure == false): a stale reading is never repeated as a new measurement.
  if (pressure_fresh) {
    const esp_node::Ms5611Sample& sample = baro.lastSample();
    if (!std::isfinite(sample.pressure_pa) || !std::isfinite(sample.temperature_c)) {
      flags |= esp_node::csv_flags::kPErr;  // unusable sample: no field is filled
    } else if (sample.in_band) {
      row.has_pressure = true;
      row.pressure_pa = sample.pressure_pa;
      row.temperature_c = sample.temperature_c;
      flags |= esp_node::csv_flags::kPFresh;
      double altitude_m = 0.0;
      if (baseline.altitudeFor(sample.pressure_pa, altitude_m)) {
        row.has_altitude = true;
        row.relative_altitude_m = altitude_m;
        flags |= esp_node::csv_flags::kAltValid;
      }
    } else {
      flags |= esp_node::csv_flags::kPOutOfRange;  // read fine, rejected by the band
    }
  } else if (baro.hasError()) {
    flags |= esp_node::csv_flags::kPErr;
  }

  if (baro.promRead() && !baro.crcOk()) {
    flags |= esp_node::csv_flags::kPromCrcMismatch;  // informational: pressure data is still used
  }
  row.flags = flags;

  char buf[esp_node::kCsvMaxRowBytes];
  if (esp_node::formatCsvRow(row, buf, sizeof(buf))) {
    Serial.println(buf);
  }
  // On truncation nothing is printed: a mangled row must never reach the host.
}
