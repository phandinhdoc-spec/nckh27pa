# NHẬT KÍ NGHIÊN CỨU DỰ ÁN — ESP32 / THIẾT BỊ ĐEO

> **Tên đề tài:** Hệ thống hỗ trợ phát hiện té ngã, choáng váng và gửi cảnh báo khẩn cấp  
> **Nhánh nhật kí:** ESP32-S3 + cảm biến + BLE  
> **Ngày bắt đầu:** 30/05/2026  
> **Cập nhật đến:** 26/09/2026

## Lưu ý về cách ghi nhật kí
Nhật kí này được tách từ nhật kí nghiên cứu chung của dự án. Các mốc trước 17/09/2026 là phần tái dựng hồi cứu từ hồ sơ còn lưu; từ 17/09/2026 trở đi được đối chiếu thêm với mã nguồn, lịch sử GitHub và kết quả thử nghiệm. Mỗi ngày giữ cùng sáu mục để thuận tiện đối chiếu. Thử nghiệm thất bại vẫn được giữ lại vì là dữ liệu của quá trình nghiên cứu.

---

# NGÀY 30/05/2026

**GIAI ĐOẠN:** Hình thành ý tưởng  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định vai trò của thiết bị đeo trong phát hiện té ngã.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Giấy ghi chép.
- Tài liệu cảm biến chuyển động.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm nhận thấy chỉ dùng một cú va đập mạnh sẽ bỏ sót ngã chậm, choáng rồi sụp xuống hoặc bám vào vật. Từ đó đặt yêu cầu thiết bị đeo phải quan sát nhiều dấu hiệu chuyển động.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có số đo; xác định cần nhiều tín hiệu thay vì một ngưỡng gia tốc.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Một ngưỡng va đập đơn lẻ dễ báo sai hoặc bỏ sót.

## 6. KẾ HOẠCH TIẾP THEO
Khảo sát cảm biến chuyển động và thay đổi độ cao.

**Ghi nhận sử dụng AI:** ChatGPT hỗ trợ phản biện các tình huống; nhóm tự quyết định hướng nghiên cứu.

---

# NGÀY 02/06/2026

**GIAI ĐOẠN:** Xác định yêu cầu  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định ESP cần hỗ trợ gì trong hệ thống tổng thể.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Ghi chép kiến trúc.
- Tài liệu ESP32.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Thiết bị đeo được định hướng làm nguồn dữ liệu gắn trên người; Android đảm nhiệm cảnh báo và liên lạc.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
ESP không cần tự thực hiện toàn bộ cuộc gọi/GPS.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tách vai trò giúp phần cứng đơn giản hơn.

## 6. KẾ HOẠCH TIẾP THEO
So sánh phương án chỉ điện thoại và điện thoại + ESP.

---

# NGÀY 05/06/2026

**GIAI ĐOẠN:** Câu hỏi nghiên cứu  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định tín hiệu cần đo để phân biệt ngã và hoạt động thường.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Tài liệu IMU.
- Máy tính.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đặt câu hỏi về gia tốc, tư thế, thay đổi độ cao và trường hợp điện thoại không nằm trên người.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có ngưỡng cuối; yêu cầu ngưỡng phải chỉnh được.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không chốt thuật toán trước khi có dữ liệu.

## 6. KẾ HOẠCH TIẾP THEO
Khảo sát ESP32 và cảm biến.

---

# NGÀY 08/06/2026

**GIAI ĐOẠN:** Chọn kiến trúc  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chốt mô hình Android + thiết bị đeo ESP32.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- Android.
- Danh sách cảm biến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
ESP32 đọc cảm biến ngoài và truyền dữ liệu về Android; hệ thống vẫn cần cho Android hoạt động độc lập khi ESP chưa kết nối.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có kiến trúc hai nguồn cảm biến.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Phát sinh bài toán đồng bộ và giao tiếp hai thiết bị.

## 6. KẾ HOẠCH TIẾP THEO
Chọn IMU, áp suất và giao tiếp.

---

# NGÀY 12/06/2026

**GIAI ĐOẠN:** Khảo sát cảm biến  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chọn nhóm cảm biến cho thiết bị đeo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Tài liệu IMU.
- Tài liệu cảm biến áp suất.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Chia tín hiệu thành gia tốc, vận tốc góc/tư thế và biến thiên áp suất để hỗ trợ suy ra thay đổi độ cao. BMP390 từng được xem xét.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Danh sách tín hiệu chính được hình thành.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Áp suất chỉ là bằng chứng bổ sung, không tự khẳng định đã ngã.

## 6. KẾ HOẠCH TIẾP THEO
Tiếp tục khảo sát linh kiện có thể mua.

---

# NGÀY 16/06/2026

