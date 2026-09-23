# T18 — Bộ test ESP32 + thuật toán phát hiện té ngã (firmware test rig)

STATUS: DONE (PASS có điều kiện — chờ kiểm chứng trên phần cứng thật)
DATE: 2026-09-22

OWNER (workers)
- W1 analysis: AGY `gemini-3.8-flash-medium` (read-only) → `.ai/T18-android-to-esp-mapping.md`
- W2 implementation: AGY `gemini-3.8-flash-high` (accept-edits) → `esp/esp32-plan.md`, `esp32-test/**`
- W2 R1 fix: AGY `gemini-3.8-flash-high` (theo 8 điểm review + 4 điểm Hermes tự kiểm)
- W3 review: AGY `claude-opus-4-6-thinking` (read-only) → PASS CÓ ĐIỀU KIỆN, 0 BLOCKER / 3 MAJOR / 4 MINOR
- PM + kiểm chứng cuối: Hermes (build lại, đọc code, kiểm diff, kiểm mtime)

GOAL
Cập nhật kế hoạch ESP theo kiến trúc Android hiện tại và viết firmware thử nghiệm thuật toán phát hiện té ngã
cho bộ phần cứng TEST_RIG (ESP32-WROOM-32 + MPU9250 + GY-63/MS5611), KHÔNG đụng firmware sản phẩm
(ESP32-S3 Super Mini + MPU6050 + GY-63).

FILES (thay đổi)
- `esp/esp32-plan.md` (+219/−6): thêm §3.6 (TARGET_PRODUCT vs TEST_RIG) và §15 (15.1–15.14: mục tiêu, mapping,
  pipeline, sampling, filtering, pressure baseline, altitude delta, features, state machine, logging, profile,
  calibration, test, migration MPU9250→MPU6050).
- `esp32-test/esp32-test.ino` (1076 dòng, 1 file, không thư viện ngoài)
- `esp32-test/README.md` (151 dòng), `esp32-test/TEST_PLAN.md` (236 dòng), `esp32-test/logs/.gitkeep`
- `.ai/T18-android-to-esp-mapping.md` (báo cáo W1)
- KHÔNG sửa: `esp/node/**` (firmware sản phẩm), `backend/**`, `docs/**`, contract BLE/HTTP.

RESULT
- Build thật (arduino-cli trong Arduino IDE, core esp32 3.3.12, FQBN `esp32:esp32:esp32`):
  **exit 0, 310232 bytes (23%) flash, 26388 bytes (8%) RAM** — Hermes tự chạy lại, không tin self-report.
- Luật xác nhận ngã bám đúng Android: `|a| ≥ 25 m/s²` → `| |a| − 9.81 | ≤ 1.0` liên tục ≥1000 ms và ≥6 mẫu,
  trong cửa sổ 3000 ms kể từ impact; gap >250 ms hủy chuỗi; impact re-latch như Android.
  Free-fall (0.5 g/80 ms), gyro (120 dps), Δp (12 Pa), ΔH (−0.40 m) chỉ là bằng chứng ghi log.
- Serial: HUMAN 4 dòng/giây (2–5 dòng/s), CSV 100 Hz có header + cột `event` mang tag thật.

NEXT_ACTION (cần người dùng làm trên phần cứng)
1. Xác minh wiring thật: SDA=GPIO21, SCL=GPIO22, 3V3, GND chung, AD0→GND (0x68), CSB→GND (0x77), pull-up 4.7k.
2. Nạp `esp32-test/esp32-test.ino`, baud 115200, giữ yên 3 s khi calibration, ghi lại dòng `[CAL]`/`[READY]`.
3. Chạy T0–T12 theo `esp32-test/TEST_PLAN.md`, lưu log vào `esp32-test/logs/`.
4. Nếu false-positive ở T2/T5: chỉnh `impactAccelerationMs2` (25 → 26–28) trước tiên.

BLOCKER đã ghi nhận
- CommandCode `-p` (glm-5.3-flash) treo >14 phút, 0 tool call → đã kill, chuyển sang AGY (không tính là lỗi task).
- Codex CLI hết quota subscription (báo reset 14:28) → không dùng được cho W2; đã chuyển sang AGY gemini-3.8-flash-high.
