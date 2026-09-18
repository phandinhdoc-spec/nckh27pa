# NHẬT KÍ NGHIÊN CỨU DỰ ÁN

> **Tên đề tài:** Hệ thống hỗ trợ phát hiện té ngã, choáng váng và gửi cảnh báo khẩn cấp  
> **Nhóm thực hiện:** Học sinh  
> **Giáo viên hướng dẫn:** ................................................  
> **Ngày bắt đầu:** 30/05/2026  
> **Ngày kết thúc:** ................................................  

## Lưu ý về cách ghi nhật kí

Nhật kí này được viết lại từ các ghi chép, kế hoạch, mã nguồn, các bản APK thử nghiệm và lịch sử GitHub hiện còn lưu. Các mốc từ **30/05/2026 đến 16/09/2026** là phần **tái dựng hồi cứu** theo tiến trình thực tế của dự án; những ngày không còn lưu giờ làm việc chính xác được ghi rõ là “không ghi lại chính xác”. Từ **17/09/2026 trở đi**, các thay đổi phần mềm được đối chiếu thêm với lịch sử commit GitHub của repository `nckh27pa`.

Khi có sử dụng AI, nhóm ghi lại mục đích sử dụng. AI được dùng để trao đổi ý tưởng, phản biện phương án, hỗ trợ tìm lỗi và sinh một phần mã nguồn ban đầu. Nhóm tự lựa chọn phương án, tự kiểm tra sản phẩm và không dùng AI để thay nhóm đưa ra kết luận nghiên cứu.

---

# NGÀY 30/05/2026

**GIAI ĐOẠN:** Hình thành ý tưởng  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Tìm một vấn đề thực tế có thể làm đề tài nghiên cứu khoa học và có khả năng tạo ra sản phẩm thử nghiệm.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy tính/điện thoại để tìm hiểu thông tin.
- Giấy ghi chép ý tưởng.
- ChatGPT dùng để trao đổi và đặt câu hỏi phản biện.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm trao đổi về tình huống người lớn tuổi bị té ngã khi ở nhà một mình. Vấn đề được đặt ra là nếu người bị ngã bị đau, choáng hoặc mất ý thức thì có thể không tự gọi điện được.

Ý tưởng ban đầu của nhóm khá đơn giản: dùng cảm biến gia tốc để phát hiện một cú va đập mạnh rồi phát cảnh báo. Sau khi trao đổi thêm, nhóm nhận thấy có những trường hợp không có va đập mạnh, ví dụ trượt khỏi ghế, choáng rồi ngồi sụp xuống, mất thăng bằng nhưng kịp bám vào vật hoặc ngã chậm từ độ cao nhỏ. Vì vậy nếu chỉ dựa vào một ngưỡng gia tốc thì có thể bỏ sót.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Chưa có số đo thực nghiệm.
- Xác định được vấn đề nghiên cứu: phát hiện nguy cơ té ngã và hỗ trợ gửi cảnh báo khi người dùng không thể tự cầu cứu.
- Xác định sơ bộ cần nhiều tín hiệu hơn một lần va đập mạnh.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Ý tưởng đầu tiên quá đơn giản. Nếu chỉ đặt một ngưỡng gia tốc thì khó phân biệt té ngã với chạy, nhảy hoặc làm rơi điện thoại.

## 6. KẾ HOẠCH TIẾP THEO
Tìm hiểu các loại cảm biến có thể dùng để đo chuyển động, tư thế và thay đổi độ cao.

**Ghi nhận sử dụng AI:** dùng ChatGPT để phản biện ý tưởng ban đầu và liệt kê các tình huống có thể làm thuật toán báo sai. Nhóm tự quyết định giữ hướng nghiên cứu té ngã.

---

# NGÀY 02/06/2026