**GIAI ĐOẠN:** Khảo sát định vị  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định có cần bắt buộc GPS trên ESP hay không.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Tài liệu GNSS.
- ESP32.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
GPS/GNSS trong nhà có thể yếu và làm tăng tiêu thụ điện. Nhóm chuyển Android thành nguồn vị trí chính; GNSS thiết bị đeo chỉ là bổ sung.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Giảm yêu cầu định vị bắt buộc trên ESP.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không nên làm thiết bị đeo quá phức tạp.

## 6. KẾ HOẠCH TIẾP THEO
Tập trung cảm biến té ngã và BLE.

---

# NGÀY 20/06/2026

**GIAI ĐOẠN:** Thiết kế SOS  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định dữ liệu ESP cần cung cấp trước khi Android kích hoạt countdown.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Sơ đồ luồng.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
ESP đóng vai trò phát hiện/cung cấp dấu hiệu; Android mới là nơi xác minh và kích hoạt SOS.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chọn luồng phát hiện → countdown khoảng 10 s → SOS nếu không hủy.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Thiết bị đeo không nên tự gọi cứu hộ khi chưa có cơ chế xác minh.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế gói dữ liệu.

---

# NGÀY 25/06/2026

**GIAI ĐOẠN:** Nguồn điện  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Đặt yêu cầu thời lượng pin thiết bị đeo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- Tài liệu pin/MAX17048.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đặt mục tiêu pin khoảng 2–3 ngày, cần báo pin yếu, sạc và mất nguồn/kết nối.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có thời lượng pin đo thực tế.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Nguồn điện là một phần của độ tin cậy hệ thống.

## 6. KẾ HOẠCH TIẾP THEO
Tách kế hoạch ESP riêng.

---

# NGÀY 02/07/2026

**GIAI ĐOẠN:** Phân chia hệ thống  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Làm rõ dữ liệu ESP phải cung cấp cho Android.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Kế hoạch Android/ESP.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
ESP được xác định là nguồn telemetry ngoài; Android quản lý người thân, vị trí và SOS.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Ranh giới chức năng rõ hơn.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần contract dữ liệu chung.

## 6. KẾ HOẠCH TIẾP THEO
Viết kế hoạch ESP.

---

# NGÀY 08/07/2026

**GIAI ĐOẠN:** Lập kế hoạch ESP32  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định nhiệm vụ firmware.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- Danh sách cảm biến.
- Máy tính.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Firmware cần đọc IMU, áp suất, pin/nút nếu có, đóng gói telemetry và truyền BLE. BLE được ưu tiên cho thiết bị đeo.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có cấu trúc dữ liệu dự kiến.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tên trường và đơn vị phải thống nhất với Android.

## 6. KẾ HOẠCH TIẾP THEO
Chuẩn bị môi trường lập trình.

---

# NGÀY 15/07/2026

**GIAI ĐOẠN:** Chuẩn bị phát triển  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Thiết lập quy trình Git và hỗ trợ lập trình.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Git/GitHub.
- Hermes/Codex/AGY.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Quy ước lưu thay đổi bằng Git; khi dùng AI chỉ đọc/sửa phần liên quan.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có quy trình quản lý mã nguồn.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Nhiệm vụ quá rộng dễ gây sửa chồng chéo.

## 6. KẾ HOẠCH TIẾP THEO
Bắt đầu triển khai firmware.

**Ghi nhận sử dụng AI:** AI hỗ trợ khảo sát và viết mã; thay đổi phải được kiểm tra.

---

# NGÀY 05/09/2026

**GIAI ĐOẠN:** Phản biện thuật toán  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Xem lại các tình huống ngã không có va đập mạnh.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- IMU/áp suất dự kiến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Xem xét ngã từ ghế, choáng bám vật, ngã chậm; quyết định kết hợp gia tốc, tư thế, thời gian và thay đổi độ cao.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa chốt bộ ngưỡng cuối.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Ngưỡng mặc định không phải kết quả nghiên cứu.

## 6. KẾ HOẠCH TIẾP THEO
Cho phép hiệu chỉnh ngưỡng từ Android.

**Ghi nhận sử dụng AI:** ChatGPT hỗ trợ phản biện trường hợp bỏ sót/báo sai.

---

# NGÀY 11/09/2026

**GIAI ĐOẠN:** Chọn cảm biến áp suất  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Tìm cảm biến thay BMP390.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- BMP388/BMP390/BMP390L/DPS310.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
So sánh các cảm biến áp suất và giữ kiến trúc đủ linh hoạt để thay linh kiện.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có danh sách thay thế; chưa đo đối chứng.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không phụ thuộc một model cảm biến.

## 6. KẾ HOẠCH TIẾP THEO
Chọn linh kiện thực tế có sẵn.

---

# NGÀY 14/09/2026

