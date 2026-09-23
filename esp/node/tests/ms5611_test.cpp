// ms5611_test.cpp — host tests for the MS5611 driver (T13 sections 5.2 and 8.2).
//
// The non-blocking state machine is driven with a fake clock (`now_us`), so the conversion
// deadlines, the 25 Hz start limit and the "poll() returns true exactly once per sample"
// contract are all asserted without hardware.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "fake_i2c.h"
#include "ms5611.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void ReportFail(const char* file, int line, const char* what) {
  ++g_failures;
  std::printf("FAIL %s:%d: %s\n", file, line, what);
}

#define CHECK(cond)                          \
  do {                                       \
    ++g_checks;                              \
    if (!(cond)) {                           \
      ReportFail(__FILE__, __LINE__, #cond); \
    }                                        \
  } while (false)

#define CHECK_NEAR(actual, expected, tol)                                        \
  do {                                                                           \
    ++g_checks;                                                                  \
    const double a_ = (actual);                                                  \
    const double e_ = (expected);                                                \
    if (!(std::fabs(a_ - e_) <= (tol))) {                                        \
      std::printf("FAIL %s:%d: %s (%g) vs %s (%g) tol %g\n", __FILE__, __LINE__, \
                  #actual, a_, #expected, e_, static_cast<double>(tol));         \
      ++g_failures;                                                              \
    }                                                                            \
  } while (false)

using esp_node::Ms5611;
using esp_node::Ms5611Config;
using esp_node::test::FakeI2c;

// Reference PROM from T13 section 5.2 (word 0 = manufacturer data, 1..6 = C1..C6).
const uint16_t kPromWords[8] = {0x4000, 40127, 36924, 23317, 23282, 33464, 28312, 0x0000};

// Datasheet test pair (T13 section 5.2): expected ~20.08 C and ~100.0 kPa.
constexpr uint32_t kD1Vector = 9085466;
constexpr uint32_t kD2Vector = 8569150;

// Low-temperature pair that exercises both second-order compensation branches
// (TEMP100 = -2654.5 < -1500). Expected: -26.5454 C, 100000.28 Pa.
constexpr uint32_t kD1Cold = 9621794;
constexpr uint32_t kD2Cold = 7381514;

constexpr uint64_t kConvWaitUs = 9100;  // OSR 4096 max conversion wait
constexpr uint64_t kResetWaitUs = 3000;

// Same datasheet AN520 CRC-4 as the driver, written out here again as a regression pin: if
// the driver algorithm is changed, this test fails.
uint8_t ReferenceCrc4(const uint16_t* words) {
  uint16_t rem = 0;
  for (int i = 0; i < 16; ++i) {
    const uint16_t word = words[i >> 1];
    rem ^= ((i % 2) == 1) ? static_cast<uint16_t>(word & 0x00FF)
                          : static_cast<uint16_t>(word >> 8);
    for (int bit = 0; bit < 8; ++bit) {
      rem = ((rem & 0x8000) != 0) ? static_cast<uint16_t>((rem << 1) ^ 0x3000)
                                  : static_cast<uint16_t>(rem << 1);
    }
  }
  return static_cast<uint8_t>((rem >> 12) & 0x0F);
}

std::vector<uint8_t> Bytes16(uint16_t value) {
  return {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0x00FF)};
}

std::vector<uint8_t> Bytes24(uint32_t value) {
  return {static_cast<uint8_t>((value >> 16) & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF),
          static_cast<uint8_t>(value & 0xFF)};
}

void SetPromWord(FakeI2c& fake, uint8_t addr, uint8_t index, uint16_t value) {
  fake.setRegBytes(addr, static_cast<uint8_t>(0xA0 + 2 * index), Bytes16(value));
}

// Programs a device with the reference PROM and a valid CRC word.
void SetupDevice(FakeI2c& fake, uint8_t addr) {
  fake.addDevice(addr);
  for (uint8_t i = 0; i < 7; ++i) {
    SetPromWord(fake, addr, i, kPromWords[i]);
  }
  SetPromWord(fake, addr, 7, ReferenceCrc4(kPromWords));
}

// Drives one conversion cycle starting at t0 (t0 must be a legal start instant) and returns
// the completion call's value.
bool DriveCycle(Ms5611& ms, uint64_t t0, uint64_t* completion_us) {
  CHECK(!ms.poll(t0));
  CHECK(ms.state() == Ms5611::State::kD1Wait);
  CHECK(!ms.poll(t0 + kConvWaitUs));
  CHECK(ms.state() == Ms5611::State::kD2Wait);
  const bool done = ms.poll(t0 + 2 * kConvWaitUs);
  if (completion_us != nullptr) {
    *completion_us = t0 + 2 * kConvWaitUs;
  }
  return done;
}

void TestBeginResetsAndReadsProm() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(ms.state() == Ms5611::State::kResetWait);
  CHECK(ms.address() == 0x77);
  // Reset command 0x1E was issued before any conversion.
  CHECK(fake.commands().size() == 1);
  CHECK(fake.commands()[0] == 0x1E);
  CHECK(!ms.promRead());

  // The reset wait is honoured: no PROM read before 3000 us.
  CHECK(!ms.poll(kResetWaitUs - 1));
  CHECK(!ms.promRead());
  CHECK(fake.readRegsOk().size() == 1);  // only the address probe read so far

  CHECK(!ms.poll(kResetWaitUs));
  CHECK(ms.promRead());
  CHECK(ms.state() == Ms5611::State::kIdle);
  CHECK(ms.initialized());
  CHECK(!ms.hasError());

  // PROM words 0..7 were read from 0xA0 + 2*i, in order (after the 0xA0 probe read).
  const std::vector<uint8_t> expected_reads = {0xA0, 0xA0, 0xA2, 0xA4, 0xA6, 0xA8, 0xAA, 0xAC,
                                               0xAE};
  CHECK(fake.readRegsOk() == expected_reads);

  CHECK(ms.promWord(0) == 0x4000);
  CHECK(ms.c(1) == 40127);
  CHECK(ms.c(2) == 36924);
  CHECK(ms.c(3) == 23317);
  CHECK(ms.c(4) == 23282);
  CHECK(ms.c(5) == 33464);
  CHECK(ms.c(6) == 28312);
  CHECK(ms.promWord(7) == static_cast<uint16_t>(ReferenceCrc4(kPromWords)));
  // Out-of-range PROM accessors are inert.
  CHECK(ms.c(0) == 0);
  CHECK(ms.c(7) == 0);
  CHECK(ms.promWord(8) == 0);

  // CRC-4 of the reference PROM: computed value and the match result agree.
  CHECK(ms.computedCrc() == ReferenceCrc4(kPromWords));
  CHECK(ms.promCrc() == ReferenceCrc4(kPromWords));
  CHECK(ms.crcOk());
}