**GIAI ĐOẠN:** Xác định yêu cầu của hệ thống  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định hệ thống cần làm được những gì ngoài việc nhận biết cú ngã.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Ghi chép ý tưởng.
- Điện thoại Android để khảo sát cảm biến có sẵn.
- ChatGPT dùng để thảo luận phương án.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm liệt kê các bước cần xảy ra sau khi hệ thống nghi ngờ có té ngã: phát hiện dấu hiệu bất thường, cho người dùng một khoảng thời gian ngắn để hủy nếu cảnh báo sai, nếu không có phản hồi thì tự chuyển sang SOS, gửi thông tin cho người thân và cố gắng kèm vị trí.

Nhóm nhận thấy điện thoại Android đã có nhiều cảm biến và khả năng gọi điện, nhắn tin, lấy vị trí. Vì vậy điện thoại có thể vừa là bộ xử lí, vừa là thiết bị cảm biến và thiết bị liên lạc.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Chưa có số đo.
- Xác định luồng sơ bộ: **phát hiện → xác minh → SOS → gửi vị trí/liên hệ**.
- Android được đưa vào phương án chính.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Nếu làm một thiết bị riêng hoàn toàn thì phải tự giải quyết thêm GPS, SIM, cuộc gọi và nguồn điện. Dùng Android có thể giảm bớt phần cứng.

## 6. KẾ HOẠCH TIẾP THEO
So sánh hai hướng: chỉ dùng điện thoại và dùng điện thoại kết hợp thiết bị đeo ESP32.

**Ghi nhận sử dụng AI:** AI hỗ trợ liệt kê ưu, nhược điểm của từng hướng. Nhóm tự chọn phương án để tiếp tục khảo sát.

---

# NGÀY 05/06/2026

**GIAI ĐOẠN:** Xây dựng câu hỏi nghiên cứu  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chuyển ý tưởng thành các câu hỏi có thể kiểm tra bằng thực nghiệm.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Sổ ghi chép.
- Máy tính.
- Tài liệu về cảm biến chuyển động.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm đặt các câu hỏi:
- Có thể phân biệt té ngã với hoạt động bình thường bằng gia tốc và tư thế không?
- Thay đổi độ cao có giúp nhận biết ngã từ đứng xuống sàn hay từ ghế xuống sàn không?
- Nếu điện thoại để trên bàn thì dữ liệu điện thoại có còn đáng tin không?
- Có cần thêm một thiết bị đeo trên người để tăng độ tin cậy không?
- Khi người bị ngã không phản hồi thì hệ thống phải làm gì?

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có số liệu thực nghiệm. Nhóm có 5 câu hỏi cần kiểm tra trong các giai đoạn sau.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không nên vội chọn một thuật toán duy nhất khi chưa có dữ liệu thực nghiệm. Cần thiết kế hệ thống sao cho các ngưỡng có thể thay đổi.

## 6. KẾ HOẠCH TIẾP THEO
Khảo sát ESP32 và cảm biến có thể gắn trên người.

---

# NGÀY 08/06/2026

**GIAI ĐOẠN:** Lựa chọn kiến trúc Android + ESP32  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chọn kiến trúc tổng thể để có thể tiếp tục mua linh kiện và viết phần mềm.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại Android.
- Tài liệu ESP32.
- Danh sách cảm biến dự kiến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm chọn phương án kết hợp: Android dùng cảm biến có sẵn, xử lí giao diện, vị trí và liên lạc; ESP32 dùng như thiết bị đeo, đọc cảm biến ngoài và truyền dữ liệu về Android.

Nhóm không muốn hệ thống bị phụ thuộc hoàn toàn vào ESP32. Nếu chưa có thiết bị đeo, Android vẫn phải có khả năng hoạt động ở mức cơ bản.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Xác định 3 chế độ cần hướng tới:
- chỉ điện thoại;
- điện thoại + ESP32;
- ESP32 cung cấp chuyển động chính, Android cung cấp vị trí và cảnh báo.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Tách hệ thống thành hai phần giúp dễ thử nghiệm hơn nhưng làm phát sinh bài toán đồng bộ dữ liệu và giao tiếp giữa hai thiết bị.

## 6. KẾ HOẠCH TIẾP THEO
Chọn cảm biến chuyển động, áp suất/độ cao và phương thức liên lạc ESP32–Android.

