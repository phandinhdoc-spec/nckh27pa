# NHẬT KÍ NGHIÊN CỨU DỰ ÁN — ANDROID

> **Tên đề tài:** Hệ thống hỗ trợ phát hiện té ngã, choáng váng và gửi cảnh báo khẩn cấp  
> **Nhánh nhật kí:** Ứng dụng Android + phát hiện + SOS + vị trí  
> **Ngày bắt đầu:** 30/05/2026  
> **Cập nhật đến:** 26/09/2026

## Lưu ý về cách ghi nhật kí
Nhật kí này được tách từ nhật kí nghiên cứu chung của dự án. Các mốc trước 17/09/2026 là phần tái dựng hồi cứu từ hồ sơ còn lưu; từ 17/09/2026 trở đi được đối chiếu thêm với mã nguồn, lịch sử GitHub và kết quả thử nghiệm. Mỗi ngày giữ cùng sáu mục để thuận tiện đối chiếu. Thử nghiệm thất bại vẫn được giữ lại vì là dữ liệu của quá trình nghiên cứu.

---

# NGÀY 30/05/2026

**GIAI ĐOẠN:** Hình thành ý tưởng  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định bài toán cảnh báo khi người bị ngã không thể tự gọi cứu hộ.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại.
- Ghi chép.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm nhận thấy cần một thiết bị có khả năng tự cảnh báo khi người dùng mất ý thức.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có phần mềm; hình thành yêu cầu cảnh báo tự động.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không thể giả định người bị nạn luôn thao tác được.

## 6. KẾ HOẠCH TIẾP THEO
Khảo sát khả năng dùng Android.

**Ghi nhận sử dụng AI:** ChatGPT hỗ trợ phản biện ý tưởng.

---

# NGÀY 02/06/2026

**GIAI ĐOẠN:** Xác định yêu cầu Android  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định vai trò của điện thoại trong hệ thống.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại Android.
- Tài liệu cảm biến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Android có cảm biến, vị trí, cuộc gọi và SMS nên được chọn làm bộ xử lý/cảnh báo chính.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Luồng sơ bộ: phát hiện → xác minh → SOS → gửi vị trí/liên hệ.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Android giúp giảm phần cứng riêng.

## 6. KẾ HOẠCH TIẾP THEO
So sánh Android độc lập và Android + ESP.

---

# NGÀY 05/06/2026

**GIAI ĐOẠN:** Câu hỏi nghiên cứu  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định trường hợp Android có thể sai.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại.
- Máy tính.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đặt câu hỏi khi điện thoại nằm trên bàn, không ở trên người; từ đó cần hỗ trợ nguồn dữ liệu ESP đeo người.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Xác định Android phải hỗ trợ nhiều nguồn sensor.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không nên phụ thuộc vị trí đặt điện thoại.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế kiến trúc đa nguồn.

---

# NGÀY 08/06/2026

**GIAI ĐOẠN:** Kiến trúc Android + ESP  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chốt Android là trung tâm cảnh báo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- ESP32.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Android xử lý giao diện, vị trí, liên lạc và có thể dùng cảm biến của chính nó; ESP là nguồn bổ sung.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có ba chế độ mục tiêu: chỉ Android; Android+ESP; ESP cung cấp chuyển động, Android cảnh báo.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Phải xử lý mất kết nối ESP.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế collector/decoder.

---

# NGÀY 12/06/2026

**GIAI ĐOẠN:** Nguồn dữ liệu  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định các tín hiệu Android cần nhận/xử lý.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Cảm biến điện thoại.
- Tài liệu IMU/áp suất.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Android cần nhận gia tốc/tư thế/áp suất khi có và vẫn đọc cảm biến điện thoại.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có thuật toán cuối.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần kết hợp nhiều bằng chứng.

## 6. KẾ HOẠCH TIẾP THEO
Khảo sát vị trí.

---

# NGÀY 16/06/2026

**GIAI ĐOẠN:** Vị trí  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chọn Android làm nguồn vị trí chính.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android GPS/location services.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Do GNSS thiết bị đeo có thể yếu trong nhà, Android được ưu tiên lấy vị trí hệ thống để gửi người thân.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Android trở thành nguồn vị trí quan trọng.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Vị trí phải được gửi cho người thân, không chỉ hiển thị cục bộ.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế SOS.

---

# NGÀY 20/06/2026