void TestCrcMismatchIsDetected() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  const uint8_t crc = ReferenceCrc4(kPromWords);
  SetPromWord(fake, 0x77, 7, static_cast<uint16_t>(crc ^ 0x05));  // wrong CRC word
  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(ms.promRead());
  CHECK(!ms.crcOk());
  CHECK(ms.promCrc() == static_cast<uint8_t>(crc ^ 0x05));
  CHECK(ms.computedCrc() == crc);

  // A single flipped PROM data bit must also break the CRC.
  FakeI2c fake2;
  SetupDevice(fake2, 0x77);
  SetPromWord(fake2, 0x77, 1, static_cast<uint16_t>(kPromWords[1] ^ 0x0001));
  Ms5611 ms2(fake2);
  CHECK(ms2.begin(0));
  CHECK(!ms2.poll(kResetWaitUs));
  CHECK(!ms2.crcOk());
}

void TestProbeFallsBackToAlternateAddress() {
  FakeI2c fake;
  SetupDevice(fake, 0x76);  // 0x77 absent
  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(ms.address() == 0x76);
  CHECK(!fake.ops().empty());
  CHECK(fake.ops().front().addr == 0x77);  // 0x77 is tried first
  CHECK(!fake.ops().front().ok);
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(ms.promRead());
  CHECK(ms.c(1) == 40127);
  for (const auto& op : fake.ops()) {
    if (op.is_write) {
      CHECK(op.addr == 0x76);
    }
  }
}

void TestNotDetectedThenReinit() {
  FakeI2c fake;  // no device at all
  Ms5611 ms(fake);
  CHECK(!ms.begin(0));
  CHECK(ms.hasError());
  CHECK(std::strcmp(ms.errorText(), "not_detected") == 0);
  CHECK(std::strcmp(ms.stateText(), "error") == 0);
  CHECK(ms.state() == Ms5611::State::kError);
  CHECK(!ms.poll(100000));  // an error state never yields a sample
  CHECK(ms.sampleCount() == 0);

  // Re-init (the logger retries every 5 s) recovers once the device answers.
  SetupDevice(fake, 0x77);
  CHECK(ms.begin(0));
  CHECK(!ms.hasError());
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(ms.promRead());
}