---

# NGÀY 12/06/2026

**GIAI ĐOẠN:** Khảo sát cảm biến  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định nhóm cảm biến cần dùng cho thiết bị đeo.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32 dự kiến sử dụng.
- Tài liệu cảm biến IMU.
- Tài liệu cảm biến áp suất khí quyển.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm chia tín hiệu cần đo thành: chuyển động nhanh bằng gia tốc, thay đổi tư thế bằng con quay/góc, thay đổi độ cao bằng áp suất khí quyển và vị trí bằng GPS hoặc điện thoại.

BMP390 được đưa vào phương án cảm biến áp suất vì có thể theo dõi biến thiên áp suất để hỗ trợ ước lượng thay đổi độ cao.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Chưa đo thực tế.
- Danh sách tín hiệu chính: gia tốc, góc/tư thế, áp suất, vị trí.
- Chưa chốt hoàn toàn model IMU.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Áp suất không thể tự khẳng định “đã ngã”; nó chỉ là một bằng chứng bổ sung. Cần kết hợp nhiều tín hiệu.

## 6. KẾ HOẠCH TIẾP THEO
Tìm hiểu khó khăn của GPS trong nhà và cách dùng vị trí điện thoại làm nguồn dự phòng.

---

# NGÀY 16/06/2026

**GIAI ĐOẠN:** Khảo sát vị trí và cảnh báo  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Tìm cách gửi vị trí người gặp nạn cho người thân.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại Android.
- Tài liệu GPS/GNSS.
- Ghi chép về module định vị.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm nhận thấy GPS của thiết bị đeo có thể yếu khi ở trong nhà. Trong khi đó điện thoại thường có thêm dữ liệu từ mạng và vị trí hệ thống.

Vì vậy nhóm chuyển hướng: ESP32 không bắt buộc phải tự giải quyết toàn bộ bài toán định vị. Android sẽ là nguồn vị trí quan trọng.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Chưa có phép đo sai số GPS.
- Quyết định: khi cảnh báo, ưu tiên vị trí Android; GNSS của thiết bị đeo chỉ là nguồn bổ sung nếu có.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Phương án “thiết bị đeo tự GPS rồi tự gửi” làm hệ thống phức tạp và tốn điện hơn dự kiến.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế cách gửi cảnh báo và thời gian chờ trước khi kích hoạt SOS.

---

# NGÀY 20/06/2026

**GIAI ĐOẠN:** Thiết kế luồng SOS  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Giảm báo động giả nhưng vẫn xử lí được trường hợp người dùng mất ý thức.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Sơ đồ luồng vẽ tay.
- Điện thoại Android.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Ban đầu nhóm nghĩ khi phát hiện ngã thì hiện nút hỏi người dùng có cần giúp không. Sau đó phát hiện vấn đề: nếu người bị ngã bất tỉnh thì họ không thể bấm nút.

Nhóm đổi logic: phát hiện nguy cơ → chạy đếm ngược → người dùng chỉ cần thao tác khi muốn **hủy cảnh báo sai** → nếu không hủy thì hệ thống tự SOS.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chọn thời gian xác minh ban đầu khoảng 10 giây để tiếp tục thử nghiệm, chưa coi đây là giá trị tối ưu.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không được thiết kế hệ thống cứu hộ theo giả định người bị nạn luôn còn tỉnh táo.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế giao diện có nút SOS/hủy cảnh báo đủ lớn cho người lớn tuổi.

---

# NGÀY 25/06/2026

**GIAI ĐOẠN:** Khảo sát nguồn điện thiết bị đeo  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Đặt yêu cầu thời lượng pin và cách cảnh báo pin yếu.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Tài liệu ESP32.
- Tài liệu mạch đo pin.
- Danh sách linh kiện dự kiến.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm đặt mục tiêu thiết bị đeo có thể hoạt động khoảng 2–3 ngày. Cần có khả năng báo pin còn ít, đang sạc, mất nguồn hoặc mất kết nối. MAX17048 được đưa vào danh sách linh kiện dự kiến để theo dõi dung lượng pin.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa lắp mạch hoàn chỉnh nên chưa có thời gian pin đo được.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không thể chỉ quan tâm thuật toán té ngã. Nếu pin hết mà người dùng không biết thì hệ thống mất tác dụng.

