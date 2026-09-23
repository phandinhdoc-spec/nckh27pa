// csv_row.h — CSV row formatting for the T13 logger (section 7 schema).
//
// Pure C++17: only <cstdio> for snprintf, no Arduino.h / Wire.h / ESP headers.
// Invalid values are emitted as EMPTY fields (`,,`) and identified by the flags column;
// a non-finite value can never reach the buffer, and the buffer size is always respected
// (formatCsvRow() reports truncation instead of overflowing).
#pragma once

#include <cstddef>
#include <cstdint>

namespace esp_node {

// T13 section 7 flags bitmask.
namespace csv_flags {
constexpr uint16_t kMpuOk = 0x01;
constexpr uint16_t kMpuErr = 0x02;
constexpr uint16_t kPFresh = 0x04;
constexpr uint16_t kPErr = 0x08;
constexpr uint16_t kAltValid = 0x10;
constexpr uint16_t kPOutOfRange = 0x20;
constexpr uint16_t kPromCrcMismatch = 0x40;
}  // namespace csv_flags

// Exact header line, printed once at startup (T13 section 7).
constexpr const char* kCsvHeader =
    "timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,temperature_c,"
    "relative_altitude_m,flags";
constexpr size_t kCsvColumnCount = 13;
constexpr size_t kCsvMaxRowBytes = 200;  // safe upper bound of one formatted row

// One CSV row. Each numeric group has an explicit "present" flag: when it is false (or the
// value is not finite) the corresponding fields are emitted empty.
struct CsvRow {
  uint32_t timestamp_ms = 0;  // monotonic since boot (micros()/1000)
  bool has_accel = false;
  double ax = 0.0;  // m/s^2, 3 decimals
  double ay = 0.0;
  double az = 0.0;
  double a_mag = 0.0;
  bool has_gyro = false;
  double gx = 0.0;  // deg/s, 2 decimals
  double gy = 0.0;
  double gz = 0.0;
  double g_mag = 0.0;
  bool has_pressure = false;  // pressure_pa + temperature_c
  double pressure_pa = 0.0;   // Pa, 1 decimal
  double temperature_c = 0.0; // deg C, 2 decimals
  bool has_altitude = false;
  double relative_altitude_m = 0.0;  // m, 3 decimals
  uint16_t flags = 0;
};

// Formats one CSV row (no trailing newline) into `out`.
// Returns true when the whole row fitted; false on truncation (the buffer is then a valid
// NUL-terminated prefix and nothing was written past `capacity` bytes). `capacity` == 0 or
// `out` == nullptr returns false without touching memory.
// The ALT_VALID bit is cleared when the row carries no altitude, so a row can never claim a
// valid altitude and leave the field empty at the same time.
bool formatCsvRow(const CsvRow& row, char* out, size_t capacity);

}  // namespace esp_node
