// mpu6050_test.cpp — host tests for the MPU6050 register driver (T13 section 8.1).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "fake_i2c.h"
#include "mpu6050.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void ReportFail(const char* file, int line, const char* what) {
  ++g_failures;
  std::printf("FAIL %s:%d: %s\n", file, line, what);
}

#define CHECK(cond)                                            \
  do {                                                         \
    ++g_checks;                                                \
    if (!(cond)) {                                             \
      ReportFail(__FILE__, __LINE__, #cond);                   \
    }                                                          \
  } while (false)

#define CHECK_NEAR(actual, expected, tol)                                          \
  do {                                                                             \
    ++g_checks;                                                                    \
    const double a_ = (actual);                                                    \
    const double e_ = (expected);                                                  \
    if (!(std::fabs(a_ - e_) <= (tol))) {                                          \
      std::printf("FAIL %s:%d: %s (%g) vs %s (%g) tol %g\n", __FILE__, __LINE__,   \
                  #actual, a_, #expected, e_, static_cast<double>(tol));           \
      ++g_failures;                                                                \
    }                                                                              \
  } while (false)

std::vector<uint32_t> g_delays;

void RecordDelay(uint32_t ms) { g_delays.push_back(ms); }

using esp_node::test::FakeI2c;

// Scripts the 14-byte ACCEL_XOUT_H burst (big-endian int16 each).
void SetBurst(FakeI2c& fake, uint8_t addr, int16_t ax, int16_t ay, int16_t az, int16_t temp,
              int16_t gx, int16_t gy, int16_t gz) {
  const int16_t values[7] = {ax, ay, az, temp, gx, gy, gz};
  std::vector<uint8_t> bytes;
  for (int16_t value : values) {
    const uint16_t raw = static_cast<uint16_t>(value);
    bytes.push_back(static_cast<uint8_t>(raw >> 8));
    bytes.push_back(static_cast<uint8_t>(raw & 0x00FF));
  }
  fake.setRegBytes(addr, esp_node::mpu6050_reg::kAccelXoutH, bytes);
}

void TestInitSequenceAtPrimaryAddress() {
  FakeI2c fake;
  fake.addDevice(0x68);
  fake.setReg(0x68, esp_node::mpu6050_reg::kWhoAmI, 0x68);
  g_delays.clear();

  esp_node::Mpu6050 mpu(fake);
  const esp_node::MpuConfig config;  // ticket defaults: DLPF 1, SMPLRT_DIV 9, FS 0
  CHECK(mpu.begin(config, RecordDelay));

  CHECK(mpu.status() == esp_node::MpuStatus::kOk);
  CHECK(std::strcmp(mpu.statusText(), "ok") == 0);
  CHECK(mpu.detected());
  CHECK(mpu.address() == 0x68);
  CHECK(mpu.lastWhoAmI() == 0x68);

  // Exact, ordered register writes (T13 section 5.1 steps 2..9).
  const std::vector<std::pair<uint8_t, uint8_t>> expected = {
      {0x6B, 0x80},  // PWR_MGMT_1 = DEVICE_RESET
      {0x6B, 0x01},  // PWR_MGMT_1 = PLL with X gyro reference
      {0x68, 0x07},  // SIGNAL_PATH_RESET = 0x07
      {0x6C, 0x00},  // PWR_MGMT_2 = 0 (all axes on)
      {0x1A, 0x01},  // CONFIG (DLPF_CFG = 1)
      {0x19, 0x09},  // SMPLRT_DIV = 9 -> 100 Hz
      {0x1B, 0x00},  // GYRO_CONFIG FS_SEL 0 -> +-250 dps
      {0x1C, 0x00},  // ACCEL_CONFIG AFS_SEL 0 -> +-2 g
      {0x38, 0x00},  // INT_ENABLE = 0
  };
  const std::vector<std::pair<uint8_t, uint8_t>> got = fake.writtenRegs();
  CHECK(got.size() == expected.size());
  for (size_t i = 0; i < expected.size() && i < got.size(); ++i) {
    if (got[i] != expected[i]) {
      std::printf("  write[%zu] = (0x%02x,0x%02x) expected (0x%02x,0x%02x)\n", i, got[i].first,
                  got[i].second, expected[i].first, expected[i].second);
    }
    CHECK(got[i] == expected[i]);
  }
  // Every write went to the probed address.
  for (const auto& op : fake.ops()) {
    if (op.is_write) {
      CHECK(op.addr == 0x68);
    }
  }

  // Two blocking reset waits of at least 100 ms each.
  CHECK(g_delays.size() == 2);
  for (uint32_t ms : g_delays) {
    CHECK(ms >= 100);
  }
  // WHO_AM_I is read once for the probe and once after the reset.
  CHECK(fake.readCount(0x68, esp_node::mpu6050_reg::kWhoAmI) == 2);
}

void TestInitFallsBackToAlternativeAddress() {
  FakeI2c fake;
  fake.addDevice(0x69);  // 0x68 stays absent
  fake.setReg(0x69, esp_node::mpu6050_reg::kWhoAmI, 0x68);
  g_delays.clear();

  esp_node::Mpu6050 mpu(fake);
  CHECK(mpu.begin(esp_node::MpuConfig{}, RecordDelay));
  CHECK(mpu.address() == 0x69);
  CHECK(mpu.lastWhoAmI() == 0x68);
  CHECK(!fake.ops().empty());
  CHECK(fake.ops().front().addr == 0x68);  // probe tries 0x68 first
  CHECK(!fake.ops().front().ok);
  for (const auto& op : fake.ops()) {
    if (op.is_write) {
      CHECK(op.addr == 0x69);
    }
  }
}

void TestWhoAmIMismatchIsReported() {
  FakeI2c fake;
  fake.addDevice(0x68);
  fake.setReg(0x68, esp_node::mpu6050_reg::kWhoAmI, 0x72);  // not an MPU6050
  g_delays.clear();

  esp_node::Mpu6050 mpu(fake);
  CHECK(!mpu.begin(esp_node::MpuConfig{}, RecordDelay));
  CHECK(mpu.status() == esp_node::MpuStatus::kWhoAmIMismatch);
  CHECK(std::strcmp(mpu.statusText(), "who_am_i_mismatch") == 0);
  CHECK(mpu.lastWhoAmI() == 0x72);  // the observed value is reported, not hidden
  CHECK(mpu.detected());            // it answered, but it is the wrong part
  // Init stops at the WHO_AM_I check: only the reset write happened.
  CHECK(fake.writtenRegs().size() == 1);
  const std::pair<uint8_t, uint8_t> reset_write = {0x6B, 0x80};
  CHECK(fake.writtenRegs()[0] == reset_write);
}

void TestNotDetected() {
  FakeI2c fake;  // no device on the bus
  g_delays.clear();

  esp_node::Mpu6050 mpu(fake);
  CHECK(!mpu.begin(esp_node::MpuConfig{}, RecordDelay));
  CHECK(mpu.status() == esp_node::MpuStatus::kNotDetected);
  CHECK(std::strcmp(mpu.statusText(), "not_detected") == 0);
  CHECK(!mpu.detected());
  CHECK(fake.writtenRegs().empty());
  CHECK(g_delays.empty());

  // A read attempt on a non-detected part fails and writes nothing.
  esp_node::MpuRaw raw;
  CHECK(!mpu.readRaw(raw));
}

void TestReadDecodesAndScales() {
  FakeI2c fake;
  fake.addDevice(0x68);
  fake.setReg(0x68, esp_node::mpu6050_reg::kWhoAmI, 0x68);
  esp_node::Mpu6050 mpu(fake);
  CHECK(mpu.begin(esp_node::MpuConfig{}, RecordDelay));

  SetBurst(fake, 0x68, /*ax=*/16384, /*ay=*/-16384, /*az=*/16384, /*temp=*/0, /*gx=*/131,
           /*gy=*/-262, /*gz=*/0);
  esp_node::MpuRaw raw;
  CHECK(mpu.readRaw(raw));
  CHECK(raw.ax == 16384);
  CHECK(raw.ay == -16384);
  CHECK(raw.az == 16384);
  CHECK(raw.gx == 131);
  CHECK(raw.gy == -262);
  CHECK(raw.gz == 0);

  esp_node::MpuSample sample;
  CHECK(mpu.read(sample));
  // 16384 LSB = 1 g = 9.80665 m/s^2, exactly.
  CHECK_NEAR(sample.ax, 9.80665, 1e-9);
  CHECK_NEAR(sample.az, 9.80665, 1e-9);
  CHECK_NEAR(sample.ay, -9.80665, 1e-9);
  // Gyro scale: 1 dps = 32768/250 = 131.072 LSB.
  CHECK_NEAR(esp_node::Mpu6050::kGyroLsbPerDps, 131.072, 1e-12);
  CHECK_NEAR(1.0 / esp_node::Mpu6050::kGyroLsbPerDps, 250.0 / 32768.0, 1e-15);
  CHECK_NEAR(esp_node::Mpu6050::gyroToDps(131), 131.0 / 131.072, 1e-12);
  CHECK_NEAR(sample.gx, 131.0 / 131.072, 1e-12);
  CHECK_NEAR(sample.gy, -262.0 / 131.072, 1e-12);
  CHECK_NEAR(sample.gz, 0.0, 1e-12);

  // Big-endian check with a distinctive value: 0x0102 = 258.
  SetBurst(fake, 0x68, /*ax=*/0x0102, 0, 0, 0, 0, 0, 0);
  CHECK(mpu.readRaw(raw));
  CHECK(raw.ax == 258);
  CHECK_NEAR(esp_node::Mpu6050::accelToMps2(raw.ax), 258.0 / 16384.0 * 9.80665, 1e-12);
}

void TestReadFailureLeavesOutputUntouched() {
  FakeI2c fake;
  fake.addDevice(0x68);
  fake.setReg(0x68, esp_node::mpu6050_reg::kWhoAmI, 0x68);
  esp_node::Mpu6050 mpu(fake);
  CHECK(mpu.begin(esp_node::MpuConfig{}, RecordDelay));

  SetBurst(fake, 0x68, 16384, 0, 0, 0, 131, 0, 0);
  esp_node::MpuSample good;
  CHECK(mpu.read(good));
  CHECK_NEAR(good.ax, 9.80665, 1e-9);

  fake.setReadsFail(true);
  esp_node::MpuRaw raw;
  raw.ax = 111;
  raw.ay = -222;
  raw.az = 333;
  raw.temp = 444;
  raw.gx = 555;
  raw.gy = -666;
  raw.gz = 777;
  CHECK(!mpu.readRaw(raw));  // no partially updated output
  CHECK(raw.ax == 111);
  CHECK(raw.ay == -222);
  CHECK(raw.az == 333);
  CHECK(raw.temp == 444);
  CHECK(raw.gx == 555);
  CHECK(raw.gy == -666);
  CHECK(raw.gz == 777);

  esp_node::MpuSample sample;
  sample.ax = 1.5;
  sample.ay = 2.5;
  sample.az = 3.5;
  sample.gx = 4.5;
  sample.gy = 5.5;
  sample.gz = 6.5;
  CHECK(!mpu.read(sample));
  CHECK_NEAR(sample.ax, 1.5, 1e-12);
  CHECK_NEAR(sample.ay, 2.5, 1e-12);
  CHECK_NEAR(sample.az, 3.5, 1e-12);
  CHECK_NEAR(sample.gx, 4.5, 1e-12);
  CHECK_NEAR(sample.gy, 5.5, 1e-12);
  CHECK_NEAR(sample.gz, 6.5, 1e-12);
}

}  // namespace

int main() {
  TestInitSequenceAtPrimaryAddress();
  TestInitFallsBackToAlternativeAddress();
  TestWhoAmIMismatchIsReported();
  TestNotDetected();
  TestReadDecodesAndScales();
  TestReadFailureLeavesOutputUntouched();

  std::printf("mpu6050_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
