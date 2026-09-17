# Quyết định và khoảng trống
Không sửa hai kế hoạch gốc. “Đề xuất” không phải quyết định đã được chủ dự án duyệt.

|ID|Vị trí và vấn đề|Tác động|Cách xử lý / trạng thái|
|---|---|---|---|
|D01/C01|ESP:129 ESP-IDF trái chỉ thị hiện tại Arduino|Toolchain/driver/build|ĐÃ QUYẾT bởi chủ dự án: Arduino ecosystem, Arduino CLI, dùng chung ~/.arduinoIDE/arduino-cli.yaml. Không sửa kế hoạch gốc. Chưa chốt FQBN.|
|D02|Chỉ thị hiện tại + Android §2,4.5|Android không được chỉ nhận ESP|ĐÃ QUYẾT: PHONE_ONLY là đường độc lập bắt buộc.|
|D03|Android:215,658; ESP:19–21|Không phản hồi/GPS yếu|ĐÃ QUYẾT: deadline tự cảnh báo; GPS không chặn; thử nghiệm chỉ test sink.|
|C02|Android:411 “mỗi gói có sequenceNumber” nhưng Status:502–515 và ACK:600–607 không có; ESP:366 nêu ngoại lệ|Decoder/schema|ĐỀ XUẤT giữ schema cụ thể: sensor/event có seq, status/ACK không thêm; chốt trước IF-002.|
|C03|Android:410 Unix time; ESP:359–368 đề xuất time=0/reserve seq|Reboot/dedupe/time/fusion|ĐỀ XUẤT áp dụng đồng thời hai phía với test; chưa triển khai/bật BLE.|
|C04|ESP:370–376, Android:413|JSON vượt MTU, chưa byte spec/checksum/timeouts|CHƯA CHỐT; IF-002 thiết kế frame/vector bounded; không tuyên bố BLE tương thích.|
|C05|Android:653–657 muốn wearable location; ESP:395 v1 chưa tọa độ|Thiếu schema|ĐỀ XUẤT M1 dùng điện thoại/không vị trí; GNSS cần phiên bản sau. Không bỏ yêu cầu dài hạn.|
|C06|ESP:305–307,335–351 bổ sung lỗi/parameters; pin int bắt buộc|Pin lỗi/nullability và command|CHƯA CHỐT extensions. Không giả0%/đổi trường; dùng SENSOR_ERROR. IF-002 chốt bảng tham số.|
|C07|ESP:221,226 CRITICAL; Android:365,380–384 cần đa dấu hiệu; SOS_CANCELLED ESP:329 thiếu liên kết|Có thể gửi sai/hủy nhầm|ĐỀ XUẤT ma trận eventType×severity, một alert cục bộ, không replay hủy không liên kết; review an toàn trước BLE.|
|D04|Chỉ thị quyền và phạm vi|Bảo toàn dữ liệu, tránh gửi nhầm|Không sudo, không flash, không gửi người thật, không secrets Git. Git init local chưa commit/push.|
|P01|M1 PHONE_ONLY demo|Có kết quả độc lập phần cứng|Lựa chọn thứ tự kỹ thuật của Hermes trong phạm vi; không thay tiêu chí phiên bản đầu. Demo foreground không thay nghĩa vụ chạy nền.|

Chốt bo/SIM/mua linh kiện/phát hành là quyền chủ dự án. Giá trị timeout/frame/ngưỡng là đề xuất kỹ thuật phải được ghi và phản biện; thay đổi phạm vi/giao thức không tương thích cần chủ dự án duyệt.