## 6. KẾ HOẠCH TIẾP THEO
Tách kế hoạch phát triển Android và ESP32 thành hai phần riêng.

---

# NGÀY 02/07/2026

**GIAI ĐOẠN:** Lập kế hoạch Android  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Viết rõ Android phải làm những nhiệm vụ nào.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy tính.
- Android Studio dự kiến sử dụng.
- Tài liệu thiết kế dự án.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm chia chức năng Android thành: đọc cảm biến điện thoại, nhận dữ liệu ESP32, phát hiện nguy cơ, hiển thị countdown, lưu sự kiện, quản lí người thân, lấy vị trí, gửi cảnh báo và cài đặt ngưỡng thử nghiệm.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có bản kế hoạch chức năng Android ban đầu. Chưa có APK.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Nếu đưa quá nhiều thông số kỹ thuật lên màn hình chính sẽ khó dùng cho người lớn tuổi.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế giao diện đơn giản, ưu tiên trạng thái an toàn và nút SOS.

---

# NGÀY 08/07/2026

**GIAI ĐOẠN:** Lập kế hoạch ESP32  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Xác định phần việc của thiết bị đeo và cách giao tiếp với Android.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- ESP32.
- Danh sách cảm biến.
- Máy tính.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm xác định ESP32 cần đọc cảm biến chuyển động, áp suất/độ cao, trạng thái pin, nút SOS vật lí, đóng gói dữ liệu và truyền sang Android. BLE được chọn làm hướng giao tiếp chính vì phù hợp thiết bị đeo và tiết kiệm điện hơn duy trì Wi‑Fi liên tục.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có dữ liệu truyền thật. Đã có cấu trúc gói dữ liệu dự kiến và yêu cầu đồng bộ thời gian.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Cần thống nhất tên biến và cấu trúc dữ liệu giữa Android và ESP32 từ sớm, nếu không hai phần sẽ khó ghép lại.

## 6. KẾ HOẠCH TIẾP THEO
Bắt đầu chuẩn bị môi trường lập trình và công cụ quản lí mã nguồn.

---

# NGÀY 15/07/2026

**GIAI ĐOẠN:** Chuẩn bị công cụ phát triển  
**THỜI GIAN:** Không ghi lại chính xác – tái dựng hồi cứu

## 1. MỤC TIÊU
Chuẩn bị môi trường để có thể phát triển phần mềm lâu dài và lưu lại lịch sử thay đổi.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy Linux.
- Git và GitHub.
- Hermes Agent, Codex, Antigravity (AGY).

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm và giáo viên hướng dẫn tìm hiểu cách dùng Git/GitHub để lưu mã nguồn. Đồng thời thử các công cụ AI hỗ trợ lập trình.

Một vấn đề gặp phải là nếu AI đọc toàn bộ repository ở mỗi lần làm việc thì tốn nhiều token và dễ lặp lại việc đã làm. Giải pháp được chọn là dùng `rg`, `grep`, `sed` để tìm đúng file và chỉ đọc đoạn liên quan.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Có quy ước dùng Git để lưu mã nguồn.
- Có quy trình khảo sát mã theo từng phần nhỏ.
- Chưa có dữ liệu thực nghiệm về té ngã.

## 5. RÚT KINH NGHIỆM & LỖI SAI
AI chỉ nên đọc đúng phần cần thiết; nếu giao nhiệm vụ quá rộng sẽ tốn tài nguyên và khó kiểm soát thay đổi.

## 6. KẾ HOẠCH TIẾP THEO
Bắt đầu xây dựng bản Android thử nghiệm.

**Ghi nhận sử dụng AI:** dùng Hermes/Codex/AGY hỗ trợ viết và rà soát mã nguồn. Các thay đổi phải được kiểm tra lại trước khi dùng.

