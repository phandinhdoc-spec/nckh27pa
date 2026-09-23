# T13 — ESP32-S3 sensor node: MPU6050 + MS5611 instrumentation logger

STATUS: DONE (host-verified by Hermes; hardware UNVERIFIED — nothing flashed)
OWNER: implemented by 2 sequential subagents (model `deepseek-v4-flash`, CommandCode); verified by Hermes.

WORKER TRAIL (recorded, not hidden):
- attempt 1 `deepseek/deepseek-v4-pro` via `hermes chat -q` — hit output-token limit on every
  continuation, 0 files, 17m20s.
- attempt 2 `z-ai/glm-5.3-flash` via `hermes chat --query-file` — stream stall, killed at 12m, 0 files.
- attempt 3 Codex CLI (AGENTS.md Level 2) — usage limit exhausted until 2026-09-22.
- attempt 4: in-process subagents (deepseek-v4-flash), 2 non-overlapping file sets → SUCCESS.

HERMES VERIFICATION (run by the coordinator, not the workers):
- `bash esp/node/run-tests.sh` → 520 checks, 0 failures (mpu6050 97 / ms5611 179 / node_math 170 /
  csv_row 74); `--sanitize` (ASan+UBSan) also 0 failures.
- `arduino-cli` 1.5.1, core esp32 3.3.12:
  `--fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled"` → exit 0,
  334450 B (25%) flash / 24456 B (7%) RAM.
  `--fqbn esp32:esp32:esp32s3` → exit 0, 314073 B (23%) / 23776 B (7%).
- `git status --short` → only new files under `esp/node/`; `git diff --check` clean.
- Pure modules contain no Arduino.h/Wire.h/ESP/timing headers (only `arduino_i2c_bus.*` and the
  `.ino` do); register maps, init order, MS5611 commands/timings and the compensation math were
  re-read by Hermes and match the grounded sources in section 5.
- Device float printf: `CONFIG_NEWLIB_NANO_FORMAT` is absent from the esp32s3 sdkconfig → full
  newlib printf, so `%.3f` in csv_row works on-device.
- Documented deviations/choices (not silent): the skipped-slot counter is tracked but never printed
  (the "CSV rows only after the header" rule wins over a final summary line); `P_FRESH` is set only
  on rows that actually carry pressure data; an invalid startup baseline keeps altitude EMPTY for the
  whole run (the datum is never re-taken); PROM CRC-4 is informational and never blocks pressure.

STILL UNVERIFIED (no board connected): real I2C ACKs/addresses, real WHO_AM_I/PROM/CRC, achieved
100 Hz / 25 Hz, loop jitter, 921600-CDC throughput, ADC noise, and any claim about FALL-01 behaviour.

