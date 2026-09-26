# CORE-002 — bằng chứng thực hiện

Phạm vi đọc: phiếu CORE-002, phiếu và kết quả REV-CORE-001, nội dung android/core. Phạm vi ghi: tests/CoreTests.kt, README.md và evidence/core-002-* trong android/core. Không sửa source hoặc runner, không sửa phiếu/trạng thái ngoài core.

## Regression bổ sung sau triển khai

- Successful reentrant sink: lưu snapshot trước/sau callback gọi ngược; kiểm tra bên ngoài sink rằng sự kiện giữ ALERTING/SENDING và ID/response, không complete hoặc gửi trùng. Sink return thành công phải thành AWAITING_HELP/SENT; complete sau return cho phép SOS mới với ID khác.
- Clock sát Long.MAX_VALUE: còn 1 ms trước hạn, snapshot tại hạn bằng 0 và không tự gửi; SAFE trước hạn hủy, SAFE tại hạn gửi NO_RESPONSE đúng một lần.
- Timeout Long.MAX_VALUE: bắt đầu 0 gửi đúng tại cực đại; bắt đầu 1 vẫn còn 1 ms tại cực đại, không gửi sớm. Không sử dụng clock âm, giảm hoặc elapsed ngoài miền hợp lệ.

Các test này được viết sau source và PASS lần chạy đầu. Không có RED tái hiện bug; không thay source để dựng quy trình TDD. Các log RED/GREEN trước đây được giữ nguyên. Thiếu TDD lịch sử chưa được giải quyết, CORE-001 chưa đạt đầy đủ yêu cầu TDD nghiêm ngặt.

## Chạy thật

- `./android/core/run-tests.sh`: PASS 12 nhóm, EXIT 0; stdout/stderr trong core-002-tests.log.
- `./android/core/run-tests.sh core.DemoKt`: EXIT 0, fake recipient, kết thúc AWAITING_HELP: SENT; stdout/stderr trong core-002-demo.log.
- Mỗi lệnh chạy một lần; không cảnh báo compiler/runtime trong output. Không dùng transport thật, network, Gradle hoặc emulator.

## Rà soát và giới hạn bàn giao

REV-CORE-001.json đã PASS và không báo lỗi chặn cho core trước thay đổi. Tự rà soát bổ sung: assertions reentrancy ở ngoài sink để exception do assertion không bị nhầm thành transport failure; kiểm tra trạng thái thành công sau return, số lần gửi và biên elapsed hợp lệ. Không phát hiện lỗi chặn trong phạm vi kiểm tra này. Chưa có review độc lập mới cho CORE-002; không gọi tự rà soát là review độc lập.

Không agent con, không commit; giữ nguyên source và Android ngoài core. Kết quả không chứng minh Android/APK, scheduler nền hoặc transport thật hoạt động.