**GIAI ĐOẠN:** Hoàn thiện kế hoạch  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Chuẩn hóa contract Android–ESP.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Kế hoạch ESP/Android.
- Git.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Chuẩn hóa tên dữ liệu để hai phía dùng cùng ý nghĩa; ESP là nguồn cảm biến độc lập gắn trên người.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có kế hoạch ESP riêng.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Contract phải chốt trước khi ghép hai phía.

## 6. KẾ HOẠCH TIẾP THEO
Tạo firmware thử nghiệm.

---

# NGÀY 15/09/2026

**GIAI ĐOẠN:** Chuẩn bị tích hợp  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Bảo đảm Android có chỗ nhận dữ liệu ESP trong kiến trúc.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP source.
- Android source.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Phần ESP tiếp tục được giữ độc lập với giao diện; chuẩn bị decoder/telemetry để ghép với Android.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có thử nghiệm BLE phần cứng đầy đủ.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần kiểm tra trên thiết bị thật, không chỉ build.

## 6. KẾ HOẠCH TIẾP THEO
Hoàn thiện giao tiếp.

---

# NGÀY 16/09/2026

**GIAI ĐOẠN:** Chuẩn hóa giao tiếp  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Chốt luồng dữ liệu ESP sang hệ thống.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Repository.
- Tài liệu kiến trúc.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Contract sensor ingestion và đồng bộ được xác định trước khi các phần code phát triển song song.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có contract ban đầu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Interface không rõ sẽ gây sửa lặp.

## 6. KẾ HOẠCH TIẾP THEO
Đưa mã lên GitHub.

---

# NGÀY 17/09/2026

**GIAI ĐOẠN:** Đưa mã lên GitHub  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Lưu ESP cùng hệ thống Android/backend.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- GitHub.
- ESP32 source.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit gốc `2e7bc994...` đưa Android, ESP32 và backend vào cùng repository.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
ESP đã có vị trí chính thức trong repo.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Chưa coi kết nối thiết bị thật là hoàn tất.

## 6. KẾ HOẠCH TIẾP THEO
Tiếp tục kiểm thử cảm biến và BLE.

---

# NGÀY 18/09/2026

**GIAI ĐOẠN:** Chuẩn bị thử nghiệm  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Đối chiếu cấu hình ESP với Android và test.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP source.
- Android tests.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Bổ sung kiểm thử cấu hình ESP và chuẩn bị cho việc kết nối phần cứng thật.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Phần mềm có test nhưng dữ liệu ngã thực tế còn thiếu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
PASS phần mềm không thay thế thử nghiệm phần cứng.

## 6. KẾ HOẠCH TIẾP THEO
Kết nối ESP thật và thu dữ liệu.

---

# NGÀY 19/09/2026

**GIAI ĐOẠN:** Rà soát phần cứng  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Chọn cấu hình cảm biến cho thiết bị đeo thật và bộ test.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32-S3 Super Mini.
- MPU6050/MPU9250.
- GY63.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Phân biệt cấu hình sản phẩm thật ESP32-S3 + MPU6050 + GY63 và bộ test ESP32-WROOM-32 + MPU9250 + GY63.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Hai cấu hình được tách rõ.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không dùng nhầm pin/I2C/code giữa bộ test và sản phẩm.

## 6. KẾ HOẠCH TIẾP THEO
Kiểm tra dây và địa chỉ I2C.

---

# NGÀY 20/09/2026

**GIAI ĐOẠN:** I2C và Serial  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Kiểm tra đọc cảm biến trên ESP32-S3.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32-S3 Super Mini.
- MPU6050.
- GY63/MS5611.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Thử đấu I2C, yêu cầu Serial phải hiển thị trạng thái dễ đọc để biết cảm biến có hoạt động. Việc dùng các bus/chân được rà soát nhiều lần theo phần cứng thực tế.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có tiêu chí Serial phục vụ kiểm thử; phát hiện cấu hình chân cần được thống nhất theo bản đang dùng.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không thể kết luận cảm biến chạy chỉ vì firmware upload thành công.

## 6. KẾ HOẠCH TIẾP THEO
Chuẩn hóa chân theo phần cứng thực tế và kiểm tra từng cảm biến.

---

# NGÀY 21/09/2026

**GIAI ĐOẠN:** Tài liệu học sinh  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Chuẩn hóa cách viết firmware dễ hiểu.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Firmware ESP.
- Tài liệu hướng dẫn.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Yêu cầu tên biến tiếng Việt không dấu, giải thích gia tốc/vận tốc góc bằng ví dụ gần gũi và giải thích cấu trúc code cho học sinh lớp 9.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có quy ước tài liệu/code dành cho học sinh.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Code chạy được nhưng khó đọc không phù hợp mục tiêu giáo dục.

