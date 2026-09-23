// node_math_test.cpp — host tests for magnitudes, relative altitude and the baseline
// (T13 section 8.3).
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "node_math.h"

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

using esp_node::BaroBaseline;

void TestMagnitude3() {
  CHECK(esp_node::magnitude3(3.0, 4.0, 0.0) == 5.0);
  CHECK(esp_node::magnitude3(-3.0, -4.0, 0.0) == 5.0);
  CHECK(esp_node::magnitude3(0.0, 0.0, 0.0) == 0.0);
  CHECK(esp_node::magnitude3(1.0, 2.0, 2.0) == 3.0);
  // A resting accelerometer: 1 g along z.
  CHECK_NEAR(esp_node::magnitude3(0.0, 0.0, 9.80665), 9.80665, 1e-12);
  CHECK_NEAR(esp_node::magnitude3(9.80665, 0.0, 0.0), 9.80665, 1e-12);
  // No overflow for large components (naive x*x+y*y+z*z would overflow here).
  const double big = esp_node::magnitude3(1e200, 1e200, 1e200);
  CHECK(std::isfinite(big));
  CHECK_NEAR(big, 1e200 * std::sqrt(3.0), 1e188);
}

void TestRelativeAltitude() {
  double out = 0.0;

  // p == p0 -> exactly 0.0 (and the API still reports an altitude).
  CHECK(esp_node::relativeAltitudeMeters(101325.0, 101325.0, out));
  CHECK(out == 0.0);

  // p0 = 101325 Pa, p = 100000 Pa -> 111 +- 1.5 m, positive (higher than the datum).
  CHECK(esp_node::relativeAltitudeMeters(100000.0, 101325.0, out));
  CHECK_NEAR(out, 110.884, 1.5);
  CHECK(out > 0.0);

  // p > p0 -> negative (magnitude is not the exact mirror of the case above).
  CHECK(esp_node::relativeAltitudeMeters(101325.0, 100000.0, out));
  CHECK_NEAR(out, -111.162, 0.01);
  CHECK(out < 0.0);

  // Lower pressure -> greater height (monotonic).
  double h1 = 0.0;
  double h2 = 0.0;
  CHECK(esp_node::relativeAltitudeMeters(100000.0, 101325.0, h1));
  CHECK(esp_node::relativeAltitudeMeters(99000.0, 101325.0, h2));
  CHECK(h2 > h1);

  // Unusable inputs report "no altitude" and leave the output untouched.
  for (double bad_p : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                       std::numeric_limits<double>::infinity()}) {
    double sentinel = -999.0;
    CHECK(!esp_node::relativeAltitudeMeters(bad_p, 101325.0, sentinel));
    CHECK(sentinel == -999.0);
  }
  for (double bad_p0 : {0.0, -101325.0, std::numeric_limits<double>::quiet_NaN()}) {
    double sentinel = -999.0;
    CHECK(!esp_node::relativeAltitudeMeters(100000.0, bad_p0, sentinel));
    CHECK(sentinel == -999.0);
  }

  CHECK(esp_node::kAltitudeScale == 44330.77);
  CHECK(esp_node::kAltitudeExponent == 0.190263);
}

void TestBaselineInvalid() {
  BaroBaseline baseline;
  CHECK(baseline.count() == 0);
  CHECK(!baseline.valid());
  CHECK(!baseline.frozen());
  CHECK(!baseline.full());

  // Not enough samples: no altitude, and it is distinguishable from 0.0 m.
  double out = -999.0;
  CHECK(!baseline.altitudeFor(101325.0, out));
  CHECK(out == -999.0);
  CHECK(!std::isfinite(baseline.baselinePa()));  // NaN, not a fake 0 Pa datum
  CHECK(BaroBaseline::kRequiredSamples == 20);
  CHECK(BaroBaseline::kCapacity == 40);

  // Out-of-band and non-finite samples are rejected and never counted.
  CHECK(!baseline.addSample(20000.0));
  CHECK(!baseline.addSample(200000.0));
  CHECK(!baseline.addSample(0.0));
  CHECK(!baseline.addSample(-101325.0));
  CHECK(!baseline.addSample(std::numeric_limits<double>::quiet_NaN()));
  CHECK(!baseline.addSample(std::numeric_limits<double>::infinity()));
  CHECK(baseline.count() == 0);

  // 19 valid samples are still not enough.
  for (int i = 0; i < 19; ++i) {
    CHECK(baseline.addSample(101325.0));
  }
  CHECK(baseline.count() == 19);
  baseline.finalize();
  CHECK(baseline.frozen());
  CHECK(!baseline.valid());
  out = -999.0;
  CHECK(!baseline.altitudeFor(101325.0, out));
  CHECK(out == -999.0);
  // A frozen, invalid baseline never accepts more samples.
  CHECK(!baseline.addSample(101325.0));
  CHECK(baseline.count() == 19);
}