TASK SPEC (unchanged):
REPO: /Users/phananh/TEMP/nckh27pa/nckh27pa
WORK DIR: esp/node/**  (all NEW files)

## 1. GOAL

Write Arduino firmware for an **ESP32-S3 Super Mini** that logs raw MPU6050 + MS5611/GY-63 data
over serial as machine-readable CSV, for the NCKH27PA fall-detection research programme
(instrumentation phase only — collecting dataset for FALL-01).

This is DATA ACQUISITION ONLY. No classification, no thresholds, no decision logic.

## 2. HARD CONSTRAINTS (violating any of these fails the task)

- Do NOT modify, rename or delete ANY existing file in the repository. Only create new files
  under `esp/node/`.
- Do NOT touch `android/`, `docs/`, `.ai/`, `esp/core/`, `esp/config_core/`.
- Do NOT scan the repository. Read only `esp/core/run-tests.sh` (style reference for the test
  runner) if you want, and nothing else. Do not read Android/Kotlin code.
- Do NOT implement any fall threshold, state machine, classifier, or "fall detected" output.
- Do NOT invent sensor APIs, register names, library names, or hardware capability. Everything
  you need is specified below with grounded sources. If something below is insufficient, say so
  in your final report instead of guessing.
- No third-party sensor libraries. Use only the ESP32 Arduino core (Wire/TwoWire, Print).
- Do NOT silently substitute fake values anywhere in the firmware or data path.
- Files you create must compile on the host with `-std=c++17 -Wall -Wextra -Werror -pedantic`.
  The pure (non-Arduino) modules must not include `Arduino.h`, `Wire.h`, or any ESP header.

## 3. HARDWARE (fixed, do not change, do not ask)

- Board: ESP32-S3 Super Mini.
- I2C bus 0: SDA = GPIO7, SCL = GPIO6  -> MPU6050 (GY-521), address 0x68 (alt 0x69).
- I2C bus 1: SDA = GPIO3, SCL = GPIO2  -> MS5611 (GY-63), address 0x77 (alt 0x76).
- TWO SEPARATE I2C CONTROLLERS. Use two distinct `TwoWire` objects constructed with explicit
  bus numbers, e.g. `TwoWire busImu(0); TwoWire busBaro(1);` then
  `busImu.begin(7, 6, 400000); busBaro.begin(3, 2, 400000);`
  (`TwoWire(uint8_t bus_num)` and `bool begin(int sda, int scl, uint32_t frequency)` are verified
  present in the installed core.)
- Bus speed 400 kHz on both.
- No USB-UART bridge assumption: never block forever on `Serial`.

## 4. FILE LAYOUT TO CREATE (exact paths)

```
esp/node/README.md                              # build/flash/log instructions (Vietnamese OK)
esp/node/run-tests.sh                           # host test runner, mirror esp/core/run-tests.sh style
esp/node/firmware/esp_node/esp_node.ino         # sketch: setup/loop, board + rates, serial CSV out
esp/node/firmware/esp_node/node_config.h        # pins, rates, DLPF, OSR, addresses, constants
esp/node/firmware/esp_node/i2c_bus.h            # abstract bus interface (pure, no Arduino.h)
esp/node/firmware/esp_node/arduino_i2c_bus.h/.cpp  # TwoWire adapter implementing I2cBus
esp/node/firmware/esp_node/mpu6050.h/.cpp       # register-level MPU6050 driver (pure, takes I2cBus&)
esp/node/firmware/esp_node/ms5611.h/.cpp        # register-level MS5611 driver, NON-BLOCKING (pure)
esp/node/firmware/esp_node/node_math.h/.cpp     # magnitudes + barometric relative altitude (pure)
esp/node/firmware/esp_node/csv_row.h/.cpp       # CSV row formatting (pure)
esp/node/tests/fake_i2c.h                       # FakeI2c implementing I2cBus (scripted register file)
esp/node/tests/mpu6050_test.cpp
esp/node/tests/ms5611_test.cpp
esp/node/tests/node_math_test.cpp
esp/node/tests/csv_row_test.cpp
```

The sketch folder name must equal the `.ino` name (`esp_node/esp_node.ino`) or arduino-cli fails.
The pure modules live next to the `.ino` in the same sketch folder so the Arduino builder compiles
them; the host tests compile the same `.cpp` files directly with g++ (`-I../firmware/esp_node`).
`arduino_i2c_bus.cpp` and `esp_node.ino` are the ONLY translation units allowed to include
`Arduino.h`/`Wire.h`, and must be excluded from the host test build.

## 5. VERIFIED SENSOR FACTS (use exactly these; already grounded — do not re-research)

Sources: datasheet MS5611 and the register map as implemented by i2cdevlib `MPU6050.cpp/.h`
and RobTillaart `MS5611.cpp` (both fetched and read during preparation).

### 5.1 MPU6050
Registers: `SMPLRT_DIV 0x19`, `CONFIG 0x1A`, `GYRO_CONFIG 0x1B`, `ACCEL_CONFIG 0x1C`,
`INT_ENABLE 0x38`, `ACCEL_XOUT_H 0x3B` (14 bytes: ax,ay,az,temp,gx,gy,gz big-endian int16),
`TEMP_OUT_H 0x41`, `GYRO_XOUT_H 0x43`, `SIGNAL_PATH_RESET 0x68`, `USER_CTRL 0x6A`,
`PWR_MGMT_1 0x6B`, `PWR_MGMT_2 0x6C`, `WHO_AM_I 0x75`.
Init sequence (exactly this order):
1. probe address: try 0x68, then 0x69 (a device ACKs if reading WHO_AM_I succeeds).
2. `PWR_MGMT_1 = 0x80` (DEVICE_RESET), wait >= 100 ms.
3. read `WHO_AM_I`; MPU6050 must return `0x68`. Anything else -> init failure with the value reported.
4. `PWR_MGMT_1 = 0x01` (CLKSEL = PLL with X gyro reference).
5. `SIGNAL_PATH_RESET = 0x07`, wait >= 100 ms, then `PWR_MGMT_2 = 0x00` (all axes on).
6. `CONFIG = kDlpfCfg` with `kDlpfCfg = 1` (DLPF accel BW 184 Hz / gyro 188 Hz, gyro output rate
   1 kHz, accel delay 2.9 ms). Rationale: keep impact transients for fall-event research; make it a
   named constant so it can be re-tuned. Valid values 0..6 = 260/184/94/44/21/10/5 Hz accel BW.
7. `SMPLRT_DIV = 9` (1 kHz / (1+9) = 100 Hz internal sample rate).
8. `GYRO_CONFIG = 0x00` (FS_SEL 0, +-250 dps) and `ACCEL_CONFIG = 0x00` (AFS_SEL 0, +-2 g).
9. `INT_ENABLE = 0x00`.
Scaling: accel `m/s^2 = raw / 16384.0 * 9.80665`; gyro `dps = raw / (32768.0 / 250.0)`.
Data read: one 14-byte burst from `ACCEL_XOUT_H` -> ax,ay,az,temp,gx,gy,gz.
Note: the data registers are NOT auto-incremented by the sensor; the burst read relies on the
device's sequential register read behaviour (standard for this part).

### 5.2 MS5611
Addresses 0x77 (CSB low) then 0x76. OSR index for OSR bits: `offset = (bits-8)*2`, so
OSR 256/512/1024/2048/4096 -> D1 command `0x40/0x42/0x44/0x46/0x48` and
D2 command `0x50/0x52/0x54/0x56/0x58`. Use OSR 4096 (`0x48` / `0x58`).
Max conversion wait per OSR, in microseconds: `600, 1200, 2300, 4600, 9100` (index 0..4).
Commands: reset `0x1E` (wait >= 3000 us), read PROM word i = `0xA0 + 2*i` (2 bytes, big-endian,
i = 0..6 are C1..C6 prefixed by the manufacturer word at i=0; word 7 = CRC), read ADC = command
`0x00` then 3 bytes big-endian.
Math (datasheet mode 0, all C values are the RAW `uint16_t` PROM words):
```
dT       = D2 - C5 * 256.0
TEMP100  = 2000.0 + dT * C6 / 8388608.0        // temperature in 0.01 C
OFF      = C2 * 65536.0 + C4 * dT / 128.0
SENS     = C1 * 32768.0 + C3 * dT / 256.0
if (TEMP100 < 2000) {                          // second order compensation
  T2 = dT * dT * 4.6566128731e-10;
  t  = (TEMP100 - 2000.0)^2;  OFF2 = 2.5 * t;  SENS2 = 1.25 * t;
  if (TEMP100 < -1500) { t = (TEMP100 + 1500.0)^2; OFF2 += 7.0 * t; SENS2 += 5.5 * t; }
  TEMP100 -= T2; OFF -= OFF2; SENS -= SENS2;
}
pressure_Pa = (D1 * SENS / 2097152.0 - OFF) / 32768.0     // UNIT IS PASCAL (not mbar/hPa)
temperature_C = TEMP100 * 0.01
```
Use `double` (or int64 for the exact terms) so the host test and the device agree.
Use the reference PROM `C1..C6 = 40127, 36924, 23317, 23282, 33464, 28312` with the datasheet test
pair `D1 = 9085466, D2 = 8569150` as the unit-test vector: expected temperature ~20.08 C,
expected pressure ~100.0 kPa (in 100000..100030 Pa), which also pins the Pa unit.
Plausibility band for a wearable node: `30000 .. 110000 Pa`. Anything outside -> invalid, never logged.

### 5.3 Relative altitude (no absolute altitude claim)
`h_m = 44330.77 * (1.0 - pow(p / p0, 0.190263))`, where `p0` = baseline pressure collected at
startup and `p` = current pressure, both in Pa. Sign: `h > 0` means above the baseline (lower
pressure). This is a pressure-derived relative height only — never present it as real altitude.
Baseline: collect valid MS5611 samples for a bounded startup window (>= 20 samples required,
target ~40 samples); baseline = median of those samples; the datum is FIXED for the whole session
and is never silently moved (documented in README). If fewer than 20 valid samples arrive, the
baseline is invalid and `relative_altitude_m` must be EMPTY (never 0, never a guess) with the
ALT_VALID flag clear, and a clear startup warning printed before the header.

## 6. FIRMWARE BEHAVIOUR

- Sampling: MPU6050 at **100 Hz** (`kMpuRateHz = 100`), non-blocking: schedule on `micros()`,
  advance the deadline by the nominal period; if the deadline has already passed by more than one
  period, resync to `now` and count the skipped slots (report the count only in the final summary
  line, never between samples). No `delay()` in the sample loop.
- MS5611 cycle: non-blocking state machine (issue D1 -> wait `9100 us` -> read ADC -> issue D2 ->
  wait -> read ADC = one sample), started at most once every **40 ms (25 Hz)**. While waiting, the
  loop must keep sampling the MPU. The driver must expose `poll(now_us)` returning true exactly when
  a new sample completed, so the state machine is testable with a fake clock.
- No blocking wait anywhere except: sensor reset delays in `begin()` and the bounded serial wait.
- Serial: 921600 baud, `Serial.begin(921600)`; if `ARDUINO_USB_CDC_ON_BOOT` is 1 call
  `Serial.setTxTimeoutMs(0)` (guarded by `#if ARDUINO_USB_CDC_ON_BOOT`, because `Serial0` does not
  have that method). Wait for the host at most 3000 ms (`while (!Serial && millis() < 3000)`).
- Startup (BEFORE the logger starts, this text is allowed): lines prefixed with `#` reporting
  firmware name/version, both bus pins, both addresses, WHO_AM_I byte, MPU DLPF/SMPLRT_DIV/FS,
  MS5611 PROM words C1..C6, PROM CRC-4 computed value + result (match/mismatch), baseline pressure
  and baseline sample count, MPU and MS5611 init status, and any init failure naming WHICH sensor
  failed. Then print the CSV header line exactly once.
- After the header, print ONLY CSV rows. No decorations, no error text, no blank lines.
- Never print `nan`, `inf`, `nan.00` or any non-numeric token inside a numeric CSV field: invalid
  values are emitted as an EMPTY field (`,,`) and are identified by the `flags` column.
- MPU I2C read failure -> all accel/gyro fields empty for that row + `MPU_ERR` flag.
  MS5611 read failure or out-of-band pressure -> pressure/temperature/altitude empty + relevant flag.
- Re-init: if the MPU was not detected at startup, retry detection at most once every 5 s
  (bounded; log the outcome through the flags column only, never as interleaved text).
  Same for MS5611 PROM/baseline if it failed at startup.
- Keep the raw measurements raw. Do NOT filter/smooth/rotate/clip the logged values, and do NOT
  apply gyro bias correction in this task (state this in the README as a deliberate choice).
- Code structure: sensor acquisition (drivers) / feature calculation (`node_math`) / logging
  (`csv_row` + `.ino`) are separated. No over-engineering: no RTOS tasks, no BLE, no flash storage,
  no JSON, no config protocol in this task.

## 7. CSV SCHEMA (exact column order; header printed once at startup)

```
timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,temperature_c,relative_altitude_m,flags
```
- `timestamp_ms`: integer, from `micros()/1000` (monotonic since boot, single epoch).
- `ax,ay,az,a_mag`: m/s^2, 3 decimals. `a_mag = sqrt(ax^2+ay^2+az^2)`.
- `gx,gy,gz,g_mag`: deg/s, 2 decimals. `g_mag = sqrt(gx^2+gy^2+gz^2)`.
- `pressure_pa`: Pa, 1 decimal. `temperature_c`: C, 2 decimals. `relative_altitude_m`: m, 3 decimals.
- `flags`: unsigned integer bitmask, decimal:
  `0x01 MPU_OK`, `0x02 MPU_ERR`, `0x04 P_FRESH` (a new MS5611 sample landed for this row),
  `0x08 P_ERR` (MS5611 read failed), `0x10 ALT_VALID` (baseline valid and this row has altitude),
  `0x20 P_OUT_OF_RANGE` (read succeeded but rejected by the plausibility band),
  `0x40 PROM_CRC_MISMATCH`.
- A row always has a valid `timestamp_ms` and `flags`; any other field may be empty.
- Freshness policy: the pressure columns are filled ONLY on rows where a new MS5611 sample
  completed since the previous row (default `kRepeatLastPressure = false`), so a stale value is
  never duplicated as a new measurement. Document this in the README (and that a plain forward-fill
  in pandas reconstructs a 25 Hz pressure series with honest sample times).

## 8. HOST TESTS (required, must pass)

`esp/node/run-tests.sh` must compile and run the tests with
`g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I../firmware/esp_node` from `esp/node/tests/`
(mirror `esp/core/run-tests.sh` structure: `set -euo pipefail`, `.build/` output dir), and exit
non-zero on any failure. Support `--sanitize` adding `-fsanitize=address,undefined` like esp/core.

Required assertions (minimum):
1. `mpu6050_test.cpp`
   - `begin()` issues the documented register writes in order (assert the recorded byte sequence),
     and fails when WHO_AM_I returns something other than 0x68 (assert the reported value).
   - `readRaw`/`read` decodes a scripted 14-byte burst into exact scaled accel/gyro values
     (e.g. raw az = 16384 -> 9.80665 m/s^2, raw gx = 131.072 -> 1.0 dps within 1e-6).
   - a bus read failure returns false and leaves no partially-updated output.
2. `ms5611_test.cpp`
   - `begin()` resets, reads PROM words at `0xA0+2i`, and stores C1..C6 raw.
   - the non-blocking cycle issues the D1 command, does not complete before the 9100 us wait,
     completes exactly on/after it, then D2 likewise; `poll()` returns true exactly once per sample
     (drive it with a fake clock, assert no double-completion).
   - datasheet test vector (`D1=9085466, D2=8569150`, reference PROM) -> temperature in
     [20.0, 20.2] C and pressure in [100000, 100030] Pa (this pins the Pa unit).
   - second-order path: a low-temperature vector must still produce finite, in-band values.
   - a bus failure mid-cycle surfaces as an error state and never yields a bogus sample.
3. `node_math_test.cpp`
   - magnitude3 exactness; `a_mag == 9.80665` for a resting ±g vector.
   - relative altitude: `p == p0` -> 0.0; `p0 = 101325`, `p = 100000` -> `111 +- 1.5 m`
     (positive = higher); `p > p0` -> negative; baseline invalid -> the API reports "no altitude"
     (must be distinguishable from 0.0 m).
   - median-of-baseline behaviour incl. rejection of out-of-band samples and the "< 20 valid"
     failure case.
4. `csv_row_test.cpp`
   - exact header string; exact column count/order for a full row; empty fields for each invalid
     combination; flags bits set correctly; no `nan`/`inf` substring ever appears; row length is
     bounded by `sizeof(buffer)` passed in (no truncation of a valid row).

## 9. ACCEPTANCE (the worker must report, and Hermes will independently verify)

1. `esp/node/run-tests.sh` passes on this machine (paste the real command and its real tail output).
   `esp/node/run-tests.sh --sanitize` should also pass if LeakSanitizer works in this environment;
   if it does not, report the exact failure instead of hiding it.
2. Every file listed in section 4 exists; the pure modules contain no `Arduino.h` include.
3. `git status --short` shows ONLY new files under `esp/node/` (no modified tracked files).
4. Final report in this exact block format, short and factual:
```
STATUS:
OWNER: deepseek/deepseek-v4-pro (CommandCode)
FILES: <created files with one-line purpose each>
GOAL:
RESULT: <what was implemented + exact test command + real result>
NEXT_ACTION:
```
Plus a short "UNVERIFIED" list: everything not proven without hardware (real I2C ACKs, real sensor
behaviour, actual sample rate, CDC throughput, ADC noise), and the exact FQBN/upload command you
expect the owner to use.

Do not claim PASS for anything you did not actually run. Do not run the Arduino compiler; the
coordinator runs `arduino-cli compile` itself and will report the result back to you.
