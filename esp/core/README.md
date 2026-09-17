# ESP local alert core (ESP-CORE-001)

Thư viện C++17 thuần, không phụ thuộc Arduino/ESP-IDF, GPIO, transport hoặc heap
trong state machine. Chưa build Arduino hoặc kiểm tra bo thật. Không phải thuật
toán phát hiện ngã: caller quyết định dấu hiệu/nguy cơ, debounce/giữ nút SOS,
và thời điểm bắt đầu xác minh.

## Chạy local

```sh
esp/core/run-tests.sh
esp/core/run-tests.sh --sanitize
esp/core/run-demo.sh
```

Nếu môi trường ptrace không hỗ trợ LeakSanitizer như phiên này:

```sh
ASAN_OPTIONS=detect_leaks=0 esp/core/run-tests.sh --sanitize
```

Runner dùng g++ với `-std=c++17 -Wall -Wextra -Werror -pedantic`.
Demo dùng clock giả lập 0–11000 ms, không chờ thực 10 giây. Countdown 10 giây
chỉ là cấu hình thử. Output và log thật nằm trong `evidence/`.

## API và quy tắc

Include `local_alert.h`, biên dịch cùng `local_alert.cpp`. Một caller tuần tự gọi
`handle(Input)` và đọc snapshot qua getters; không thread-safe. Mọi `now` là
`uint64_t` monotonic milliseconds cùng một epoch, không phải Unix time. Caller
phải gọi Tick định kỳ; thư viện không có thread/timer tự chạy. Mọi input cũng
kiểm tra timeout trước khi xử lý lệnh. Clock đi lùi bị từ chối không đổi state.
Không hỗ trợ wrap clock; countdown làm tràn phép cộng bị từ chối.

- `Suspect`: giữ ID, phát action Publish, chuyển SUSPECTED. Chưa đủ nguy cơ nên
  chưa có timeout; caller dùng Countdown khi cần xác minh hoặc Sos khi khẩn.
- `Countdown`: tạo hoặc nâng sự kiện sang VERIFYING, bật âm, đặt deadline.
  Thời lượng hợp lệ 1–60000 ms (giới hạn nội bộ thử nghiệm). Cùng ID chỉ được
  rút ngắn deadline; lặp không bật lại âm đã STOP. ID khác bị Busy.
- `Sos`: báo ngay, kể cả DEGRADED. Nếu đang có sự kiện, nâng khẩn sự kiện đó và
  giữ ID gốc, không tạo sự kiện thứ hai. SOS khi đã cảnh báo là no-op.
- `Tick`: tới hoặc qua deadline chuyển LOCAL_ALERTING, bật lại âm, đánh dấu
  alerted và yêu cầu Publish một lần.
- `AckEvent`: ghi nhận Android đã nhận, không tắt âm hoặc hủy timeout.
- `StopBuzzer`: chỉ tắt âm. Timeout còn chờ vẫn phát cảnh báo và bật âm lại.
- `CancelAlert`/`Safe`: chỉ kết thúc active đúng ID, tắt âm, giữ bản ghi kết thúc.
  SAFE đến đúng/sau deadline vẫn giữ `alerted=true` cùng `safe=true`; không xóa
  dấu vết cảnh báo. Lệnh cho ID kết thúc gần nhất là no-op.
- `SensorError`/`SensorRecovered`: cờ sức khỏe độc lập; chỉ đổi trạng thái sang
  DEGRADED/MONITORING khi không có active. Không xóa sự kiện hoặc deadline.
- `Connected`/`Disconnected`: chỉ đổi cờ kết nối, không reset countdown.

ID dài 1–32 byte, không chứa NUL; được copy vào buffer 33 byte. Một active và
một record kết thúc gần nhất, không tăng bộ nhớ theo số lệnh. ID kết thúc gần
nhất không được tái sử dụng; lịch sử khử trùng cũ hơn nằm ngoài phạm vi. Caller
cấp ID mới cho sự kiện/SOS mới. Không có queue bền vững, journal flash hoặc
khôi phục qua reboot; adapter tương lai phải lưu các cập nhật nếu cần lịch sử
lâu dài. Record kết thúc cũ bị thay khi sự kiện kế tiếp kết thúc.

`Result.status` mô tả kết quả input; `actions` là bitmask yêu cầu caller xử lý:
Publish=1, StartBuzzer=2, StopBuzzer=4, RecordUpdated=8, SensorChanged=16.
Actions vẫn có thể xuất hiện cùng status từ chối lệnh vì timeout vừa hết.
Nếu cùng một lần gọi có cả StartBuzzer và StopBuzzer (SAFE đúng deadline),
caller dùng getter `buzzer()` làm trạng thái âm cuối cùng; lưu snapshot `last()`
để giữ cả dấu vết cảnh báo và kết thúc. Publish là yêu cầu xuất bản snapshot,
không có nghĩa transport đã gửi hoặc người thân đã nhận.

Caller phải xác thực/ủy quyền lệnh trước khi gọi; thư viện không giả xác thực
BLE. API enum/struct là nội bộ, không bổ sung wire fields hoặc tuyên bố tương
thích decoder Android. Không xử lý JSON, commandId cache, pairing hoặc bonding.

## Bằng chứng TDD

`tests/local_alert_test.cpp` được viết trước phần triển khai. Lần RED biên dịch
thành công với handle chỉ trả Ignored: cả 11 nhóm fail bằng assertion (exit 1),
không phải lỗi build. Sau đó thay stub bằng logic; cùng bộ test không sửa chạy
GREEN (exit 0). Các nhóm gồm SUSPECTED/one active, no-response, SOS, disconnect,
duplicate/deadline, cancel/wrong ID, ACK/STOP, monotonic near max, invalid
countdown/ID, sensor recovery và SAFE late. Static assertions kiểm tra object
trivially copyable và kích thước tối đa 160 byte.

- `evidence/tdd-red.log`: 11/11 fail trước triển khai.
- `evidence/tdd-green.log`: 11/11 pass.
- `evidence/sanitizer.log`: lần đầu exit 1, LeakSanitizer không hỗ trợ ptrace.
- `evidence/sanitizer-retry.log`: ASan/UBSan pass, exit 0, tắt riêng leak check.
- `evidence/demo.log`: demo exit 0, giữ record alerted + safe sau mất BLE.

Hermes đã chạy lại suite và sanitizer host không tắt leak check, đều exit0. Reviewer độc lập Codex PASS static;2 đề xuất action-mask coverage đã bổ sung như regression sau triển khai, suite11 nhóm+sanitizer+demo chạy lại PASS (docs/evidence/ESP-CORE-final-regression.log). Không thay implementation.
