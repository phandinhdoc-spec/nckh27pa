# T17 — PATCH the existing NCKH27PA_ESP32S3.ino (real-device observations)

STATUS: TODO
OWNER: one implementation worker (in-process subagent, model `deepseek-v4-flash`)
FILE TO EDIT: esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino — the ONLY file in that folder.
NOT a rewrite: patch the existing 789-line sketch in place, keep its structure, comments style and
`#if SERIAL_TEST_MODE` split. Do not create or modify any other file anywhere (no .h/.cpp/README/
CMakeLists/sdkconfig; do not touch `esp/node/`, `android/`, `docs/`, `.ai/`). Do not scan the repo.
A copy of the current file is at /tmp/before_T17.ino for reference.

HARDWARE OBSERVATIONS THAT TRIGGER THIS PATCH (from the real board, keep the fixed wiring)
- MS5611 reported ~50749 Pa at ~31.6 C — almost exactly half of normal atmospheric pressure.
- MPU6050 and MS5611 alternate between OK and ERROR across the 5 s retry cycle.
- Fixed pins stay: MPU6050 SDA GPIO7 / SCL GPIO6 (bus 0), MS5611 GY-63 SDA GPIO3 / SCL GPIO2 (bus 1).

## PATCH 1 — MS5611 mathMode (the 50% pressure bug)

