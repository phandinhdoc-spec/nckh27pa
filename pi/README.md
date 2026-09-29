# Pi chính thức — triển khai theo từng phần

Không nhập mã runtime từ `Pi 3/`; thư mục đó chỉ là bản tham khảo cũ.

Giai đoạn 1: `gestures.py` là bộ phân loại sự kiện nút thuần Python và trạng thái bật/tắt Plan 1. Chưa kết nối GPIO25 hoặc service đang chạy, chưa thu âm/gửi server. Nhấn đôi/ba chỉ trả sự kiện dành riêng, không kích hoạt chế độ chưa triển khai. Nút một lần dừng Plan 1; lần sau khởi động phiên mới. Phân loại đợi 1,5 giây sau lần bấm cuối; debounce 60 ms, mốc đúng 1,5 giây được tính vào cùng chuỗi.

Kiểm thử: `PYTHONPATH=. python3 -m unittest discover -s tests -v`.

Triển khai thử nghiệm vào `/home/hieunga/pi/` trên Pi, không chạm `/opt/thiennhan` hoặc service hiện hành. Phần tiếp theo cần kết nối một chủ GPIO duy nhất, xử lý hủy, mic/VAD, Groq và giao thức Plan 1 chỉ TEXT với server Python; kiểm thử trước khi chuyển service.