void TestBaselineMedianAndFreeze() {
  BaroBaseline baseline;
  // 20 distinct in-band values, deliberately unsorted.
  const double values[20] = {100019.0, 100000.0, 100011.0, 100002.0, 100015.0, 100006.0,
                             100017.0, 100008.0, 100001.0, 100012.0, 100003.0, 100014.0,
                             100005.0, 100016.0, 100007.0, 100018.0, 100009.0, 100004.0,
                             100013.0, 100010.0};
  for (double value : values) {
    CHECK(baseline.addSample(value));
  }
  CHECK(baseline.count() == 20);
  baseline.finalize();
  CHECK(baseline.valid());
  // Median of 100000..100019 = mean of the two middle values = 100009.5
  CHECK_NEAR(baseline.baselinePa(), 100009.5, 1e-9);

  // The datum is fixed: further samples are refused, the baseline does not move.
  CHECK(!baseline.addSample(105000.0));
  CHECK_NEAR(baseline.baselinePa(), 100009.5, 1e-9);

  // Altitude against the frozen datum.
  double out = 0.0;
  CHECK(baseline.altitudeFor(100000.0, out));
  CHECK_NEAR(out, esp_node::kAltitudeScale *
                       (1.0 - std::pow(100000.0 / 100009.5, esp_node::kAltitudeExponent)),
             1e-9);
  CHECK(out > 0.0);

  // Out-of-band measurement -> no altitude for that row.
  double sentinel = -999.0;
  CHECK(!baseline.altitudeFor(5000.0, sentinel));
  CHECK(sentinel == -999.0);
  CHECK(!baseline.altitudeFor(120000.0, sentinel));
  CHECK(sentinel == -999.0);

  // The window fills at 40 samples and freezes itself with the median of the 40.
  BaroBaseline full;
  for (int i = 0; i < 39; ++i) {
    CHECK(full.addSample(101000.0 + i));
  }
  CHECK(!full.frozen());
  CHECK(full.addSample(101039.0));  // 40th sample
  CHECK(full.frozen());
  CHECK(full.full());
  CHECK(full.valid());
  CHECK_NEAR(full.baselinePa(), (101019.0 + 101020.0) / 2.0, 1e-9);
  CHECK(!full.addSample(101100.0));

  // Median helper: odd/even/empty.
  const double odd[3] = {3.0, 1.0, 2.0};
  CHECK(BaroBaseline::medianOf(odd, 3) == 2.0);
  const double even[4] = {4.0, 1.0, 3.0, 2.0};
  CHECK(BaroBaseline::medianOf(even, 4) == 2.5);
  CHECK(!std::isfinite(BaroBaseline::medianOf(nullptr, 0)));
  CHECK(!std::isfinite(BaroBaseline::medianOf(odd, 0)));
  CHECK(!std::isfinite(BaroBaseline::medianOf(odd, BaroBaseline::kCapacity + 1)));

  // reset() reopens the window.
  baseline.reset();
  CHECK(baseline.count() == 0);
  CHECK(!baseline.valid());
  CHECK(!baseline.frozen());
  CHECK(!std::isfinite(baseline.baselinePa()));
}

void TestBaselineInBand() {
  CHECK(BaroBaseline::inBand(30000.0));
  CHECK(BaroBaseline::inBand(110000.0));
  CHECK(BaroBaseline::inBand(101325.0));
  CHECK(!BaroBaseline::inBand(29999.9));
  CHECK(!BaroBaseline::inBand(110000.1));
  CHECK(!BaroBaseline::inBand(std::numeric_limits<double>::quiet_NaN()));
}

}  // namespace

int main() {
  TestMagnitude3();
  TestRelativeAltitude();
  TestBaselineInvalid();
  TestBaselineMedianAndFreeze();
  TestBaselineInBand();

  std::printf("node_math_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
