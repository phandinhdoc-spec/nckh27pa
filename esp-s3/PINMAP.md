# BẢNG PHÂN BỔ CHÂN GPIO (PINMAP) — ESP32-S3 SUPER MINI

Tài liệu này xác định phân bổ chân GPIO cho sản phẩm chính thức NCKH27PA chạy trên vi điều khiển **ESP32-S3 Super Mini**.

---

## 1. Bảng phân bổ chân chi tiết

| GPIO | Tên tín hiệu | Hướng | Nguồn gốc / Cơ sở | Trạng thái | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GPIO 8** | `I2C0_SDA` | I/O | Phần cứng chốt (verified) | **CONFIRMED** | Bus I2C 0: MPU6050 SDA (GY-521). Kéo trở ngoài 4.7k. Tần số 400 kHz. |
| **GPIO 9** | `I2C0_SCL` | Out | Phần cứng chốt (verified) | **CONFIRMED** | Bus I2C 0: MPU6050 SCL (GY-521). Tần số 400 kHz. |
| **GPIO 7** | `I2C1_SDA` | I/O | Phần cứng chốt (verified) | **CONFIRMED** | Bus I2C 1: MS5611 SDA (GY-63). Kéo trở ngoài 4.7k. Tần số 400 kHz. |
| **GPIO 6** | `I2C1_SCL` | Out | Phần cứng chốt (verified) | **CONFIRMED** | Bus I2C 1: MS5611 SCL (GY-63). Tần số 400 kHz. |
| **GPIO 4** | `BTN_SOS` | In | Giả định thiết kế | `TODO(HW)` | Nút nhấn khẩn cấp SOS. Cấu hình `INPUT_PULLUP`, tích cực mức THẤP. Nhấn giữ >= 2s. |
| **GPIO 5** | `BTN_CANCEL`| In | Giả định thiết kế | `TODO(HW)` | Nút hủy cảnh báo (an toàn). Cấu hình `INPUT_PULLUP`, tích cực mức THẤP. Giữ >= 300ms. |
| **GPIO 1** | `BUZZER` | Out | Giả định thiết kế | `TODO(HW)` | Tín hiệu điều khiển còi báo động hoặc mô tơ rung. |
| **GPIO 8** | `LED_STATUS`| Out | Chuẩn bo Super Mini | `TODO(HW)` | LED báo trạng thái bo mạch (thường là LED tích hợp màu xanh trên ESP32-S3 Super Mini). |
| **GPIO 9** | `LED_BAT_1` | Out | Giả định thiết kế | `TODO(HW)` | LED dải pin mức 1 (Đỏ / < 25%). |
| **GPIO 10**| `LED_BAT_2` | Out | Giả định thiết kế | `TODO(HW)` | LED dải pin mức 2 (Vàng / 50%). |
| **GPIO 11**| `LED_BAT_3` | Out | Giả định thiết kế | `TODO(HW)` | LED dải pin mức 3 (Xanh / 100%). |
| **GPIO 12**| `BAT_ADC` | In | Giả định thiết kế | `TODO(HW)` | Chân đọc điện áp pin qua cầu chia trở (ADC1 Channel 1). |

---

## 2. Nguồn gốc & Phân loại tính xác thực

### A. Nhóm chân ĐÃ CÓ CĂN CỨ (CONFIRMED)
Phần cứng chốt (verified):
- `I2C0_SDA = 8`, `I2C0_SCL = 9` (cho MPU6050)
- `I2C1_SDA = 7`, `I2C1_SCL = 6` (cho MS5611)
Việc tách riêng 2 bus I2C vật lý trên ESP32-S3 giúp tránh hiện tượng xung đột địa chỉ, giảm độ trễ và đảm bảo tính độc lập khi một cảm biến gặp sự cố bus.

### B. Nhóm chân GIẢ ĐỊNH CHỜ SƠ ĐỒ MẠCH (`TODO(HW)`)
Các chân ngoại vi còn lại (nút bấm, còi, LED, ADC pin) hiện chưa có sơ đồ nguyên lý (schematic) phần cứng cuối cùng từ đội phần cứng (Hardware Team). 
- Trong mã nguồn `esp-s3.ino`, tất cả các chân này được gom tại mục `PINMAP & HARDWARE DEFINITIONS` ở đầu file và gán nhãn `TODO(HW)`.
- Khi có sơ đồ PCB chính thức, chỉ cần thay đổi số chân GPIO tại các hằng số này mà không phải sửa logic bên dưới.
