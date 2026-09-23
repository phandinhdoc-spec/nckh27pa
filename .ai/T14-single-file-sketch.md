# T14 — ESP32-S3 SINGLE-FILE Arduino sketch (NCKH27PA_ESP32S3)

STATUS: TODO
OWNER: one implementation worker (in-process subagent, model `deepseek-v4-flash`, CommandCode)
REPO: /Users/phananh/TEMP/nckh27pa/nckh27pa

## 0. DELIVERABLE (exactly one new file)

`esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino`

- That folder must contain ONLY this `.ino`. No `.h`, no `.cpp`, no `CMakeLists.txt`, no
  `sdkconfig`, no `platformio.ini`, no README, no data folder. Arduino IDE must be able to open
  this file directly and compile it with zero extra project files.
- Do NOT modify anything else in the repository (in particular do NOT touch `esp/node/`, `android/`,
  `docs/`, `.ai/`). Do not create any other file anywhere.
- Do not scan the repository. All facts you need are below.

## 1. HARDWARE (fixed — do not ask, do not change)

- Board: ESP32-S3 Super Mini; Arduino IDE board setting "ESP32S3 Dev Module" (`esp32:esp32:esp32s3`).
- Two SEPARATE I2C buses (two ESP32 I2C controllers):
  - MPU6050 (GY-521, addr 0x68, fallback 0x69): SDA = GPIO7, SCL = GPIO6.
  - MS5611 / GY-63 (addr 0x77, fallback 0x76): SDA = GPIO3, SCL = GPIO2.
- Both buses 400 kHz. Use exactly:
  `TwoWire I2C_MPU = TwoWire(0);` and `TwoWire I2C_BARO = TwoWire(1);`
  then `I2C_MPU.begin(7, 6, 400000);` and `I2C_BARO.begin(3, 2, 400000);`
  (`TwoWire(uint8_t bus_num)` and `begin(int sda, int scl, uint32_t freq)` are verified in core
  esp32 3.3.12.)

## 2. LIBRARIES — ALREADY VERIFIED AND ALREADY INSTALLED ON THIS MACHINE

Use these two Library-Manager libraries. Both accept a custom `TwoWire` object, so the MPU6050 is
never forced onto the same bus as the MS5611. Both are installed in
`/Users/phananh/Documents/Arduino/libraries` right now.

1. **MPU6050** by Electronic Cats, version 1.4.5 (Library Manager name: `MPU6050`).
   - Constructor (verified in `MPU6050/src/MPU6050.h:456`,
     `MPU6050_Base(uint8_t address = MPU6050_DEFAULT_ADDRESS, void *wireObj = 0)`), and its own
     example `MPU6050/examples/MPU6050_raw/MPU6050_raw.ino` documents the second-bus form verbatim:
     `//MPU6050 mpu(0x68, &Wire1);` → use `MPU6050 mpu(0x68, &I2C_MPU);`
   - Required include pair: `#include "I2Cdev.h"` then `#include "MPU6050.h"` (the example shows both).
   - API to use (all verified in the header):
     `bool testConnection();` (true when the device answers, also proves the address),
     `void initialize();`, `void setFullScaleAccelRange(uint8_t)` with `MPU6050_ACCEL_FS_2`,
     `void setFullScaleGyroRange(uint8_t)` with `MPU6050_GYRO_FS_250`,
     `void setRate(uint8_t)`, `void setDLPFMode(uint8_t)`,
     `void getAcceleration(int16_t* x, int16_t* y, int16_t* z);` (raw LSB),
     `void getRotation(int16_t* x, int16_t* y, int16_t* z);` (raw LSB).
   - Scaling at the chosen ranges: accel `m/s^2 = raw / 16384.0 * 9.80665` (±2 g),
     gyro `deg/s = raw / (32768.0 / 250.0)` (±250 dps). `setRate(9)` = 1 kHz / (1+9) = 100 Hz
     internal sample rate; `setDLPFMode(1)` = accel BW 184 Hz / gyro BW 188 Hz (keeps impact
     transients, named constant so it can be re-tuned later).
