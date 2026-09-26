# CORE-001 — lõi Kotlin ngoại tuyến

Lõi thử nghiệm, không Android dependency. Chỉ đọc nguồn Android §4.4, §7, §9 và phiếu CORE-001. Không thuật toán phát hiện ngã: bên tích hợp cung cấp `suspected()` và `evidenceConfirmed()`.

Chạy từ root repo:

```sh
./android/core/run-tests.sh
./android/core/run-tests.sh core.DemoKt
```

Runner dùng Kotlin 2.3.10 tại `/home/pdd/.local/opt/nckh27pa/android-studio/plugins/Kotlin/kotlinc/bin/kotlinc` và `JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64`. Compile jar local, chạy Java thật, dọn thư mục build tạm; không Gradle/network/thư viện tải thêm. Đường dẫn công cụ là điều kiện môi trường, không tự cài nếu thiếu.

## Hợp đồng tích hợp

- Tạo và gọi mọi API core/sink trên cùng một thread, kể cả snapshot. Sai thread ném `IllegalStateException` trước khi sửa state. Host chịu trách nhiệm serialize callback lên thread đó; đây không phải class đa luồng có lock.
- Clock cung cấp elapsed milliseconds không giảm, không lấy từ civil/wall time. Khoảng elapsed phải nằm trong miền Long không âm. Fake clock được dùng trong test/demo; chưa có adapter clock Android.
- `evidenceConfirmed()` chỉ bắt đầu countdown từ SUSPECTED, mặc định 10.000 ms. Callback lặp trong sự kiện không khởi động lại hạn. `riskCleared()` chỉ hủy SUSPECTED.
- Host phải gọi `tick()` tại hạn hoặc ngay khi tiếp tục chạy. Snapshot chỉ phục vụ hiển thị, không phát side effect. Không có scheduler/thread nền: “tự gửi” nghĩa là tick đến hạn gửi mà không cần phản hồi người dùng; không bảo đảm chạy đúng giờ khi host ngủ hoặc process chết.
- SAFE trước hạn hủy; SAFE tại/sau hạn xử lý timeout trước nên không xóa cảnh báo. NEED_HELP/SOS gửi ngay từ MONITORING/SUSPECTED/VERIFYING. Trong ALERTING/AWAITING_HELP, SOS tiếp theo bị bỏ qua để tránh spam.
- Sink đồng bộ: return thành công nghĩa là sink chấp nhận (SENT), không chứng minh người nhận thật đã nhận. Exception thành FAILED, giữ ALERTING, không tự retry vì transport có thể đã nhận trước khi ném lỗi. Fatal JVM Error không được xử lý như lỗi transport thông thường.
- State ALERTING/SENDING và cờ gửi được đặt trước khi gọi sink. Sink gọi ngược SOS/complete không gửi trùng hoặc đóng sự kiện đang gửi. `complete()` sau SENT hoặc FAILED quay MONITORING/ACKNOWLEDGED; SOS mới có ID mới. ID chỉ tăng trong vòng đời instance, không bền qua restart. ACKNOWLEDGED biểu thị host kết thúc sự kiện, không phải biên nhận transport.
- Core hiện luôn gửi thông báo chưa xác định vị trí, không chờ GPS. Chưa chọn/lấy vị trí thật hoặc dựng payload liên hệ đầy đủ trong §7. Không có raw phone/contact/location trong event log. `RecordedEvent` chỉ chứa ID/state/status; log bỏ bản cũ khi đầy (mặc định 32). `RecordingSink` cũng hữu hạn, trả bản sao danh sách.
- Snapshot giữ response/status cuối sau complete, lần suspected/SOS mới bắt đầu sự kiện mới. Các callback phải thuộc sự kiện hiện tại; chưa có token lọc callback cũ xuyên qua complete.

## Bằng chứng và bàn giao

`evidence/01..07-{red,green}.log` là output chạy thật theo các nhóm hành vi: chuyển trạng thái/callback, deadline/thiếu vị trí, SAFE boundary, HELP/SOS/complete, lỗi/reentrancy, config, bounded recording sink. RED 07 là compile failure vì RecordingSink chưa có. RED 05 phát hiện đệ quy gửi lại gây StackOverflow trước khi thêm guard. Hai script `tdd-*.py` lưu các bước lịch sử, **không chạy lại trên source cuối**.

`08-contract-checks.log` kiểm tra wall time và owner thread bổ sung, chỉ GREEN: guard thread và clock injection đã có từ scaffold, vì vậy không tuyên bố TDD RED riêng cho mọi điều kiện con. Bounded event log cũng đã có trong scaffold; vòng 07 thêm TDD cho RecordingSink và kiểm tra cả hai bộ nhớ. Đây là giới hạn so với yêu cầu nghiêm ngặt “RED/GREEN từng hành vi”.

`final-tests.log` và `demo.log` là lần chạy bản cuối. Không dùng agent con, không nới sandbox, không commit. Hermes đã chạy lại 9 nhóm test và demo exit0 (docs/evidence/core-tests-hermes.log, core-demo-hermes.log). Reviewer độc lập REV-CORE-001 kết luận PASS trong phạm vi core offline, không phát hiện lỗi chặn (docs/evidence/REV-CORE-001.json). Chưa tích hợp Android; thiếu sót TDD nêu trên vẫn chưa được đóng. Các probe bổ sung reviewer báo đã chạy chưa phải regression test lưu trong repo. Chưa xác minh SUR-001 trong phiên này vì giới hạn chỉ đọc các mục được dẫn.

Không coi CORE này là hoàn thành AND-001/APK. Chưa giải quyết sensor, UI, background, persistence, Android permissions hay real transport.

## CORE-002 — regression bổ sung sau triển khai

Đã lưu thêm 3 nhóm regression trong `tests/CoreTests.kt`: sink reentrant trả về thành công (giữ ALERTING/SENDING trong callback, không gửi trùng hoặc complete sớm, sau return thành AWAITING_HELP/SENT và cho phép sự kiện mới sau complete); deadline tại `Long.MAX_VALUE` với SAFE trước/tại hạn; timeout `Long.MAX_VALUE` với clock bắt đầu ở 0 và 1. Trường hợp bắt đầu ở 1 vẫn còn 1 ms khi clock đạt cực đại, không được gửi sớm do cộng deadline tràn số. Các clock đều không âm, không giảm, không wrap.

Đây là **kiểm thử bổ sung sau triển khai**, chạy PASS ngay trên source hiện có; không phải test-first, không tạo hay tuyên bố RED/GREEN hồi tố. Không phát hiện bug trong các trường hợp này nên giữ nguyên `src/`. Thiếu sót TDD lịch sử của CORE-001 vẫn còn; không đánh dấu CORE-001 đạt đầy đủ.

Kiểm chứng CORE-002 chạy thật: [suite](evidence/core-002-tests.log) PASS 12 nhóm, exit 0; [demo](evidence/core-002-demo.log) exit 0, chỉ fake recipient. Cả hai log gộp stdout/stderr, không có cảnh báo compiler/runtime và không phát cảnh báo qua transport thật. Giữ nguyên log lịch sử. Xem [biên bản](evidence/core-002-report.md).

Review độc lập REV-CORE-001 trước thay đổi đã PASS core offline, không có lỗi chặn. Phiên CORE-002 này tự rà soát các test bổ sung, chưa có review độc lập mới; không dùng kết quả cũ để tuyên bố review độc lập cho thay đổi mới. Không agent con, network, Gradle/emulator hay commit; không sửa Android ngoài core đang do Hermes xử lý.