---

# NGÀY 05/09/2026

**GIAI ĐOẠN:** Phản biện thuật toán phát hiện té ngã  
**THỜI GIAN:** Buổi tối

## 1. MỤC TIÊU
Kiểm tra xem ý tưởng phát hiện ngã hiện tại còn thiếu trường hợp nào.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Điện thoại Android.
- ESP32 và phương án cảm biến.
- ChatGPT để phản biện.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm xem xét các tình huống đang ngồi rồi ngã, ngã từ ghế, choáng nhưng bám vào vật, không có va đập lớn và người dùng không thể phản hồi.

Nhóm quyết định không cố xác định té ngã chỉ bằng một điều kiện. Cần kết hợp gia tốc, tư thế, thời gian bất động và thay đổi độ cao nếu có.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Chưa có bộ ngưỡng cuối cùng. Xác định rõ rằng các ngưỡng phải có khả năng chỉnh trong giai đoạn thử nghiệm.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Giá trị ngưỡng mặc định do lập trình viên đặt không thể coi là kết quả nghiên cứu nếu chưa thử nghiệm thực tế.

## 6. KẾ HOẠCH TIẾP THEO
Đưa cấu hình ngưỡng lên ứng dụng Android để có thể thay đổi và lưu nhiều bộ thử.

**Ghi nhận sử dụng AI:** ChatGPT được dùng để phản biện các trường hợp báo sai/bỏ sót. Nhóm tự chọn các biến cần thử nghiệm.

---

# NGÀY 11/09/2026

**GIAI ĐOẠN:** Tìm cảm biến áp suất thay thế  
**THỜI GIAN:** Buổi sáng

## 1. MỤC TIÊU
Tìm phương án thay BMP390 khi linh kiện khó mua hoặc hết hàng.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Thông số BMP388, BMP390, BMP390L.
- Thông số DPS310.
- Danh sách linh kiện có thể mua.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm so sánh các cảm biến áp suất có thể dùng cho đo biến thiên độ cao. BMP390 vẫn là phương án mong muốn nhưng cần có linh kiện thay thế để dự án không bị dừng nếu không mua được.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Các lựa chọn được đưa vào danh sách: BMP388, BMP390, BMP390L và DPS310. Chưa có phép đo đối chứng trên cùng một điều kiện.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Không nên thiết kế cả dự án phụ thuộc vào đúng một loại cảm biến.

## 6. KẾ HOẠCH TIẾP THEO
Mua loại phù hợp có sẵn và giữ giao diện phần mềm đủ linh hoạt để thay cảm biến.

---

# NGÀY 14/09/2026

**GIAI ĐOẠN:** Hoàn thiện kế hoạch Android và ESP32  
**THỜI GIAN:** Buổi sáng

## 1. MỤC TIÊU
Viết lại kế hoạch hai phần sao cho có thể triển khai và sau này ghép hệ thống.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy Linux.
- Tài liệu `android-plan.md`.
- Kế hoạch ESP32.
- Git/GitHub.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Kế hoạch Android được chỉnh để nhấn mạnh rằng điện thoại **tự sử dụng cảm biến của chính nó**, không chỉ nhận dữ liệu ESP32. Kiến trúc được chia thành frontend Android, logic phát hiện, backend/API, giao tiếp ESP32 và cảnh báo khẩn cấp.

Nhóm cũng chuẩn hóa tên biến/API để Android và ESP32 dùng cùng ý nghĩa dữ liệu.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
Có tài liệu kế hoạch Android chi tiết và kế hoạch ESP32 riêng.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Bản kế hoạch trước chưa thể hiện đủ vai trò cảm biến điện thoại. Sau khi sửa, Android được coi là một nguồn cảm biến độc lập.

## 6. KẾ HOẠCH TIẾP THEO
Tạo bản APK thử nghiệm và kiểm tra trên điện thoại.