Verified library facts (MS5611 by Rob Tillaart 0.5.2, already installed):
- `MS5611/MS5611.h:68`: `bool reset(uint8_t mathMode = 0);` (public; comment: "mathMode = 0 (default),
  1 = factor 2 fix")
- `MS5611/MS5611.cpp:41`: `begin()` → `isConnected()` then `return reset(0);`
- `MS5611/README.md:38`: "Some device types will return only 50% of the pressure value. This is solved
  by calling **reset(1)** to select the math method used." `reset(1)` re-reads the ROM and returns
  false if the ROM could not be read.

Change `tryInitBaro()` to:
```cpp
bool tryInitBaro(MS5611 *dev) {
  if (!dev->begin()) return false;       // isConnected() + reset(0) + doc PROM
  if (!dev->reset(1)) return false;      // mathMode 1 = "factor 2 fix" (MS5611 0.5.2 README)
  dev->setOversampling(OSR_STANDARD);
  return true;
}
```
Add a short Vietnamese comment stating why (some MS56xx/MS5611 report half the pressure with mathMode
0; reset(1) selects the appnote math). DO NOT scale the pressure value by 2 anywhere — the library
handles it via mathMode. Also record a `const uint8_t kBaroMathMode = 1;` constant and use it in the
startup report line for MS5611 (CSV mode report may mention `mathMode 1`; text only, before the header).

## PATCH 2 — I2C diagnostic speed: 400 kHz → 100 kHz

- `const uint32_t kI2cFreq = 100000;` with a comment: lowered for hardware-diagnostics robustness
  (was 400 kHz); it is a single constant used by BOTH buses.
- Update EVERY printed string that mentions the frequency so the text cannot lie: the CSV-mode
  startup report line "Board ESP32-S3 Super Mini — 2 bus I2C doc lap @400 kHz" must reflect the
  constant (use the constant, e.g. `kI2cFreq / 1000` in a printf, or the literal after changing it).
- Do NOT change sample-rate goals: MPU still 100 Hz (test mode) / 50 Hz (CSV mode), MS5611 still
  `read()` at most every 40 ms. The 14-byte burst at 100 kHz is ~1.3 ms and the MS5611 read is
  bounded by its conversion waits (~4.6 ms) — both still fit the 10 ms slot; mention this in a comment.

## PATCH 3 — MS5611 baseline after a later recovery

Today: if MS5611 fails at boot, the 5 s retry may recover it, but the baseline stays invalid for the
whole session, so `pressure_delta_pa` / `relative_altitude_m` / `Xu huong` stay `--` forever.

Required: when a retry succeeds AND `baselineValid == false`, collect a new baseline. Rules:
- Implement it NON-BLOCKING so CSV logging never stalls: while `!baselineValid && baselineCollecting`,
  every valid barometer sample from the normal loop path is fed into the same accumulator
  (`kBaselineTarget` = 30 target, `kBaselineMin` = 15 minimum, window `kBaselineTimeoutMs` = 3000 ms,
  same 30000..110000 Pa band, mean of valid samples) and finalize as soon as the target count or the
  window deadline is reached. Do not call the blocking `collectBaseline()` from `loop()`.
- Reuse/extend the existing baseline globals (`baselineCount`, `baselinePa`, `baselineValid`) plus the
  new collecting state; keep `collectBaseline()` (setup path, with its progress bar) working as today.
- Never recalibrate an already valid baseline: if `baselineValid == true`, a later reconnect must NOT
  restart collection or move the datum.
- Test mode only: print ONE informative line when the recovery baseline starts (e.g.
  `MS5611 da hoi phuc: dang lay baseline moi...`) and ONE when it finishes (value, or an honest
  failure line with the valid-sample count). CSV mode: totally silent — no text after the header.
- A failed recovery baseline must be retryable the NEXT time the sensor recovers.

## PATCH 4 — MPU6050 health: NOT FOUND vs CONNECTION LOST vs OK

Today a runtime read failure is reported as "Khong tim thay MPU6050", which is misleading. Introduce
three explicit states and counters:
- A. NOT FOUND: never detected at 0x68/0x69 (`!mpuEverDetected`).
- B. CONNECTION LOST: was detected before, and >= `kMpuFailStreakLimit` (10) consecutive 14-byte
  reads failed (`mpuEverDetected && !mpuOk`).
- C. OK: valid reads arriving (`mpuOk`).
Counters required: total MPU read failures (`mpuReadErrors`), consecutive failures (`mpuFailStreak`,
already exists), number of reconnects (`mpuReconnects`, incremented when a re-detection succeeds after
a previous loss). Reset the consecutive counter on any successful burst and on a successful
re-detection; keep the existing silent ≤5 s re-detect path (it is what performs the reconnect).

TEST-mode MPU block must look like one of these (keep the existing accel/state/gyro lines under OK):
```
[MPU6050] OK
  Address     : 0x68
  Read errors : 0
  Reconnects  : 0
```
```
[MPU6050] CONNECTION LOST
  Address     : 0x68
  Consecutive errors: 10
  Read errors : 137      Reconnects: 1
  Retry in <= 5 s
```
```
[MPU6050]  !!! ERROR !!!
  Khong tim thay MPU6050
  I2C: SDA GPIO7 / SCL GPIO6   (da thu 0x68 va 0x69)
```
Never claim "not found" for a device that was found and then lost its connection. Values stay `--`
whenever no valid sample exists for the window (no fake zeros). CSV mode: the counters are internal
only — the CSV stream stays byte-identical (same 13 columns, same header, no extra text).

## PATCH 5 — MS5611 diagnostics in TEST mode

After successful init, the TEST-mode `[MS5611]` block must clearly show address, math mode, pressure,
temperature, baseline and relative altitude, e.g.:
```
[MS5611]   OK
  Address      : 0x77
  Math mode    : 1
  Ap suat      : 100842 Pa
  Nhiet do     : 29.4 C
  Baseline     : 100827 Pa          (-- khi baseline chua co)
  Do cao tuong doi : +0.27 m        (-- khi baseline chua co)
  Chenh ap     : -3.2 Pa            (-- khi baseline chua co)
  Xu huong     : ON DINH            (-- khi baseline chua co)
```
`Math mode` must print the real `kBaroMathMode` value used at init (1). Keep the existing ERROR block
shape for the not-found case. Do not add any fall-detection threshold or classification.

## KEEP (do not regress)

One file only; two independent `TwoWire` buses; libraries MPU6050 (Electronic Cats 1.4.5) + MS5611
(Rob Tillaart 0.5.2); honest 14-byte `I2Cdev::readBytes` burst; 2 Hz display in test mode; 100 Hz test /
50 Hz CSV acquisition; `OSR_STANDARD`; JSON-free plain Arduino C++; no `delay()` in `loop()` (only the
setup host wait); no LED heartbeat (comment stays); labels remain DISPLAY-ONLY with the existing
warning comment block updated where needed; the `BINH THUONG`/`DANG YEN` nominal-magnitude check stays.

## ACCEPTANCE (run and paste real output)

```
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
# ESP32S3 Dev Module (as requested) + the USB-CDC variant
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile --fqbn esp32:esp32:esp32s3 esp/NCKH27PA_ESP32S3
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" esp/NCKH27PA_ESP32S3
# flag=0 must still compile: copy to a scratch dir OUTSIDE the repo, set the flag to 0, compile, delete
ls -la esp/NCKH27PA_ESP32S3            # exactly one file
git status --short                      # nothing new/modified outside esp/NCKH27PA_ESP32S3/
diff /tmp/before_T17.ino esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino   # (for your own review of the patch)
```
Both compiles must pass with 0 errors; report the sketch sizes. Never upload/flash; never run
`arduino-cli monitor`.

FINAL REPORT: STATUS / FILES / exact changed functions + line ranges (`diff` output summary, not the
whole file) / compile results / expected TEST-mode serial output sample. Keep it short and factual.