2. **MS5611** by Rob Tillaart, version 0.5.2 (Library Manager name: `MS5611`).
   - Constructor verified at `MS5611/MS5611.h:59`:
     `explicit MS5611(uint8_t deviceAddress = MS5611_DEFAULT_ADDRESS, TwoWire *wire = &Wire);`
     → use `MS5611 baro(0x77, &I2C_BARO);`
   - API: `bool begin();` (reset + PROM read; false when no device/invalid PROM), `bool isConnected();`,
     `int read();` / `int read(uint8_t bits);` (returns status; `MS5611_READ_OK` on success),
     `void setOversampling(osr_t)` with `OSR_ULTRA_LOW=8 … OSR_ULTRA_HIGH=12`,
     `float getPressure()` (mBar), `float getPressurePascal()` (Pa), `float getTemperature()` (°C).
   - KNOWN BEHAVIOUR you must design around (verified in `MS5611/MS5611.cpp`, `convert()`):
     each `read()` performs two conversions and **blocks in a `while (micros() - start < waitTime)
     { yield(); delayMicroseconds(10); }` loop** for the conversion time (marketed as ~1 ms per
     conversion at OSR_ULTRA_LOW, ~10 ms at OSR_ULTRA_HIGH). Therefore:
     * call `setOversampling(OSR_STANDARD)` (1024, ~3 ms per conversion → ~6 ms per `read()`), and
     * call `read()` ONLY immediately after an MPU6050 sample has been taken, inside the remainder of
       that sampling slot, and at most once every `kBaroIntervalMs` (default 40 ms ⇒ ~25 Hz).
     With a 50 Hz MPU slot (20 ms) the ~6 ms blocking read fits with margin; with 100 Hz (10 ms slot)
     it also fits but with less margin. Document this arithmetic in a comment.
   - Install command to tell the user (Library Manager names): `MPU6050` (Electronic Cats) and
     `MS5611` (Rob Tillaart). Both are in the Arduino Library Manager; no manual GitHub clone.

No other library may be added. Nothing may be vendored into the sketch folder.

## 3. SAMPLING AND SCHEDULING (no `delay(1000)` in loop; schedule with millis()/micros())

- Put the sample rate in ONE named constant at the top, e.g. `const uint32_t kSampleRateHz = 50;`
  with a comment line stating the byte-budget arithmetic that justifies the default:
  one CSV row is ~100–110 bytes incl. `\n`; at 115200 baud the link carries at most 11520 B/s, so
  100 Hz × ~105 B ≈ 10.5 kB/s would sit at ~91 % utilisation and stutter, while 50 Hz ≈ 5.3 kB/s is
  safe. Tell the reader they may raise it to 100 when they use native USB-CDC (the Super Mini's
  built-in USB ignores the baud setting) — with the same rate constant.
- MPU6050: sampled every `1000000 / kSampleRateHz` µs using a `micros()` deadline accumulator
  (advance by the nominal period; if the deadline was already missed by more than one period,
  resync to now — no burst of catch-up rows).
- MS5611: `baro.read()` at most once every `kBaroIntervalMs` (40 ms ≈ 25 Hz), placed right after the
  MPU sample inside its slot. Never inside setup's baseline collection loop with more than the
  library's own conversion wait.
- Never call `delay()` in `loop()`. `delay(1)` is allowed only in the bounded setup host-wait.
- Keep the logged numbers raw: no filtering, no smoothing, no clipping, no gyro-bias correction,
  no rotation, no rounding of stored values other than the CSV printf formatting.

## 4. BASELINE (setup, after the sensors are confirmed working)

- Collect MS5611 pressure samples for a bounded window: target 30 valid samples, hard deadline
  `kBaselineTimeoutMs = 3000` ms, sampling every `kBaroIntervalMs`.
- A sample is valid when `isfinite(p)` and `30000 <= p <= 110000` Pa.
- Baseline = mean of the valid samples. Require at least 15 valid samples, otherwise the baseline is
  INVALID: print a clear `# WARNING` line before logging starts, and for the whole session emit EMPTY
  `pressure_delta_pa` and `relative_altitude_m` (never 0, never a guess) while still logging raw
  `pressure_pa`/`temperature_c` when they are valid.
- The datum is FIXED for the session (never silently re-taken). It is a pressure reference only —
  never call it a real altitude.

## 5. CSV OUTPUT (exact schema, exact order — this task's schema differs from any earlier work)

- `Serial.begin(115200);` plus a bounded host wait (max 3000 ms: `while (!Serial && millis() - t0 < 3000)`).
- BEFORE logging starts you may print human-readable startup lines. They must state, clearly and
  separately for each sensor, whether init succeeded and, when it failed, WHICH sensor failed, on
  which pins/address, and why (e.g. `testConnection()` false, MS5611 `begin()` false, which address
  answered). Then print the CSV header exactly once. AFTER the header the output is PURE CSV: no
  debug text, no blank lines, no banners, ever.