**Ghi nhận sử dụng AI:** AI hỗ trợ chỉnh cấu trúc tài liệu kỹ thuật và sinh một phần mã. Nhóm/giáo viên kiểm tra lại yêu cầu trước khi triển khai.

---

# NGÀY 15/09/2026

**GIAI ĐOẠN:** Tạo APK thử nghiệm và chỉnh giao diện  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Có một APK chạy được để kiểm tra giao diện và dữ liệu cảm biến điện thoại.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Máy Linux.
- Android project.
- Điện thoại Android thật.
- Gradle.
- Hermes/AGY/Codex hỗ trợ lập trình.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Nhóm build APK debug và cài lên điện thoại. Qua quan sát, giao diện ban đầu còn giống màn hình kỹ thuật, nhiều thông tin và chưa cân đối cho người lớn tuổi.

Nhóm đề xuất bố cục mới: nút SOS lớn ở trung tâm, bốn vùng chức năng xung quanh, các khối giao diện bo theo nút trung tâm và màn hình chính chỉ hiển thị thông tin thật sự cần thiết.

Số điện thoại người thân được chuyển vào phần Cài đặt để có thể thêm, sửa, xóa.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Có APK debug chạy được trên thiết bị thử.
- Cảm biến điện thoại đã có dữ liệu đưa vào ứng dụng.
- Giao diện cần tiếp tục chỉnh.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Một giao diện có nhiều thông số có thể thuận tiện cho người nghiên cứu nhưng không phù hợp người sử dụng lớn tuổi. Cần tách “chế độ sử dụng” và “chế độ thử nghiệm”.

## 6. KẾ HOẠCH TIẾP THEO
Thiết kế backend rõ ràng hơn và kết nối frontend qua API.

---

# NGÀY 16/09/2026

**GIAI ĐOẠN:** Thiết kế backend và API  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Tách phần giao diện khỏi phần xử lí dữ liệu và chuẩn hóa giao tiếp Android–backend.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Repository dự án.
- Tài liệu kiến trúc.
- Hermes điều phối AGY/Codex.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Trước khi sửa code, nhóm yêu cầu khảo sát kiến trúc hiện có và ghi lại trong `.ai/architecture.md`.

Backend được thống nhất dùng REST `/api/v1`. Các nhóm chức năng cần có: sensor ingestion, contacts, sự kiện, SOS/countdown/ACK và đồng bộ dữ liệu.

Nhóm quyết định chốt contract trước khi để các worker sửa code song song.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Có tài liệu kiến trúc.
- Có contract API ban đầu.
- Chưa kiểm thử toàn bộ chức năng cứu hộ trên điện thoại thật.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Nếu sửa frontend và backend cùng lúc mà chưa thống nhất interface thì dễ phải sửa đi sửa lại.

## 6. KẾ HOẠCH TIẾP THEO
Đưa toàn bộ mã hiện có lên GitHub và tiếp tục triển khai theo contract.

**Ghi nhận sử dụng AI:** Hermes được dùng để chia việc; AGY ưu tiên frontend; Codex/Sol ưu tiên logic, backend và kiểm thử.

---

# NGÀY 17/09/2026

**GIAI ĐOẠN:** Đưa hệ thống lên GitHub và hoàn thiện các chức năng thử nghiệm  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Lưu phiên bản đầy đủ của dự án và tiếp tục hoàn thiện các chức năng cần cho thử nghiệm.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- GitHub repository `nckh27pa`.
- Android source.
- ESP32 source.
- Backend.
- Git.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Repository có commit đầu tiên:

`2e7bc994fdc94681b59b3c08417886fc0c9ae7f4`  
**Message:** `Initial commit: nckh27pa Android ESP32 backend`

Ở phiên bản này đã có ba phần lớn: Android, ESP32 và backend. Trong Android đã có các thành phần như `HomeScreen.kt`, `ContactsScreen.kt`, `PhoneSensorCollector.kt`, `MonitoringService.kt`, `Esp32PacketDecoder.kt`, `ApiRepository.kt`, `ApiService.kt` và `SyncCoordinator.kt`.

