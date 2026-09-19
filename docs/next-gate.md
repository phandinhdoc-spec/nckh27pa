# Cổng tích hợp tiếp theo — cần chủ dự án chốt
Không xem lab proposal là quyết định đã duyệt. Chưa bật BLE thật hoặc gửi người thật.

1. Cho phép dùng framing draft0.1 làm chuẩn nguyên mẫu tích hợp?
   Đề xuất Hermes: header16byte little-endian, JSONv1 giữ nguyên, payload≤1024byte,≤2message đang ráp, timeout2s, không checksum mới. Host Kotlin/C++ và sanitizer đã qua; đo MTU/RAM/pin trên bo vẫn bắt buộc. Chi tiết docs/interface-framing-draft.md. ACK2s/2retry và backoff còn là đề xuất chưa nghiệm thu, không gom vào lời tuyên bố tương thích.
2. Thiết bị đích thực tế?
   Cần mã/ảnh rõ bo ESP32 và IMU, danh sách khí áp/pin monitor nếu có, mạch nguồn/sạc, sơ đồ nối/nút/còi/rung. Xem hardware-needed.md. Không chọn FQBN/GPIO hoặc flash trước thông tin này và phép nạp riêng.
3. Cảnh báo ra ngoài sau này dùng kênh nào và người nhận thử nào được phép?
   Chưa yêu cầu cung cấp số điện thoại qua chat lúc này. Hermes đề xuất tiếp tục fake sink cho tới khi chủ dự án chọn SMS/cuộc gọi/kênh thông báo phù hợp và xác nhận người thử, giới hạn quyền Android. Không tự mua dịch vụ/SIM/backend hoặc phát hành.

Android hiện đủ demo foreground + thử FGS trên emulator, chưa là bản đầu đầy đủ theo Android§12. Persist/Room, location/bước/pin/âm-rung, BLE/fusion và nghiên cứu còn backlog, không bị đánh dấu hoàn thành. Các hạng mục nghiệm thu thực địa và giao thức tích hợp phụ thuộc những chốt trên; không lách bằng bo giả, tọa độ giả trình bày như thật, hoặc người nhận thật chưa được phép.

4. SOS có được phép TỰ ĐỘNG gọi SIM khi adapter thoại backend chưa cấu hình?
   ĐÃ CHỐT bởi chủ dự án (xem D09 `docs/decisions.md`, cập nhật 2026-09-19): có — một lần duy nhất, tới
   người nhận ưu tiên, chỉ sau khi SMS đã được chuyển cho thiết bị gửi. Cuộc gọi SIM là nhánh cứu hộ ĐỘC LẬP:
   adapter thoại/thông báo máy chủ là phụ trợ best-effort, nên máy chủ đã tiếp nhận (`STARTED`), máy chủ lỗi
   hay máy chủ vắng mặt đều KHÔNG chặn cuộc gọi SIM; SMS và vị trí cũng không quyết định cuộc gọi đó.
   Thiếu `CALL_PHONE` thì báo thiếu quyền, không crash, các nhánh khác vẫn chạy.
   Thao tác gọi tay trong thẻ "GỌI NGƯỜI THÂN" không đổi. Muốn đổi số lần/thứ tự/người nhận cần quyết định mới.
5. Kiểm thử thực địa quyền/SOS trên điện thoại thật có SIM và GPS?
   Emulator đã xác minh permission center, luồng xin/từ chối/từ chối vĩnh viễn, mở App Settings, cấp lại quyền, vị trí gần đúng, mở Google Maps có/không cài, và suy giảm SOS không crash. Còn phải thử trên thiết bị thật: hộp thoại quyền theo nhà sản xuất, đa SIM và biên nhận SMS thật, cuộc gọi SIM thật, GPS ngoài trời, hạn chế FGS khi khoá màn hình, và người nhận thử được chủ dự án cho phép (xem docs/permission-flow.md §Manual test).
