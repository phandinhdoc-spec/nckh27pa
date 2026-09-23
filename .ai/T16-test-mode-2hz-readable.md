# T16 — TEST MODE dễ đọc cho con người (2 Hz) cho esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino

STATUS: DONE + R1 (host-verified by Hermes; NO hardware verification — nothing flashed)
FINAL FILE: esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino — 789 lines / 32 937 B, the only file in its folder.
HERMES VERIFICATION (coordinator's own runs, not worker self-reports):
- flag=1 `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled` -> exit 0, 332 802 B (25%) / 24 360 B RAM
- flag=1 `esp32:esp32:esp32s3` -> exit 0, 312 421 B (23%) / 23 680 B RAM
- flag=0 scratch copy outside the repo -> exit 0, 330 054 B (23%) / 23 552 B RAM
- CSV header byte-identical, printed once; all labels live inside `#if SERIAL_TEST_MODE` so none can reach the dataset
- `git diff --check` clean; only `esp/…` untracked entries; `#include` limited to Wire/I2Cdev/MPU6050/MS5611/math
R1 (review fix after T16): `kAMagNominalG` was declared but unused, so a stuck/free-fall sensor could still read
`BINH THUONG` / `DANG YEN`. Now `BINH THUONG`/`DANG YEN` require spread < 0.05 g AND |a_mag - 1 g| <= 0.15 g
(new display-only `kAMagNominalTolG`); otherwise the screen prints the honest `BAT THUONG (|a| xa 1 g)`.
STILL UNVERIFIED: real sensor ACKs/addresses, achieved 100 Hz, 2 Hz screen cadence on a real host, 115200
throughput, ADC noise, and everything about FALL-01 (no thresholds exist by design).

FILE (was to edit, now final): esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino — the ONLY file in that folder. Do not create
or modify anything else (no .h/.cpp/README/CMakeLists/sdkconfig; do not touch `esp/node/`, `android/`,
`docs/`, `.ai/`). Do not scan the repo.

This ticket REPLACES the display design of T15. The sketch already contains `#define SERIAL_TEST_MODE 1`,
a ~10 Hz dumps-everything test block and a wrong 50 Hz sample-rate comment for test mode; rewrite that
test-mode UI in place. CSV mode (flag 0) must stay behaviourally identical to today.

## 1. RATES (display and acquisition are decoupled)

```cpp
#if SERIAL_TEST_MODE
const uint32_t kSampleRateHz = 100;  // test mode: MPU acquisition 100 Hz (khong bi gioi han bang thong CSV)
#else
const uint32_t kSampleRateHz = 50;   // CSV mode: 115200 baud chi chiu ~11520 B/s
#endif
const uint32_t kDisplayHz = 2;                               // Serial Monitor refresh: 2 Hz
const uint32_t kDisplayEverySamples = kSampleRateHz / kDisplayHz;  // 50 mau o 100 Hz
```
- Sensor acquisition NEVER drops to the display rate: MPU burst every `kSlotPeriodUs`
  (100 Hz in test mode, 50 Hz in CSV mode), MS5611 `read()` at most every 40 ms (~25 Hz).
- The serial status screen is printed every `kDisplayEverySamples` samples (~2 Hz), not per sample.
- No `delay()` in `loop()`; the throttle is a counter.

## 2. STARTUP (test mode: step by step, as it really happens)

Print (wording may be adjusted, order/logic must match reality):
```
NCKH27PA ESP32-S3
Starting...

MPU6050 bus:
  SDA = GPIO7
  SCL = GPIO6
  -> FOUND            (hoac -> NOT FOUND, tap dia chi 0x68 / 0x69)

MS5611 bus:
  SDA = GPIO3
  SCL = GPIO2
  -> FOUND            (hoac -> NOT FOUND, tap dia chi 0x77 / 0x76)

Calibrating pressure baseline...
[##########] DONE          <- tien do THAT theo so mau hop le da thu duoc (10 o)
Baseline pressure: 100827 Pa     (neu baseline khong hop le: ghi ro WARNING + so mau thieu)

Starting sensor acquisition...
MPU rate target: 100 Hz

READY.
```
- Only after `READY.` does the repeating status screen begin.
- Baseline rules are unchanged (dai 30000..110000 Pa, muc tieu 30 mau hop le, toi thieu 15, het han
  3000 ms, cố định cả phiên). The progress bar must reflect real collected samples.
- In CSV mode keep exactly the current startup report, then the CSV header once, then rows only.

## 3. STATUS SCREEN (~2 Hz), mục tiêu: đọc bằng mắt, không phải "rừng số"

Layout mong muốn (spacing không cần tuyệt đối, cấu trúc và nội dung thì cần):
```
==================================================
 NCKH27PA - ESP32-S3 SENSOR TEST
==================================================

[MPU6050]  OK
  Gia toc tong : 1.01 g        -> BINH THUONG
  Trang thai   : DANG YEN

  X: +0.03 g
  Y: -0.06 g
  Z: +1.00 g

  Xoay tong    : 2.1 deg/s     -> RAT NHO

--------------------------------------------------

[MS5611]   OK
  Ap suat      : 100824 Pa
  Chenh ap     : -3.2 Pa
  Do cao tuong doi : +0.27 m
  Xu huong     : DANG LEN

  Nhiet do     : 29.4 C

--------------------------------------------------

[HE THONG]
  MPU sample rate : 99.8 Hz (dat 998/1000 slot)
  Serial refresh  : 2 Hz
  Uptime          : 00:01:26

==================================================
```
- NO per-line timestamp in test mode (detailed timestamps belong to CSV mode). `Uptime: HH:MM:SS`
  from `millis()` is the only clock shown.
- Do NOT print raw LSB values or any other number that a human cannot use at a glance.
- Accel shown in g (with sign, 2 decimals). Pressure in Pa (integer). Delta in Pa (1 decimal).
  Relative altitude in m (2 decimals, signed).

## 4. DIỄN GIẢI (nhãn chữ) — CHỈ ĐỂ XEM, KHÔNG PHẢI THUẬT TOÁN NGÃ

Add a prominent comment block in the file stating: these labels are DISPLAY/VISUALIZATION ONLY, they
are NOT a fall detector, they must not generate any event, must not be written to the CSV, and the
thresholds used for the labels must not be reused as detection thresholds.

Labels required:
- acceleration magnitude `a_mag` (g): `~1 g` and stable -> `-> BINH THUONG`;
  moderate variation -> `-> DANG CHUYEN DONG`; strong -> `-> CHUYEN DONG MANH`.
- state line `Trang thai`: `DANG YEN` / `CHUYEN DONG NHE` / `DANG CHUYEN DONG`, derived from the
  variation of `a_mag` inside the current display window (e.g. spread max-min of |a| in g and how
  many samples deviate from 1 g by more than a small amount). Suggested labels' constants:
  spread < 0.05 g -> DANG YEN; < 0.35 g -> CHUYEN DONG NHE; else DANG CHUYEN DONG; and
  `CHUYEN DONG MANH` when any sample in the window leaves 0.5 g .. 2.0 g. Use named constants,
  document them as display-only.
- gyro: default shows ONLY `Xoay tong : x.x deg/s -> RAT NHO / NHE / MANH`
  (RAT NHO < 10 deg/s, NHE < 60, MANH >= 60). Print the three axes `GX:`, `GY:`, `GZ: deg/s` ONLY
  while the total is `NHE` or `MANH` (i.e. when the board is really rotating) — this is the
  noise-reduction requirement.
- pressure trend `Xu huong`: `DANG LEN` / `DANG XUONG` / `ON DINH`. Compute it from a DISPLAY-ONLY
  smoothed relative altitude (small moving average over the last up-to-5 valid barometer samples)
  plus hysteresis so the word does not flap: e.g. switch to DANG LEN only when the smoothed value
  rises by >= 0.15 m above the last switch point, DANG XUONG when it falls >= 0.15 m, otherwise
  ON DINH. Comment that this smoothing/hysteresis is display-only and NEVER touches the raw
  pressure/temperature/delta/altitude values that CSV mode logs.

## 5. LỖI CẢM BIẾN — PHẢI NHÌN LÀ BIẾT NGAY

```
[MPU6050]  !!! ERROR !!!
  Khong tim thay MPU6050
  I2C: SDA GPIO7 / SCL GPIO6   (da thu 0x68 va 0x69)
```
same shape for `[MS5611]  !!! ERROR !!!` (`Khong tim thay GY-63/MS5611`, SDA GPIO3 / SCL GPIO2,
tried 0x77 and 0x76). For a failed/invalid sensor print `--` for its values — never fake zeros, never
numbers that look like a working sensor. Retry behaviour stays silent (no text spam), and if a sensor
recovers during the run, the next status screen simply shows it as OK again (do not print extra
notification lines between refreshes; the screen itself is the notification).

## 6. CSV MODE = UNCHANGED

`#define SERIAL_TEST_MODE 0` keeps exactly today's behaviour: existing startup report → CSV header
once (byte-identical string) → ONLY CSV rows, 50 Hz, accel in m/s², empty fields for invalid data, no
labels, no separators, no banners, no uptime lines, no `#`/`=` decorations anywhere in the stream.

## 7. KEEP EVERYTHING ELSE

Two buses (`TwoWire I2C_MPU = TwoWire(0)` SDA7/SCL6, `TwoWire I2C_BARO = TwoWire(1)` SDA3/SCL2,
400 kHz), libraries MPU6050 (Electronic Cats 1.4.5) + MS5611 (Rob Tillaart 0.5.2), the honest 14-byte
`I2Cdev::readBytes` burst with the 10-consecutive-failure re-detection, `OSR_STANDARD`, baseline
rules, silent 5 s retries, no thresholds used for any decision, no filtering of logged data, no
gyro-bias correction, no LED heartbeat (keep the comment explaining the pin is not verified —
inventing it is forbidden), still exactly ONE file, plain Arduino C++ (no classes/templates/std
containers/dynamic allocation), helpers defined before use, short Vietnamese comments.

## 8. ACCEPTANCE (run and paste real output)

```
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
# flag = 1 (as committed in the repo file)
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" esp/NCKH27PA_ESP32S3
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 esp/NCKH27PA_ESP32S3
# flag = 0 must also compile: copy the sketch to a scratch dir OUTSIDE the repo, set the flag to 0
# there, compile that copy, then delete the scratch copy. The repo file must be left with flag = 1.
ls -la esp/NCKH27PA_ESP32S3        # exactly one file
rg -n '#include' esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino
git status --short
```
Report STATUS / FILES / RESULT (real output tails: sketch sizes + exit codes for all three builds) /
3 lines on what the status screen shows / confirmation that the folder still has exactly one file.
Do not upload/flash; do not run `arduino-cli monitor`.