**GIAI ĐOẠN:** Luồng SOS  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Thiết kế cơ chế giảm báo giả nhưng vẫn cứu được người bất tỉnh.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Sơ đồ luồng.
- Android.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đổi từ hỏi người dùng có cần giúp sang countdown tự động: chỉ cần thao tác nếu muốn hủy; hết khoảng 10 s thì SOS.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chọn 10 s làm giá trị thử nghiệm ban đầu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không yêu cầu xác nhận chủ động từ người có thể đã bất tỉnh.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế giao diện countdown/SOS.

---

# NGÀY 25/06/2026

**GIAI ĐOẠN:** Trạng thái thiết bị  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định Android cần hiển thị pin/kết nối thiết bị đeo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- Thiết kế trạng thái.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Android cần biết pin yếu, đang sạc hoặc mất kết nối nếu ESP cung cấp.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có dữ liệu pin thật.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Trạng thái hệ thống là một phần của an toàn.

## 6. KẾ HOẠCH TIẾP THEO
Lập kế hoạch Android.

---

# NGÀY 02/07/2026

**GIAI ĐOẠN:** Lập kế hoạch Android  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Liệt kê đầy đủ chức năng ứng dụng.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy tính.
- Android Studio.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Chia chức năng: sensor điện thoại, dữ liệu ESP, detector, countdown, sự kiện, contacts, vị trí, SOS và cấu hình ngưỡng.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có kế hoạch chức năng ban đầu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Màn hình chính không nên quá kỹ thuật.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế UI đơn giản.

---

# NGÀY 08/07/2026

**GIAI ĐOẠN:** Contract ESP  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định dữ liệu Android cần nhận từ thiết bị đeo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android/ESP plan.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Chuẩn bị decoder và cấu trúc dữ liệu chung để BLE có thể cấp telemetry.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có yêu cầu đồng bộ dữ liệu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tên trường/đơn vị phải thống nhất.

## 6. KẾ HOẠCH TIẾP THEO
Chuẩn hóa API.

---

# NGÀY 15/07/2026

**GIAI ĐOẠN:** Chuẩn bị phát triển  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Thiết lập Git và công cụ hỗ trợ viết app.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Git/GitHub.
- Hermes/Codex/AGY.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Thiết lập quy trình phát triển, chỉ sửa phần liên quan và kiểm tra lại build/test.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có quy trình quản lý thay đổi.

## 5. RÚT KINH NGHIỆM & LỖI SAI
AI không thay thế kiểm thử thiết bị thật.

## 6. KẾ HOẠCH TIẾP THEO
Bắt đầu APK.

**Ghi nhận sử dụng AI:** AI hỗ trợ code và review.

---

# NGÀY 05/09/2026

**GIAI ĐOẠN:** Phản biện detector  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Mở rộng thuật toán cho ngã chậm/choáng.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- Cảm biến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Quyết định không dùng một ngưỡng duy nhất; cần kết hợp gia tốc, tư thế, bất động và độ cao nếu có.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Ngưỡng phải chỉnh được.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Ngưỡng mặc định chưa phải kết quả nghiên cứu.

## 6. KẾ HOẠCH TIẾP THEO
Tạo profile/ngưỡng cấu hình.

---

# NGÀY 11/09/2026

**GIAI ĐOẠN:** Khả năng thay cảm biến  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Bảo đảm Android không phụ thuộc một model áp suất duy nhất.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Tài liệu sensor.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Thiết kế dữ liệu theo đại lượng vật lý thay vì gắn chặt UI với model cảm biến.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Kiến trúc linh hoạt hơn.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tách hardware model khỏi logic app.

## 6. KẾ HOẠCH TIẾP THEO
Hoàn thiện kế hoạch.

---

# NGÀY 14/09/2026

**GIAI ĐOẠN:** Hoàn thiện kiến trúc  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Khẳng định Android tự dùng cảm biến điện thoại, không chỉ nhận ESP.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- `android-plan.md`.
- Git.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Kiến trúc chia frontend, detector, backend/API, ESP communication và emergency. Chuẩn hóa tên dữ liệu.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có kế hoạch Android chi tiết.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Bản kế hoạch cũ đánh giá thiếu vai trò cảm biến điện thoại.

## 6. KẾ HOẠCH TIẾP THEO
Build APK.

---

# NGÀY 15/09/2026

**GIAI ĐOẠN:** APK và UI  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Có APK chạy trên điện thoại thật.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android project.
- Gradle.
- Điện thoại.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Build/cài APK; giao diện ban đầu quá kỹ thuật. Đề xuất SOS tròn lớn ở trung tâm, bốn vùng chức năng; contacts chuyển vào Settings.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
APK debug chạy, cảm biến điện thoại có dữ liệu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần tách chế độ sử dụng và thử nghiệm.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế backend/API.