- Header line (exact, single line):
```
timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,pressure_delta_pa,temperature_c,relative_altitude_m
```
- Column semantics:
  * `timestamp_ms` — integer ms since boot (millis()).
  * `ax,ay,az,a_mag` — m/s², 3 decimals; `a_mag = sqrt(ax*ax + ay*ay + az*az)`.
  * `gx,gy,gz,g_mag` — deg/s, 2 decimals; `g_mag = sqrt(gx*gx + gy*gy + gz*gz)`.
  * `pressure_pa` — Pa, 1 decimal (use `getPressurePascal()`).
  * `pressure_delta_pa` — Pa, 1 decimal = `pressure_pa - baseline_pa`.
  * `temperature_c` — °C, 2 decimals.
  * `relative_altitude_m` — m, 3 decimals = `44330.77 * (1 - pow(p/p0, 0.190263))`, positive above
    the baseline (lower pressure). Pressure-derived relative height only.
- Honesty rules: if an MPU read fails, its four accel + four gyro fields are EMPTY (`,,`) for that
  row; if a barometer sample is stale/invalid/NaN/out of band, its four fields are EMPTY. Never write
  `0` as a substitute, never print `nan`/`inf`. A single bad barometer sample must not crash or stall
  the loop, and must not stop later rows (fields simply stay empty until a valid sample arrives).
- Do not raise the baud above 115200 and do not change the schema.

## 6. RELIABILITY

- Report both sensors' init status before the header (see §5). Never silently continue as if a failed
  sensor were fine.
- If a sensor failed at init, retry detection at most once every 5000 ms from `loop()` — silently
  (no text after the header); the row's empty fields show the state.
- `MS5611::begin()` returning false must be reported with the addresses tried; if only the alternate
  address answers, construct/use it explicitly and say so.
- Guard every computed value with `isfinite()` before formatting. Reject out-of-band pressures.
- No fall threshold, no fall/not-fall decision, no classifier, no state machine — data acquisition
  only. The data must stay good enough to later separate walking / standing / sitting / lying /
  slide-from-chair / fall / impact / slow lowering / syncope without impact.

## 7. CODE STYLE FOR THE SINGLE FILE

- Sections in this order, each with a short comment header: includes → constants/config → globals →
  helper functions → `setup()` → `loop()`.
- Helpers allowed, all defined in this same `.ino` (Arduino auto-generates prototypes; do not rely
  on that — define helpers before use or keep them above setup()).
- Plain Arduino C++: no templates, no classes, no namespaces, no dynamic allocation, no C++17-only
  tricks, no `std::` containers; `math.h` functions (`sqrtf`, `isfinite`, `pow`) are fine.
- Comment in Vietnamese for the sections a maintainer must understand (pins, bus choice, scheduling
  arithmetic, baseline rule, honesty rules, CSV columns), keep it short and factual. No decorative
  banners, no dead code, no commented-out experiments.
- Aim for a readable ~300–450 line file. Do not pad it.

## 8. ACCEPTANCE (run these yourself, paste real output)

1. The file exists and the folder contains only it:
   `ls -la esp/NCKH27PA_ESP32S3`
   `find esp/NCKH27PA_ESP32S3 -type f`
2. Whole implementation is inside the single file:
   `rg -n '#include' esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino`
   (must show only the two library include pairs, `Wire.h`, and standard C headers — no local `"*.h"`).
3. No stray files added to the repo: `git status --short` (only `esp/NCKH27PA_ESP32S3/` new, nothing
   else from you) and `git diff --check`.
4. Compile (skip nothing; both must succeed):
```
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" \
  esp/NCKH27PA_ESP32S3
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 esp/NCKH27PA_ESP32S3
```
   Fix every compile error yourself and re-run until both pass. Do NOT upload/flash (no board
   connected) and do NOT run `arduino-cli monitor`. Do not install extra libraries.

## 9. FINAL REPORT BLOCK (short and factual)

```
STATUS:
OWNER: subagent (model: deepseek-v4-flash, CommandCode)
FILES: <the one path + line count>
GOAL:
RESULT: <exact commands run + real output tails: sketch size for both FQBNs, ls/find result,
        the include grep result, git status>
NEXT_ACTION:
```
Add a SHORT "CHO NGƯỜI DÙNG" list: library names to install in Arduino IDE Library Manager, board
setting to select, chosen sample rate + why, the CSV header line, and the macOS open command
`open -a "Arduino IDE" esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino`. Also list honestly what is NOT
proven without hardware (real sensor ACKs, achieved rates, 115200 throughput, ADC noise).
Do not claim PASS for anything you did not run, and do not describe the file in prose instead of
writing it.
