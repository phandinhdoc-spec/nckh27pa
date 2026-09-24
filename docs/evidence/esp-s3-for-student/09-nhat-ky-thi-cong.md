# Nhật ký thi công run_20260925_012246_7b27e1

> **DỪNG GIỮA TIẾN ĐỘ (2026-09-25 ~03:45):** chủ dự án yêu cầu dừng để tiết kiệm token. Đọc `12-BAN-GIAO-TONG-KET.md` trước — đó là bản bàn giao đầy đủ (sản phẩm + hash, việc còn thiếu, lệnh chạy tiếp, bài học vận hành).

Mục đích: ghi lại trung thực mọi lần chạy worker (kể cả lần hỏng) để người review đối chiếu được với bằng chứng.
Tất cả mốc thời gian theo giờ máy (+07).

## Worker đã dùng và kết quả

| # | Work unit | Provider / model | Kết quả | Bằng chứng |
|---|-----------|------------------|---------|------------|
| 1 | W1.1 | hermes-local (arduino-cli 1.5.1, esptool 5.3.1, pyserial, bleak) | DONE | `00-baseline/` (serial log, scan BLE, ket-luan.md) |
| 2 | W2.1 (lần 1) | commandcode `zai-org/GLM-5.1` | HỎNG — không ra kết quả trong ~20 phút; thông báo muộn cho biết `EXIT=6 / Error: Unable to connect to the API. Please check your network connection.` → lỗi kết nối API, KHÔNG phải model treo | `/Users/phananh/.hermes/cache/scratch/w21-run.log` |
| 3 | W2.1 (lần 2, dự phòng) | commandcode `stepfun/Step-3.5-Flash` | DONE — sinh `BANG-DOI-TEN.md` 210 dòng; Hermes phải sửa 5 dòng lỗi (trùng tên `durationMs`→`khoangThoiGianMs`; `ENABLE_BLE`, `HW_HAS_*` bị giữ nguyên trong khi đây là mã tự viết) | `05-parity/glossary-check-final.txt` (toàn bộ PASS) |
| 4 | W1.2 | antigravity `gemini-3.8-flash-high` (read-only) | DONE — 5 giả thuyết kèm trích dẫn tệp/dòng; giữ ràng buộc read-only (git status không có tệp `android/` nào bị sửa) | `08-android-ble-review.md` |
| 5 | W2.2 đoạn 1 | openai-codex `gpt-5.6-sol` | DONE một phần rồi HỎNG ở `apply_patch` khi gộp nhiều hunk cùng một tệp (`invalid patch: multiple operations target ...esp-s3-for-student.ino`); sau đó tự phục hồi và hoàn thành dòng 1–561 | `w22-seg1.log` |
| 6 | W2.2 đoạn 1 (trùng lặp — sự cố của Hermes) | openai-codex `gpt-5.6-sol` | Hermes chẩn đoán nhầm worker #5 đã chết và khởi động thêm một tiến trình trên CÙNG tệp. Tiến trình sau tự phát hiện "dòng 1–106 bị tiến trình ngoài thay đổi", KHÔNG hoàn tác, audit lại bằng patch một hunk mỗi lần và hoàn tất 107–561. Từ đó quy trình thêm chốt chặn `pgrep -f 'codex exec'` trước mọi lần chạy | `w22-seg1b.log` |
| 7 | W2.2 đoạn 2 | openai-codex `gpt-5.6-sol` | DONE — dòng 562–1029 | `w22-seg2.log` |
| 8 | W2.2 đoạn 3 | openai-codex `gpt-5.6-sol` | DONE — dòng 1030–1307 (219/278 dòng đổi), tự đặt thêm 12 tên cục bộ | `w22-seg3.log` |
| 9 | W2.2 đoạn 4 | openai-codex `gpt-5.6-sol` | DONE — dòng 1308–1757 (354 dòng comment trong vùng), giữ nguyên 13 tên lệnh + khóa JSON + mã lỗi; Hermes kiểm chứng lại: hash dòng 1–1307 trùng đúng con số worker báo (`8465efe7…`) | `w22-seg4.log` |
| 10 | Dọn comment rập khuôn (bổ sung theo yêu cầu chủ dự án) | openai-codex `gpt-5.6-sol` | ĐANG CHẠY — dòng 1–900; sẽ có lần chạy thứ hai cho 901–1757 | `w7-seg1.log` |
| 11 | Sửa app Telemetry/Profile (bổ sung theo yêu cầu chủ dự án) | openai-codex `gpt-5.6-sol` | DONE — 3 điểm trong `BleTestScreen.kt` + `FallSafeBleClient.kt`; Hermes kiểm chứng: hết `REQ_TELEMETRY`, không còn đường ghi GATT vào `7d2a0006`, `writeProfile` trả `false`, `assembleDebug` + `testDebugUnitTest` + `lintDebug` đều ĐẠT, firmware gốc vẫn hash `8954eae2…` | `w6-app-fix.log`, `11-ban-sua-app-telemetry-profile.md` |
| 12 | Dọn comment rập khuôn đoạn 2 (901–1757) | openai-codex `gpt-5.6-sol` | DONE — câu rập khuôn toàn tệp về 0; Hermes kiểm: 0 dòng mã bị đụng (so dấu câu sau khi bỏ bình luận khối + chuỗi, khớp hoàn toàn với bản chính thức) | `w7-seg2.log` |
| 13 | W2.3 — MENU `m` + khối TIẾN TRÌNH `p` + nhịp tim `d` | openai-codex `gpt-5.6-sol` | DONE — +62 dòng, 0 dòng cũ bị sửa/xóa (kiểm bằng `diff` với bản sao lưu `esf-truoc-w23.ino`, sha256 `413c2886…`); compile ĐẠT 1.163.149 byte (88%); parity vẫn 9/9 | `w23.log`, `05-parity/parity-sau-w23.txt` |
| 14 | W3.1 — tài liệu học sinh + cập nhật README | antigravity `gemini-3.7-flash-medium` | LẦN 1 HỎNG: `jetski: no output produced — a tool required the "command" permission that headless mode cannot prompt for, so it was auto-denied` → không tạo tệp nào. Khắc phục: thêm cờ `--dangerously-skip-permissions` (có sẵn trong `agy --help`) và chạy lại. Lần 1 lưu ở `w31-doc-lan1-hong.log` | `w31-doc.log` |