---

# NGÀY 16/09/2026

**GIAI ĐOẠN:** Backend/API  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Tách UI khỏi xử lý và chuẩn hóa REST.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Repository.
- Hermes/AGY/Codex.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Chốt REST `/api/v1` cho sensor ingestion, contacts, event, SOS/countdown/ACK và sync.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có contract API ban đầu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Chốt interface trước khi sửa song song.

## 6. KẾ HOẠCH TIẾP THEO
Đưa mã lên GitHub.

---

# NGÀY 17/09/2026

**GIAI ĐOẠN:** GitHub và chức năng cứu hộ  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Lưu Android đầy đủ và rà luồng vị trí/SOS.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- GitHub.
- Android source.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `2e7bc994...` có `HomeScreen`, `ContactsScreen`, `PhoneSensorCollector`, `MonitoringService`, `Esp32PacketDecoder`, API và sync. Rà lại nút vị trí: mục tiêu là gửi vị trí cho người thân, không chỉ mở bản đồ.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có cấu trúc app chính thức trong repo.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Luồng vị trí trước đó chưa đúng mục tiêu cứu hộ.

## 6. KẾ HOẠCH TIẾP THEO
Bổ sung quyền call/SMS/location.

---

# NGÀY 18/09/2026

**GIAI ĐOẠN:** Quyền SOS và kiểm thử  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Hoàn thiện permission flow và validation.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- Permission dialogs.
- Test scripts.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `a995d8b...` hoàn thiện quyền SOS; thêm permission/capability/location/emergency/calibration/profile. Acceptance permission cuối đạt 7/7 PASS; các APK được đối chiếu, bản (5) có profile và REST/sync.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Permission acceptance 7/7 PASS ở bộ test đã lưu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
FAIL test có thể do test-driver; phải giữ bằng chứng. Ngưỡng phải hiệu chỉnh bằng thực nghiệm.

## 6. KẾ HOẠCH TIẾP THEO
Test SIM/SMS/call/location và ESP thật.

**Ghi nhận sử dụng AI:** AI hỗ trợ code/test; kết luận dựa trên kết quả chạy.

---

# NGÀY 19/09/2026

**GIAI ĐOẠN:** Điện thoại thật  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Rà lỗi gọi điện và chẩn đoán runtime.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android phone.
- SIM/telephony.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `2b0eacc...` sửa nhận diện khả năng telephony và runtime diagnostics. Tiếp tục xem xét âm thanh SOS, loa ngoài và quyền liên quan.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có chẩn đoán telephony tốt hơn.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Emulator/build không chứng minh cuộc gọi thật hoạt động.

## 6. KẾ HOẠCH TIẾP THEO
Kiểm tra location và fall logger.

---

# NGÀY 20/09/2026

**GIAI ĐOẠN:** Location + FALL-01  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Khôi phục vị trí khi người dùng bật Location sau khi app đã chạy và instrument detector.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- Điện thoại thật.
- Gradle/tests.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `8761541...` sửa location recovery. Bộ kiểm chứng ghi nhận 30 suites/206 tests PASS; LOC-01 6/6 PASS và SOS regression 2/2 PASS. FALL-01 bổ sung logger/tracing để chờ test thực tế.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
206 tests PASS ở lần kiểm chứng; LOC-01 6/6; SOS regression 2/2.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Test tự động vẫn cần đối chiếu thiết bị thật và dữ liệu sensor.

## 6. KẾ HOẠCH TIẾP THEO
Cài APK mới, hiển thị sensor và thử FALL-01.

---

# NGÀY 21/09/2026

**GIAI ĐOẠN:** Hiển thị cảm biến  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Làm dữ liệu sensor dễ quan sát khi thử nghiệm.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android Settings.
- Sensor collector.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Yêu cầu đưa lại vùng hiển thị giá trị cảm biến trong Settings vì phiên bản trước từng cho phép quan sát trực tiếp, giúp biết app có thật sự đọc sensor.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Xác định yêu cầu UI chẩn đoán sensor.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không nên ẩn toàn bộ dữ liệu kỹ thuật trong giai đoạn nghiên cứu.

## 6. KẾ HOẠCH TIẾP THEO
Dùng dữ liệu hiển thị để kiểm tra detector.

---

# NGÀY 22/09/2026