Nhóm tiếp tục thiết kế lại chức năng vị trí. Nút vị trí không nên chỉ mở bản đồ trên điện thoại người bị nạn; mục tiêu là người thân phải nhận được tọa độ hoặc đường link vị trí.

Nhóm cũng thống nhất nội dung cảnh báo cuộc gọi theo hướng: người nhận được thông báo rằng người dùng đã bị ngã và vị trí đã được gửi qua tin nhắn.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- 1 commit gốc lưu đồng thời Android, ESP32 và backend.
- Đã có cấu trúc contacts, sensor collector, API và đồng bộ.
- Chưa coi chức năng gọi/SMS/vị trí là hoàn thiện.

## 5. RÚT KINH NGHIỆM & LỖI SAI
Trước đây nút “Vị trí” được hiểu thiên về mở bản đồ trên điện thoại hiện tại. Cách đó chưa đáp ứng mục tiêu cứu hộ. Vị trí cần được **gửi cho người thân**.

## 6. KẾ HOẠCH TIẾP THEO
- Bổ sung quyền Android cho vị trí, SMS và cuộc gọi.
- Hoàn thiện màn hình xin quyền.
- Kiểm thử cả trường hợp người dùng từ chối quyền.

**Ghi nhận sử dụng AI:** dùng ChatGPT để phân tích luồng cứu hộ và tạo yêu cầu; Hermes/Codex/AGY hỗ trợ triển khai code. Nhóm kiểm tra lại luồng chức năng sau mỗi thay đổi.

---

# NGÀY 18/09/2026

**GIAI ĐOẠN:** Hoàn thiện quyền SOS, hiệu chỉnh ngưỡng và kiểm thử  
**THỜI GIAN:** Trong ngày

## 1. MỤC TIÊU
Hoàn thiện các quyền Android cần cho SOS và kiểm tra luồng xin quyền trong nhiều tình huống.

## 2. DỤNG CỤ & VẬT LIỆU SỬ DỤNG
- Android source.
- Android permission dialogs.
- Script kiểm thử.
- GitHub.
- Backend test.
- Các APK debug đã lưu.

## 3. TIẾN TRÌNH THỰC HIỆN & HIỆN TƯỢNG
Từ commit gốc ngày 17/09 đến commit:

`a995d8b3a7a52fd7467690b1d54705efd947cfcd`  
**Message:** `feat: complete Android SOS permission flow and validation`

GitHub ghi nhận 3 commit mới và nhiều thay đổi. Các phần đáng chú ý được thêm/sửa gồm `PermissionCenterSection.kt`, `CapabilityAccess.kt`, `AndroidCapabilityPlatform.kt`, `AndroidEmergencyLocationController.kt`, `EmergencyCore.kt`, `AndroidSmsManagerGateway.kt`, `ManualSimCallFallback.kt`, `FallDetectionCalibrationScreen.kt`, `FallDetectionProfiles.kt` và `MonitoringForegroundPolicy.kt`.

Nhóm cũng bổ sung nhiều test về permission, location, emergency logic, fall detection profile, ESP configuration và API/sync. Các file bằng chứng kiểm thử được lưu trong `docs/evidence/permission-dialog/` và `docs/evidence/permission-flow/`.

Trong quá trình kiểm thử đã có nhiều trạng thái FAIL. Một số lỗi không phải do chức năng chính mà do script/test-driver chưa xử lí đúng dialog hệ thống. Sau khi chỉnh quy trình kiểm thử, file kết quả cuối `acceptance-results.json` đạt **7/7 PASS** cho bộ kiểm tra permission dialog đã thực hiện.

Cùng ngày nhóm kiểm tra các APK theo thứ tự `app-debug.apk` → `(1)` → `(2)` → `(3)` → `(5)`. Phát hiện `(2)` và `(3)` có cùng SHA-256 nên thực tế là cùng một bản build. Bản `(5)` có thêm cấu hình ngưỡng, profile, REST API `/api/v1` và đồng bộ backend.

