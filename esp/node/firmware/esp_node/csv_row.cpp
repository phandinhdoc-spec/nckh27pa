// csv_row.cpp — CSV row formatting (T13 section 7).
#include "csv_row.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace esp_node {
namespace {

// Append-only writer with a hard capacity: it never writes past `capacity` bytes and always
// keeps the buffer NUL-terminated. Once the row does not fit, `truncated_` latches and no
// further bytes are written (the buffer holds a valid prefix).
class LineWriter {
 public:
  LineWriter(char* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {
    if (capacity_ > 0) {
      buffer_[0] = '\0';
    }
  }

  void Comma() { Append(",", 1); }

  void Uint(uint32_t value) {
    char text[16];
    const int written = std::snprintf(text, sizeof(text), "%lu",
                                     static_cast<unsigned long>(value));
    if (written < 0) {
      truncated_ = true;
      return;
    }
    Append(text, static_cast<size_t>(written));
  }

  // Emits an empty field when the value is absent or not finite — `nan`/`inf` can never
  // reach the buffer this way.
  void Number(const char* format, double value, bool present) {
    if (!present || !std::isfinite(value)) {
      return;
    }
    char text[40];
    const int written = std::snprintf(text, sizeof(text), format, value);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(text)) {
      truncated_ = true;
      return;
    }
    Append(text, static_cast<size_t>(written));
  }

  bool truncated() const { return truncated_; }

 private:
  void Append(const char* data, size_t len) {
    if (truncated_) {
      return;
    }
    if (capacity_ == 0 || used_ + len + 1 > capacity_) {
      truncated_ = true;
      return;
    }
    std::memcpy(buffer_ + used_, data, len);
    used_ += len;
    buffer_[used_] = '\0';
  }

  char* buffer_;
  size_t capacity_;
  size_t used_ = 0;
  bool truncated_ = false;
};

}  // namespace

bool formatCsvRow(const CsvRow& row, char* out, size_t capacity) {
  if (out == nullptr || capacity == 0) {
    return false;
  }
  LineWriter writer(out, capacity);

  uint16_t flags = row.flags;
  if (!row.has_altitude) {
    flags = static_cast<uint16_t>(flags & ~csv_flags::kAltValid);
  }

  writer.Uint(row.timestamp_ms);
  writer.Comma();
  writer.Number("%.3f", row.ax, row.has_accel);
  writer.Comma();
  writer.Number("%.3f", row.ay, row.has_accel);
  writer.Comma();
  writer.Number("%.3f", row.az, row.has_accel);
  writer.Comma();
  writer.Number("%.3f", row.a_mag, row.has_accel);
  writer.Comma();
  writer.Number("%.2f", row.gx, row.has_gyro);
  writer.Comma();
  writer.Number("%.2f", row.gy, row.has_gyro);
  writer.Comma();
  writer.Number("%.2f", row.gz, row.has_gyro);
  writer.Comma();
  writer.Number("%.2f", row.g_mag, row.has_gyro);
  writer.Comma();
  writer.Number("%.1f", row.pressure_pa, row.has_pressure);
  writer.Comma();
  writer.Number("%.2f", row.temperature_c, row.has_pressure);
  writer.Comma();
  writer.Number("%.3f", row.relative_altitude_m, row.has_altitude);
  writer.Comma();
  writer.Uint(static_cast<uint32_t>(flags));

  return !writer.truncated();
}

}  // namespace esp_node