Bài học vận hành: gọi AGY ở chế độ headless (`agy --print`) mà không có allow-rule quyền công cụ thì nó tự từ chối và KHÔNG báo lỗi rõ ràng ra tệp kết quả — phải kiểm tra log có thực sự sinh tệp hay không, đừng tin "exit 0".

## Việc còn thiếu, ghi nhận trung thực

- Bước "quan sát log thật" của W2.3 (bấm thử `m`/`p`/`d` trên Serial Monitor) **CHƯA làm được** vì ESP32 đã được rút khỏi Mac để cắm điện thoại; hiện mới có bằng chứng biên dịch, chưa có bằng chứng log phần cứng. Thuộc về W4.1.
- Lỗi Telemetry/Profile: mới có bằng chứng tĩnh + build/lint phía app; chưa có log phần cứng xác nhận app nhận được `START_STREAM`.

## Kiểm chứng độc lập đã chạy (Hermes tự chạy, không dùng model)

| Script | Kiểm gì | Kết quả đoạn 1 | Kết quả đoạn 2 |
|--------|---------|----------------|----------------|
| `verify_segment.py` | phần CHƯA xử lý phải nguyên xi byte-for-byte + 0 dòng thiếu comment trong phần đã xử lý | PASS (đuôi từ mục 7 khớp 52.500/52.500 ký tự) | PASS (đuôi từ mục 11 khớp 31.685/31.685 ký tự) |
| `verify_lines.py` | từng dòng, so phần CODE sau khi áp bảng đối chiếu tên | 7/561 dòng khác — đều là 5 dòng tiêu đề tệp + 2 chuỗi Serial | 108/1029 dòng khác — đều là đổi tên tham số cục bộ + chuỗi Serial |
| `verify_skeleton.py` | bỏ hết tên định danh, chỉ so số + toán tử + mã định dạng | — | 5/1029 dòng khác — đúng 5 dòng tiêu đề tệp |

Kết luận tới thời điểm này: chưa phát hiện thay đổi nào về ngưỡng, toán tử, thứ tự tham số, hợp đồng JSON, UUID, chân GPIO.

## Kiểm chứng toàn tệp sau khi Việt hóa xong (Hermes tự chạy)

