// node_math.cpp — magnitudes, barometric relative altitude, startup baseline.
#include "node_math.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace esp_node {

double magnitude3(double x, double y, double z) {
  // Scaled form of sqrt(x^2 + y^2 + z^2): exact for the small vectors used here (the
  // scaling by the largest component keeps full precision and avoids overflow).
  const double ax = std::fabs(x);
  const double ay = std::fabs(y);
  const double az = std::fabs(z);
  const double scale = std::max(ax, std::max(ay, az));
  if (scale == 0.0) {
    return 0.0;
  }
  const double sx = x / scale;
  const double sy = y / scale;
  const double sz = z / scale;
  return scale * std::sqrt(sx * sx + sy * sy + sz * sz);
}

bool relativeAltitudeMeters(double pressure_pa, double baseline_pa, double& out) {
  if (!std::isfinite(pressure_pa) || !std::isfinite(baseline_pa)) {
    return false;
  }
  if (pressure_pa <= 0.0 || baseline_pa <= 0.0) {
    return false;
  }
  const double ratio = pressure_pa / baseline_pa;
  out = kAltitudeScale * (1.0 - std::pow(ratio, kAltitudeExponent));
  return true;
}

void BaroBaseline::reset() {
  for (size_t i = 0; i < kCapacity; ++i) {
    samples_[i] = 0.0;
  }
  count_ = 0;
  frozen_ = false;
  baseline_pa_ = std::numeric_limits<double>::quiet_NaN();
}

bool BaroBaseline::inBand(double pressure_pa) {
  // Same plausibility band as the MS5611 driver (T13 section 5.2).
  return std::isfinite(pressure_pa) && pressure_pa >= 30000.0 && pressure_pa <= 110000.0;
}

bool BaroBaseline::addSample(double pressure_pa) {
  if (frozen_ || count_ >= kCapacity) {
    return false;
  }
  if (!inBand(pressure_pa)) {
    return false;
  }
  samples_[count_] = pressure_pa;
  ++count_;
  if (count_ >= kCapacity) {
    finalize();
  }
  return true;
}

void BaroBaseline::finalize() {
  if (frozen_) {
    return;
  }
  frozen_ = true;
  baseline_pa_ = valid() ? medianOf(samples_, count_)
                         : std::numeric_limits<double>::quiet_NaN();
}

double BaroBaseline::baselinePa() const {
  return valid() ? baseline_pa_ : std::numeric_limits<double>::quiet_NaN();
}

double BaroBaseline::medianOf(const double* values, size_t n) {
  if (values == nullptr || n == 0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (n > kCapacity) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double sorted[kCapacity];
  for (size_t i = 0; i < n; ++i) {
    sorted[i] = values[i];
  }
  for (size_t i = 1; i < n; ++i) {  // insertion sort: n <= 40
    const double key = sorted[i];
    size_t j = i;
    while (j > 0 && sorted[j - 1] > key) {
      sorted[j] = sorted[j - 1];
      --j;
    }
    sorted[j] = key;
  }
  if ((n % 2) == 1) {
    return sorted[n / 2];
  }
  return 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
}

bool BaroBaseline::altitudeFor(double pressure_pa, double& out) const {
  if (!valid()) {
    return false;  // "no altitude" — never 0.0 as a substitute
  }
  if (!inBand(pressure_pa)) {
    return false;  // out-of-band measurement: no altitude for this row (P_OUT_OF_RANGE)
  }
  return relativeAltitudeMeters(pressure_pa, baseline_pa_, out);
}

}  // namespace esp_node
