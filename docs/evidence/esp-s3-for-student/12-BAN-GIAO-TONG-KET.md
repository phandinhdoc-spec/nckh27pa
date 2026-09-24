# BÀN GIAO TỔNG KẾT — run_20260925_012246_7b27e1 (NCKH27PA)

Ngày viết: 2026-09-25, khoảng 03:45 (+07). Người viết: Hermes (phiên `20260925_010323_d903c6`).
Lý do dừng: chủ dự án yêu cầu dừng để tiết kiệm token; **công việc CHƯA hoàn tất 100%** — phần còn thiếu ghi rõ ở mục 6.

## 1. Tóm tắt một đoạn

Đã Việt hóa xong toàn bộ firmware ESP32-S3 thành bản cho học sinh (`esp-s3-for-student/esp-s3-for-student.ino`, 1819 dòng, tên định danh tiếng Việt không dấu, comment tiếng Việt có dấu trên mọi dòng, chuỗi Serial không dấu), thêm MENU/khối TIẾN TRÌNH/nhịp tim, viết tài liệu hướng dẫn học sinh, và sửa xong hai lỗi Telemetry/Profile phía app Android (đã build, test, lint, cài lên điện thoại). Tệp gốc `esp-s3/esp-s3.ino` KHÔNG bị sửa một byte nào. Việc còn thiếu: kiểm chứng trên phần cứng (nạp bản học sinh, quét BLE, bấm thử phím Serial, chốt log `START_STREAM` từ app) và review độc lập (đã chạy được ~1/3 thì phải dừng).

## 2. Sản phẩm và trạng thái (có hash để đối chiếu)

| Sản phẩm | Trạng thái | Bằng chứng |
|----------|-----------|------------|
| `esp-s3-for-student/esp-s3-for-student.ino` — 1819 dòng, sha256 `0eed1ad9274bc6bb82406d8d16f377093a61ffb178434e835e4e4746f7477ecb` | XONG, biên dịch ĐẠT | `05-parity/parity-sau-w23.txt`, log compile |
| `esp-s3-for-student/BANG-DOI-TEN.md` — 209 dòng đối chiếu tên | XONG, phủ 209/209 | `05-parity/glossary-check-final.txt` |
| `esp-s3-for-student/HUONG-DAN-HOC-SINH.md` — 398 dòng, 10 mục, sha256 `caa72c00fb232d8e1eeab11b5179d7c93472182b359fe535fde24051daee68ad` | XONG, dữ kiện đã đối chiếu mã | mục 5 dưới đây |
| `esp-s3/README.md` — đã cập nhật, sha256 `a82a784765a3c5c6d0fe95a0367da3e38beef3dddb50bf80a693e2b5537b225c` | XONG | git status |
| `esp-s3/esp-s3.ino` — sha256 `8954eae25ae581848f60ece649876250b6ab9286e5270afc1c8df6fbf792a995` | **KHÔNG đổi** (đúng yêu cầu) | `git status --short esp-s3/esp-s3.ino` rỗng |
| `android/.../bluetooth/BleTestScreen.kt` — sha256 `2ef5389734b3a0fa8b81f727fadd3a9b47a3965c9be4a13b3f50ed9bf414c5dd` | XONG (sửa lỗi Telemetry + khoá nút Profile) | `11-ban-sua-app-telemetry-profile.md` |
| `android/.../bluetooth/FallSafeBleClient.kt` — sha256 `f0aee6135768ce1bacfb30ab11085c0ed5f00f9fb95dd5f5dc64b5222ba44b48` | XONG (`writeProfile` trả `false`) | như trên |
| APK debug `android/app/build/outputs/apk/debug/app-debug.apk` (13.039.470 byte, build 03:07) | XONG, **đã cài lên điện thoại** lúc 03:11:50 (`versionName=0.3-permission`) | `adb shell dumpsys package` |

## 3. Việc đã hoàn thành theo work unit của run

