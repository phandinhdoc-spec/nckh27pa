/**
 * ============================================================================
 * NCKH27PA — ESP32-S3 Super Mini Fall Detection Official Firmware
 * Target Hardware: ESP32-S3 Super Mini (esp32:esp32:esp32s3)
 * Sensors: MPU6050 (GY-521, I2C0) + MS5611 (GY-63, I2C1)
 * Protocol: BLE GATT (FALLSAFE-xxxx) + Serial CLI / Telemetry
 * Plan Reference: esp/esp32-plan.md (§5, §6, §7, §8) & android/android-plan.md (§6)
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <esp_random.h>

#define ENABLE_BLE 1

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#endif

// ============================================================================
// 0. USER TYPES DECLARED FIRST (Arduino auto-prototype ordering requirement)
// ============================================================================
// The Arduino builder auto-generates prototypes for every function and inserts them
// just before the first user declaration. A type used in a function signature must
// therefore be declared BEFORE those prototypes (i.e. near the very top of the sketch),
// otherwise the generated prototype fails to compile.
// Keep this enum here; `deviceStateToString()` and all users live further below.
enum DeviceState {
    STATE_BOOT_SELF_TEST,
    STATE_CALIBRATING,
    STATE_MONITORING,
    STATE_SUSPECTED,
    STATE_VERIFYING,
    STATE_LOCAL_ALERTING,
    STATE_DEGRADED
};

// ============================================================================
// 1. PINMAP & HARDWARE DEFINITIONS
// ============================================================================
// --- Confirmed hardware pins (from node_config.h on ESP32-S3) ---
static const int PIN_I2C0_SDA = 7;     // MPU6050 SDA (Bus 0) - CONFIRMED
static const int PIN_I2C0_SCL = 6;     // MPU6050 SCL (Bus 0) - CONFIRMED
static const int PIN_I2C1_SDA = 3;     // MS5611 SDA (Bus 1)  - CONFIRMED
static const int PIN_I2C1_SCL = 2;     // MS5611 SCL (Bus 1)  - CONFIRMED

// --- Peripherals without official schematic: marked TODO(HW) ---
static const int PIN_BUTTON_SOS    = 4;   // TODO(HW): Active LOW, internal pull-up assumed
static const int PIN_BUTTON_CANCEL = 5;   // TODO(HW): Active LOW, internal pull-up assumed
static const int PIN_BUZZER        = 1;   // TODO(HW): Passive/Active PWM buzzer
static const int PIN_LED_STATUS    = 8;   // TODO(HW): Status LED (ESP32-S3 Super Mini on-board LED is often GPIO 8)
static const int PIN_LED_BAT_1     = 9;   // TODO(HW): Battery bar LED 1
static const int PIN_LED_BAT_2     = 10;  // TODO(HW): Battery bar LED 2
static const int PIN_LED_BAT_3     = 11;  // TODO(HW): Battery bar LED 3
static const int PIN_BAT_ADC       = 12;  // TODO(HW): Battery voltage divider analog input

// Hardware capability flags (unfitted hardware -> UNAVAILABLE)
#define HW_HAS_FUEL_GAUGE  0   // MAX17048 not fitted yet (FIX 6: report -1, BATTERY_READ_FAILED)
#define HW_HAS_GNSS        0   // GNSS not fitted yet
#define HW_HAS_CELLULAR    0   // 4G module not fitted yet

// ============================================================================
// 2. PROTOCOL & UUID SPECIFICATIONS (Plan §8)
// ============================================================================
#define PROTOCOL_VERSION 1
#define FIRMWARE_VERSION "esp-s3 1.0.0"

#define UUID_SERVICE         "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001"
#define UUID_CHAR_STREAM     "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001" // Notify
#define UUID_CHAR_EVENT      "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001" // Indicate
#define UUID_CHAR_STATUS     "7d2a0004-6f45-4c2b-9a1e-38a8f5c10001" // Read/Notify
#define UUID_CHAR_COMMAND    "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001" // Write
#define UUID_CHAR_ACK        "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001" // Notify

// Global device identification and sequence counter (§8.6: shared for sensor/event)
static char s_bleDeviceName[24] = "FALLSAFE-0000";
static uint32_t s_globalSequenceNumber = 0;

// Timestamp synchronization state (§8.6)
static bool s_timeSynced = false;
static uint64_t s_deviceEpochTimeMs = 0;
static uint32_t s_epochSyncLocalMs = 0;

static uint64_t getMonotonicTimeMs() {
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static uint64_t getCurrentTimestampMs() {
    if (s_timeSynced) {
        return s_deviceEpochTimeMs + (uint64_t)(millis() - s_epochSyncLocalMs);
    }
    return getMonotonicTimeMs();
}

// ============================================================================
// 3. FALL DETECTION CONFIGURATION (Parity with Android FallDetectionConfig.DEFAULT)
// ============================================================================
struct FallDetectionProfile {
    // Parity with Android FallDetectionProfiles.kt
    float impactAccelerationMs2;            // 25.0 m/s^2 [Source: Android FallDetectionConfig.DEFAULT]
    float stillnessTargetAccelerationMs2;   // 9.81 m/s^2 [Source: Android FallDetectionConfig.DEFAULT]
    float stillnessToleranceMs2;            // 1.0 m/s^2  [Source: Android FallDetectionConfig.DEFAULT]
    uint32_t postImpactWindowMs;            // 3000 ms    [Source: Android FallDetectionConfig.DEFAULT]
    uint32_t postImpactStillnessDurationMs; // 1000 ms    [Source: Android FallDetectionConfig.DEFAULT]
    uint32_t minimumStillnessSamples;       // 6 samples  [Source: Android FallDetectionConfig.DEFAULT]
    uint32_t maximumSampleGapMs;            // 250 ms     [Source: Android FallDetectionConfig.DEFAULT]
    float phonePressureMinimumRisePa;       // 12.0 Pa    [Source: Android (disabled in decision)]
    bool usePressureFallFilter;             // false      [Source: Android (pressure flag disabled)]

    // Derived / heuristic auxiliary thresholds (used only for triggerReasons & secondary logging)
    float freeFallThresholdMs2;             // 3.0 m/s^2  [Source: Suy luan he thong - logging only]
    float highAngularSpeedDps;              // 200.0 dps  [Source: Suy luan he thong - logging only]
    float postureChangeThresholdDeg;        // 45.0 deg   [Source: Suy luan he thong - logging only]
};

static const FallDetectionProfile PROFILE_DEFAULT = {
    .impactAccelerationMs2 = 25.0f,
    .stillnessTargetAccelerationMs2 = 9.81f,
    .stillnessToleranceMs2 = 1.0f,
    .postImpactWindowMs = 3000,
    .postImpactStillnessDurationMs = 1000,
    .minimumStillnessSamples = 6,
    .maximumSampleGapMs = 250,
    .phonePressureMinimumRisePa = 12.0f,
    .usePressureFallFilter = false,
    .freeFallThresholdMs2 = 3.0f,
    .highAngularSpeedDps = 200.0f,
    .postureChangeThresholdDeg = 45.0f
};

// ============================================================================
// 4. DATA TYPES & SYSTEM STATES
// ============================================================================
// NOTE: `enum DeviceState` is declared at the TOP of this sketch (section 0) on purpose:
// the Arduino builder inserts generated function prototypes just before the first user
// declarations, so any user type used in a function signature must be visible before
// those prototypes — otherwise GCC reports "''DeviceState'' was not declared in this scope".

static const char* deviceStateToString(DeviceState s) {
    switch(s) {
        case STATE_BOOT_SELF_TEST: return "BOOT_SELF_TEST";
        case STATE_CALIBRATING:    return "CALIBRATING";
        case STATE_MONITORING:     return "MONITORING";
        case STATE_SUSPECTED:      return "SUSPECTED";
        case STATE_VERIFYING:      return "VERIFYING";
        case STATE_LOCAL_ALERTING: return "LOCAL_ALERTING";
        case STATE_DEGRADED:       return "DEGRADED";
        default:                   return "UNKNOWN";
    }
}

// Fixed IMU / Baro sample record for circular ring buffer
struct SensorSample {
    uint32_t timestampMs;
    float ax, ay, az;     // m/s^2
    float gx, gy, gz;     // deg/s
    float magnitude;      // m/s^2
    float pressurePa;     // Pa
    float altitudeDeltaM; // m (relative to reference baseline, FIX 3)
};

// ============================================================================
// CIRCULAR RING BUFFER (Plan §5.2, FIX 7)
// ============================================================================
// - Pre-event buffer: 10 seconds @ 100 Hz = 1000 samples.
// - Memory check: sizeof(SensorSample) is 40 bytes (1x uint32_t + 9x float).
//   1000 samples * 40 bytes = 40,000 bytes (~39.1 KB).
//   This comfortably fits in ESP32-S3's 512 KB internal SRAM (with > 300 KB free heap).
// - Post-event window policy: When an episode opens in SUSPECTED / VERIFYING,
//   the device continues recording continuously into this ring buffer and transmitting
//   for up to 20 s post-event until confirmation, timeout, or cancellation.
// - Full-buffer / congestion policy: Priority is strictly given to retaining and
//   transmitting event records (Esp32EventPacket); regular stream samples are dropped
//   if BLE/processing queue is saturated, incrementing s_sampleDropCount.
#define RING_BUFFER_SIZE 1000
static SensorSample s_ringBuffer[RING_BUFFER_SIZE];
static uint16_t s_ringHead = 0;
static uint16_t s_ringCount = 0;
static uint32_t s_sampleDropCount = 0;

// Dynamic sample rate control (FIX 3: SET_SAMPLE_RATE 100 Hz and 50 Hz supported)
static uint32_t s_imuSampleRateHz = 100;
static uint32_t s_imuSamplePeriodUs = 10000; // 100 Hz = 10,000 us nominal slot

// Reference pressure baseline (Plan §6.2, FIX 3)
static float s_referencePressurePa = 101325.0f;

// Last error code vocabulary (Plan §8.3, FIX 4, FIX 6):
// "IMU_READ_FAILED", "BAROMETER_READ_FAILED", "BATTERY_READ_FAILED",
// "TIME_NOT_SYNCED", "BUFFER_OVERFLOW", or nullptr if no error.
#if HW_HAS_FUEL_GAUGE
static const char* s_lastErrorCode = "TIME_NOT_SYNCED";
#else
static const char* s_lastErrorCode = "BATTERY_READ_FAILED";
#endif

// Battery state (Plan §3.4, §8.2, FIX 6) — declared HERE, before the BLE packet builders
// that format them (a global must be declared before the function that reads it):
//   HW_HAS_FUEL_GAUGE == 0  ->  batteryPercent = -1 (sentinel "no hardware", never a fake 100%)
//                               batteryVoltageMv = null, lastErrorCode = BATTERY_READ_FAILED
static int s_batteryPercent = -1;
static bool s_isCharging = false;
static bool s_lowBatWarned = false;
static bool s_criticalBatWarned = false;

// ============================================================================
// 5. MPU6050 REGISTER-LEVEL DRIVER (Bus 0: Wire, GPIO 7 / GPIO 6)
// ============================================================================
#define MPU6050_ADDR_A       0x68
#define MPU6050_ADDR_B       0x69
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_WHO_AM_I     0x75

struct Mpu6050Driver {
    uint8_t address;
    bool online;
    bool saturated;
    float gyroBiasX;
    float gyroBiasY;
    float gyroBiasZ;
    float baselineGravityX;
    float baselineGravityY;
    float baselineGravityZ;
};

static Mpu6050Driver s_mpu = {
    .address = MPU6050_ADDR_A,
    .online = false,
    .saturated = false,
    .gyroBiasX = 0, .gyroBiasY = 0, .gyroBiasZ = 0,
    .baselineGravityX = 0, .baselineGravityY = 0, .baselineGravityZ = 9.81f
};

static bool i2cWriteByte(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t data) {
    wire.beginTransmission(devAddr);
    wire.write(regAddr);
    wire.write(data);
    return (wire.endTransmission() == 0);
}

static bool i2cReadBytes(TwoWire &wire, uint8_t devAddr, uint8_t regAddr, uint8_t *buffer, size_t length) {
    wire.beginTransmission(devAddr);
    wire.write(regAddr);
    if (wire.endTransmission(false) != 0) return false;
    size_t count = wire.requestFrom(devAddr, (uint8_t)length);
    if (count != length) return false;
    for (size_t i = 0; i < length; i++) {
        buffer[i] = wire.read();
    }
    return true;
}

static bool initMPU6050() {
    uint8_t who = 0;
    s_mpu.address = MPU6050_ADDR_A;
    if (!i2cReadBytes(Wire, s_mpu.address, MPU6050_WHO_AM_I, &who, 1) || who != 0x68) {
        s_mpu.address = MPU6050_ADDR_B;
        if (!i2cReadBytes(Wire, s_mpu.address, MPU6050_WHO_AM_I, &who, 1) || who != 0x68) {
            s_mpu.online = false;
            return false;
        }
    }

    // 1. Wake up device, clock source PLL with X gyro
    i2cWriteByte(Wire, s_mpu.address, MPU6050_PWR_MGMT_1, 0x01);
    delay(10);
    // 2. SMPLRT_DIV = 9 -> 1 kHz / (1 + 9) = 100 Hz internal sample rate (FIX 8)
    // [Source: esp/node/firmware/esp_node/node_config.h kSmplrtDiv = 9]
    // Matches the 100 Hz main loop scheduling and validated node configuration.
    i2cWriteByte(Wire, s_mpu.address, MPU6050_SMPLRT_DIV, 0x09);
    // 3. CONFIG: DLPF_CFG = 1 (accel BW 184Hz, delay 2.0ms, gyro BW 188Hz) [Source: node_config.h kDlpfCfg = 1]
    i2cWriteByte(Wire, s_mpu.address, MPU6050_CONFIG, 0x01);
    // 4. GYRO_CONFIG: FS_SEL = 3 (±2000 dps) -> 0x18 (avoids saturation during violent rotational falls)
    i2cWriteByte(Wire, s_mpu.address, MPU6050_GYRO_CONFIG, 0x18);
    // 5. ACCEL_CONFIG: AFS_SEL = 3 (±16 g) -> 0x18 (CRITICAL: avoids saturation at 25 m/s^2 impact)
    i2cWriteByte(Wire, s_mpu.address, MPU6050_ACCEL_CONFIG, 0x18);

    s_mpu.online = true;
    return true;
}

static bool readMPU6050(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
    if (!s_mpu.online) return false;
    uint8_t raw[14];
    if (!i2cReadBytes(Wire, s_mpu.address, MPU6050_ACCEL_XOUT_H, raw, 14)) {
        return false;
    }

    int16_t rawAx = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t rawAy = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t rawAz = (int16_t)((raw[4] << 8) | raw[5]);
    // raw[6..7] is temperature
    int16_t rawGx = (int16_t)((raw[8] << 8) | raw[9]);
    int16_t rawGy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t rawGz = (int16_t)((raw[12] << 8) | raw[13]);

    // Check ±16g ADC saturation limit (raw reaches ±32767 near ±16g)
    if (abs(rawAx) >= 32700 || abs(rawAy) >= 32700 || abs(rawAz) >= 32700) {
        s_mpu.saturated = true;
    } else {
        s_mpu.saturated = false;
    }

    // Scale factors:
    // Accel: ±16g range -> 2048 LSB/g. 1g = 9.80665 m/s^2.
    const float accelScale = 9.80665f / 2048.0f;
    ax = (float)rawAx * accelScale;
    ay = (float)rawAy * accelScale;
    az = (float)rawAz * accelScale;

    // Gyro: ±2000 dps range -> 16.4 LSB/(deg/s)
    const float gyroScale = 1.0f / 16.4f;
    gx = ((float)rawGx * gyroScale) - s_mpu.gyroBiasX;
    gy = ((float)rawGy * gyroScale) - s_mpu.gyroBiasY;
    gz = ((float)rawGz * gyroScale) - s_mpu.gyroBiasZ;

    return true;
}

// ============================================================================
// 6. MS5611 REGISTER-LEVEL NON-BLOCKING DRIVER (Bus 1: Wire1, GPIO 3 / GPIO 2)
// ============================================================================
#define MS5611_ADDR_A       0x77
#define MS5611_ADDR_B       0x76
#define MS5611_CMD_RESET    0x1E
#define MS5611_CMD_PROM_RD  0xA0
#define MS5611_CMD_CONV_D1  0x48 // OSR 4096 pressure
#define MS5611_CMD_CONV_D2  0x58 // OSR 4096 temperature
#define MS5611_CMD_ADC_RD   0x00

enum Ms5611State {
    MS_IDLE,
    MS_CONV_D1_WAIT,
    MS_CONV_D2_WAIT
};

struct Ms5611Driver {
    uint8_t address;
    bool online;
    bool crcValid;
    bool hasReadError;
    uint16_t c[8]; // Calibration coefficients
    Ms5611State state;
    uint32_t convStartUs;
    uint32_t d1Raw;
    uint32_t d2Raw;
    float pressurePa;
    float temperatureC;
    float altitudeDeltaM; // Relative altitude delta (FIX 3)
};

static Ms5611Driver s_baro = {
    .address = MS5611_ADDR_A,
    .online = false,
    .crcValid = false,
    .hasReadError = false,
    .c = {0},
    .state = MS_IDLE,
    .convStartUs = 0,
    .d1Raw = 0,
    .d2Raw = 0,
    .pressurePa = 101325.0f,
    .temperatureC = 25.0f,
    .altitudeDeltaM = 0.0f
};

// AN520 CRC4 calculation for MS5611
static bool checkMS5611CRC4(uint16_t prom[]) {
    uint32_t n_rem = 0;
    uint16_t promBackup[8];
    for (int i = 0; i < 8; i++) promBackup[i] = prom[i];
    uint16_t crcRead = promBackup[7] & 0x000F;
    promBackup[7] = promBackup[7] & 0xFF00;

    for (int cnt = 0; cnt < 16; cnt++) {
        if (cnt % 2 == 1) {
            n_rem ^= (uint16_t)(promBackup[cnt >> 1] & 0x00FF);
        } else {
            n_rem ^= (uint16_t)(promBackup[cnt >> 1] >> 8);
        }
        for (uint8_t n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem = (n_rem << 1);
            }
        }
    }
    n_rem = (0x000F & (n_rem >> 12));
    return (n_rem == crcRead);
}

static bool initMS5611() {
    s_baro.address = MS5611_ADDR_A;
    Wire1.beginTransmission(s_baro.address);
    if (Wire1.endTransmission() != 0) {
        s_baro.address = MS5611_ADDR_B;
        Wire1.beginTransmission(s_baro.address);
        if (Wire1.endTransmission() != 0) {
            s_baro.online = false;
            return false;
        }
    }

    // Reset command
    Wire1.beginTransmission(s_baro.address);
    Wire1.write(MS5611_CMD_RESET);
    Wire1.endTransmission();
    delay(10);

    // Read 8 PROM words
    for (uint8_t i = 0; i < 8; i++) {
        Wire1.beginTransmission(s_baro.address);
        Wire1.write(MS5611_CMD_PROM_RD + (i * 2));
        if (Wire1.endTransmission(false) != 0) {
            s_baro.online = false;
            return false;
        }
        if (Wire1.requestFrom(s_baro.address, (uint8_t)2) != 2) {
            s_baro.online = false;
            return false;
        }
        s_baro.c[i] = (Wire1.read() << 8) | Wire1.read();
    }

    s_baro.crcValid = checkMS5611CRC4(s_baro.c);
    s_baro.online = true;
    s_baro.hasReadError = false;
    s_baro.state = MS_IDLE;
    return true;
}

static void calculateMS5611() {
    int64_t dt = (int64_t)s_baro.d2Raw - ((int64_t)s_baro.c[5] << 8);
    int64_t temp = 2000 + ((dt * (int64_t)s_baro.c[6]) >> 23);

    int64_t off = ((int64_t)s_baro.c[2] << 16) + (((int64_t)s_baro.c[4] * dt) >> 7);
    int64_t sens = ((int64_t)s_baro.c[1] << 15) + (((int64_t)s_baro.c[3] * dt) >> 8);

    // Second order temperature compensation
    if (temp < 2000) {
        int64_t t2 = (dt * dt) >> 31;
        int64_t off2 = 5 * ((temp - 2000) * (temp - 2000)) >> 1;
        int64_t sens2 = 5 * ((temp - 2000) * (temp - 2000)) >> 2;
        if (temp < -1500) {
            off2 += 7 * ((temp + 1500) * (temp + 1500));
            sens2 += 11 * ((temp + 1500) * (temp + 1500)) >> 1;
        }
        temp -= t2;
        off -= off2;
        sens -= sens2;
    }

    int64_t p = ((((int64_t)s_baro.d1Raw * sens) >> 21) - off) >> 15;
    s_baro.pressurePa = (float)p; // Output is in Pascals (100000 = 1000.00 mbar)
    s_baro.temperatureC = (float)temp / 100.0f;

    // Relative altitude delta (Plan §6.2, FIX 3):
    // altitudeDeltaM = 44330 * (1 - (p_now / s_referencePressurePa)^(1/5.255))
    // Positive when climbing, negative when descending ("âm khi xuống").
    if (s_baro.pressurePa > 10000.0f && s_referencePressurePa > 10000.0f) {
        s_baro.altitudeDeltaM = 44330.0f * (1.0f - powf(s_baro.pressurePa / s_referencePressurePa, 1.0f / 5.255f));
    }
}

static void pollMS5611(uint32_t nowUs) {
    if (!s_baro.online) return;

    switch (s_baro.state) {
        case MS_IDLE:
            Wire1.beginTransmission(s_baro.address);
            Wire1.write(MS5611_CMD_CONV_D1); // Start D1 conversion (pressure OSR 4096)
            if (Wire1.endTransmission() == 0) {
                s_baro.convStartUs = nowUs;
                s_baro.state = MS_CONV_D1_WAIT;
            } else {
                s_baro.hasReadError = true;
                s_lastErrorCode = "BAROMETER_READ_FAILED";
            }
            break;

        case MS_CONV_D1_WAIT:
            // OSR 4096 requires max 9.04 ms
            if (nowUs - s_baro.convStartUs >= 9500) {
                Wire1.beginTransmission(s_baro.address);
                Wire1.write(MS5611_CMD_ADC_RD);
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(s_baro.address, (uint8_t)3) == 3) {
                    s_baro.d1Raw = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read();
                    // Start D2 conversion (temperature)
                    Wire1.beginTransmission(s_baro.address);
                    Wire1.write(MS5611_CMD_CONV_D2);
                    if (Wire1.endTransmission() == 0) {
                        s_baro.convStartUs = nowUs;
                        s_baro.state = MS_CONV_D2_WAIT;
                    } else {
                        s_baro.hasReadError = true;
                        s_lastErrorCode = "BAROMETER_READ_FAILED";
                        s_baro.state = MS_IDLE;
                    }
                } else {
                    s_baro.hasReadError = true;
                    s_lastErrorCode = "BAROMETER_READ_FAILED";
                    s_baro.state = MS_IDLE;
                }
            }
            break;

        case MS_CONV_D2_WAIT:
            if (nowUs - s_baro.convStartUs >= 9500) {
                Wire1.beginTransmission(s_baro.address);
                Wire1.write(MS5611_CMD_ADC_RD);
                if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(s_baro.address, (uint8_t)3) == 3) {
                    s_baro.d2Raw = ((uint32_t)Wire1.read() << 16) | ((uint32_t)Wire1.read() << 8) | Wire1.read();
                    calculateMS5611();
                    s_baro.hasReadError = false;
                } else {
                    s_baro.hasReadError = true;
                    s_lastErrorCode = "BAROMETER_READ_FAILED";
                }
                s_baro.state = MS_IDLE; // Ready for next cycle
            }
            break;
    }
}

// ============================================================================
// 7. SYSTEM STATUS, COUNTERS & STATE MACHINE
// ============================================================================
struct SystemCounters {
    uint32_t imuSamplesRead;
    uint32_t baroSamplesRead;
    uint32_t lateLoopSlots;
    uint32_t fallEventsConfirmed;
    uint32_t sosTriggers;
    uint32_t blePacketsDropped;
};

static SystemCounters s_counters = {0};
static DeviceState s_state = STATE_BOOT_SELF_TEST;
static DeviceState s_lastState = STATE_BOOT_SELF_TEST;

// Fall Detection State Machine Variables (Parity with Android)
struct FallVerificationState {
    uint32_t impactTimestampMs;
    float peakImpactAcc;
    float preImpactMinAcc;
    uint32_t stillnessStartMs;
    uint32_t stillnessSampleCount;
    uint32_t lastSampleMs;
    bool inVerificationWindow;
    float preImpactGravity[3];
    float postImpactGravity[3];
    float baselineAltitude;
    char triggerReasons[96];
};

static FallVerificationState s_fallState = {0};

// Alerting & buzzer timer
static uint32_t s_alertStartMs = 0;
static uint32_t s_buzzerPatternDeadlineMs = 0;
static uint16_t s_buzzerPatternId = 0;

static bool s_bleConnected = false;
static bool s_bleStreaming = false;

// ============================================================================
// 8. BLE GATT SERVER IMPLEMENTATION (Plan §8)
// ============================================================================
#if ENABLE_BLE
static BLEServer *s_pServer = nullptr;
static BLECharacteristic *s_pCharStream = nullptr;
static BLECharacteristic *s_pCharEvent = nullptr;
static BLECharacteristic *s_pCharStatus = nullptr;
static BLECharacteristic *s_pCharCommand = nullptr;
static BLECharacteristic *s_pCharAck = nullptr;

static char s_lastCommandId[37] = "";
static bool s_pendingCommand = false;
static char s_pendingCmdPayload[128] = "";

// Event tracking for persistent eventId on retransmit (§8.4)
static uint32_t s_eventCounter = 0;
static char s_currentEventId[32] = "";
static uint32_t s_currentEventSeq = 0;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        s_bleConnected = true;
    }
    void onDisconnect(BLEServer* pServer) override {
        s_bleConnected = false;
        s_bleStreaming = false;
        BLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0 && rxValue.length() < sizeof(s_pendingCmdPayload)) {
            strncpy(s_pendingCmdPayload, rxValue.c_str(), sizeof(s_pendingCmdPayload) - 1);
            s_pendingCmdPayload[sizeof(s_pendingCmdPayload) - 1] = '\0';
            s_pendingCommand = true;
        }
    }
};

// FIX 2: Esp32CommandAck (§8.5)
// Fields: protocolVersion · commandId · deviceId · timestampMs · commandStatus · errorCode · message
// commandStatus must be one of: ACCEPTED, COMPLETED, REJECTED, FAILED
static void sendBleAck(const char* cmdId, const char* commandStatus, const char* errorCode, const char* message) {
    if (!s_bleConnected || s_pCharAck == nullptr) return;
    char buffer[256];
    char errBuf[48];
    char msgBuf[96];

    if (errorCode != nullptr) {
        snprintf(errBuf, sizeof(errBuf), "\"%s\"", errorCode);
    } else {
        snprintf(errBuf, sizeof(errBuf), "null");
    }

    if (message != nullptr) {
        snprintf(msgBuf, sizeof(msgBuf), "\"%s\"", message);
    } else {
        snprintf(msgBuf, sizeof(msgBuf), "null");
    }

    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"commandId\":\"%s\",\"deviceId\":\"%s\",\"timestampMs\":%llu,"
             "\"commandStatus\":\"%s\",\"errorCode\":%s,\"message\":%s}",
             cmdId ? cmdId : "",
             s_bleDeviceName,
             getCurrentTimestampMs(),
             commandStatus ? commandStatus : "COMPLETED",
             errBuf,
             msgBuf);

    if (strlen(buffer) > 512) {
        s_counters.blePacketsDropped++;
        return;
    }
    s_pCharAck->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharAck->notify();
}

// FIX 5: Esp32EventPacket (§8.4)
// Fields: protocolVersion · eventId · deviceId · sequenceNumber · timestampMs ·
// eventType · eventSeverity · sosButtonPressed · eventConfidence · peakAccelerationMs2 ·
// orientationChangeDeg · altitudeDeltaM · inactivityDurationMs · checksum · triggerReasons
static void sendBleEvent(const char* eventType, const char* severity, int confidence,
                         float peakAcc, float orientationDeg, float altDeltaM,
                         uint32_t inactivityDurationMs, const char* reasons,
                         bool hasPeakAcc = true, bool hasOrientation = false, bool hasAltDelta = false,
                         bool isRetransmit = false) {
    if (!s_bleConnected || s_pCharEvent == nullptr) return;

    if (!isRetransmit || strlen(s_currentEventId) == 0) {
        snprintf(s_currentEventId, sizeof(s_currentEventId), "evt-%04X-%04u",
                 (uint16_t)(esp_random() & 0xFFFF), ++s_eventCounter);
        s_currentEventSeq = ++s_globalSequenceNumber;
    }

    uint64_t ts = getCurrentTimestampMs();
    bool sosPressed = (digitalRead(PIN_BUTTON_SOS) == LOW);

    char peakBuf[24];
    if (hasPeakAcc) snprintf(peakBuf, sizeof(peakBuf), "%.2f", peakAcc);
    else snprintf(peakBuf, sizeof(peakBuf), "null");

    char orientBuf[24];
    if (hasOrientation) snprintf(orientBuf, sizeof(orientBuf), "%.1f", orientationDeg);
    else snprintf(orientBuf, sizeof(orientBuf), "null");

    char altBuf[24];
    if (hasAltDelta) snprintf(altBuf, sizeof(altBuf), "%.2f", altDeltaM);
    else snprintf(altBuf, sizeof(altBuf), "null");

    char inactBuf[24];
    if (inactivityDurationMs > 0) snprintf(inactBuf, sizeof(inactBuf), "%u", inactivityDurationMs);
    else snprintf(inactBuf, sizeof(inactBuf), "null");

    // Note (§8.4 / §8.7): checksum is null in v1 contract.
    // triggerReasons is an extension field outside v1 contract retained for debug/telemetry.
    char buffer[384];
    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"eventId\":\"%s\",\"deviceId\":\"%s\",\"sequenceNumber\":%u,"
             "\"timestampMs\":%llu,\"eventType\":\"%s\",\"eventSeverity\":\"%s\","
             "\"sosButtonPressed\":%s,\"eventConfidence\":%d,"
             "\"peakAccelerationMs2\":%s,\"orientationChangeDeg\":%s,\"altitudeDeltaM\":%s,"
             "\"inactivityDurationMs\":%s,\"checksum\":null,\"triggerReasons\":[\"%s\"]}",
             s_currentEventId,
             s_bleDeviceName,
             s_currentEventSeq,
             ts,
             eventType,
             severity,
             sosPressed ? "true" : "false",
             confidence,
             peakBuf,
             orientBuf,
             altBuf,
             inactBuf,
             reasons ? reasons : "");

    // Safety check against MTU overflow (§8.7 MTU gap)
    if (strlen(buffer) > 512) {
        s_counters.blePacketsDropped++;
        return;
    }
    s_pCharEvent->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharEvent->indicate();
}

// FIX 4: Esp32DeviceStatus (§8.3)
// Fields: protocolVersion · deviceId · timestampMs · firmwareVersion · uptimeSeconds ·
// batteryPercent · batteryVoltageMv · isCharging · imuStatus · barometerStatus · gnssStatus ·
// bufferUsagePercent · lastErrorCode
static void sendBleDeviceStatus() {
    if (!s_bleConnected || s_pCharStatus == nullptr) return;

    const char* imuStatusStr = "OK";
    if (s_state == STATE_CALIBRATING) imuStatusStr = "CALIBRATING";
    else if (!s_mpu.online) imuStatusStr = "ERROR";
    else imuStatusStr = "OK";

    const char* baroStatusStr = "UNAVAILABLE";
    if (s_state == STATE_CALIBRATING) baroStatusStr = "CALIBRATING";
    else if (s_baro.hasReadError) baroStatusStr = "ERROR";
    else if (s_baro.online) baroStatusStr = "OK";
    else baroStatusStr = "UNAVAILABLE";

    int bufferUsage = (int)((s_ringCount * 100UL) / RING_BUFFER_SIZE);
    if (bufferUsage > 100) bufferUsage = 100;

    char errBuf[48];
    if (s_lastErrorCode != nullptr && strlen(s_lastErrorCode) > 0) {
        snprintf(errBuf, sizeof(errBuf), "\"%s\"", s_lastErrorCode);
    } else {
        snprintf(errBuf, sizeof(errBuf), "null");
    }

    char buffer[320];
    snprintf(buffer, sizeof(buffer),
             "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"timestampMs\":%llu,"
             "\"firmwareVersion\":\"%s\",\"uptimeSeconds\":%lu,\"batteryPercent\":%d,"
             "\"batteryVoltageMv\":null,\"isCharging\":%s,\"imuStatus\":\"%s\","
             "\"barometerStatus\":\"%s\",\"gnssStatus\":\"UNAVAILABLE\","
             "\"bufferUsagePercent\":%d,\"lastErrorCode\":%s}",
             s_bleDeviceName,
             getCurrentTimestampMs(),
             FIRMWARE_VERSION,
             (unsigned long)(esp_timer_get_time() / 1000000ULL),
             s_batteryPercent,
             s_isCharging ? "true" : "false",
             imuStatusStr,
             baroStatusStr,
             bufferUsage,
             errBuf);

    if (strlen(buffer) > 512) {
        s_counters.blePacketsDropped++;
        return;
    }
    s_pCharStatus->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharStatus->notify();
}

// FIX 1: Esp32SensorPacket (§8.2)
// Fields: protocolVersion · deviceId · sequenceNumber · timestampMs ·
// accelXMs2, accelYMs2, accelZMs2 · gyroXDps, gyroYDps, gyroZDps ·
// pressurePa · temperatureC · altitudeDeltaM · batteryPercent · batteryVoltageMv ·
// isCharging · sosButtonPressed · sensorQuality
static void sendBleSensorStream(float ax, float ay, float az, float gx, float gy, float gz,
                                float p, float altDelta, float tempC) {
    if (!s_bleConnected || !s_bleStreaming || s_pCharStream == nullptr) return;

    uint32_t seq = ++s_globalSequenceNumber;
    uint64_t ts = getCurrentTimestampMs();
    bool sosPressed = (digitalRead(PIN_BUTTON_SOS) == LOW);

    // Sensor quality score calculation (0 - 100)
    int quality = 100;
    if (!s_mpu.online) quality = 0;
    else if (s_mpu.saturated) quality = 40;
    else if (!s_baro.online || s_baro.hasReadError) quality = 80;

    char buffer[384];
    if (s_baro.online && !s_baro.hasReadError) {
        snprintf(buffer, sizeof(buffer),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu,"
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f,"
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f,"
                 "\"pressurePa\":%.1f,\"temperatureC\":%.1f,\"altitudeDeltaM\":%.2f,"
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s,"
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}",
                 s_bleDeviceName, seq, ts,
                 ax, ay, az,
                 gx, gy, gz,
                 p, tempC, altDelta,
                 s_batteryPercent,
                 s_isCharging ? "true" : "false",
                 sosPressed ? "true" : "false",
                 quality);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%u,\"timestampMs\":%llu,"
                 "\"accelXMs2\":%.2f,\"accelYMs2\":%.2f,\"accelZMs2\":%.2f,"
                 "\"gyroXDps\":%.1f,\"gyroYDps\":%.1f,\"gyroZDps\":%.1f,"
                 "\"pressurePa\":null,\"temperatureC\":null,\"altitudeDeltaM\":null,"
                 "\"batteryPercent\":%d,\"batteryVoltageMv\":null,\"isCharging\":%s,"
                 "\"sosButtonPressed\":%s,\"sensorQuality\":%d}",
                 s_bleDeviceName, seq, ts,
                 ax, ay, az,
                 gx, gy, gz,
                 s_batteryPercent,
                 s_isCharging ? "true" : "false",
                 sosPressed ? "true" : "false",
                 quality);
    }

    // MTU overflow check (§8.7): v1 has no binary fragmentation yet.
    // If packet exceeds MTU budget, count as dropped rather than corrupting stream.
    if (strlen(buffer) > 512) {
        s_counters.blePacketsDropped++;
        return;
    }
    s_pCharStream->setValue((uint8_t*)buffer, strlen(buffer));
    s_pCharStream->notify();
}
#endif // ENABLE_BLE

// ============================================================================
// 9. SERIAL OUTPUT & COMMAND PARSER (§7)
// ============================================================================
#define SERIAL_RAW_MODE 0 // 0: Human mode (default ~4 Hz), 1: CSV mode
static bool s_rawCsvMode = (SERIAL_RAW_MODE == 1);
static uint32_t s_lastHumanLogMs = 0;

static void printProfile() {
    Serial.println(F("\n======================================================="));
    Serial.println(F("NCKH27PA ESP32-S3 PRODUCT FALL DETECTION PROFILE"));
    Serial.println(F("NOTE: EXPERIMENTAL THRESHOLDS - NOT MEDICALLY VERIFIED"));
    Serial.println(F("======================================================="));
    Serial.printf("  impactAccelerationMs2:            %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.impactAccelerationMs2);
    Serial.printf("  stillnessTargetAccelerationMs2:   %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.stillnessTargetAccelerationMs2);
    Serial.printf("  stillnessToleranceMs2:            %.2f m/s^2   [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.stillnessToleranceMs2);
    Serial.printf("  postImpactWindowMs:               %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.postImpactWindowMs);
    Serial.printf("  postImpactStillnessDurationMs:    %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.postImpactStillnessDurationMs);
    Serial.printf("  minimumStillnessSamples:          %u samples  [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.minimumStillnessSamples);
    Serial.printf("  maximumSampleGapMs:               %u ms       [Android FallDetectionConfig.DEFAULT]\n", PROFILE_DEFAULT.maximumSampleGapMs);
    Serial.printf("  phonePressureMinimumRisePa:       %.1f Pa     [Android (flag disabled in decision)]\n", PROFILE_DEFAULT.phonePressureMinimumRisePa);
    Serial.printf("  freeFallThresholdMs2:             %.2f m/s^2   [Heuristic telemetry logging only]\n", PROFILE_DEFAULT.freeFallThresholdMs2);
    Serial.printf("  highAngularSpeedDps:              %.1f dps    [Heuristic telemetry logging only]\n", PROFILE_DEFAULT.highAngularSpeedDps);
    Serial.println(F("=======================================================\n"));
}

static void printSystemStatus() {
    Serial.println(F("\n--- DEVICE SYSTEM STATUS ---"));
    Serial.printf("Device ID: %s | Firmware: %s\n", s_bleDeviceName, FIRMWARE_VERSION);
    Serial.printf("State: %s (Last: %s)\n", deviceStateToString(s_state), deviceStateToString(s_lastState));
    Serial.printf("IMU MPU6050: %s (addr: 0x%02X, saturated: %s, rate: %u Hz)\n",
                  s_mpu.online ? "ONLINE" : "OFFLINE", s_mpu.address,
                  s_mpu.saturated ? "YES" : "NO", s_imuSampleRateHz);
    Serial.printf("Barometer MS5611: %s (addr: 0x%02X, CRC: %s, ref: %.1f Pa)\n",
                  s_baro.online ? (s_baro.hasReadError ? "ERROR" : "ONLINE") : "UNAVAILABLE",
                  s_baro.address, s_baro.crcValid ? "VALID" : "INVALID", s_referencePressurePa);
    Serial.printf("BLE: %s (Streaming: %s)\n", s_bleConnected ? "CONNECTED" : "DISCONNECTED", s_bleStreaming ? "ON" : "OFF");
    Serial.printf("Buffer Usage: %u/%u samples (%d%%) | Drops: %u\n",
                  s_ringCount, RING_BUFFER_SIZE, (int)((s_ringCount * 100UL) / RING_BUFFER_SIZE), s_sampleDropCount);
    Serial.printf("Counters: IMU=%u, Baro=%u, LateSlots=%u, Falls=%u, SOS=%u, BLE Drops=%u\n",
                  s_counters.imuSamplesRead, s_counters.baroSamplesRead, s_counters.lateLoopSlots,
                  s_counters.fallEventsConfirmed, s_counters.sosTriggers, s_counters.blePacketsDropped);
    Serial.printf("Battery: %d%% (unfitted = -1), Free Heap: %u bytes\n", s_batteryPercent, ESP.getFreeHeap());
    Serial.printf("Last Error Code: %s\n", s_lastErrorCode ? s_lastErrorCode : "NONE");
    Serial.println(F("-----------------------------\n"));
}

static void printCsvHeader() {
    // FIX 3: Consistent field naming with altitudeDeltaM
    Serial.println(F("timestamp_ms,ax,ay,az,mag,gx,gy,gz,pressure_pa,altitudeDeltaM,state,event"));
}

// ============================================================================
// 10. CALIBRATION & SELF TEST (§7.1)
// ============================================================================
static void performSelfTestAndCalibration() {
    s_state = STATE_BOOT_SELF_TEST;
    Serial.println(F("[BOOT] Starting Hardware Self-Test..."));

    bool imuOk = initMPU6050();
    Serial.printf("[BOOT] IMU MPU6050 on I2C0 (SDA 7, SCL 6): %s (addr: 0x%02X)\n",
                  imuOk ? "PASS" : "FAIL", s_mpu.address);

    bool baroOk = initMS5611();
    Serial.printf("[BOOT] Barometer MS5611 on I2C1 (SDA 3, SCL 2): %s (addr: 0x%02X, CRC: %s)\n",
                  baroOk ? "PASS" : "UNAVAILABLE", s_baro.address, s_baro.crcValid ? "PASS" : "FAIL/UNVERIFIED");

    // FIX 6: Emit SENSOR_ERROR if critical sensor self-test fails
    if (!imuOk) {
        Serial.println(F("[CRITICAL] IMU failed self-test. System transitioning to DEGRADED mode."));
        s_state = STATE_DEGRADED;
        s_lastErrorCode = "IMU_READ_FAILED";
#if ENABLE_BLE
        sendBleEvent("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_SELF_TEST_FAILED", false, false, false);
        sendBleDeviceStatus();
#endif
        return;
    }

    s_state = STATE_CALIBRATING;
    Serial.println(F("[CALIB] Calibrating gyro bias & baseline gravity (keep device STILL for 2 sec)..."));

    float sumGx = 0, sumGy = 0, sumGz = 0;
    float sumAx = 0, sumAy = 0, sumAz = 0;
    float sumP = 0;
    int samples = 0;
    int vibrationWarnings = 0;

    uint32_t calibStart = millis();
    while (millis() - calibStart < 2000) {
        float ax, ay, az, gx, gy, gz;
        if (readMPU6050(ax, ay, az, gx, gy, gz)) {
            float mag = sqrtf(ax * ax + ay * ay + az * az);
            if (fabsf(mag - 9.81f) > 2.0f) {
                vibrationWarnings++;
            }
            sumAx += ax; sumAy += ay; sumAz += az;
            sumGx += gx; sumGy += gy; sumGz += gz;
            samples++;
        }
        if (s_baro.online) {
            pollMS5611(micros());
            sumP += s_baro.pressurePa;
        }
        delay(10);
    }

    if (samples > 50) {
        s_mpu.gyroBiasX = sumGx / samples;
        s_mpu.gyroBiasY = sumGy / samples;
        s_mpu.gyroBiasZ = sumGz / samples;
        s_mpu.baselineGravityX = sumAx / samples;
        s_mpu.baselineGravityY = sumAy / samples;
        s_mpu.baselineGravityZ = sumAz / samples;
        if (s_baro.online) {
            // FIX 3: Initialize s_referencePressurePa from median/average during calibration
            s_referencePressurePa = sumP / samples;
            s_baro.altitudeDeltaM = 0.0f;
        }

        if (vibrationWarnings > (samples * 0.20f)) {
            Serial.println(F("[CALIB_WARNING] Device motion detected during calibration! Baseline may have offsets."));
        } else {
            Serial.println(F("[CALIB] Calibration complete. Gravity vector stabilized."));
        }
    }

    s_state = STATE_MONITORING;
    Serial.println(F("[SYSTEM] Transitioned to STATE_MONITORING. Ready."));
}

// ============================================================================
// 11. BAROMETER DRIFT TRACKING & BATTERY MANAGEMENT (§6.2, §3.4, FIX 3, FIX 6)
// ============================================================================
// Condition (b): Baseline reference updates automatically ONLY when:
// 1) Device is in STATE_MONITORING and NO suspected fall episode is active (!s_fallState.inVerificationWindow)
// 2) Ambient pressure remains stable within ±10 Pa (approx ±0.8 m) for at least 10 consecutive seconds.
// The baseline is NEVER modified during an active suspected fall or verification window.
static uint32_t s_stablePressureStartMs = 0;
static float s_lastStableCheckPressurePa = 0.0f;

static void updatePressureBaselineTracking(uint32_t nowMs, float currentPressurePa) {
    if (s_state != STATE_MONITORING || s_fallState.inVerificationWindow) {
        s_stablePressureStartMs = 0;
        return;
    }
    if (fabsf(currentPressurePa - s_lastStableCheckPressurePa) > 10.0f) {
        s_lastStableCheckPressurePa = currentPressurePa;
        s_stablePressureStartMs = nowMs;
    } else {
        if (s_stablePressureStartMs == 0) {
            s_stablePressureStartMs = nowMs;
            s_lastStableCheckPressurePa = currentPressurePa;
        } else if (nowMs - s_stablePressureStartMs >= 10000) {
            // Pressure remained stable within +/- 10 Pa for >= 10 seconds: update baseline
            s_referencePressurePa = currentPressurePa;
            s_stablePressureStartMs = nowMs;
        }
    }
}

// Battery Management & Hysteresis Alerting (Plan §3.4, §8.2, FIX 6):
// The state variables (s_batteryPercent, s_isCharging, s_lowBatWarned, s_criticalBatWarned)
// are declared in the global block ABOVE the BLE section, because the packet builders read them.
// When hardware fuel gauge is NOT fitted (HW_HAS_FUEL_GAUGE == 0):
// - batteryPercent = -1 (sentinel indicating no hardware; avoids fake 100%)
// - batteryVoltageMv = null
// - s_lastErrorCode = "BATTERY_READ_FAILED"

static void updateBatteryStatus(uint32_t nowMs) {
#if HW_HAS_FUEL_GAUGE
    // When MAX17048 or ADC fuel gauge hardware is fitted in future revision:
    // Read fuel gauge IC / ADC...
    // e.g. s_batteryPercent = readFuelGaugePercent();
    //
    // Hysteresis alert thresholds (Plan §3.4):
    // Warning: <= 20% (clears when > 25%)
    // Critical: <= 10% (clears when > 15%)
    if (s_batteryPercent >= 0) {
        if (s_batteryPercent <= 10 && !s_criticalBatWarned) {
            s_criticalBatWarned = true;
            s_lowBatWarned = true;
#if ENABLE_BLE
            sendBleEvent("LOW_BATTERY", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_CRITICAL_LE_10PCT", false, false, false);
            sendBleDeviceStatus();
#endif
        } else if (s_batteryPercent <= 20 && !s_lowBatWarned) {
            s_lowBatWarned = true;
#if ENABLE_BLE
            sendBleEvent("LOW_BATTERY", "WARNING", 100, 0.0f, 0.0f, 0.0f, 0, "BATTERY_LOW_LE_20PCT", false, false, false);
            sendBleDeviceStatus();
#endif
        } else if (s_batteryPercent > 25) {
            s_lowBatWarned = false;
            s_criticalBatWarned = false;
        } else if (s_batteryPercent > 15) {
            s_criticalBatWarned = false;
        }
    }
#else
    // Hardware fuel gauge NOT fitted (HW_HAS_FUEL_GAUGE = 0).
    // Per Plan §8.2, §8.3 & Task FIX 6: Never send fake 100% battery!
    s_batteryPercent = -1;
#endif
}

// ============================================================================
// 12. BUTTONS, SOS & BUZZER HANDLING
// ============================================================================
static uint32_t s_sosPressStartMs = 0;
static bool s_sosTriggered = false;
static uint32_t s_cancelPressStartMs = 0;

static void updateButtons(uint32_t nowMs) {
    // SOS Button: Active LOW with pull-up. Must be held for >= 2000 ms
    int sosVal = digitalRead(PIN_BUTTON_SOS);
    if (sosVal == LOW) {
        if (s_sosPressStartMs == 0) {
            s_sosPressStartMs = nowMs;
        } else if (nowMs - s_sosPressStartMs >= 2000 && !s_sosTriggered) {
            s_sosTriggered = true;
            s_counters.sosTriggers++;
            s_state = STATE_LOCAL_ALERTING;
            s_alertStartMs = nowMs;
            Serial.println(F("[ALERT] >>> SOS BUTTON TRIGGERED! Entering LOCAL_ALERTING directly. <<<"));
#if ENABLE_BLE
            sendBleEvent("SOS_PRESSED", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "SOS_BUTTON_2S_HOLD", false, false, false);
            sendBleDeviceStatus();
#endif
        }
    } else {
        s_sosPressStartMs = 0;
        s_sosTriggered = false;
    }

    // Cancel Button: Active LOW. Held >= 300 ms cancels any active alert
    int cancelVal = digitalRead(PIN_BUTTON_CANCEL);
    if (cancelVal == LOW) {
        if (s_cancelPressStartMs == 0) {
            s_cancelPressStartMs = nowMs;
        } else if (nowMs - s_cancelPressStartMs >= 300) {
            if (s_state == STATE_LOCAL_ALERTING || s_state == STATE_VERIFYING || s_state == STATE_SUSPECTED) {
                Serial.println(F("[ACTION] Alert cancelled by user via CANCEL button."));
                s_state = STATE_MONITORING;
                digitalWrite(PIN_BUZZER, LOW);
#if ENABLE_BLE
                sendBleEvent("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "USER_CANCEL_BUTTON", false, false, false);
                sendBleDeviceStatus();
#endif
            }
        }
    } else {
        s_cancelPressStartMs = 0;
    }
}

static void updateBuzzer(uint32_t nowMs) {
    if (s_state == STATE_LOCAL_ALERTING) {
        // High urgency alternating beep (200ms ON / 200ms OFF)
        bool on = ((nowMs - s_alertStartMs) / 200) % 2 == 0;
        digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
        digitalWrite(PIN_LED_STATUS, on ? HIGH : LOW);
    } else if (s_buzzerPatternDeadlineMs > nowMs) {
        // Custom buzzer pattern triggered by BLE command
        bool on = (nowMs / 150) % 2 == 0;
        digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
    } else {
        digitalWrite(PIN_BUZZER, LOW);
        // Status LED heartbeat in monitoring: slow pulse
        if (s_state == STATE_MONITORING) {
            digitalWrite(PIN_LED_STATUS, (nowMs % 1000 < 50) ? HIGH : LOW);
        } else if (s_state == STATE_DEGRADED) {
            digitalWrite(PIN_LED_STATUS, (nowMs % 250 < 125) ? HIGH : LOW); // Fast error blink
        }
    }
}

// ============================================================================
// 13. FALL DETECTION ENGINE (Strict Android Parity)
// ============================================================================
static void processFallDetection(uint32_t nowMs, float ax, float ay, float az, float gx, float gy, float gz,
                                 float pressurePa, float altitudeDeltaM, const char* &outEventLabel) {
    outEventLabel = nullptr;
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    float angularSpeed = sqrtf(gx * gx + gy * gy + gz * gz);

    // Track pre-impact minimum acceleration
    if (!s_fallState.inVerificationWindow) {
        if (mag < s_fallState.preImpactMinAcc || s_fallState.preImpactMinAcc == 0.0f) {
            s_fallState.preImpactMinAcc = mag;
        }
    }

    // Step 1: Detect impact (|a| >= 25.0 m/s^2)
    if (mag >= PROFILE_DEFAULT.impactAccelerationMs2) {
        // Impact detected or re-latched
        s_fallState.impactTimestampMs = nowMs;
        s_fallState.peakImpactAcc = mag;
        s_fallState.stillnessStartMs = 0;
        s_fallState.stillnessSampleCount = 0;
        s_fallState.inVerificationWindow = true;
        s_fallState.lastSampleMs = nowMs;
        s_state = STATE_SUSPECTED;
        outEventLabel = "IMPACT_DETECTED";

        // Record pre-impact gravity reference
        s_fallState.preImpactGravity[0] = s_mpu.baselineGravityX;
        s_fallState.preImpactGravity[1] = s_mpu.baselineGravityY;
        s_fallState.preImpactGravity[2] = s_mpu.baselineGravityZ;

        // Populate telemetry trigger reasons
        snprintf(s_fallState.triggerReasons, sizeof(s_fallState.triggerReasons),
                 "IMPACT_%.1f%s%s",
                 mag,
                 (s_fallState.preImpactMinAcc < PROFILE_DEFAULT.freeFallThresholdMs2) ? ",FREE_FALL" : "",
                 (angularSpeed > PROFILE_DEFAULT.highAngularSpeedDps) ? ",HIGH_ROTATION" : "");

        Serial.printf("[FALL_ENGINE] Impact detected! |a| = %.2f m/s^2 >= %.2f. Entering SUSPECTED.\n",
                      mag, PROFILE_DEFAULT.impactAccelerationMs2);
#if ENABLE_BLE
        sendBleEvent("IMPACT_DETECTED", "WARNING", 60, mag, 0.0f, altitudeDeltaM, 0,
                     s_fallState.triggerReasons, true, false, (s_baro.online && !s_baro.hasReadError));
#endif
        return;
    }

    // Step 2: Post-impact window monitoring (<= 3000 ms)
    if (s_fallState.inVerificationWindow) {
        uint32_t elapsedMs = nowMs - s_fallState.impactTimestampMs;

        // Check sample gap constraint (max 250 ms)
        if (nowMs - s_fallState.lastSampleMs > PROFILE_DEFAULT.maximumSampleGapMs) {
            Serial.println(F("[FALL_ENGINE] Sample gap > 250 ms exceeded. Resetting verification window."));
            s_fallState.inVerificationWindow = false;
            s_state = STATE_MONITORING;
            return;
        }
        s_fallState.lastSampleMs = nowMs;

        // Window timeout (exceeded 3000 ms without full stillness confirmation)
        if (elapsedMs > PROFILE_DEFAULT.postImpactWindowMs) {
            Serial.println(F("[FALL_ENGINE] Post-impact 3000 ms window expired without sustained stillness. Return to MONITORING."));
            s_fallState.inVerificationWindow = false;
            s_state = STATE_MONITORING;
            return;
        }

        // Check stillness condition: | |a| - 9.81 | <= 1.0 m/s^2
        float stillnessDiff = fabsf(mag - PROFILE_DEFAULT.stillnessTargetAccelerationMs2);
        if (stillnessDiff <= PROFILE_DEFAULT.stillnessToleranceMs2) {
            if (s_fallState.stillnessStartMs == 0) {
                s_fallState.stillnessStartMs = nowMs;
                s_fallState.stillnessSampleCount = 1;
                s_state = STATE_VERIFYING;
            } else {
                s_fallState.stillnessSampleCount++;
            }

            uint32_t stillDuration = nowMs - s_fallState.stillnessStartMs;

            // Check confirmation threshold: count >= 6 AND duration >= 1000 ms
            if (s_fallState.stillnessSampleCount >= PROFILE_DEFAULT.minimumStillnessSamples &&
                stillDuration >= PROFILE_DEFAULT.postImpactStillnessDurationMs) {

                // Calculate orientation change from post-impact gravity vector
                float dot = (s_fallState.preImpactGravity[0] * ax +
                             s_fallState.preImpactGravity[1] * ay +
                             s_fallState.preImpactGravity[2] * az);
                float magPre = sqrtf(s_fallState.preImpactGravity[0]*s_fallState.preImpactGravity[0] +
                                     s_fallState.preImpactGravity[1]*s_fallState.preImpactGravity[1] +
                                     s_fallState.preImpactGravity[2]*s_fallState.preImpactGravity[2]);
                float angleChangeDeg = 0.0f;
                if (magPre > 0.1f && mag > 0.1f) {
                    float cosTheta = dot / (magPre * mag);
                    if (cosTheta > 1.0f) cosTheta = 1.0f;
                    if (cosTheta < -1.0f) cosTheta = -1.0f;
                    angleChangeDeg = acosf(cosTheta) * (180.0f / 3.14159265f);
                }

                // Confidence heuristic score (0 - 100)
                int confidence = 85;
                if (angleChangeDeg >= PROFILE_DEFAULT.postureChangeThresholdDeg) confidence += 10;
                if (s_fallState.preImpactMinAcc < PROFILE_DEFAULT.freeFallThresholdMs2) confidence += 5;
                if (confidence > 100) confidence = 100;

                s_state = STATE_LOCAL_ALERTING;
                s_alertStartMs = nowMs;
                s_counters.fallEventsConfirmed++;
                s_fallState.inVerificationWindow = false;
                outEventLabel = "FALL_CONFIRMED";

                Serial.printf("[FALL_ENGINE] *** FALL VERIFIED! *** Stillness: %u ms (%u samples), Angle: %.1f deg, Conf: %d\n",
                              stillDuration, s_fallState.stillnessSampleCount, angleChangeDeg, confidence);

#if ENABLE_BLE
                sendBleEvent("INACTIVITY_DETECTED", "CRITICAL", confidence,
                             s_fallState.peakImpactAcc, angleChangeDeg, altitudeDeltaM,
                             stillDuration, s_fallState.triggerReasons, true, true, (s_baro.online && !s_baro.hasReadError));
                sendBleDeviceStatus();
#endif
            }
        } else {
            // Stillness interrupted during window: reset stillness counters
            s_fallState.stillnessStartMs = 0;
            s_fallState.stillnessSampleCount = 0;
        }
    }
}

// ============================================================================
// 14. COMMAND DISPATCHER (§8.5)
// ============================================================================
#if ENABLE_BLE
static void dispatchBleCommand(const char* payload) {
    // Robust JSON parser for standard command fields: commandType or command, commandId
    char cmdName[32] = "";
    char cmdId[37] = "";

    const char* pName = strstr(payload, "\"commandType\":");
    if (!pName) pName = strstr(payload, "\"command\":");
    if (pName) {
        if (strstr(pName, "\"commandType\":") == pName) {
            sscanf(pName, "\"commandType\":\"%31[^\"]\"", cmdName);
        } else {
            sscanf(pName, "\"command\":\"%31[^\"]\"", cmdName);
        }
    }
    const char* pId = strstr(payload, "\"commandId\":");
    if (pId) {
        sscanf(pId, "\"commandId\":\"%36[^\"]\"", cmdId);
    }

    // Deduplication check: return REJECTED with DUPLICATE_COMMAND
    if (strlen(cmdId) > 0 && strcmp(s_lastCommandId, cmdId) == 0) {
        sendBleAck(cmdId, "REJECTED", "DUPLICATE_COMMAND", "Command was already processed");
        return;
    }
    if (strlen(cmdId) > 0) {
        strncpy(s_lastCommandId, cmdId, sizeof(s_lastCommandId) - 1);
    }

    Serial.printf("[BLE_CMD] Dispatching command '%s' (ID: %s)\n", cmdName, cmdId);

    if (strcmp(cmdName, "PING") == 0) {
        sendBleAck(cmdId, "COMPLETED", nullptr, "PONG");
    } else if (strcmp(cmdName, "GET_STATUS") == 0) {
        sendBleDeviceStatus();
        sendBleAck(cmdId, "COMPLETED", nullptr, "Status reported");
    } else if (strcmp(cmdName, "START_STREAM") == 0) {
        s_bleStreaming = true;
        sendBleAck(cmdId, "COMPLETED", nullptr, "Telemetry stream started");
    } else if (strcmp(cmdName, "STOP_STREAM") == 0) {
        s_bleStreaming = false;
        sendBleAck(cmdId, "COMPLETED", nullptr, "Telemetry stream stopped");
    } else if (strcmp(cmdName, "SET_SAMPLE_RATE") == 0) {
        // FIX 3: Parse sampleRateHz (accept only 100 and 50 Hz)
        int requestedRate = 0;
        const char* pRate = strstr(payload, "\"sampleRateHz\":");
        if (pRate) {
            if (sscanf(pRate, "\"sampleRateHz\":\"%d\"", &requestedRate) != 1) {
                sscanf(pRate, "\"sampleRateHz\":%d", &requestedRate);
            }
        }
        if (requestedRate == 100 || requestedRate == 50) {
            s_imuSampleRateHz = requestedRate;
            s_imuSamplePeriodUs = 1000000UL / s_imuSampleRateHz;
            if (s_mpu.online) {
                uint8_t smplrtDiv = (requestedRate == 50) ? 19 : 9;
                i2cWriteByte(Wire, s_mpu.address, MPU6050_SMPLRT_DIV, smplrtDiv);
            }
            char msg[64];
            snprintf(msg, sizeof(msg), "Sample rate set to %d Hz", requestedRate);
            sendBleAck(cmdId, "COMPLETED", nullptr, msg);
        } else {
            sendBleAck(cmdId, "REJECTED", "UNSUPPORTED_SAMPLE_RATE", "Only 100 Hz and 50 Hz are supported");
        }
    } else if (strcmp(cmdName, "SET_REFERENCE_ALTITUDE") == 0) {
        // FIX 3: Set current stable pressure as 0 m reference altitude baseline
        if (s_baro.online && !s_baro.hasReadError && s_baro.pressurePa > 10000.0f) {
            s_referencePressurePa = s_baro.pressurePa;
            s_baro.altitudeDeltaM = 0.0f;
            sendBleAck(cmdId, "COMPLETED", nullptr, "Reference altitude reset to 0m at current pressure");
        } else {
            sendBleAck(cmdId, "FAILED", "BAROMETER_UNAVAILABLE", "Cannot set reference altitude: barometer offline");
        }
    } else if (strcmp(cmdName, "TRIGGER_BUZZER") == 0) {
        int durationMs = 2000;
        const char* pMs = strstr(payload, "\"remainingMs\":");
        if (pMs) sscanf(pMs, "\"remainingMs\":%d", &durationMs);
        s_buzzerPatternDeadlineMs = millis() + durationMs;
        sendBleAck(cmdId, "COMPLETED", nullptr, "Buzzer pattern active");
    } else if (strcmp(cmdName, "STOP_BUZZER") == 0) {
        s_buzzerPatternDeadlineMs = 0;
        digitalWrite(PIN_BUZZER, LOW);
        sendBleAck(cmdId, "COMPLETED", nullptr, "Buzzer silenced");
    } else if (strcmp(cmdName, "ACK_EVENT") == 0) {
        // Plan §7.3, §8.5: ACK_EVENT does NOT silence local alarm
        sendBleAck(cmdId, "COMPLETED", nullptr, "Event acknowledged (local alarm remains active until cancel)");
    } else if (strcmp(cmdName, "CANCEL_ALERT") == 0) {
        if (s_state == STATE_LOCAL_ALERTING || s_state == STATE_VERIFYING || s_state == STATE_SUSPECTED) {
            s_state = STATE_MONITORING;
            digitalWrite(PIN_BUZZER, LOW);
            sendBleEvent("SOS_CANCELLED", "INFO", 100, 0.0f, 0.0f, 0.0f, 0, "BLE_CANCEL_COMMAND", false, false, false);
            sendBleDeviceStatus();
            sendBleAck(cmdId, "COMPLETED", nullptr, "Alert cancelled by remote BLE command");
        } else {
            sendBleAck(cmdId, "COMPLETED", nullptr, "No active alert to cancel");
        }
    } else if (strcmp(cmdName, "START_SELF_TEST") == 0) {
        performSelfTestAndCalibration();
        sendBleAck(cmdId, "COMPLETED", nullptr, "Self test finished");
    } else if (strcmp(cmdName, "SET_DEVICE_TIME") == 0) {
        // Plan §8.5, §8.6: Synchronize Unix timestamp (milliseconds)
        uint64_t ep = 0;
        const char* pTime = strstr(payload, "\"unixTimeMs\":");
        if (!pTime) pTime = strstr(payload, "\"epochTime\":");
        if (pTime) {
            if (sscanf(pTime, "\"unixTimeMs\":%llu", &ep) == 1 ||
                sscanf(pTime, "\"unixTimeMs\":\"%llu\"", &ep) == 1 ||
                sscanf(pTime, "\"epochTime\":%llu", &ep) == 1) {
                if (ep < 10000000000ULL) ep *= 1000ULL; // Convert seconds to milliseconds
                s_deviceEpochTimeMs = ep;
                s_epochSyncLocalMs = millis();
                s_timeSynced = true;
                if (s_lastErrorCode != nullptr && strcmp(s_lastErrorCode, "TIME_NOT_SYNCED") == 0) {
                    s_lastErrorCode = nullptr;
                }
            }
        }
        sendBleAck(cmdId, "COMPLETED", nullptr, "Epoch clock synchronized");
    } else if (strcmp(cmdName, "REBOOT_DEVICE") == 0) {
        // Plan §8.5: Reject reboot if device is currently in alerting state
        if (s_state == STATE_LOCAL_ALERTING || s_state == STATE_VERIFYING) {
            sendBleAck(cmdId, "REJECTED", "DEVICE_ALERTING", "Cannot reboot during active alert");
        } else {
            sendBleAck(cmdId, "ACCEPTED", nullptr, "Rebooting device...");
            delay(100);
            esp_restart();
        }
    } else {
        sendBleAck(cmdId, "REJECTED", "UNKNOWN_COMMAND", "Command not recognized");
    }
}
#endif

// ============================================================================
// 15. SETUP & INITIALIZATION
// ============================================================================
void setup() {
    Serial.begin(115200);

#if ARDUINO_USB_CDC_ON_BOOT
    // Wait briefly for native USB-CDC to attach if present
    delay(500);
#endif

    // Generate BLE Device Name: FALLSAFE-xxxx from BT MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(s_bleDeviceName, sizeof(s_bleDeviceName), "FALLSAFE-%02X%02X", mac[4], mac[5]);

    Serial.println(F("\n\n======================================================="));
    Serial.println(F("NCKH27PA - ESP32-S3 SUPER MINI OFFICIAL FIRMWARE"));
    Serial.printf("Device ID: %s | Build: %s %s | Protocol: %d\n",
                  s_bleDeviceName, __DATE__, __TIME__, PROTOCOL_VERSION);
    Serial.println(F("======================================================="));

    // Configure GPIOs
    pinMode(PIN_BUTTON_SOS, INPUT_PULLUP);
    pinMode(PIN_BUTTON_CANCEL, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_BAT_1, OUTPUT);
    pinMode(PIN_LED_BAT_2, OUTPUT);
    pinMode(PIN_LED_BAT_3, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Initialize I2C Bus 0 for MPU6050 (SDA = GPIO 7, SCL = GPIO 6, 400kHz)
    Wire.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 400000);
    Wire.setTimeOut(25); // 25ms timeout prevents bus lockup from blocking SOS button

    // Initialize I2C Bus 1 for MS5611 (SDA = GPIO 3, SCL = GPIO 2, 400kHz)
    Wire1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, 400000);
    Wire1.setTimeOut(25);

    // Print Profile and Thresholds
    printProfile();

    // Hardware Self-Test & Calibration
    performSelfTestAndCalibration();

#if ENABLE_BLE
    BLEDevice::init(s_bleDeviceName);
    s_pServer = BLEDevice::createServer();
    s_pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = s_pServer->createService(UUID_SERVICE);

    s_pCharStream = pService->createCharacteristic(
        UUID_CHAR_STREAM,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharStream->addDescriptor(new BLE2902());

    s_pCharEvent = pService->createCharacteristic(
        UUID_CHAR_EVENT,
        BLECharacteristic::PROPERTY_INDICATE
    );
    s_pCharEvent->addDescriptor(new BLE2902());

    s_pCharStatus = pService->createCharacteristic(
        UUID_CHAR_STATUS,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharStatus->addDescriptor(new BLE2902());

    s_pCharCommand = pService->createCharacteristic(
        UUID_CHAR_COMMAND,
        BLECharacteristic::PROPERTY_WRITE
    );
    s_pCharCommand->setCallbacks(new CommandCallbacks());

    s_pCharAck = pService->createCharacteristic(
        UUID_CHAR_ACK,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    s_pCharAck->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(UUID_SERVICE);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] GATT Server initialized. Device name: %s\n", s_bleDeviceName);
#endif

    // Watchdog Timer Initialization (5 seconds timeout)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = 0,
        .trigger_panic = false
    };
    esp_task_wdt_deinit();
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL);
#else
    esp_task_wdt_init(5, false);
    esp_task_wdt_add(NULL);
#endif

    if (s_rawCsvMode) {
        printCsvHeader();
    }
    Serial.println(F("[SYSTEM] Initialization complete. System active."));
}

// ============================================================================
// 16. MAIN LOOP (Configurable 100/50 Hz IMU Scheduling & Non-blocking Tasks)
// ============================================================================
void loop() {
    // Reset hardware watchdog
    esp_task_wdt_reset();

    uint32_t nowMs = millis();
    uint32_t nowUs = micros();

    // 1. Non-blocking Barometer Polling (Bus 1)
    pollMS5611(nowUs);

    // Track baseline pressure drift under condition (b)
    if (s_baro.online && !s_baro.hasReadError) {
        updatePressureBaselineTracking(nowMs, s_baro.pressurePa);
    }

    // 2. High-rate IMU Sampling (Bus 0: 100 Hz = 10,000 us or 50 Hz = 20,000 us schedule)
    static uint32_t s_lastImuSampleUs = 0;
    static uint8_t s_imuConsecutiveFailures = 0;
    if (nowUs - s_lastImuSampleUs >= s_imuSamplePeriodUs) {
        if (s_lastImuSampleUs > 0 && (nowUs - s_lastImuSampleUs > (s_imuSamplePeriodUs * 3 / 2))) {
            s_counters.lateLoopSlots++;
        }
        s_lastImuSampleUs = nowUs;

        float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
        bool imuSuccess = readMPU6050(ax, ay, az, gx, gy, gz);

        if (imuSuccess) {
            s_imuConsecutiveFailures = 0;
            s_counters.imuSamplesRead++;
            float mag = sqrtf(ax * ax + ay * ay + az * az);

            // Store in circular buffer (1000 samples @ 100 Hz = 10s pre-event buffer)
            s_ringBuffer[s_ringHead].timestampMs = nowMs;
            s_ringBuffer[s_ringHead].ax = ax;
            s_ringBuffer[s_ringHead].ay = ay;
            s_ringBuffer[s_ringHead].az = az;
            s_ringBuffer[s_ringHead].gx = gx;
            s_ringBuffer[s_ringHead].gy = gy;
            s_ringBuffer[s_ringHead].gz = gz;
            s_ringBuffer[s_ringHead].magnitude = mag;
            s_ringBuffer[s_ringHead].pressurePa = s_baro.pressurePa;
            s_ringBuffer[s_ringHead].altitudeDeltaM = s_baro.altitudeDeltaM;
            s_ringHead = (s_ringHead + 1) % RING_BUFFER_SIZE;
            if (s_ringCount < RING_BUFFER_SIZE) {
                s_ringCount++;
            }

            // Fall Detection Engine
            const char* eventLabel = nullptr;
            processFallDetection(nowMs, ax, ay, az, gx, gy, gz,
                                 s_baro.pressurePa, s_baro.altitudeDeltaM, eventLabel);

            // CSV output stream (FIX 3: altitudeDeltaM)
            if (s_rawCsvMode) {
                Serial.printf("%u,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%s,%s\n",
                              nowMs, ax, ay, az, mag, gx, gy, gz,
                              s_baro.pressurePa, s_baro.altitudeDeltaM,
                              deviceStateToString(s_state),
                              eventLabel ? eventLabel : "");
            }

#if ENABLE_BLE
            // Send BLE real-time telemetry if streaming requested (decimated to ~25Hz to save BLE bandwidth)
            static uint8_t s_streamDecimator = 0;
            uint8_t decimateTarget = (s_imuSampleRateHz == 50) ? 2 : 4;
            if (++s_streamDecimator >= decimateTarget) {
                s_streamDecimator = 0;
                sendBleSensorStream(ax, ay, az, gx, gy, gz,
                                    s_baro.pressurePa, s_baro.altitudeDeltaM, s_baro.temperatureC);
            }
#endif
        } else {
            s_sampleDropCount++;
            if (++s_imuConsecutiveFailures >= 10 && s_state != STATE_DEGRADED) {
                s_state = STATE_DEGRADED;
                s_lastErrorCode = "IMU_READ_FAILED";
                s_mpu.online = false;
#if ENABLE_BLE
                sendBleEvent("SENSOR_ERROR", "CRITICAL", 100, 0.0f, 0.0f, 0.0f, 0, "IMU_READ_FAILED", false, false, false);
                sendBleDeviceStatus();
#endif
            }
        }
    }

    // 3. Periodic Battery Status Check (hysteresis evaluation)
    static uint32_t s_lastBatCheckMs = 0;
    if (nowMs - s_lastBatCheckMs >= 10000) {
        s_lastBatCheckMs = nowMs;
        updateBatteryStatus(nowMs);
    }

    // 4. Button and SOS State Updates
    updateButtons(nowMs);

    // 5. Buzzer & LED Rhythm
    updateBuzzer(nowMs);

#if ENABLE_BLE
    // 6. Process pending BLE command asynchronously
    if (s_pendingCommand) {
        s_pendingCommand = false;
        dispatchBleCommand(s_pendingCmdPayload);
    }

    // Periodic Device Status over BLE (every 5 seconds)
    static uint32_t s_lastBleStatusMs = 0;
    if (nowMs - s_lastBleStatusMs >= 5000) {
        s_lastBleStatusMs = nowMs;
        sendBleDeviceStatus();
    }
#endif

    // 7. Serial Human Mode Display (~4 lines/second)
    if (!s_rawCsvMode && (nowMs - s_lastHumanLogMs >= 250)) {
        s_lastHumanLogMs = nowMs;
        float curAx = 0, curAy = 0, curAz = 0, curGx = 0, curGy = 0, curGz = 0;
        readMPU6050(curAx, curAy, curAz, curGx, curGy, curGz);
        float mag = sqrtf(curAx * curAx + curAy * curAy + curAz * curAz);

        Serial.printf("[T=%06u ms] State: %-14s | |a|: %5.2f m/s^2 | AltDelta: %6.2f m | BLE: %s\n",
                      nowMs,
                      deviceStateToString(s_state),
                      mag,
                      s_baro.altitudeDeltaM,
                      s_bleConnected ? (s_bleStreaming ? "STREAMING" : "CONNECTED") : "ADV");
    }

    // 8. Interactive Serial Console Commands
    while (Serial.available() > 0) {
        char ch = (char)Serial.read();
        if (ch == 'r' || ch == 'R') {
            s_rawCsvMode = !s_rawCsvMode;
            if (s_rawCsvMode) {
                printCsvHeader();
            } else {
                Serial.println(F("[SERIAL] Switched to HUMAN TELEMETRY MODE (~4 Hz)."));
            }
        } else if (ch == 't' || ch == 'T') {
            printProfile();
        } else if (ch == 'c' || ch == 'C') {
            performSelfTestAndCalibration();
        } else if (ch == 's' || ch == 'S') {
            printSystemStatus();
        } else if (ch == 'h' || ch == 'H') {
            Serial.println(F("\n--- SERIAL CLI HELP ---"));
            Serial.println(F("  r : Toggle Raw CSV mode ON/OFF"));
            Serial.println(F("  t : Print Profile & Fall Detection Thresholds"));
            Serial.println(F("  c : Re-run Calibration & Self-Test"));
            Serial.println(F("  s : Print System Status & Performance Counters"));
            Serial.println(F("  h : Print this Help"));
            Serial.println(F("-----------------------\n"));
        }
    }
}
