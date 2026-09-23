/*
 * test-i2c.ino — Quét bus I2C cho ESP32-C6 SuperMini trên một bộ điều khiển duy nhất
 *
 * Yêu cầu & Bối cảnh phần cứng:
 *   - Bo: ESP32-C6 SuperMini (chỉ có duy nhất 1 controller I2C phần cứng: I2C0).
 *   - MPU6050: SCL = GPIO 6, SDA = GPIO 7 (địa chỉ dự kiến 0x68 hoặc 0x69).
 *   - GY-63 (MS5611): SCL = GPIO 2, SDA = GPIO 3 (địa chỉ dự kiến 0x77 hoặc 0x76).
 *   - Quy ước: pin đầu là SCL, pin sau là SDA.
 *   - Tuyệt đối không khai báo hai đối tượng TwoWire (không dùng TwoWire(1) vì ESP32-C6 không hỗ trợ).
 *   - Sử dụng đối tượng Wire duy nhất, chuyển đổi linh hoạt cặp chân qua GPIO Matrix
 *     bằng cơ chế Wire.end() và Wire.begin(sda, scl, freq).
 *   - Chỉ sử dụng thư viện chuẩn Wire.h, không dùng thư viện ngoài.
 */

#include <Arduino.h>
#include <Wire.h>

// Định nghĩa chân I2C theo settled decisions
// Cặp chân cho MPU6050: SCL = 6, SDA = 7
constexpr int PIN_MPU_SCL = 6;
constexpr int PIN_MPU_SDA = 7;

// Cặp chân cho GY-63 (MS5611): SCL = 2, SDA = 3
constexpr int PIN_GY63_SCL = 2;
constexpr int PIN_GY63_SDA = 3;

// Tần số I2C chuẩn để quét bus
constexpr uint32_t I2C_SCAN_FREQ = 100000; // 100 kHz

// Hàm chuyển đổi chân cho bộ điều khiển I2C phần cứng duy nhất
bool switchI2CPins(int sdaPin, int sclPin, uint32_t freq = I2C_SCAN_FREQ) {
  Wire.end();
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  return Wire.begin(sdaPin, sclPin, freq);
}

// Hàm quét địa chỉ trên bus hiện tại
void scanBus(const char *sensorName, int sclPin, int sdaPin) {
  Serial.println();
  Serial.println("==================================================");
  Serial.printf("Quét bus cho: %s\n", sensorName);
  Serial.printf("Cấu hình chân: SCL = GPIO %d, SDA = GPIO %d\n", sclPin, sdaPin);
  Serial.println("==================================================");

  int deviceCount = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("  -> Tìm thấy thiết bị tại địa chỉ: 0x%02X (%d)", address, address);

      if (address == 0x68) {
        Serial.print("  [MPU6050: AD0 = GND, mặc định]");
      } else if (address == 0x69) {
        Serial.print("  [MPU6050: AD0 = 3.3V]");
      } else if (address == 0x77) {
        Serial.print("  [GY-63 / MS5611: CSB = VCC, mặc định]");
      } else if (address == 0x76) {
        Serial.print("  [GY-63 / MS5611: CSB = GND]");
      }

      Serial.println();
      deviceCount++;
    } else if (error == 4) {
      Serial.printf("  ! Lỗi đường truyền tại địa chỉ 0x%02X\n", address);
    }
  }

  if (deviceCount == 0) {
    Serial.println("  -> Không tìm thấy thiết bị nào trên cặp chân này.");
  } else {
    Serial.printf("  -> Tổng cộng: %d thiết bị tìm thấy trên %s.\n", deviceCount, sensorName);
  }
}

void performScan() {
  // 1. Quét cặp chân MPU6050 (SCL = GPIO 6, SDA = GPIO 7)
  // Trong Arduino ESP32: Wire.begin(sda, scl, freq) nhận SDA trước, SCL sau
  Serial.printf("\n[1] Cấu hình I2C cho MPU6050 (SDA = GPIO %d, SCL = GPIO %d)...\n", PIN_MPU_SDA, PIN_MPU_SCL);
  if (switchI2CPins(PIN_MPU_SDA, PIN_MPU_SCL)) {
    scanBus("MPU6050", PIN_MPU_SCL, PIN_MPU_SDA);
  } else {
    Serial.println("! Lỗi: Không thể khởi tạo I2C trên chân MPU6050.");
  }

  delay(100);

  // 2. Quét cặp chân GY-63 (SCL = GPIO 2, SDA = GPIO 3)
  Serial.printf("\n[2] Chuyển đổi chân I2C cho GY-63 (SDA = GPIO %d, SCL = GPIO %d)...\n", PIN_GY63_SDA, PIN_GY63_SCL);
  if (switchI2CPins(PIN_GY63_SDA, PIN_GY63_SCL)) {
    scanBus("GY-63 (MS5611)", PIN_GY63_SCL, PIN_GY63_SDA);
  } else {
    Serial.println("! Lỗi: Không thể chuyển đổi I2C sang chân GY-63.");
  }
}

void setup() {
  Serial.begin(115200);

  // Chờ cổng Serial ổn định (đặc biệt khi dùng giao tiếp USB CDC của ESP32-C6)
  delay(2000);

  Serial.println("\n**************************************************");
  Serial.println("  ESP32-C6 SuperMini — Single Controller I2C Scanner");
  Serial.println("**************************************************");
  Serial.println("Bo chỉ có 1 I2C controller phần cứng (I2C0).");
  Serial.println("Code sử dụng kỹ thuật luân chuyển chân qua GPIO Matrix.");

  performScan();

  Serial.println("\n**************************************************");
  Serial.println("  Hoàn tất lần quét đầu tiên.");
  Serial.println("  (Sẽ tự động quét lại sau mỗi 5 giây)");
  Serial.println("**************************************************");
}

void loop() {
  delay(5000);
  Serial.println("\n>>> Bắt đầu chu kỳ quét mới...");
  performScan();
}