void TestNonBlockingCycleTiming() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  fake.queueRead(0x77, 0x00, Bytes24(kD1Vector));  // D1 read
  fake.queueRead(0x77, 0x00, Bytes24(kD2Vector));  // D2 read

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(ms.busy());
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(fake.commands().size() == 1);

  const uint64_t start = kResetWaitUs + 1;  // 3001: first cycle may start immediately
  CHECK(!ms.poll(start));
  CHECK(ms.state() == Ms5611::State::kD1Wait);
  CHECK(ms.busy());
  CHECK(fake.commands().size() == 2);
  CHECK(fake.commands()[1] == 0x48);  // D1 command for OSR 4096
  CHECK(fake.readCount(0x77, 0x00) == 0);

  // Does not complete before the 9100 us conversion window, completes exactly on it.
  CHECK(!ms.poll(start + kConvWaitUs - 1));
  CHECK(ms.state() == Ms5611::State::kD1Wait);
  CHECK(fake.commands().size() == 2);
  CHECK(!ms.poll(start + kConvWaitUs));
  CHECK(ms.state() == Ms5611::State::kD2Wait);
  CHECK(fake.commands().size() == 3);
  CHECK(fake.commands()[2] == 0x58);  // D2 command for OSR 4096
  CHECK(fake.readCount(0x77, 0x00) == 1);

  CHECK(!ms.poll(start + 2 * kConvWaitUs - 1));
  CHECK(ms.state() == Ms5611::State::kD2Wait);
  CHECK(ms.poll(start + 2 * kConvWaitUs));
  CHECK(ms.state() == Ms5611::State::kIdle);
  CHECK(!ms.busy());
  CHECK(ms.sampleCount() == 1);
  CHECK(fake.readCount(0x77, 0x00) == 2);
  CHECK(ms.lastSample().d1 == kD1Vector);
  CHECK(ms.lastSample().d2 == kD2Vector);
  CHECK(ms.lastSample().in_band);

  // No double completion: further polls before the next allowed start do nothing at all.
  CHECK(!ms.poll(start + 2 * kConvWaitUs + 1));
  CHECK(!ms.poll(30000));
  CHECK(!ms.poll(43000));  // one microsecond before last_start + 40000
  CHECK(ms.sampleCount() == 1);
  CHECK(fake.commands().size() == 3);
  CHECK(fake.readCount(0x77, 0x00) == 2);

  // 25 Hz start limit: only at last_start + 40000 (= 43001) does a new cycle begin.
  CHECK(!ms.poll(43001));
  CHECK(ms.sampleCount() == 1);  // started, not completed
  CHECK(fake.commands().size() == 4);
  CHECK(ms.state() == Ms5611::State::kD1Wait);
}

void TestPollCompletesExactlyOncePerSample() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  // Both ADC reads return the same bytes: only completion counting is asserted here.
  fake.setRegBytes(0x77, 0x00, Bytes24(9085466));

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  uint32_t completions = 0;
  uint64_t previous = 0;
  for (uint64_t now = 0; now <= 200000; now += 100) {
    if (ms.poll(now)) {
      ++completions;
      // A start costs 2*9100 us of conversion plus the 40000 us start to start period, so
      // consecutive completions can never be closer than 40000 us.
      if (completions > 1) {
        CHECK(now - previous >= 40000);
      }
      previous = now;
    }
  }
  // Over 200 ms with a 25 Hz start rate and 18.2 ms per cycle: 5 samples, no more.
  CHECK(completions == 5);
  CHECK(ms.sampleCount() == 5);
  CHECK(completions == ms.sampleCount());
}

void TestDatasheetVector() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  fake.queueRead(0x77, 0x00, Bytes24(kD1Vector));
  fake.queueRead(0x77, 0x00, Bytes24(kD2Vector));

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(!ms.poll(kResetWaitUs));
  uint64_t completion = 0;
  CHECK(DriveCycle(ms, kResetWaitUs + 1, &completion));
  const esp_node::Ms5611Sample& sample = ms.lastSample();
  CHECK(sample.d1 == kD1Vector);
  CHECK(sample.d2 == kD2Vector);
  // T13 section 5.2: ~20.08 C and ~100.0 kPa, which pins the pascal unit.
  CHECK(sample.temperature_c >= 20.0 && sample.temperature_c <= 20.2);
  CHECK(sample.pressure_pa >= 100000.0 && sample.pressure_pa <= 100030.0);
  CHECK_NEAR(sample.temperature_c, 20.079854, 1e-6);
  CHECK_NEAR(sample.pressure_pa, 100009.070, 1e-3);
  CHECK(sample.in_band);
}

void TestSecondOrderCompensation() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  fake.queueRead(0x77, 0x00, Bytes24(kD1Cold));
  fake.queueRead(0x77, 0x00, Bytes24(kD2Cold));

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(DriveCycle(ms, kResetWaitUs + 1, nullptr));
  const esp_node::Ms5611Sample& sample = ms.lastSample();
  CHECK(std::isfinite(sample.pressure_pa));
  CHECK(std::isfinite(sample.temperature_c));
  CHECK(sample.temperature_c < -20.0);  // second order path really was taken
  CHECK_NEAR(sample.temperature_c, -26.545408, 1e-5);
  CHECK_NEAR(sample.pressure_pa, 100000.283, 1e-2);
  CHECK(sample.in_band);
}

