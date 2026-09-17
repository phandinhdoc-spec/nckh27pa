# Thông tin phần cứng thực sự còn thiếu
Chưa thấy thiết bị qua adb devices hoặc nckh-arduino board list ở khảo sát; chưa gán FQBN/GPIO, chưa flash.
1. Mã bo/module ESP32 chính xác (ảnh rõ hai mặt hoặc mã in), dung lượng flash/PSRAM và phiên bản bo nếu có. Cần để chọn FQBN/options và chân an toàn.
2. IMU đang có (mã chip/module, điện áp, bus/địa chỉ); khí áp kế và bộ đo pin có hay không, mã thực nếu có. Không giả định MPU6050/BMP390/MAX17048 là quyết định đã chọn.
3. Sơ đồ nối hiện tại hoặc chân dự định; nút SOS/SAFE, còi/motor/LED và mạch driver tương ứng. Không cấp tải qua GPIO khi chưa biết dòng.
4. Pin (loại/cell/dung lượng), mạch sạc/bảo vệ/nguồn và chân báo sạc. Sơ đồ nguồn cần được kiểm tra trước cấp pin/flash.
5. Khi thử thực: thiết bị USB/serial nào là bo đích và cho phép nạp firmware; điện thoại Android model/OS và phép cài bản thử. Không cần thông tin này để chạy host core hoặc emulator.
GNSS/4G không chặn lõi; không yêu cầu mua/SIM lúc này.