## 4. KẾT QUẢ & SỐ LIỆU THÔ
- Permission dialog acceptance: **7/7 PASS** ở lần kiểm tra cuối đã lưu.
- Từ commit gốc đến commit hoàn thiện permission: **3 commit mới**.
- APK `(2)` và `(3)`: trùng nhau ở mức file.
- Bản `(5)` là bản có nhiều chức năng nhất trong nhóm APK đã kiểm tra.

## 5. RÚT KINH NGHIỆM & LỖI SAI
- Test tự động cũng có thể sai; không được xem mọi FAIL là lỗi sản phẩm.
- Cần lưu bằng chứng kiểm thử để phân biệt lỗi chương trình và lỗi script.
- Việc chỉnh ngưỡng phải dựa trên dữ liệu thực nghiệm, không chỉ dựa trên giá trị mặc định trong code.
- Phần mềm đã tiến xa về kiến trúc nhưng vẫn cần thử trên điện thoại thật với SIM, GPS, cảm biến và ESP32.

## 6. KẾ HOẠCH TIẾP THEO
- Kiểm thử SMS và cuộc gọi trên điện thoại có SIM.
- Kiểm thử vị trí trong nhà và ngoài trời.
- Kết nối ESP32 thật.
- Bắt đầu thu dữ liệu các tình huống ngã và không ngã.
- So sánh các bộ profile ngưỡng sau khi có dữ liệu.

**Ghi nhận sử dụng AI:** AI hỗ trợ viết một phần code, tạo test và phân tích lỗi. Kết quả PASS/FAIL được xác định từ quá trình chạy test và bằng chứng lưu trong repository, không lấy từ kết luận do AI tự đưa ra.

---

# PHỤ LỤC A – NHẬT KÍ SỬ DỤNG AI TÓM TẮT

| Giai đoạn | Công cụ | Mục đích sử dụng | Cách kiểm soát |
|---|---|---|---|
| Hình thành ý tưởng | ChatGPT | Phản biện ý tưởng, liệt kê tình huống | Nhóm tự chọn vấn đề nghiên cứu |
| Chọn cảm biến | ChatGPT | So sánh phương án, gợi ý biến cần đo | Kiểm tra lại thông số linh kiện trước khi mua |
| Thiết kế Android/ESP32 | ChatGPT | Hỗ trợ cấu trúc kế hoạch | Nhóm/giáo viên duyệt yêu cầu |
| Viết phần mềm | Hermes, Codex, AGY | Sinh và sửa một phần code | Review diff, build và test |
| Kiểm thử | Codex/AGY/Hermes | Hỗ trợ tạo script, tìm nguyên nhân lỗi | Dựa vào kết quả chạy test và bằng chứng thực tế |
| Viết nhật kí | ChatGPT | Hỗ trợ sắp xếp lại ghi chép theo mẫu | Nội dung được đối chiếu với hồ sơ dự án và GitHub |

---

# PHỤ LỤC B – MỐC MÃ NGUỒN GITHUB

1. **17/09/2026**  
   Commit: `2e7bc994fdc94681b59b3c08417886fc0c9ae7f4`  
   Nội dung: đưa Android, ESP32 và backend lên repository.

2. **18/09/2026**  
   Commit: `a995d8b3a7a52fd7467690b1d54705efd947cfcd`  
   Nội dung: hoàn thiện luồng quyền SOS Android và validation; bổ sung nhiều thành phần emergency, permission, location, calibration và test.

---

# GHI CHÚ CHO CÁC LẦN CẬP NHẬT SAU

Mỗi ngày tiếp theo cần ghi đủ:
1. Mục tiêu.
2. Dụng cụ/vật liệu.
3. Tiến trình thực hiện và hiện tượng.
4. Kết quả và số liệu thô.
5. Rút kinh nghiệm và lỗi sai.
6. Kế hoạch tiếp theo.

Không xóa các thử nghiệm thất bại. Nếu một giả thuyết, cách nối mạch, đoạn code hoặc test bị sai thì vẫn giữ lại và ghi rõ cách sửa.