void TestPressureBand() {
  CHECK(Ms5611::pressureInBand(30000.0));
  CHECK(Ms5611::pressureInBand(110000.0));
  CHECK(Ms5611::pressureInBand(101325.0));
  CHECK(!Ms5611::pressureInBand(29999.9));
  CHECK(!Ms5611::pressureInBand(110000.1));
  CHECK(!Ms5611::pressureInBand(-1.0));
  CHECK(Ms5611::kPressureMinPa == 30000.0);
  CHECK(Ms5611::kPressureMaxPa == 110000.0);
}

void TestOutOfBandSampleStillCompletes() {
  FakeI2c fake;
  SetupDevice(fake, 0x77);
  fake.queueRead(0x77, 0x00, Bytes24(0));  // nonsense D1 -> pressure far out of band
  fake.queueRead(0x77, 0x00, Bytes24(kD2Vector));

  Ms5611 ms(fake);
  CHECK(ms.begin(0));
  CHECK(!ms.poll(kResetWaitUs));
  CHECK(DriveCycle(ms, kResetWaitUs + 1, nullptr));
  // The read succeeded, so it is a completed sample — flagged out of band, not discarded
  // silently by the driver (the logger turns this into P_OUT_OF_RANGE + empty fields).
  CHECK(ms.sampleCount() == 1);
  CHECK(std::isfinite(ms.lastSample().pressure_pa));
  CHECK(!ms.lastSample().in_band);
}

void TestBusFailureMidCycleNeverYieldsASample() {
  {  // failure while reading D1
    FakeI2c fake;
    SetupDevice(fake, 0x77);
    Ms5611 ms(fake);
    CHECK(ms.begin(0));
    CHECK(!ms.poll(kResetWaitUs));
    fake.failNextRead(0x77, 0x00);
    const uint64_t start = kResetWaitUs + 1;  // consumed by the D1 ADC read below
    CHECK(!ms.poll(start));
    CHECK(!ms.poll(start + kConvWaitUs));  // D1 read fails here
    CHECK(std::strcmp(ms.errorText(), "d1_read_failed") == 0);
    CHECK(ms.state() == Ms5611::State::kError);
    CHECK(ms.sampleCount() == 0);
    CHECK(!ms.hasSample());
    CHECK(!ms.lastSample().in_band);
    CHECK(!ms.poll(1000000));
    CHECK(ms.sampleCount() == 0);
  }
  {  // failure while reading D2
    FakeI2c fake;
    SetupDevice(fake, 0x77);
    Ms5611 ms(fake);
    CHECK(ms.begin(0));
    CHECK(!ms.poll(kResetWaitUs));
    const uint64_t start = kResetWaitUs + 1;
    CHECK(!ms.poll(start));
    CHECK(!ms.poll(start + kConvWaitUs));  // D1 read: ok
    CHECK(ms.state() == Ms5611::State::kD2Wait);
    fake.failNextRead(0x77, 0x00);
    CHECK(!ms.poll(start + 2 * kConvWaitUs));  // D2 read fails here
    CHECK(std::strcmp(ms.errorText(), "d2_read_failed") == 0);
    CHECK(ms.sampleCount() == 0);
    CHECK(!ms.hasSample());
  }
  {  // failure while issuing the conversion command
    FakeI2c fake;
    SetupDevice(fake, 0x77);
    Ms5611 ms(fake);
    CHECK(ms.begin(0));
    CHECK(!ms.poll(kResetWaitUs));
    fake.setWritesFail(true);
    CHECK(!ms.poll(kResetWaitUs + 1));
    CHECK(std::strcmp(ms.errorText(), "d1_write_failed") == 0);
    CHECK(ms.sampleCount() == 0);
  }
  {  // failure while reading the PROM during init
    FakeI2c fake;
    SetupDevice(fake, 0x77);
    Ms5611 ms(fake);
    CHECK(ms.begin(0));
    fake.setReadsFail(true);
    CHECK(!ms.poll(kResetWaitUs));
    CHECK(std::strcmp(ms.errorText(), "prom_read_failed") == 0);
    CHECK(!ms.promRead());
    CHECK(!ms.initialized());
  }
}

}  // namespace

int main() {
  TestBeginResetsAndReadsProm();
  TestCrcMismatchIsDetected();
  TestProbeFallsBackToAlternateAddress();
  TestNotDetectedThenReinit();
  TestNonBlockingCycleTiming();
  TestPollCompletesExactlyOncePerSample();
  TestDatasheetVector();
  TestSecondOrderCompensation();
  TestPressureBand();
  TestOutOfBandSampleStillCompletes();
  TestBusFailureMidCycleNeverYieldsASample();

  std::printf("ms5611_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
