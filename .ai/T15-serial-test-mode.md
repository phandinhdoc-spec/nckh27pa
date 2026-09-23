# T15 — SERIAL_TEST_MODE cho esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino

STATUS: TODO
OWNER: one implementation worker (in-process subagent, model `deepseek-v4-flash`)
FILE TO EDIT: esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino  — the ONLY file in that folder; do not
create or modify anything else anywhere (no .h/.cpp/README/CMakeLists/sdkconfig; do not touch
`esp/node/`, `android/`, `docs/`, `.ai/`). Do not scan the repo.

## GOAL

Add a compile-time switch so the sketch can (a) print a human-readable sensor test to the Arduino
Serial Monitor, or (b) log the pure CSV dataset exactly as it does today. Sensor acquisition must
keep running at the real rate (50 Hz) in BOTH modes.

## 1. TOP-OF-FILE COMMENT + FLAG (exact)

Immediately after the existing file header comment, add:

```cpp
// SERIAL_TEST_MODE = 1 : human-readable sensor test
// SERIAL_TEST_MODE = 0 : clean CSV dataset logging
#define SERIAL_TEST_MODE 1
```

Default is 1. Both values must compile.

## 2. TEST MODE (SERIAL_TEST_MODE == 1)

- Baud stays `Serial.begin(115200)` with the existing bounded host wait.
- Sensor sampling is UNCHANGED: MPU6050 burst every `kSlotPeriodUs` (50 Hz), MS5611 `read()` at most
  every `kBaroIntervalMs` (40 ms). Only the PRINTING is throttled: print a block about 5–10 times per
  second (use a named constant, e.g. `kTestPrintDivider = 5` at 50 Hz ⇒ 10 Hz). Never lower the
  sensor rate to the print rate, and never add a long `delay()`; the throttle must be a counter /
  `millis()` check, not a blocking wait.
- First, once, print the init/model block (values shown are the real reported state — never invented):
```
==============================
ESP32-S3 SENSOR TEST
==============================
MPU6050: OK            (hoac FAIL, va ghi ro chan/dia chi dang dung)
MS5611 : OK            (hoac FAIL, va ghi ro chan/dia chi dang dung)
accel in g, gyro in deg/s — CSV mode logs m/s² ; baud 115200
```
- If a sensor failed, print an explicit error line and NEVER print 0-looking values for it:
  `ERROR: MPU6050 NOT FOUND` / `ERROR: MS5611 NOT FOUND` (add the pins/addresses tried). Values for a
  missing/invalid sensor print as `--` (e.g. `Pressure: --`), never `0.0`.
- Then, every print tick (~10 Hz), print a block like:
```
MPU6050
AX: 0.012 g (raw 201)
AY: -0.018 g (raw -295)
AZ: 0.998 g (raw 16352)
A_MAG: 0.998 g
GX: 0.24 deg/s (raw 31)
GY: -0.15 deg/s (raw -20)
GZ: 0.08 deg/s (raw 10)
G_MAG: 0.29 deg/s

MS5611
Pressure: 100842.3 Pa
Delta P : -2.4 Pa
Temp    : 29.31 C
Rel Alt : 0.20 m

Loop rate: 50.1 Hz (slots 50/50) | baro: 25.0 Hz (5/5)
------------------------------
```
  Formatting need not be byte-exact but must be that readable; keep the numbers truthful.
- Include: init OK/FAIL for both sensors, converted AND raw values for the 6 axes, `a_mag` and
  `g_mag`, pressure, pressure delta, temperature, relative altitude, and the MEASURED sample rate.
- Measured rate: count executed MPU slots and elapsed `micros()` over a ~1000 ms window and print the
  measured Hz plus executed/expected slot counts; do the same for the barometer (valid samples /
  expected cycles). Reset the counters each window. This is the only honest way to report the
  achieved rate — do not print a nominal constant as if it were measured.
- In test mode the pressure block may show the most recent VALID sample (human monitor), but if no
  valid sample exists yet it must print `--`; in CSV mode the no-repeat-fresh-data rule stays exactly
  as it is now.
- In test mode, keep the CSV header line OUT of the stream (test mode is not a dataset). Print the
  separator line between ticks so runs are easy to scroll.

## 3. CSV MODE (SERIAL_TEST_MODE == 0)

- Unchanged behaviour: the existing startup report, then the CSV header exactly once, then ONLY CSV
  rows — no "Sensor OK", no separators, no debug text, no blank lines in the dataset.
- Header (must stay byte-identical):
`timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,pressure_delta_pa,temperature_c,relative_altitude_m`
- Row example shape: `12540,0.012,-0.018,0.998,0.998,0.24,-0.15,0.08,0.29,100842.3,-2.4,29.31,0.20`
- Empty fields stay EMPTY for invalid/missing data (never 0/nan/inf); 50 Hz; accel in m/s².

## 4. LED HEARTBEAT

Do NOT add a heartbeat LED. The ESP32-S3 Super Mini onboard LED GPIO is not documented/verified for
this board in this project, and inventing a pin is forbidden. Add ONE short comment stating exactly
that, so it is clear the omission is deliberate (not an oversight).

## 5. EVERYTHING ELSE STAYS

Two buses (`TwoWire I2C_MPU = TwoWire(0)` SDA7/SCL6, `TwoWire I2C_BARO = TwoWire(1)` SDA3/SCL2,
400 kHz), libraries (`MPU6050` Electronic Cats 1.4.5 + `MS5611` Rob Tillaart 0.5.2), the honest
14-byte `I2Cdev::readBytes` MPU burst with the 10-consecutive-failure re-detection, `OSR_STANDARD`,
baseline rules, silent 5 s retries, no thresholds / no fall decision / no filtering / no gyro-bias
correction. Still ONE file, plain Arduino C++, helpers defined before use, short Vietnamese comments.
No new library, no `delay()` in `loop()`.

## 6. ACCEPTANCE (run and paste real output)

```
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
# test mode build (default flag = 1)
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" esp/NCKH27PA_ESP32S3
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 esp/NCKH27PA_ESP32S3
# CSV mode build must also compile: temporarily set the flag to 0 with a scratch copy OUTSIDE the
# repo (e.g. cp -R to /tmp/csvmode and sed the flag there), compile that copy, then delete it.
# Never leave the repo file with the flag at 0 and never leave extra files in the repo.
ls -la esp/NCKH27PA_ESP32S3        # still exactly one file
rg -n '#include' esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino
git status --short
```
Report: STATUS / FILES / RESULT (real output tails for all three builds: sizes + exit codes) / what
the two modes print (3 lines) / confirmation that the repo folder still has exactly one file.
Do not upload/flash, do not run `arduino-cli monitor`.