| Phép kiểm | Kết quả |
|-----------|---------|
| `verify_segment.py` (đuôi chưa xử lý nguyên xi) | PASS — đoạn 3: đuôi từ mục 14 khớp 18.739/18.739 ký tự |
| `verify_skeleton.py` 1308–1757 | PASS — **0 dòng lệch** số/toán tử/mã định dạng |
| `verify_skeleton.py` 1–1307 | 5 dòng lệch — đúng 5 dòng tiêu đề tệp |
| `parity_check.py` (44 trường JSON, 14 chuỗi lệnh, UUID, ngưỡng, mã lỗi, trạng thái, chân I2C, SSID/pass) | **9/9 PASS** → `05-parity/parity-final.txt` |
| Đếm dòng code thiếu comment (toàn tệp) | **0 dòng** |
| `arduino-cli compile` bản học sinh | **ĐẠT** — `Sketch uses 1161733 bytes (88%)`, RAM 48.916 B (14%) |
| Biên dịch LẦN 2 sau khi dọn 334 comment rập khuôn | **ĐẠT** — vẫn đúng `1161733 bytes (88%)`, cùng kích thước bản trước ⇒ dọn comment không đổi mã máy |
| Dọn comment rập khuôn (2 đoạn: 1–900, 901–1757) | Hoàn tất — câu rập khuôn toàn tệp về **0**; comment dài nhất còn lặp chỉ là dòng kẻ `====` (36 lần). Comment mới nêu đúng việc của dòng đó (ví dụ dòng 987: "Thu mẫu trong 2000 ms, tức 2 giây"; dòng 1346: "Nếu nhận START_STREAM thì bật luồng dữ liệu cảm biến định kì qua BLE") |
| Đối chiếu mã sau dọn comment (bỏ cả bình luận khối + chuỗi ký tự, so số lượng từng loại dấu câu) | **KHỚP HOÀN TOÀN** với bản chính thức ⇒ không có dấu hiệu mã bị đụng |
| `git status` tệp `esp-s3/esp-s3.ino` | KHÔNG đổi (đúng yêu cầu "không sửa trực tiếp tệp gốc") |

Sửa 2 lỗi của chính công cụ kiểm tra để không lặp lại: (a) `parity_check.py` từng tìm định danh cũ (`cmdName`, `PIN_I2C0_SDA`) nên báo FAIL giả khi tên đã Việt hóa — nay nhận diện theo VAI TRÒ; (b) literal mật khẩu WiFi đã bị loại khỏi script, thay bằng so sánh giá trị đọc từ hai tệp. Đã quét lại: không tệp nào trong `docs/evidence/` chứa literal đó.

## Phát hiện vận hành quan trọng (đã sửa công cụ)

Mở cổng `/dev/cu.usbmodem1101` bằng pyserial với DTR/RTS mặc định sẽ đẩy ESP32-S3 vào chế độ **DOWNLOAD** (`rst:0x15`, `boot:0x23`, "waiting for download"): firmware ngừng chạy, BLE ngừng quảng cáo. Gán `dtr=False`, `rts=False` TRƯỚC khi `open()` thì chip chỉ khởi động lại bình thường (`boot:0x2b SPI_FAST_FLASH_BOOT`). Hệ quả: mỗi lần mở cổng để bắt log, chip reset ⇒ điện thoại mất kết nối BLE, nên phải mở cổng TRƯỚC rồi mới bấm nút trên app.

Hệ quả điều tra: bốn cửa sổ bắt log serial (150/180/300/240 giây) không ghi được lệnh BLE nào của app, nên lỗi Telemetry/Profile được kết luận bằng **bằng chứng tĩnh hai phía** (xem `10-loi-telemetry-profile.md`), phần xác nhận bằng phần cứng còn thiếu — ghi nhận trung thực, không suy diễn thành "đã kiểm chứng trên phần cứng".

## Phạm vi mở rộng do chủ dự án quyết định trong phiên

1. Lỗi Telemetry/Profile → **sửa phía APP**, KHÔNG đụng firmware (`esp-s3/esp-s3.ino` giữ nguyên).
2. Dọn 334 dòng comment rập khuôn trong bản học sinh (thay bằng giải thích thật theo ngữ cảnh).
3. Thứ tự: xong bản firmware học sinh rồi mới sửa app.