- **T1 / W1.1 — DONE**: baseline có bằng chứng. Kết luận: BLE phía thiết bị KHÔNG có lỗi; trạng thái "không dò được" là do chip ở chế độ DOWNLOAD (`boot:0x23`) và 2 khuyết điểm phía app. Vì vậy **không sửa firmware chính thức**.
- **T1 / W1.2 — DONE**: điều tra read-only app Android, 5 giả thuyết A1–A5 kèm trích dẫn tệp:dòng → `08-android-ble-review.md` (117 dòng).
- **T2 / W2.1 — DONE**: bảng đối chiếu tên (`BANG-DOI-TEN.md`), worker đầu (GLM-5.1) hỏng vì lỗi mạng API, chạy lại bằng `stepfun/Step-3.5-Flash` rồi Hermes sửa 5 dòng lỗi.
- **T2 / W2.2 — DONE**: Việt hóa 4 đoạn (1–561, 562–1029, 1030–1307, 1308–1757) bằng openai-codex `gpt-5.6-sol`, kiểm chứng bằng 4 lớp script tự viết.
- **T2 / W2.3 — DONE**: thêm `m` (MENU 8 phím), `p` (khối TIẾN TRÌNH 6 bước + cảnh báo `STATE_DEGRADED`), `d` (bật/tắt nhịp tim 5 giây). +62 dòng, **0 dòng cũ bị sửa/xóa** (kiểm bằng `diff` với bản sao lưu `esf-truoc-w23.ino`, sha256 `413c2886…`).
- **Dọn 334 comment rập khuôn** (phát sinh theo yêu cầu chủ dự án): câu "Thực hiện bước xử lí tương ứng." từng lặp 244 lần → nay 0.
- **Sửa lỗi app Telemetry/Profile** (phát sinh theo yêu cầu chủ dự án): chọn hướng **sửa phía app**, không đụng firmware.
- **T3 / W3.1 — DONE**: tài liệu hướng dẫn học sinh + cập nhật README.

## 4. Kiểm chứng đã chạy (Hermes tự chạy, không dùng model)

| Phép kiểm | Kết quả |
|-----------|---------|
| `parity_check.py` (44 trường JSON, 14 chuỗi lệnh BLE, UUID, ngưỡng, mã lỗi, giá trị trạng thái, chân I2C 9/8/6/7, SSID/pass) | **9/9 PASS** (trước và sau W2.3) |
| `verify_segment.py` (đuôi chưa xử lý nguyên xi byte-for-byte) | PASS cả 3 mốc (mục 7, 11, 14) |
| `verify_skeleton.py` (bỏ định danh, so số/toán tử/mã định dạng) | 1308–1757: **0 dòng lệch**; 1–1307: 5 dòng lệch = 5 dòng tiêu đề tệp |
| Đối chiếu dấu câu sau khi bỏ bình luận khối + chuỗi | **KHỚP HOÀN TOÀN** với bản chính thức |
| Đếm dòng code thiếu comment | **0 dòng** (toàn tệp, kể cả 62 dòng mới) |
| `arduino-cli compile` (fqbn `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc`) | ĐẠT 3 lần: 1.161.733 B (88%) → sau dọn comment vẫn 1.161.733 B → sau W2.3: 1.163.149 B (88%), RAM 48.916 B (14%) |
| `./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug` | **BUILD SUCCESSFUL** cả ba; lint 53 vấn đề đều là vấn đề cũ ngoài vùng sửa (không phát sinh mới) |
| Rà rò rỉ mật khẩu WiFi trong script và hồ sơ | Không tệp nào chứa literal mật khẩu (so bằng giá trị đọc từ mã, không nhúng vào script) |
| Review chỉ đọc thật sự | 6/6 hash tệp vẫn `OK` sau khi reviewer chạy |

## 5. Dữ kiện tài liệu học sinh đã được đối chiếu với mã

UUID `7d2a0001`–`7d2a0006` khớp; chân `GPIO 8/9` (bus 0) và `GPIO 7/6` (bus 1) khớp; địa chỉ `0x68/0x69` và `0x76/0x77` khớp; thuộc tính đặc trưng khớp (0002 Notify, 0003 Indicate, 0004 Read+Notify, 0005 Write, 0006 Notify); ngưỡng `giaTocVaChamMs2 = 25.0f`, `nguongDungYenMs2 = 9.81f`, `cuaSoSauVaChamMs = 3000`, `minimumStillnessSamples = 6`, `maximumSampleGapMs = 250` khớp; tần số luồng BLE ~25 Hz khớp (chia 4 ở 100 Hz, chia 2 ở 50 Hz); fqbn trong hướng dẫn khớp.

## 6. VIỆC CÒN THIẾU (không được coi là xong)

