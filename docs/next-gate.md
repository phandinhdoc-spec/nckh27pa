# Cổng tích hợp tiếp theo — cần chủ dự án chốt
Không xem lab proposal là quyết định đã duyệt. Chưa bật BLE thật hoặc gửi người thật.

1. Cho phép dùng framing draft0.1 làm chuẩn nguyên mẫu tích hợp?
   Đề xuất Hermes: header16byte little-endian, JSONv1 giữ nguyên, payload≤1024byte,≤2message đang ráp, timeout2s, không checksum mới. Host Kotlin/C++ và sanitizer đã qua; đo MTU/RAM/pin trên bo vẫn bắt buộc. Chi tiết docs/interface-framing-draft.md. ACK2s/2retry và backoff còn là đề xuất chưa nghiệm thu, không gom vào lời tuyên bố tương thích.
2. Thiết bị đích thực tế?
   Cần mã/ảnh rõ bo ESP32 và IMU, danh sách khí áp/pin monitor nếu có, mạch nguồn/sạc, sơ đồ nối/nút/còi/rung. Xem hardware-needed.md. Không chọn FQBN/GPIO hoặc flash trước thông tin này và phép nạp riêng.
3. Cảnh báo ra ngoài sau này dùng kênh nào và người nhận thử nào được phép?
   Chưa yêu cầu cung cấp số điện thoại qua chat lúc này. Hermes đề xuất tiếp tục fake sink cho tới khi chủ dự án chọn SMS/cuộc gọi/kênh thông báo phù hợp và xác nhận người thử, giới hạn quyền Android. Không tự mua dịch vụ/SIM/backend hoặc phát hành.

Android hiện đủ demo foreground + thử FGS trên emulator, chưa là bản đầu đầy đủ theo Android§12. Persist/Room, location/bước/pin/âm-rung, BLE/fusion và nghiên cứu còn backlog, không bị đánh dấu hoàn thành. Các hạng mục nghiệm thu thực địa và giao thức tích hợp phụ thuộc những chốt trên; không lách bằng bo giả, tọa độ giả trình bày như thật, hoặc người nhận thật chưa được phép.
