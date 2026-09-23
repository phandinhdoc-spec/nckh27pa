// node_config.h — single place for the T13 sensor-node hardware/rate constants.
//
// Pure C++17: no Arduino.h / Wire.h / ESP header (it only pulls the two pure driver
// headers, so the host tests and the device build see exactly the same numbers).
// Every value here is a deliberate, documented choice from T13 sections 3, 5 and 6.
#pragma once

#include <cstdint>

#include "mpu6050.h"
#include "ms5611.h"
#include "node_math.h"

namespace esp_node {
namespace node_config {

// ---- identity -------------------------------------------------------------
constexpr const char* kFirmwareName = "esp_node";
constexpr const char* kFirmwareVersion = "1.0.0";

// ---- I2C buses (T13 section 3) -------------------------------------------
// TWO independent ESP32 I2C controllers: the MPU6050 and the MS5611 do not share a bus.
constexpr uint8_t kImuBusNumber = 0;   // TwoWire bus 0 -> MPU6050 (GY-521)
constexpr int kImuSdaPin = 7;          // GPIO7
constexpr int kImuSclPin = 6;          // GPIO6
constexpr uint8_t kBaroBusNumber = 1;  // TwoWire bus 1 -> MS5611 (GY-63)
constexpr int kBaroSdaPin = 3;         // GPIO3
constexpr int kBaroSclPin = 2;         // GPIO2
constexpr uint32_t kI2cFrequencyHz = 400000;  // 400 kHz on both buses
constexpr uint16_t kI2cTimeoutMs = 20;        // transaction timeout: a stuck bus cannot hang the loop

// ---- MPU6050 (T13 section 5.1) -------------------------------------------
// The driver probes kAddrPrimary first and falls back to kAddrAlt on its own.
constexpr uint8_t kMpuAddressPrimary = Mpu6050::kAddrPrimary;  // 0x68
constexpr uint8_t kMpuAddressAlt = Mpu6050::kAddrAlt;          // 0x69
constexpr uint8_t kMpuWhoAmIExpected = Mpu6050::kWhoAmIValue;  // 0x68
// DLPF accel BW 184 Hz / gyro 188 Hz, gyro output rate 1 kHz, accel delay 2.9 ms.
// Named constant on purpose: impact transients must survive, so it can be re-tuned in one place.
constexpr uint8_t kDlpfCfg = 1;
constexpr uint8_t kSmplrtDiv = 9;      // 1 kHz / (1 + 9) = 100 Hz internal sample rate
constexpr uint8_t kGyroFsSel = 0;      // +-250 dps
constexpr uint8_t kAccelFsSel = 0;     // +-2 g
constexpr int kGyroFsDps = 250;        // printed in the startup report (report aid only)
constexpr int kAccelFsG = 2;           // printed in the startup report (report aid only)
constexpr uint32_t kMpuResetWaitMs = 100;  // >= 100 ms after DEVICE_RESET / SIGNAL_PATH_RESET

// ---- MS5611 (T13 section 5.2) --------------------------------------------
// The driver probes kBaroAddressPrimary first and falls back to kBaroAddressAlt.
constexpr uint8_t kBaroAddressPrimary = Ms5611::kAddrPrimary;  // 0x77 (CSB low)
constexpr uint8_t kBaroAddressAlt = Ms5611::kAddrAlt;          // 0x76
constexpr uint8_t kBaroOsrIndex = ms5611_reg::kOsr4096;        // OSR 4096 (D1 0x48 / D2 0x58)
constexpr uint32_t kBaroConversionWaitUs = ms5611_reg::kMaxConversionWaitUs[kBaroOsrIndex];

// ---- rates (T13 section 6) -----------------------------------------------
constexpr uint32_t kMpuRateHz = 100;
constexpr uint32_t kMpuPeriodUs = 1000000u / kMpuRateHz;  // 10000 us nominal slot period
constexpr uint32_t kBaroCycleIntervalUs = 40000;          // a cycle starts at most every 40 ms
constexpr uint32_t kBaroRateHz = 1000000u / kBaroCycleIntervalUs;  // 25 Hz

// ---- serial + startup ----------------------------------------------------
constexpr uint32_t kSerialBaud = 921600;
constexpr uint32_t kSerialWaitMs = 3000;     // bounded wait for a host; never blocks forever
constexpr uint32_t kRetryIntervalMs = 5000;  // bounded re-detection interval for a failed sensor
constexpr uint32_t kBaselineWindowMs = 2000; // bounded startup window for the pressure baseline
constexpr size_t kBaselineTargetSamples = BaroBaseline::kCapacity;   // 40
constexpr size_t kBaselineRequiredSamples = BaroBaseline::kRequiredSamples;  // 20

// ---- model switches (deliberate, documented in esp/node/README.md) -------
// false -> the pressure/temperature/altitude columns are filled ONLY on rows where a new
// MS5611 sample completed; a stale reading is never repeated as a fresh measurement.
constexpr bool kRepeatLastPressure = false;

}  // namespace node_config
}  // namespace esp_node