1. **W4.1 — kiểm chứng trên phần cứng: CHƯA LÀM.** Cần ESP32 cắm vào Mac (hiện đã rút để cắm điện thoại; cổng `/dev/cu.usbmodem1101` không còn tồn tại). Phải làm: nạp bản học sinh → quét BLE (`ble_scan.py`) → bấm `m`/`p`/`d` trên Serial Monitor và lưu log → mời chủ dự án xác nhận trên app.
2. **Bằng chứng phần cứng cho lỗi Telemetry đã sửa: CHƯA CÓ.** Bốn cửa sổ bắt log serial (150/180/300/240 giây) đều không ghi được lệnh BLE nào của app; hai cửa sổ đầu còn hỏng do lỗi công cụ. Kết luận hiện dựa trên **bằng chứng tĩnh hai phía** + build/lint, KHÔNG phải log phần cứng.
3. **W5.1 — review độc lập: DỪNG GIỮA.** Reviewer `claude-opus-4-6-thinking` đã chạy được một phần (nó tự tách 4 auditor song song) rồi bị dừng theo yêu cầu để tiết kiệm token. Phần kết quả dở lưu ở `/Users/phananh/.hermes/cache/scratch/w51-review-dung-giua.chung-cu.log`.
   - Phát hiện sớm của reviewer: *"Serial blocking ~26ms can exceed 10ms/100Hz sampling window; state bounce could compound the issue. STATE mapping is complete and correct."*
   - Hermes đo lại độc lập: mỗi lệnh in riêng lẻ chỉ 15–92 ký tự nên **lọt trong bộ đệm TX 256 byte** của Arduino-ESP32; nhịp tim thêm ~126 ký tự mỗi 5 giây (~26 ký tự/giây) so với tải sẵn có ~440 ký tự/giây (~4 Hz) tức khoảng **6%**. Con số ~26 ms chỉ đúng khi bộ đệm TX đã đầy (ví dụ Serial Monitor bị tạm dừng). **Chưa có kết luận chung thức** — cần chạy nốt review hoặc đo bằng thực nghiệm trên board.
4. **Chưa cập nhật `docs/api-contract.md`** về việc firmware chưa hỗ trợ ghi profile; hiện chỉ ghi trong báo cáo điều tra và README.
5. **Chưa xử lý 2 khuyết điểm phía app** tìm thấy ở W1.2 (thiếu `neverForLocation` trong AndroidManifest, và `startScan` gọi trước khi có quyền ở `BleTestScreen.kt:181-194` + callback rỗng ở `MainActivity.kt:53-55`) — chủ dự án chưa yêu cầu sửa.
6. **53 vấn đề lint cũ** (8 Error, 4 Fatal) trong app vẫn còn nguyên — không thuộc phạm vi đã sửa.

## 7. Lệnh để chạy tiếp (đã kiểm chứng chạy được)

```bash
# Biên dịch / nạp bản học sinh
ACLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
"$ACLI" compile --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
  /Users/phananh/TEMP/nckh27pa/nckh27pa/esp-s3-for-student
"$ACLI" upload  --fqbn esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc \
  -p /dev/cu.usbmodem1101 /Users/phananh/TEMP/nckh27pa/nckh27pa/esp-s3-for-student

# Đọc serial KHÔNG đẩy chip vào chế độ DOWNLOAD (xem mục 9)
/Users/phananh/.hermes/cache/scratch/ble-venv/bin/python \
  /Users/phananh/.hermes/cache/scratch/serial_capture.py /dev/cu.usbmodem1101 120

# Quét BLE từ Mac
/Users/phananh/.hermes/cache/scratch/ble-venv/bin/python \
  /Users/phananh/.hermes/cache/scratch/ble_scan.py 25

# Kiểm hợp đồng / xương cá / đối chiếu tên
python3.11 /Users/phananh/.hermes/cache/scratch/parity_check.py \
  esp-s3/esp-s3.ino esp-s3-for-student/esp-s3-for-student.ino
python3.11 /Users/phananh/.hermes/cache/scratch/verify_skeleton.py \
  esp-s3/esp-s3.ino esp-s3-for-student/esp-s3-for-student.ino 1 1819

# Build + cài app
cd android && export JAVA_HOME=$(/usr/libexec/java_home -v 17) \
  && ./gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
$HOME/Library/Android/sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk

# Chạy nốt review độc lập
agy --print "$(cat /Users/phananh/.hermes/cache/scratch/w51-review-prompt.md)" \
  --model claude-opus-4-6-thinking --mode plan --dangerously-skip-permissions \
  --add-dir /Users/phananh/TEMP/nckh27pa/nckh27pa > /Users/phananh/.hermes/cache/scratch/w51-review.log 2>&1
```

