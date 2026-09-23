// node_math.h — feature math for the T13 sensor node.
//
// Pure C++17: no Arduino.h / Wire.h / ESP headers, no dynamic allocation.
// Only magnitudes, the barometric relative altitude and the startup baseline live here;
// no filtering, no classification, no thresholds (T13 section 6).
#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_node {

// T13 section 5.3: h_m = 44330.77 * (1 - (p/p0)^0.190263), p and p0 in Pa.
// Sign: h > 0 means above the baseline (lower pressure). Pressure-derived relative
// height only — never an absolute altitude.
constexpr double kAltitudeScale = 44330.77;
constexpr double kAltitudeExponent = 0.190263;

// sqrt(x^2 + y^2 + z^2) without the overflow of a naive squared sum.
double magnitude3(double x, double y, double z);

// Relative altitude. Returns false (and leaves `out` untouched) when the inputs are not
// usable (non-finite or <= 0) — "no altitude" is distinguishable from 0.0 m.
bool relativeAltitudeMeters(double pressure_pa, double baseline_pa, double& out);

// Startup pressure baseline: collect valid MS5611 samples for a bounded window
// (>= 20 valid samples required, target 40), baseline = median of the collected samples.
// Once frozen the datum never moves (T13 section 5.3).
class BaroBaseline {
 public:
  static constexpr size_t kRequiredSamples = 20;
  static constexpr size_t kCapacity = 40;

  void reset();

  // Accepts a sample when it is finite, inside the plausibility band and the window is
  // still open. Returns true when the sample was stored. Reaching kCapacity freezes it.
  bool addSample(double pressure_pa);

  // Closes the startup window (called by the logger when its time budget expires);
  // the datum is then fixed for the rest of the session.
  void finalize();

  bool frozen() const { return frozen_; }
  bool full() const { return count_ >= kCapacity; }
  size_t count() const { return count_; }
  bool valid() const { return frozen_ && count_ >= kRequiredSamples; }

  // Median of the collected samples. Only meaningful when valid(); returns NaN otherwise
  // so an accidental use is visible instead of looking like a real pressure.
  double baselinePa() const;

  // Relative altitude against the frozen baseline. Returns false ("no altitude") while
  // the baseline is invalid, and leaves `out` untouched — never 0.0 as a substitute.
  bool altitudeFor(double pressure_pa, double& out) const;

  static bool inBand(double pressure_pa);
  static double medianOf(const double* values, size_t n);

 private:
  double samples_[kCapacity] = {};
  size_t count_ = 0;
  bool frozen_ = false;
  double baseline_pa_ = 0.0;
};

}  // namespace esp_node
