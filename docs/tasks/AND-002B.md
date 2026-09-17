# AND-002B — độ cao tương đối từ khí áp kế

STATUS: DONE
OWNER: Codex / gpt-5.6-sol; một primary worker, không reviewer vì đây là logic chuẩn hóa cục bộ, không đổi cảnh báo/giao thức.
FILES: Đọc android/android-plan.md dòng 223–304; android/app/src/main/java/vn/nckh27pa/fallsafe/DemoLogic.kt; android/app/src/main/java/vn/nckh27pa/fallsafe/PhoneSensorCollector.kt; android/app/src/test/java/vn/nckh27pa/fallsafe/DemoTests.kt; android/app/build.gradle.kts. Chỉ được sửa DemoLogic.kt và DemoTests.kt.
GOAL: Điền altitudeDeltaM từ TYPE_PRESSURE theo độ cao tương đối trong cửa sổ ngắn; giữ pressurePa và mọi tên/trường hiện có; không biến khí áp thành bằng chứng ngã độc lập.
SOURCE: android-plan.md §4.5.1–4.5.3, đặc biệt pressurePa/altitudeDeltaM, trường thiếu phải null và độ cao chỉ tương đối trong khoảng ngắn.
DEPENDENCIES: AND-002A/M1 đã PASS. Không phụ thuộc ESP32, BLE hoặc phần cứng thật.
DELIVERABLE: Logic pure Kotlin có trạng thái hữu hạn/bounded, reset qua clear(), chống NaN/Infinity/áp suất <=0 và timestamp đảo; tests RED→GREEN cho baseline, thay đổi áp suất có dấu/hợp lý, stale/null, invalid, reset và không tăng bộ nhớ theo thời gian.
ACCEPTANCE: Đơn vị pressurePa vẫn Pa; altitudeDeltaM finite khi có baseline hợp lệ, null khi pressure thiếu/stale; không tự điền dữ liệu cho phần cứng thiếu; không sửa detector, service, UI, protocol hay plan. `./gradlew testDebugUnitTest lintDebug assembleDebug` exit 0.
VERIFY: Worker chạy test mục tiêu rồi full Gradle gate; Hermes đọc diff và chạy lại gate. Tối đa 2 lần không tiến triển; usage/quota/sandbox blocker thì dừng, không đổi model vô cớ.
RESULT: Codex viết 3 test trước; sandbox không ghi được Gradle lock nên Hermes chạy RED trên host (3 test, 2 fail), cùng phiên Codex triển khai GREEN. Logic giữ history khí áp tối đa 64 mẫu/5 giây, đổi hPa→Pa và tính delta tương đối; invalid/clear xóa baseline, timestamp đảo bị bỏ. Hermes clean-test: 24 tests, 0 failure/error/skipped; lintDebug và assembleDebug PASS, 7 warnings/0 errors; APK tồn tại. Evidence: AND-002B-codex.log, AND-002B-hermes-clean-test.log, AND-002B-hermes-final-build.log.
NEXT_ACTION: Không mở rộng khí áp thành detector. Chọn lát AND-002 tiếp theo bằng phiếu mới và một worker mới; thử trên điện thoại có barometer chỉ khi có thiết bị.
