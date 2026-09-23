# T14-R1 — review fix on esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino

STATUS: TODO
OWNER: same single-file owner as T14 (one worker, one file)
FILE TO EDIT: esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino  (do not create or touch any other file)

## FINDING (found by the coordinator reading the finished file)

`MPU6050::getAcceleration()` / `getRotation()` from MPU6050 (Electronic Cats) 1.4.5 ignore the
I2C result: they call `I2Cdev::readBytes(...)` and combine whatever bytes the local buffer holds.
On a NACK/short read the buffer is not written, so the sketch can print garbage/stale accel and gyro
values as if they were a valid sample. That violates the "never silently emit fake data" rule for a
bus glitch that happens after a successful init.

Verified facts:
- `MPU6050/src/I2Cdev.h:142`:
  `static int8_t readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data, uint16_t timeout=I2Cdev::readTimeout, void *wireObj=0);`
- `MPU6050/src/I2Cdev.cpp:209+` returns the number of bytes actually read (0 when the device does not
  ACK or nothing arrives), and for `ARDUINO_ARCH_ESP32` it uses `endTransmission(false)` (repeated
  start) + `requestFrom`, so it is safe on the second bus when `wireObj` is passed.
- `MPU6050_RA_ACCEL_XOUT_H` is `0x3B` (already used by the library); the 14-byte burst is
  ax,ay,az,temp,gx,gy,gz big-endian int16.

## REQUIRED CHANGE (minimal, keep the file single-file and library-based)

1. Rewrite `sampleMpu()` so it does ONE explicit 14-byte burst read and reports failure honestly:
   - `uint8_t buf[14];`
   - `int8_t n = I2Cdev::readBytes(mpuAddrInUse, MPU6050_RA_ACCEL_XOUT_H, 14, buf, I2Cdev::readTimeout, &I2C_MPU);`
   - return false unless `n == 14` (then the caller already leaves the 8 accel/gyro CSV fields EMPTY);
   - decode `ax..gz` as big-endian int16 from `buf[0..13]` (ax=buf[0..1] .. gz=buf[12..13]);
   - keep the existing scaling constants (`kAccelLsbPerG`, `kGyroLsbPerDps`, `kGravity`) and keep
     `sqrtf` magnitude math in the caller unchanged;
   - keep the `if (!mpuOk) return false;` guard.
2. Add a consecutive-failure counter: after N = 10 consecutive failed bursts (about 200 ms at 50 Hz),
   set `mpuOk = false` so the existing silent 5 s re-detection path runs again (mirror the retry logic
   already in the file; reset the counter on any successful burst). No new printing — nothing may be
   written to Serial after the CSV header.
3. Update the comment on `sampleMpu()` to state the reason (library getters do not report I2C errors;
   a failing burst must produce empty fields, never garbage).
4. Do not change anything else: pins, buses, rates, CSV header/schema, column order, baseline logic,
   startup report, OSR_STANDARD, kSampleRateHz = 50 stay exactly as they are. No new library, no new
   file, no `delay()` in loop.

## ACCEPTANCE (run and paste real output)

```
ARDUINO_CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled" esp/NCKH27PA_ESP32S3
"$ARDUINO_CLI" --config-file ~/.arduinoIDE/arduino-cli.yaml compile \
  --fqbn esp32:esp32:esp32s3 esp/NCKH27PA_ESP32S3
ls -la esp/NCKH27PA_ESP32S3          # still exactly ONE file
rg -n '#include' esp/NCKH27PA_ESP32S3/NCKH27PA_ESP32S3.ino
git status --short                   # nothing new outside esp/NCKH27PA_ESP32S3/
```
Both compiles must pass with 0 errors. Report: STATUS / FILES / RESULT (real command output tails) /
what changed in `sampleMpu()` in 3 lines. Do not upload/flash, do not run `arduino-cli monitor`.