## 6. KẾ HOẠCH TIẾP THEO
Tạo bản firmware học sinh riêng.

---

# NGÀY 22/09/2026

**GIAI ĐOẠN:** Chuẩn hóa I2C  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Làm rõ bus và địa chỉ cảm biến.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- MPU6050.
- GY63.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Rà soát địa chỉ MPU6050 0x68/0x69 và GY63 0x77/0x76; ưu tiên cấu hình dây rõ ràng và tránh nhầm bus.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có hướng dẫn đấu dây/kiểm tra I2C.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Sai bus/chân có thể làm cảm biến im lặng dù code đúng.

## 6. KẾ HOẠCH TIẾP THEO
Giữ sơ đồ dây đi cùng firmware.

---

# NGÀY 23/09/2026

**GIAI ĐOẠN:** Cập nhật repository  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Đưa trạng thái dự án mới lên GitHub.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Git/GitHub.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `5f1d2ec...` cập nhật toàn bộ project ngày 23/09.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Repository có bản cập nhật mới.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần tiếp tục ghi rõ thay đổi thay vì commit chung chung.

## 6. KẾ HOẠCH TIẾP THEO
Hoàn thiện BLE trên S3.

---

# NGÀY 24/09/2026

**GIAI ĐOẠN:** BLE + firmware học sinh  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Làm BLE/telemetry hoạt động và tạo bản firmware dễ học.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32-S3.
- MPU6050.
- GY63.
- Arduino CLI.
- Android.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Commit `222b672...` cập nhật BLE ESP32-S3 và cấu hình I2C thực tế. Commit `cf802959...` thêm `esp-s3-for-student.ino`, tài liệu học sinh, menu Serial và sửa contract Telemetry/Profile phía app. Firmware học sinh compile thành công; parity check 9/9 PASS. Ghi rõ phần ghi profile từ Android chưa được ESP hỗ trợ đầy đủ.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Firmware học sinh compile: 1,163,149 B, khoảng 88% vùng chương trình; parity 9/9 PASS. BLE phía thiết bị được kiểm tra ở mức phần mềm/build.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tính năng `START_STREAM` và ghi profile là hai việc khác nhau; không được coi ACK/Notify là đặc tính ghi cấu hình.

## 6. KẾ HOẠCH TIẾP THEO
Kiểm chứng phần cứng thật và thử telemetry liên tục.

---

# NGÀY 25/09/2026

**GIAI ĐOẠN:** Thử nghiệm thả rơi  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Dùng phép thả rơi–bắt bằng tay để kiểm tra chuỗi phát hiện.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32-S3.
- Điện thoại Android.
- BLE telemetry.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Telemetry chạy liên tục nhưng ban đầu thả rơi chưa làm Android vào SOS. Qua kiểm tra xác định dữ liệu telemetry phải được đưa trực tiếp vào bộ phát hiện té ngã phía Android. Các commit Android trong ngày sửa đường dữ liệu này và hạ ngưỡng cho thử nghiệm tay.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
ESP cung cấp được telemetry liên tục; vấn đề chính được xác định ở đường xử lý phía Android chứ không phải ngừng stream.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Có telemetry không đồng nghĩa telemetry đã đi vào thuật toán cảnh báo. Cần kiểm tra toàn chuỗi cảm biến → BLE → decoder → detector → countdown.

## 6. KẾ HOẠCH TIẾP THEO
Tiếp tục thu log thử nghiệm thật, sau đó hiệu chỉnh ngưỡng dựa trên dữ liệu.

---

# NGÀY 26/09/2026

**GIAI ĐOẠN:** Tổng hợp hiện trạng  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Tách nhật kí ESP riêng và chốt trạng thái hiện tại.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- GitHub repository.
- Lịch sử commit.
- Nhật kí nghiên cứu chung.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Đối chiếu lịch sử từ ý tưởng đến firmware ESP32-S3, GY63, MPU6050, BLE và telemetry. Tách riêng phần ESP để tránh trộn với nhật kí ứng dụng Android.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Nhật kí ESP được cập nhật đến 26/09/2026. Chưa có commit kỹ thuật mới ngày 26/09 tại thời điểm tổng hợp.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần phân biệt rõ kết quả đã kiểm chứng và kế hoạch; ngưỡng phát hiện cuối cùng vẫn phải dựa trên thực nghiệm.

## 6. KẾ HOẠCH TIẾP THEO
Thu bộ dữ liệu ngã/không ngã có nhãn, kiểm tra nguồn/pin và chốt cấu hình firmware sản phẩm.


---

## GHI CHÚ CẬP NHẬT
Mọi ngày tiếp theo tiếp tục ghi đủ sáu mục trên. Không xóa thử nghiệm thất bại; ghi rõ giả thuyết, hiện tượng, cách sửa và kết quả kiểm tra lại.