## 8. Nhật ký worker (kể cả lần hỏng) — tất cả trong `/Users/phananh/.hermes/cache/scratch/`

| Tệp log | Nội dung |
|---------|----------|
| `w21-run.log` | CommandCode `zai-org/GLM-5.1` HỎNG: `EXIT=6 / unable to connect to the API` (lỗi mạng, không phải model treo) |
| `w21-run2.log` | CommandCode `stepfun/Step-3.5-Flash` — sinh bảng đối chiếu tên |
| `w12-agy-*` | AGY `gemini-3.8-flash-high` — điều tra app read-only |
| `w22-seg1.log`, `w22-seg1b.log` | Codex đoạn 1, gồm sự cố Hermes chạy trùng 2 tiến trình trên cùng tệp |
| `w22-seg2.log`, `w22-seg3.log`, `w22-seg4.log` | Codex đoạn 2/3/4 |
| `w7-seg1.log`, `w7-seg2.log` | Dọn comment rập khuôn đoạn 1/2 |
| `w6-app-fix.log` | Sửa app Telemetry/Profile |
| `w23.log` | W2.3 MENU/tiến trình/nhịp tim |
| `w31-doc-lan1-hong.log` | W3.1 HỎNG lần 1: AGY headless tự từ chối quyền công cụ, **không tạo tệp nào dù exit 0** |
| `w31-doc.log` | W3.1 lần 2 THÀNH CÔNG |
| `w51-review-dung-giua.chung-cu.log` | Review W5.1 chạy dở rồi bị dừng |
| `compile-student*.log` | 3 lần biên dịch ĐẠT |
| `android-build-baseline.log`, `android-build-after-fix.log`, `android-lint-after-fix.log` | Build/test/lint app |
| `esf-truoc-w23.ino` | Bản sao lưu trước W2.3 (sha256 `413c2886…`) dùng để chứng minh chỉ thêm dòng |
| `review-before.sha` | Hash 6 tệp trước review, dùng để chứng minh review chỉ đọc |

## 9. Bài học vận hành (đã kiểm chứng, dùng lại được)

1. **Mở cổng serial bằng pyserial với DTR/RTS mặc định sẽ đẩy ESP32-S3 vào chế độ DOWNLOAD** (`rst:0x15`, `boot:0x23`, "waiting for download") — firmware ngừng chạy, BLE ngừng quảng cáo. Đây chính là trạng thái "không dò được thiết bị". Cách đúng: gán `dtr=False`, `rts=False` TRƯỚC khi `open()`. Hệ quả: mỗi lần mở cổng, chip reset ⇒ phải mở cổng TRƯỚC rồi mới kết nối app.
2. **AGY ở chế độ headless cần `--dangerously-skip-permissions`** (hoặc allow-rule trong `permissions.allow`), nếu không nó tự từ chối công cụ và **tạo ra 0 tệp dù exit 0**. Đã lưu thành skill `antigravity-cli`.
3. **Codex CLI không nhận `apply_patch` gộp nhiều hunk trên cùng một tệp** — mỗi lần gọi chỉ một vùng liên tục, chia nhỏ theo đoạn.
4. **Đừng tin "exit 0" hay báo cáo của worker** — kiểm chứng bằng script đọc tệp (hash, diff, đếm) thay vì đọc mắt.
5. **Chốt chặn chống chạy trùng**: `pgrep -f 'codex exec'` trước mỗi lần khởi động worker (đã từng có 2 tiến trình cùng sửa một tệp).
6. Khi script kiểm tra báo FAIL, phải xét khả năng **script sai** trước (đã gặp 2 lần: tìm định danh cũ sau khi đã Việt hóa; và đếm nhầm thẻ XML của lint).

## 10. Trạng thái run trong CMD

`run_20260925_012246_7b27e1` (prompt đã chọn = grilled, plan đã duyệt). Đã chốt DONE: W1.1, W1.2, W2.1, W2.2, W2.3, W3.1. Còn PENDING: **W4.1** (kiểm chứng phần cứng) và **W5.1** (review độc lập, đã chạy dở rồi dừng).