**GIAI ĐOẠN:** Chuẩn bị ghép ESP  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Bảo đảm Android nhận đúng đại lượng từ ESP.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android decoder.
- ESP contract.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Rà tên/đơn vị và cấu hình để chuẩn bị telemetry từ thiết bị thật.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Contract tiếp tục được chuẩn hóa.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Sai mapping dữ liệu có thể làm detector sai dù BLE vẫn kết nối.

## 6. KẾ HOẠCH TIẾP THEO
Thử BLE thực tế.

---

# NGÀY 23/09/2026

**GIAI ĐOẠN:** Cập nhật dự án  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Đồng bộ trạng thái Android lên repository.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Git/GitHub.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `5f1d2ec...` cập nhật project ngày 23/09.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có snapshot mới trên GitHub.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Commit tổng quát khó truy vết hơn commit mô tả rõ chức năng.

## 6. KẾ HOẠCH TIẾP THEO
Hoàn thiện BLE/telemetry.

---

# NGÀY 24/09/2026

**GIAI ĐOẠN:** Telemetry BLE  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Cho Android yêu cầu stream đúng contract và xử lý dữ liệu ESP.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android.
- ESP32-S3.
- BLE.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `cf802959...` sửa nút `Yêu cầu Telemetry` gửi JSON `START_STREAM` đúng contract; `writeProfile` trả false và khóa nút gửi profile vì đặc tính ACK là Notify, chưa phải đường ghi profile. Build/test/lint Android đạt trong lần kiểm chứng của commit.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Android có lệnh yêu cầu telemetry đúng contract; chức năng ghi profile được đánh dấu chưa hỗ trợ thay vì giả vờ thành công.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Phải phân biệt telemetry stream với cấu hình profile.

## 6. KẾ HOẠCH TIẾP THEO
Thử thả rơi và kiểm tra detector có nhận telemetry.

---

# NGÀY 25/09/2026

**GIAI ĐOẠN:** Telemetry → detector → SOS  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Sửa lỗi ESP có telemetry nhưng Android không bật SOS khi thử thả rơi.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android phone.
- ESP32-S3.
- BLE telemetry.
- GitHub.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Ban đầu telemetry hiển thị/chạy liên tục nhưng không kích hoạt SOS. Các commit `a0bb7d8...`, `e5ff7fca...`, `04c3863...`, `be0c5ac...` lần lượt nối sự kiện ESP vào alert countdown, đưa telemetry ESP vào fall detector, hạ ngưỡng cho phép thử thả–bắt bằng tay và giữ stream telemetry liên tục khi đồng thời cấp dữ liệu cho detector.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Đường dữ liệu chính được sửa thành telemetry ESP → detector Android → countdown/SOS; ngưỡng thử nghiệm được hạ cho thí nghiệm mô phỏng. Telemetry vẫn tiếp tục stream.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Hiển thị telemetry không chứng minh detector đang tiêu thụ nó. Cần test end-to-end. Ngưỡng hạ để thử tay không phải ngưỡng an toàn cuối cùng.

## 6. KẾ HOẠCH TIẾP THEO
Thu log nhiều lần ngã/không ngã, sau đó hiệu chỉnh profile khoa học và kiểm tra cuộc gọi/SMS thật.

---

# NGÀY 26/09/2026

**GIAI ĐOẠN:** Tổng hợp hiện trạng  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Tách nhật kí Android riêng và chốt trạng thái hiện tại.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- GitHub.
- Lịch sử commit.
- Nhật kí chung.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đối chiếu toàn bộ tiến trình Android: sensor điện thoại, ESP telemetry, detector, countdown 10 s, SOS, contacts, location, permission, API/sync và kiểm thử. Tách phần Android thành nhật kí độc lập.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Nhật kí Android cập nhật đến 26/09/2026. Tại thời điểm tổng hợp chưa có commit kỹ thuật mới ngày 26/09 sau commit telemetry ngày 25/09.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần tiếp tục phân biệt kết quả phần mềm với bằng chứng thực nghiệm trên người/thiết bị thật.

## 6. KẾ HOẠCH TIẾP THEO
Thực nghiệm end-to-end có ghi log: hoạt động thường, ngã mô phỏng, ngã chậm/choáng; sau đó chốt ngưỡng và kiểm tra SOS thật.


---

## GHI CHÚ CẬP NHẬT
Mọi ngày tiếp theo tiếp tục ghi đủ sáu mục trên. Không xóa thử nghiệm thất bại; ghi rõ giả thuyết, hiện tượng, cách sửa và kết quả kiểm tra lại.
