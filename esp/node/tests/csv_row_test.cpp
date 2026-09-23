// csv_row_test.cpp — host tests for the CSV row formatter (T13 sections 7 and 8.4).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include "csv_row.h"

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

#define CHECK_STR_EQ(actual, expected)                                                     \
  do {                                                                                     \
    ++g_checks;                                                                            \
    const std::string a_ = (actual);                                                       \
    const std::string e_ = (expected);                                                     \
    if (a_ != e_) {                                                                        \
      std::printf("FAIL %s:%d: got [%s] expected [%s]\n", __FILE__, __LINE__, a_.c_str(),  \
                  e_.c_str());                                                             \
      ++g_failures;                                                                        \
    }                                                                                      \
  } while (false)

using esp_node::CsvRow;

const char* kExpectedHeader =
    "timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,temperature_c,"
    "relative_altitude_m,flags";

std::vector<std::string> Fields(const std::string& line) {
  std::vector<std::string> out;
  std::string current;
  for (char c : line) {
    if (c == ',') {
      out.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  out.push_back(current);
  return out;
}

// Formats into a generously sized buffer and returns the resulting std::string.
std::string Format(const CsvRow& row, bool* ok = nullptr) {
  char buffer[esp_node::kCsvMaxRowBytes];
  std::memset(buffer, 0, sizeof(buffer));
  const bool complete = esp_node::formatCsvRow(row, buffer, sizeof(buffer));
  if (ok != nullptr) {
    *ok = complete;
  }
  return std::string(buffer);
}

CsvRow FullRow() {
  CsvRow row;
  row.timestamp_ms = 1234;
  row.has_accel = true;
  row.ax = 1.0;
  row.ay = 2.0;
  row.az = 3.0;
  row.a_mag = 3.742;
  row.has_gyro = true;
  row.gx = 1.5;
  row.gy = -2.25;
  row.gz = 0.0;
  row.g_mag = 2.704;
  row.has_pressure = true;
  row.pressure_pa = 100000.5;
  row.temperature_c = 20.08;
  row.has_altitude = true;
  row.relative_altitude_m = 12.345;
  row.flags = static_cast<uint16_t>(esp_node::csv_flags::kMpuOk |
                                    esp_node::csv_flags::kPFresh |
                                    esp_node::csv_flags::kAltValid);
  return row;
}

void TestHeader() {
  CHECK_STR_EQ(esp_node::kCsvHeader, kExpectedHeader);
  CHECK(esp_node::kCsvColumnCount == 13);
  CHECK(Fields(esp_node::kCsvHeader).size() == esp_node::kCsvColumnCount);
  // Exact column order.
  const std::vector<std::string> expected = {"timestamp_ms", "ax",   "ay",    "az",
                                             "a_mag",        "gx",   "gy",    "gz",
                                             "g_mag",        "pressure_pa", "temperature_c",
                                             "relative_altitude_m", "flags"};
  CHECK(Fields(esp_node::kCsvHeader) == expected);
  CHECK(esp_node::kCsvMaxRowBytes >= 120);
}

void TestFullRow() {
  const CsvRow row = FullRow();
  bool complete = false;
  const std::string line = Format(row, &complete);
  CHECK(complete);
  CHECK_STR_EQ(line, "1234,1.000,2.000,3.000,3.742,1.50,-2.25,0.00,2.70,100000.5,20.08,12.345,21");
  CHECK(line.size() < esp_node::kCsvMaxRowBytes);
  CHECK(line.find('\n') == std::string::npos);
  CHECK(Fields(line).size() == esp_node::kCsvColumnCount);
  // Every field is exactly what the schema demands.
  const std::vector<std::string> expected = {"1234",    "1.000",  "2.000",  "3.000", "3.742",
                                             "1.50",    "-2.25",  "0.00",   "2.70",  "100000.5",
                                             "20.08",   "12.345", "21"};
  CHECK(Fields(line) == expected);
}

void TestEmptyFields() {
  {  // nothing valid: 11 empty fields, only timestamp and flags carry data
    CsvRow row;
    row.timestamp_ms = 1234;
    row.flags = static_cast<uint16_t>(esp_node::csv_flags::kMpuErr |
                                      esp_node::csv_flags::kPErr);
    bool complete = false;
    const std::string line = Format(row, &complete);
    CHECK(complete);
    CHECK_STR_EQ(line, "1234,,,,,,,,,,,,10");  // MPU_ERR|P_ERR = 2|8
    CHECK(Fields(line).size() == esp_node::kCsvColumnCount);
  }
  {  // IMU valid, no fresh pressure sample for this row
    CsvRow row = FullRow();
    row.has_pressure = false;
    row.has_altitude = false;
    row.flags = esp_node::csv_flags::kMpuOk;
    const std::string line = Format(row);
    CHECK_STR_EQ(line, "1234,1.000,2.000,3.000,3.742,1.50,-2.25,0.00,2.70,,,,1");
  }
  {  // pressure only, no altitude: ALT_VALID must not survive without an altitude
    CsvRow row;
    row.timestamp_ms = 1234;
    row.has_pressure = true;
    row.pressure_pa = 101325.0;
    row.temperature_c = 25.0;
    row.flags = static_cast<uint16_t>(esp_node::csv_flags::kPFresh |
                                      esp_node::csv_flags::kAltValid);
    const std::string line = Format(row);
    CHECK_STR_EQ(line, "1234,,,,,,,,,101325.0,25.00,,4");  // 0x10 cleared -> 4
  }
  {  // altitude present and valid -> ALT_VALID kept
    CsvRow row;
    row.timestamp_ms = 9;
    row.has_altitude = true;
    row.relative_altitude_m = 0.0;
    row.flags = esp_node::csv_flags::kAltValid;
    const std::string line = Format(row);
    CHECK_STR_EQ(line, "9,,,,,,,,,,,0.000,16");
  }
  {  // MS5611 read failure: pressure band flagged out of range, all pressure fields empty
    CsvRow row;
    row.timestamp_ms = 7;
    row.has_gyro = true;
    row.gx = 0.0;
    row.gy = 0.0;
    row.gz = 0.0;
    row.g_mag = 0.0;
    row.flags = static_cast<uint16_t>(esp_node::csv_flags::kMpuErr |
                                      esp_node::csv_flags::kPOutOfRange);
    const std::string line = Format(row);
    CHECK_STR_EQ(line, "7,,,,,0.00,0.00,0.00,0.00,,,,34");  // 2|32 = 34
  }
}

void TestNeverNonFinite() {
  {  // a row that is "present" but non-finite everywhere: every field stays empty
    CsvRow row;
    row.timestamp_ms = 1234;
    row.has_accel = true;
    row.ax = std::numeric_limits<double>::quiet_NaN();
    row.ay = std::numeric_limits<double>::infinity();
    row.az = -std::numeric_limits<double>::infinity();
    row.a_mag = std::numeric_limits<double>::quiet_NaN();
    row.has_gyro = true;
    row.gx = std::numeric_limits<double>::infinity();
    row.gy = std::numeric_limits<double>::quiet_NaN();
    row.gz = std::numeric_limits<double>::quiet_NaN();
    row.g_mag = std::numeric_limits<double>::infinity();
    row.has_pressure = true;
    row.pressure_pa = std::numeric_limits<double>::quiet_NaN();
    row.temperature_c = std::numeric_limits<double>::infinity();
    row.has_altitude = true;
    row.relative_altitude_m = std::numeric_limits<double>::quiet_NaN();
    row.flags = esp_node::csv_flags::kPromCrcMismatch;  // 64
    bool complete = false;
    const std::string line = Format(row, &complete);
    CHECK(complete);
    CHECK_STR_EQ(line, "1234,,,,,,,,,,,,64");
    CHECK(line.find("nan") == std::string::npos);
    CHECK(line.find("inf") == std::string::npos);
    CHECK(line.find("NAN") == std::string::npos);
    CHECK(line.find("INF") == std::string::npos);
  }
  {  // mixed: finite accel, non-finite gyro
    CsvRow row = FullRow();
    row.gy = std::numeric_limits<double>::quiet_NaN();
    row.g_mag = std::numeric_limits<double>::infinity();
    const std::string line = Format(row);
    CHECK_STR_EQ(line, "1234,1.000,2.000,3.000,3.742,1.50,,0.00,,100000.5,20.08,12.345,21");
    CHECK(line.find("nan") == std::string::npos);
    CHECK(line.find("inf") == std::string::npos);
    CHECK(Fields(line).size() == esp_node::kCsvColumnCount);
  }
}

void TestFlagBits() {
  const struct {
    uint16_t bits;
    const char* text;
  } cases[] = {
      {esp_node::csv_flags::kMpuOk, "1"},
      {esp_node::csv_flags::kMpuErr, "2"},
      {esp_node::csv_flags::kPFresh, "4"},
      {esp_node::csv_flags::kPErr, "8"},
      {esp_node::csv_flags::kAltValid, "16"},
      {esp_node::csv_flags::kPOutOfRange, "32"},
      {esp_node::csv_flags::kPromCrcMismatch, "64"},
      {static_cast<uint16_t>(esp_node::csv_flags::kMpuOk | esp_node::csv_flags::kMpuErr |
                             esp_node::csv_flags::kPFresh | esp_node::csv_flags::kPErr |
                             esp_node::csv_flags::kAltValid |
                             esp_node::csv_flags::kPOutOfRange |
                             esp_node::csv_flags::kPromCrcMismatch),
       "127"},
  };
  for (const auto& test_case : cases) {
    CsvRow row;
    row.timestamp_ms = 5;
    row.has_altitude = true;  // so ALT_VALID is allowed to be emitted
    row.relative_altitude_m = 1.0;
    row.flags = test_case.bits;
    const std::vector<std::string> fields = Fields(Format(row));
    CHECK(fields.size() == esp_node::kCsvColumnCount);
    CHECK_STR_EQ(fields.back(), test_case.text);
  }
  // Documented bit values.
  CHECK(esp_node::csv_flags::kMpuOk == 0x01);
  CHECK(esp_node::csv_flags::kMpuErr == 0x02);
  CHECK(esp_node::csv_flags::kPFresh == 0x04);
  CHECK(esp_node::csv_flags::kPErr == 0x08);
  CHECK(esp_node::csv_flags::kAltValid == 0x10);
  CHECK(esp_node::csv_flags::kPOutOfRange == 0x20);
  CHECK(esp_node::csv_flags::kPromCrcMismatch == 0x40);
}

void TestBufferBounds() {
  const CsvRow row = FullRow();
  const std::string full = Format(row);
  CHECK(full.size() < esp_node::kCsvMaxRowBytes);

  {  // exact fit: size + NUL
    std::vector<char> buffer(full.size() + 1, '#');
    CHECK(esp_node::formatCsvRow(row, buffer.data(), buffer.size()));
    CHECK_STR_EQ(std::string(buffer.data()), full);
  }
  {  // one byte short -> reported as truncated, still a NUL-terminated prefix of the row
    std::vector<char> buffer(full.size(), '#');
    CHECK(!esp_node::formatCsvRow(row, buffer.data(), buffer.size()));
    const size_t written = std::strlen(buffer.data());
    CHECK(written < buffer.size());
    CHECK(full.compare(0, written, buffer.data()) == 0);
    for (size_t i = buffer.size() - 1; i >= written + 1; --i) {
      CHECK(buffer[i] == '#');  // nothing beyond the terminator was touched
    }
  }
  {  // small buffer with a guard region: nothing may be written past `capacity`
    char buffer[24];
    std::memset(buffer, '#', sizeof(buffer));
    CHECK(!esp_node::formatCsvRow(row, buffer, 16));
    CHECK(std::strlen(buffer) < 16);
    CHECK(std::string(buffer) == full.substr(0, std::strlen(buffer)));
    for (size_t i = 16; i < sizeof(buffer); ++i) {
      CHECK(buffer[i] == '#');
    }
  }
  {  // zero capacity / null buffer: no write, false
    char buffer[4] = {'a', 'b', 'c', 'd'};
    CHECK(!esp_node::formatCsvRow(row, buffer, 0));
    CHECK(buffer[0] == 'a' && buffer[1] == 'b' && buffer[2] == 'c' && buffer[3] == 'd');
    CHECK(!esp_node::formatCsvRow(row, nullptr, 64));
  }
  {  // a row always fits in kCsvMaxRowBytes, even with every field at its longest
    CsvRow wide;
    wide.timestamp_ms = 4294967295u;
    wide.has_accel = true;
    wide.ax = -123.456;
    wide.ay = -123.456;
    wide.az = -123.456;
    wide.a_mag = 1234.567;
    wide.has_gyro = true;
    wide.gx = -32768.0;
    wide.gy = -32768.0;
    wide.gz = -32768.0;
    wide.g_mag = 32768.0;
    wide.has_pressure = true;
    wide.pressure_pa = 110000.0;
    wide.temperature_c = -85.25;
    wide.has_altitude = true;
    wide.relative_altitude_m = -12345.678;
    wide.flags = 127;
    char buffer[esp_node::kCsvMaxRowBytes];
    CHECK(esp_node::formatCsvRow(wide, buffer, sizeof(buffer)));
    CHECK(Fields(std::string(buffer)).size() == esp_node::kCsvColumnCount);
  }
}

}  // namespace

int main() {
  TestHeader();
  TestFullRow();
  TestEmptyFields();
  TestNeverNonFinite();
  TestFlagBits();
  TestBufferBounds();

  std::printf("csv_row_test: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
