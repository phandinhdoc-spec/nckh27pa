/*
 * ================================================================
 * FallSafe v13 — Thiế t bị phát hiện ngã dùng ESP32-S3
 * ================================================================
 *
 * CÁCH HOẠT ĐỘNG (giải thích cho học sinh lớp 9):
 * ─────────────────────────────────────────────────
 * Thiế t bị đeo trên người. Bên trong có 2 cảm biến:
 *
 * 1. MPU-6050 — đo gia tố c (g) và tốc độ xoay (gyro).
 *     Giống như cảm biến trong điện thoại khi chơi game nghiêng.
 *
 * 2. GY-63 (MS5611) — đo áp suấ t không khí.
 *     Áp suất thay đổi khi độ cao thay đổi, giúp biết người bị ngã
 *     xuống hay đứng dậy.
 *
 * Bộ nhận dạng ngã hoạt động theo 4 bước:
 *
 *    THEO_DOI → NGHI_NGO → XAC_MINH → CANH_BAO
 *
 *    • THEO_DOI: Bình thường, theo dõi liên tục.
 *    • NGHI_NGO: Phát hiện gia tốc giảm mạnh (rơi tự do ≈ 0g).
 *    • XAC_MINH: Va đập + xoay, hoặc va đập rất mạnh → đợi nằm yên.
 *    • CANH_BAO: Xác nhận ngã → gửi SOS qua Bluetooth đến điện thoại.
 *
 * v13: sử a tuổi mẫu MPU, đếm mẫu bị loại; GY-63 không xóa chuỗ i MPU.
 * Va đập thường vẫ n cần bằng chứng GY-63. Va đập rất mạnh sau rơi tự do
 * có thểxác nhận bằng MPU + nằm yên. Không thay các ngưỡng đã lưu NVS.
 * Chờ ACK: vẫ n đọc cảm biến, giữ một cảnh báo chờ gửi; không tạo SOS lặp.
 * SOS thật được ACK mới nghỉ 5 phút; hế t hạn ACK thì theo dõi lại ngay.
 *
 * Dữ liệu gử i qua Bluetooth Low Energy (BLE) dưới dạng JSON.
 * Điện thoại Android nhận và hiể n thị cảnh báo cho người thân.
 *
 * Phầ n cứng: ESP32-S3 Super Mini
 *    MPU-6050: SDA → GPIO7, SCL → GPIO5 (Wire)
 *    GY-63:    SDA → GPIO10, SCL → GPIO8 (Wire1)
 * ================================================================
 */

// ================================================================
// DANH SÁCH THƯ VIỆN DỰNG SẴ N (Lớp 8 mượn công cụ có sẵn từ chuyên gia):
// - Arduino.h, Wire.h: Mượn đểđiề  u khiển chân ESP32 và bus truyền thông I2C.
// - BLEDevice.h, BLEServer.h...: Mượn đểphát sóng Bluetooth (không phải tự viế   t mã sóng).
// - Adafruit_MPU6050.h: Mượn của hãng Adafruit đểbật chip MPU-6050.
// - LittleFS.h, Preferences.h: Mượn của ESP32 đểlưu cấ  u hình vào Flash (tắ t nguồn không mất).
// - cJSON.h: Mượn đểđọc hiể  u dữ liệu JSON trao đổi với điện thoại Android.
// - math.h: Mượn hàm toán học sqrtf() tính căn bậc hai véc-tơ gia tố c.
// ================================================================
// ── Thư viện ngoài ──────────────────────────────────────────────
#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_MPU6050.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <cJSON.h>
#include <atomic>
#include <math.h>

// ================================================================
// PHẦ N 1: CẤU HÌNH NGƯỠNG PHÁT HIỆN NGÃ
// [NHÓM TỰ CODE - BỘ THÔNG SỐVẬT LÝ CỦ ỦA ĐỀTÀI]
// (Do Thi & Hân nghiên cứu thửnghiệm thực tếvà tự lập trình định nghĩa;
//   các con sốnày được điề u chỉnh linh hoạt từ điện thoại qua BLE)
// ================================================================
struct CauHinhNga
{



  // -- Ngưỡng gia tốc --
  float nguongVaDapMs2 = 13.0f;     // Ngưỡng xét va đập; chưa đủ đểkế t luận ngã (m/s²)
  float giaTocNamYenMs2 = 9.81f;    // Gia tốc khi nằm yên ≈ 9.81 m/s² (trọng lực)
  float saiSoNamYenMs2 = 1.0f;      // Sai sốcho phép quanh mức nằm yên (±1 m/s²)
  float nguongRoiTuDoMs2 = 4.9f;    // Gia tốc < giá trị này → đang rơi (≈ 0.5g)
  float nguongXoayManhDps = 120.0f; // Tốc độ xoay > giá trị này là "ngã lộn" (°/s)

  // -- Ngưỡng thời gian --
  uint32_t thoiGianRoiTuDoToiThieuMs = 80; // Rơi phải kéo dài ít nhấ t 80ms
  uint32_t cuaSoSauVaDapMs = 3000;          // Sau va đập, xác minh trong tố i đa 3 giây
  uint32_t thoiGianNamYenSauVaDapMs = 1000; // Phải nằm yên ít nhất 1 giây
  uint16_t soMauNamYenToiThieu = 6;         // Cần ít nhất 6 mẫu liên tiếp nằm yên
  uint32_t khoangDutMauToiDaMs = 250;       // Nếu mất mẫu > 250ms, bắt đầ u lại
  uint32_t thoiHanMauMs = 100;              // Mẫu cảm biến cũ hơn 100ms bị bỏ

  // -- Bằng chứng áp suất (hỗtrợ xác nhận ngã) --
  float mucTangApSuatToiThieuPa = 12.0f; // Áp suất tăng ≥ 12 Pa → người đã xuống thấp
  uint32_t cuaSoApSuatMs = 5000;         // Chỉ dùng bằng chứng áp suất trong 5 giây
  float mucGiamDoCaoToiThieuM = -0.40f; // Độ cao giảm ≥ 40cm → bổsung bằng chứng ngã

  // -- Va đập rất mạnh thì không cần xoay (v12) --
  // Va đập ≥ hệ số× nguongVaDapMs2 được chấ p nhận dù không xoay mạnh
  // (ngã thẳng xuống). Đặt 0 đểTẮ T (luôn đòi xoay như v11).
  float heSoVaDapManh = 2.0f;

  // Kiểm tra xem bộ thông sốcó hợp lệ không (tránh lưu giá trị vô lý vào thiế t bị)
  bool hopLe() const
  {
    return isfinite(nguongVaDapMs2) && nguongVaDapMs2 >= 1 && nguongVaDapMs2 <= 100 &&
           isfinite(giaTocNamYenMs2) && giaTocNamYenMs2 >= 0 && giaTocNamYenMs2 <= 20 &&
           isfinite(saiSoNamYenMs2) && saiSoNamYenMs2 >= 0.1f && saiSoNamYenMs2 <= 10 &&
           cuaSoSauVaDapMs >= 500 && cuaSoSauVaDapMs <= 10000 &&
           thoiGianNamYenSauVaDapMs >= 100 && thoiGianNamYenSauVaDapMs <= cuaSoSauVaDapMs &&
           soMauNamYenToiThieu >= 2 && soMauNamYenToiThieu <= 100 &&
           khoangDutMauToiDaMs >= 10 && khoangDutMauToiDaMs <= 2000 &&
           isfinite(nguongRoiTuDoMs2) && nguongRoiTuDoMs2 >= 0.1f && nguongRoiTuDoMs2 <= 20 &&
           thoiGianRoiTuDoToiThieuMs >= 20 && thoiGianRoiTuDoToiThieuMs <= 1000 &&
           isfinite(nguongXoayManhDps) && nguongXoayManhDps >= 1 && nguongXoayManhDps <= 2000 &&
           isfinite(mucTangApSuatToiThieuPa) && mucTangApSuatToiThieuPa >= 1 && mucTangApSuatToiThieuPa <= 1000 &&
           cuaSoApSuatMs >= 100 && cuaSoApSuatMs <= 10000 &&
           isfinite(mucGiamDoCaoToiThieuM) && mucGiamDoCaoToiThieuM >= -10 && mucGiamDoCaoToiThieuM <= -0.01f &&
           thoiHanMauMs >= 20 && thoiHanMauMs <= 2000 &&
           isfinite(heSoVaDapManh) && (heSoVaDapManh == 0 || (heSoVaDapManh >= 1 && heSoVaDapManh <= 10));
  }
};

// ================================================================
// PHẦ  N 2: BỘ NHẬN DẠNG NGÃ
// [NHÓM TỰ CODE 100% - "TRÁI TIM" VÀ THUẬT TOÁN ĐỘC QUYỀ     ỦA ĐỀTÀI]
                                                           N CỦ
// (Máy trạng thái 4 bước: THEO_DOI → NGHI_NGO → XAC_MINH → CANH_BAO.
//    Không có thư viện ngoài nào làm sẵn; toàn bộ logic nhận diện rơi tự do,
//    va đập, xoay tròn, nằm yên và bù trừ thời gian v13 đều do nhóm tự viết)
// ================================================================
enum class TrangThaiNga : uint8_t
{
   THEO_DOI,
   NGHI_NGO,
   XAC_MINH,
   CANH_BAO
};

struct BoPhatHienNga
{
  CauHinhNga cauHinh;
  TrangThaiNga trangThai = TrangThaiNga::THEO_DOI;

  uint16_t maSuKien = 0;    // Mỗi lần ngã có 1 mã riêng đểđiện thoại biết
  uint32_t soLanDutMau = 0; // Đếm sốlầ n mất mẫu (dùng gỡ lỗi)

  // Các mốc thời gian (lưu millis() của từng sự kiện)
  uint32_t lucLayMauTruocMs = 0;
  uint32_t lucBatDauRoiTuDoMs = 0;
  uint32_t lucXoayManhMs = 0;
  uint32_t lucVaDapMs = 0;
  uint32_t lucBatDauNamYenMs = 0;


// Dữ liệu độ cao và áp suất
float doCaoThamChieuM = 0;    // Độ cao "bình thường" (cập nhật chậm khi đứng)
float doCaoTruocNgaM = 0;     // Độ cao lúc bắt đầu rơi
float doThayDoiDoCaoM = 0;    // Độ cao hiện tại - độ cao lúc rơi
float apSuatTruocNgaPa = NAN; // Áp suất lúc bắt đầu rơi

uint16_t soMauNamYen = 0;
bool daCoMau = false;
bool daCoDoCaoThamChieu = false;
bool dangRoiTuDo = false;
bool daThayXoayManh = false;
bool dangNamYen = false;
bool daCoVaDapRatManh = false; // Ghi nhận tại va đập, không tính lại từ mẫu nằm yên

// Thông tin gỡ lỗi (in ra Serial khi chuyển trạng thái)
char ghiChu[140] = {};
float dinhGiaToc = 0, dinhXoay = 0, doCaoThapNhatM = 0;
uint32_t namYenDaiNhatMs = 0;

// Ghi lý do hủy nếu đang trong NGHI_NGO / XAC_MINH
void ghiLyDoHuy(const char *lyDo)
{
  if (trangThai == TrangThaiNga::NGHI_NGO || trangThai == TrangThaiNga::XAC_MINH)
    snprintf(ghiChu, sizeof(ghiChu), "huy: %s", lyDo);
}

// Xóa sạch bằng chứng ngã đang xét (dùng khi điều kiện không thỏa)
void xoaUngVienNga()
{
  if (trangThai != TrangThaiNga::CANH_BAO)
    trangThai = TrangThaiNga::THEO_DOI;
  daCoDoCaoThamChieu = dangRoiTuDo = daThayXoayManh = dangNamYen = false;
  doThayDoiDoCaoM = 0;
  apSuatTruocNgaPa = NAN;
  soMauNamYen = 0;
  daCoVaDapRatManh = false;
}

// Áp dụng cấu hình mới (xóa trạng thái cũ đểtránh nhầ
                                                     m lẫn)
void apDungCauHinh(const CauHinhNga &ch)
{
  cauHinh = ch;
  xoaUngVienNga();
  daCoMau = false;
}

/*
  * Hàm chính: nhận mẫ  u cảm biến và trả vềTRUE khi xác nhận ngã.
  *
  * Tham số  :
  *    t           — thời điểm hiện tại (millis)
  *    giaToc      — độ lớn gia tốc (m/s²), √(ax²+ay²+az²)
  *    xoay        — tốc độ xoay tổng hợp (°/s)
  *    doCao       — độ cao tương đối so với mốc ban đầu (m)
  *    hopLe       — mẫu MPU có hợp lệ và còn hạn không; GY-63 thiếu thì truyền NAN
  *    apSuatPa    — áp suất hiện tại (Pa), có thểlà NAN nế u chưa có
  */
bool xuLyMau(uint32_t t, float giaToc, float xoay, float doCao,
                bool hopLe, float apSuatPa = NAN)
{
    // Nếu đã phát hiện ngã rồ i thì không xửlý nữa
    if (trangThai == TrangThaiNga::CANH_BAO)
      return false;

  // Nếu cảm biến lỗi → xóa bằng chứng và thoát
  if (!hopLe || !isfinite(giaToc) || !isfinite(xoay))
  {
    ++soLanDutMau;
    ghiLyDoHuy("MPU khong hop le / mau qua han");
    xoaUngVienNga();
    daCoMau = false;
    return false;
  }



// Nếu mất mẫu quá lâu → coi như chuỗi bằng chứng bị gián đoạn, bắt đầu lại
if (daCoMau && uint32_t(t - lucLayMauTruocMs) > cauHinh.khoangDutMauToiDaMs)
{
  ++soLanDutMau;
  ghiLyDoHuy("mat mau qua lau");
  xoaUngVienNga();
}
lucLayMauTruocMs = t;
daCoMau = true;

// Cập nhật độ cao tham chiếu (chỉ lần đầu) và ghi nhận xoay mạnh
if (!isfinite(doCao))
{
  // Không dùng mốc cũ nếu GY-63 mất mẫu hoặc khởi tạo lại.
  daCoDoCaoThamChieu = false;
  doCaoTruocNgaM = NAN;
  apSuatTruocNgaPa = NAN;
}
if (!daCoDoCaoThamChieu && isfinite(doCao))
{
  doCaoThamChieuM = doCao;
  daCoDoCaoThamChieu = true;
}
if (xoay > cauHinh.nguongXoayManhDps)
{
  daThayXoayManh = true;
  lucXoayManhMs = t;
}

// ── Bước 1: THEO_DOI → NGHI_NGO ─────────────────────────────
// Khi gia tốc giảm xuống dưới ngưỡng rơi tự do → bắt đầu nghi ngờ
if (trangThai == TrangThaiNga::THEO_DOI)
{
  if (giaToc < cauHinh.nguongRoiTuDoMs2)
  {
    if (!dangRoiTuDo)
    {
      dangRoiTuDo = true;
      lucBatDauRoiTuDoMs = t;
      doCaoTruocNgaM = daCoDoCaoThamChieu ? doCaoThamChieuM : NAN;
      apSuatTruocNgaPa = apSuatPa;
    }
    // Rơi đủ lâu → chuyển sang NGHI_NGO
    if (uint32_t(t - lucBatDauRoiTuDoMs) >= cauHinh.thoiGianRoiTuDoToiThieuMs)
    {
      trangThai = TrangThaiNga::NGHI_NGO;
      dinhGiaToc = giaToc;
      dinhXoay = xoay;
    }
  }
  else
  {
    dangRoiTuDo = false;
    // Cập nhật chậm độ cao "bình thường" (lọc thấp 2%)
    if (isfinite(doCao) && daCoDoCaoThamChieu)
      doCaoThamChieuM += 0.02f * (doCao - doCaoThamChieuM);
  }
}

// ── Bước 2: NGHI_NGO → XAC_MINH ─────────────────────────────
// Cần va đập mạnh + xoay gần đây (trong 150ms) đểxác nhận có ngã
if (trangThai == TrangThaiNga::NGHI_NGO)
{
  if (giaToc > dinhGiaToc)
    dinhGiaToc = giaToc;
  if (xoay > dinhXoay)
    dinhXoay = xoay;
  if (uint32_t(t - lucBatDauRoiTuDoMs) > 1500)
  {
    snprintf(ghiChu, sizeof(ghiChu),
             "huy NGHI_NGO sau 1.5s: dinh gia toc=%.1f (can>=%.1f), dinh xoay=%.0f (can>%.0f)",
             dinhGiaToc, cauHinh.nguongVaDapMs2, dinhXoay, cauHinh.nguongXoayManhDps);
    xoaUngVienNga();
    return false;
  }


    // v12: va đập ≥ ngưỡng và (có xoay gần đây HOẶC va đập rất mạnh)
    const bool coXoayGanDay = daThayXoayManh && uint32_t(t - lucXoayManhMs) <= 150;
    const bool vaDapRatManh = cauHinh.heSoVaDapManh > 0 &&
                              giaToc >= cauHinh.nguongVaDapMs2 * cauHinh.heSoVaDapManh;
    if (giaToc >= cauHinh.nguongVaDapMs2 && (coXoayGanDay || vaDapRatManh))
    {
      trangThai = TrangThaiNga::XAC_MINH;
      daCoVaDapRatManh = vaDapRatManh;
      lucVaDapMs = t;
      dangNamYen = false;
      soMauNamYen = 0;
      namYenDaiNhatMs = 0;
      doCaoThapNhatM = 0;
      snprintf(ghiChu, sizeof(ghiChu), "va dap=%.1f m/s2, dinh xoay=%.0f dps (%s)",
               giaToc, dinhXoay, coXoayGanDay ? "co xoay" : "khong xoay, va dap rat manh");
    }
}

// ── Bước 3: XAC_MINH → CANH_BAO ─────────────────────────────
// Nằm yên đủ lâu + (bằng chứng GY-63 HOẶC va đập rất mạnh sau rơi tự do).
else if (trangThai == TrangThaiNga::XAC_MINH)
{
  if (uint32_t(t - lucVaDapMs) > cauHinh.cuaSoSauVaDapMs)
  {
    snprintf(ghiChu, sizeof(ghiChu),
             "huy XAC_MINH sau %lums: nam yen dai nhat=%lums (can>=%lu), do cao thap nhat=%.2fm (can<=%.2f)",
             (unsigned long)cauHinh.cuaSoSauVaDapMs, (unsigned long)namYenDaiNhatMs,
             (unsigned long)cauHinh.thoiGianNamYenSauVaDapMs, doCaoThapNhatM, cauHinh.mucGiamDoCaoToiThieuM);
    xoaUngVienNga();
    return false;
  }
  doThayDoiDoCaoM = doCao - doCaoTruocNgaM;
  if (doThayDoiDoCaoM < doCaoThapNhatM)
    doCaoThapNhatM = doThayDoiDoCaoM;

    if (fabsf(giaToc - cauHinh.giaTocNamYenMs2) <= cauHinh.saiSoNamYenMs2)
    {
      // Gia tốc gần 9.81 → người đang nằm yên
      if (!dangNamYen)
      {
        dangNamYen = true;
        lucBatDauNamYenMs = t;
        soMauNamYen = 0;
      }
      if (soMauNamYen < UINT16_MAX)
        ++soMauNamYen;
      uint32_t namYenMs = uint32_t(t - lucBatDauNamYenMs);
      if (namYenMs > namYenDaiNhatMs)
        namYenDaiNhatMs = namYenMs;

      // Bằng chứng áp suất: áp suất tăng → người đang ở vị trí thấp hơn
      bool bangChungApSuat = isfinite(apSuatPa) && isfinite(apSuatTruocNgaPa) &&
                             uint32_t(t - lucBatDauRoiTuDoMs) <= cauHinh.cuaSoApSuatMs &&
                             apSuatPa - apSuatTruocNgaPa >= cauHinh.mucTangApSuatToiThieuPa;

      // GY-63 hỗtrợ xác nhận; thiế u GY-63 không xóa chuỗi MPU.
      // Nhánh bỏ qua GY-63 vẫn bắt buộc rơi tự do + va đập rất mạnh + nằm yên.
      const bool bangChungDoCao = isfinite(doThayDoiDoCaoM) &&
          uint32_t(t - lucBatDauRoiTuDoMs) <= cauHinh.cuaSoApSuatMs &&
          doThayDoiDoCaoM <= cauHinh.mucGiamDoCaoToiThieuM;
      if (uint32_t(t - lucBatDauNamYenMs) >= cauHinh.thoiGianNamYenSauVaDapMs &&
          soMauNamYen >= cauHinh.soMauNamYenToiThieu &&
          (bangChungDoCao || bangChungApSuat || daCoVaDapRatManh))
      {
        snprintf(ghiChu, sizeof(ghiChu), "xac nhan: %s",
                 (bangChungDoCao || bangChungApSuat) ? "MPU + GY-63" : "MPU: roi + va dap rat manh + nam yen");
        trangThai = TrangThaiNga::CANH_BAO;
        if (++maSuKien == 0)
          ++maSuKien; // Tránh mã = 0 (0 có nghĩa "chưa có")
        return true; // ← BÁO NGÃ!
      }
    }
    else
    {
      dangNamYen = false;


            soMauNamYen = 0; // Người cửđộng → đế
                                                m lại
        }
      }
      return false;
  }
};

// ================================================================
// PHẦ N 3: HẰNG SỐVÀ BIẾ N TOÀN CỤC
// ================================================================

// ── Phần cứng I2C ───────────────────────────────────────────────
constexpr int MPU_SDA = 7, MPU_SCL = 5;    // Chân I2C của MPU-6050
constexpr int GY63_SDA = 10, GY63_SCL = 8; // Chân I2C của GY-63
TwoWire &busMpu = Wire;   // Bus cho MPU-6050 (GPIO7=SDA, GPIO5=SCL)
TwoWire &busGy63 = Wire1; // Bus cho GY-63    (GPIO10=SDA, GPIO8=SCL)
constexpr uint8_t DIA_CHI_MPU = 0x68;
constexpr uint8_t DIA_CHI_GY63 = 0x77;

// ── UUID Bluetooth (Android dùng các mã này đểtìm thiế t bị) ────
constexpr const char *UUID_DICH_VU = "7d2a0001-6f45-4c2b-9a1e-38a8f5c10001";
constexpr const char *UUID_DU_LIEU = "7d2a0002-6f45-4c2b-9a1e-38a8f5c10001";
constexpr const char *UUID_SU_KIEN = "7d2a0003-6f45-4c2b-9a1e-38a8f5c10001";
constexpr const char *UUID_YEU_CAU = "7d2a0005-6f45-4c2b-9a1e-38a8f5c10001";
constexpr const char *UUID_XAC_NHAN = "7d2a0006-6f45-4c2b-9a1e-38a8f5c10001";

// ── Cờ trạng thái Bluetooth (atomic = an toàn khi nhiều luồng truy cập) ──
// "atomic" giống như ổkhóa: đảm bảo giá trị không bị đọc/ghi cùng lúc
std::atomic<bool> bleDaKetNoi{false};    // Điện thoại đang kết nối?
std::atomic<bool> daDangKySuKien{false}; // Điện thoại đã đăng ký nhận SOS?
std::atomic<bool> canGuiLaiSos{false};   // Cần gửi lại SOS từ đầu (sau khi ngắt kết nối)?
std::atomic<bool> guiSosLoi{false};      // Lần gửi SOS vừa rồi bị lỗi?
std::atomic<bool> canQuangBaLai{false}; // Cầ n bật Bluetooth quảng bá lại?
std::atomic<bool> canGuiDuLieu{false};   // Android vừa yêu cầu gửi dữ liệu cảm biến?
std::atomic<uint16_t> maAckSos{0};       // Mã xác nhận Android gửi về
std::atomic<uint16_t> mtuBle{23};        // Kích thước gói tối đa (byte), mặc định 23

// ── Con trỏ đến các kênh BLE (được tạo trong khoiTaoBle()) ─────
BLECharacteristic *bleKenhDuLieu = nullptr;
BLECharacteristic *bleKenhSuKien = nullptr;
BLECharacteristic *bleKenhXacNhan = nullptr;

// ── Dữ liệu cảm biế n mới nhất ───────────────────────────────────
struct MauMpu
{
   uint32_t tMs;     // Thời điểm lấy mẫu
   float ax, ay, az; // Gia tốc 3 trục (m/s²)
   float gx, gy, gz; // Tốc độ xoay 3 trục (rad/s)
   bool ok;          // Mẫu có hợp lệ không?
};

struct MauGy63
{
   uint32_t tMs;                     // Thời điểm lấy mẫu
   float apSuatPa, nhietDoC, doCaoM; // Áp suất, nhiệt độ, độ cao tương đối
   bool ok;
};

MauMpu mpuMau = {};
MauGy63 gy63Mau = {};

// ── Đối tượng cảm biến và lưu trữ ───────────────────────────────
Adafruit_MPU6050 mpu;
BoPhatHienNga boNhanDang;
Preferences nvs; // Bộ nhớ không mất khi mất điện (NVS)

// ── Hệ sốhiệu chuẩ n GY-63 đọc từ PROM (bộ nhớ trong chip) ────
uint16_t promGy63[8] = {};
float apSuatMocPa = 0;     // Áp suất ban đầu làm mốc tính độ cao
float apSuatLocPa = 0;     // Áp suất sau khi lọc (giảm nhiễu)
double tongApKhoiDong = 0; // Tổng 25 mẫu đầu đểtính mố c chính xác hơn
uint8_t soMauKhoiDong = 0;
uint32_t soMauMpu = 0, soMauGy63 = 0;
bool mpuOk = false, gy63Ok = false;
bool fsOk = false, fsDayBo = false;


// Máy trạng thái GY-63 (không dùng delay đểkhông chặn chương trình)
enum class GdGy63 : uint8_t
{
   RANH,
   CHO_NHIET_DO,
   CHO_AP_SUAT
};
GdGy63 gdGy63 = GdGy63::RANH;
uint32_t lucBatDauDoUs = 0;   // Mốc micros() khi bắt đầu chuyển đổi
uint32_t nhietDoThoGy63 = 0; // Lưu tạm sốđo nhiệt độ thô
uint32_t lucDoGy63TiepMs = 0; // Lịch lần đo tiếp theo

// ── Hàng đợi SOS ────────────────────────────────────────────────
// SOS được ưu tiên gửi trước dữ liệu cảm biến thường
char bufSos[96] = {};
size_t daiSos = 0, viTriSos = 0;
uint16_t soKhungSos = 0, tongKhungSos = 0;
uint16_t maSuKienSos = 0;
bool sosLaThu = false, sosDangChoAck = false;
uint32_t lucGuiSosCuoiMs = 0, lucXepSosMs = 0;

// ── Hàng đợi dữ liệu cảm biến ───────────────────────────────────
char bufDuLieu[512] = {};
size_t daiDuLieu = 0, viTriDuLieu = 0;
uint16_t soKhungDuLieu = 0, tongKhungDuLieu = 0;
uint32_t soThuTuDuLieu = 0;

// ── Cấu hình và hàng đợi JSON từ Android ────────────────────────
std::atomic<bool> coCauHinhMoi{false};
char bufCauHinh[512] = {};
size_t daiCauHinh = 0;

// ── Chếđộ nghỉ sau SOS (dừng cảm biế n 5 phút, vẫn xửlý BLE) ──
constexpr uint32_t NGHI_SAU_SOS_MS = 5UL * 60UL * 1000UL; // 5 phút
constexpr uint32_t HAN_ACK_SOS_MS = NGHI_SAU_SOS_MS;
bool dangNghi = false;
uint32_t lucBatDauNghiMs = 0;

constexpr const char *FILE_CSV = "/mpu.csv";

// ── Khai báo trước (forward declarations) ───────────────────────
void xepSos(uint16_t maSuKien, bool laThu = false);
bool i2cPing(TwoWire &bus, uint8_t addr);
void inImpactAcceleration();

// ================================================================
// PHẦ                               Ỉ VÀ CỨU HỘ ĐƯỜNG TRUYỀ
       N 4: TIỆN ÍCH I2C (QUÉT ĐỊA CHỈ                      N)
// [KẾ T HỢP: Thư viện Wire.h + NHÓM TỰ CODE THUẬT TOÁN CỨU HỘ BUS]
// (Nhóm tự viế t vòng lặp quét tìm địa chỉ chip cảm biến và chuỗi 9 xung clock
//   cứu kẹt đường truyền SDA khi chip bị treo)
// ================================================================

// Kiểm tra thiết bị I2C có phản hồi không (giố
                                              ng gọi điện đểxem có người nghe máy không)
bool i2cPing(TwoWire &bus, uint8_t addr)
{
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

// Đọc 1 thanh ghi từ thiết bị I2C
bool i2cDocByte(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t &val)
{
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0 || bus.requestFrom(addr, (uint8_t)1) != 1)
    return false;
  val = bus.read();
  return true;
}

// ================================================================
// PHẦ N 5: MPU-6050 (Cảm biến gia tốc + con quay hồi chuyển)
// [KẾ T HỢP: Thư viện Adafruit_MPU6050 + NHÓM TỰ CODE XỬLÝ VÉC-TƠ]
// (Mượn thư viện Adafruit đểlấ  y 3 trục thô; nhóm TỰ CODE công thức định lý


//   Pythagoras 3D tính độ lớn gia tốc tổng a và tốc độ xoay omega, cùng nhãn
//   thời gian chuẩn xác cho thuật toán v13)
// ================================================================

// Khôi phục bus I2C khi MPU bị treo (tạo xung SCL đểgiải phóng SDA)
void khoiPhucBusMpu()
{
  busMpu.end();
  pinMode(MPU_SDA, INPUT_PULLUP);
  pinMode(MPU_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(MPU_SCL, HIGH);
  for (uint8_t i = 0; i < 9 && digitalRead(MPU_SDA) == LOW; ++i)
  {
    digitalWrite(MPU_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(MPU_SCL, HIGH);
    delayMicroseconds(5);
  }
  // Gửi điều kiện STOP của I2C đểkế t thúc bất kỳ giao dịch dở dang nào
  pinMode(MPU_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(MPU_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(MPU_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(MPU_SDA, HIGH);
  busMpu.begin(MPU_SDA, MPU_SCL, 100000);
  busMpu.setTimeOut(5);
}

// Khởi tạo MPU-6050 (thang ±16g, 100Hz, thửlại sau 5 giây nế u lỗi)
void khoiTaoMpu()
{
  static uint32_t thuLaiMs = 0;
  uint32_t t = millis();
  if (thuLaiMs && int32_t(t - thuLaiMs) < 0)
    return;
  thuLaiMs = t + 5000;
  if (!i2cPing(busMpu, DIA_CHI_MPU))
    khoiPhucBusMpu();
  mpuOk = mpu.begin(DIA_CHI_MPU, &busMpu);
  if (mpuOk)
  {
    // Chọn ±16g đểgiảm nguy cơ bão hòa khi va đập mạnh
    mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
    mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
    mpu.setSampleRateDivisor(9); // 1000Hz / (9+1) = 100Hz
  }
  Serial.println(mpuOk ? "MPU: san sang" : "MPU: loi khoi tao");
}

/*
  * Đọc nhanh MPU qua I2C thẳng (không dùng thư viện cao cấp)
  * → lấy 14 byte liên tiếp, tính độ lớn gia tốc và xoay
  * Trả vềfalse ngay khi đọc lỗ i; sau 3 lỗi liên tiếp sẽ khởi tạo lại
  */
bool docMpuNhanh(float &giaToc, float &xoay)
{
   static uint8_t soLoi = 0;
   if (!mpuOk)
     khoiTaoMpu();
   if (!mpuOk)
   {
     mpuMau.ok = false;
     return false;
   }
   uint8_t b[14];
   busMpu.beginTransmission(DIA_CHI_MPU);
   busMpu.write(0x3B); // Địa chỉ thanh ghi ACCEL_XOUT_H
   if (busMpu.endTransmission(false) || busMpu.requestFrom(DIA_CHI_MPU, (uint8_t)14) != 14)
   {
     if (++soLoi >= 3)
       mpuOk = false;
     while (busMpu.available())
       busMpu.read();


      mpuMau.ok = false;
      return false;
    }
    soLoi = 0;
    for (auto &x : b)
      x = busMpu.read();

    // Ghép 2 byte thành số16-bit có dấ u
    auto raw = [&](int i)
    { return int16_t((uint16_t(b[i]) << 8) | b[i + 1]); };
    // Hệ sốđổ i từ sốthô → m/s² và °/s
    constexpr float kG = 9.80665f / 2048.0f; // 2048 LSB/g ở thang ±16g
    constexpr float kW = 1.0f / 16.4f;       // ÷16.4 vì thang ±2000°/s

    float ax = raw(0) * kG, ay = raw(2) * kG, az = raw(4) * kG;
    float wx = raw(8) * kW, wy = raw(10) * kW, wz = raw(12) * kW;
    giaToc = sqrtf(ax * ax + ay * ay + az * az);
    xoay = sqrtf(wx * wx + wy * wy + wz * wz);
    // Lưu mẫu đầy đủ (đổi °/s → rad/s = nhân 0.01745)
    mpuMau = {millis(), ax, ay, az, wx * 0.017453f, wy * 0.017453f, wz * 0.017453f, true};
    ++soMauMpu;
    return true;
}

// Ghi mẫ u MPU hiện có vào nhật ký CSV (không đọc lại cảm biến)
void docVaLuuMpuDayDu()
{
  // Ghi mẫ u đã dùng cho bộ nhận dạng; không đọc thêm rồi thay đổi dấu thời gian.
  if (!mpuMau.ok)
    return;
  Serial.printf("MPU: ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f\n",
                 mpuMau.ax, mpuMau.ay, mpuMau.az, mpuMau.gx, mpuMau.gy, mpuMau.gz);
  if (!fsOk || fsDayBo)
    return;
  File f = LittleFS.open(FILE_CSV, "a");
  if (!f)
    return;
  if (f.size() >= LittleFS.totalBytes() * 3 / 4)
  {
    fsDayBo = true;
    f.close();
    return;
  }
  if (f.size() == 0)
    f.println("time_ms,ax,ay,az,gx,gy,gz");
  f.printf("%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
            (unsigned long)mpuMau.tMs, mpuMau.ax, mpuMau.ay, mpuMau.az,
            mpuMau.gx, mpuMau.gy, mpuMau.gz);
  f.close();
}

// ================================================================
// PHẦ N 6: GY-63 / MS5611 (Cảm biến áp suất → đo độ cao)
// [NHÓM TỰ CODE 100% THEO DATASHEET - KHÔNG DÙNG THƯ VIỆN NGOÀI]
// (Nhóm tự giao tiế p I2C thô qua Wire1, tự đọc PROM hiệu chuẩn C1..C6,
//   tự lập trình máy trạng thái đo không chặn 9ms và tự cài đặt công thức
//   bù bậc 2 toán học từ datasheet của nhà sản xuất MS5611)
// ================================================================

// Gửi lệnh đến GY-63 (reset, bắt đầu đo, v.v.)
bool gy63Lenh(uint8_t cmd)
{
  busGy63.beginTransmission(DIA_CHI_GY63);
  busGy63.write(cmd);
  return busGy63.endTransmission() == 0;
}

// Đọc soLuong byte từ GY-63
bool gy63Doc(uint8_t cmd, uint8_t *out, uint8_t n)
{
  busGy63.beginTransmission(DIA_CHI_GY63);
  busGy63.write(cmd);
  if (busGy63.endTransmission(false) || busGy63.requestFrom(DIA_CHI_GY63, n) != n)
  {
    while (busGy63.available())


        busGy63.read();
      return false;
    }
    for (uint8_t i = 0; i < n; ++i)
      out[i] = busGy63.read();
    return true;
}

/*
  * Tính CRC 4-bit đểkiể m tra xem hệ sốPROM đọc có bị lỗ i không.
  * Giống như mã kiểm tra trên thẻ ngân hàng đểphát hiện gõ sai số .
  */
uint8_t gy63Crc(const uint16_t *p)
{
   uint16_t r = 0;
   for (uint8_t i = 0; i < 16; ++i)
   {
     uint16_t w = p[i / 2];
     if (i / 2 == 7)
       w &= 0xFF00;
     r ^= (i & 1) ? (w & 0xFF) : (w >> 8);
     for (uint8_t b = 0; b < 8; ++b)
       r = (r & 0x8000) ? uint16_t((r << 1) ^ 0x3000) : uint16_t(r << 1);
   }
   return (r >> 12) & 0x0F;
}

// Khởi tạo GY-63: reset → đọc PROM → kiể m CRC → đặt mốc áp suấ t
void khoiTaoGy63()
{
  gy63Ok = false;
  gy63Mau.ok = false;
  gdGy63 = GdGy63::RANH;
  lucDoGy63TiepMs = millis() + 5000;
  if (!gy63Lenh(0x1E))
    return; // Reset chip
  delay(4);
  for (uint8_t i = 0; i < 8; ++i)
  {
    uint8_t b[2];
    if (!gy63Doc(0xA0 + 2 * i, b, 2))
       return;
    promGy63[i] = (uint16_t(b[0]) << 8) | b[1];
    // Hệ số0 hoặc 0xFFFF là bấ  t thường
    if (i >= 1 && i <= 6 && (promGy63[i] == 0 || promGy63[i] == 0xFFFF))
       return;
  }
  gy63Ok = (gy63Crc(promGy63) == (promGy63[7] & 0x0F));
  // Một sốmodule dùng CRC=0, kiể  m tra bằng cách đọc lại 2 lần
  if (!gy63Ok && (promGy63[7] & 0x0F) == 0)
  {
    gy63Ok = true;
    for (uint8_t i = 0; i < 8 && gy63Ok; ++i)
    {
       uint8_t b[2];
       if (!gy63Doc(0xA0 + 2 * i, b, 2))
       {
         gy63Ok = false;
         break;
       }
       if (promGy63[i] != ((uint16_t(b[0]) << 8) | b[1]))
         gy63Ok = false;
    }
    if (gy63Ok)
       Serial.println("GY-63: CRC=0, da kiem tra 2 lan OK");
  }
  Serial.println(gy63Ok ? "GY-63: he so PROM hop le" : "GY-63: he so PROM loi");
  if (gy63Ok)
  {
    apSuatMocPa = apSuatLocPa = tongApKhoiDong = 0;
    soMauKhoiDong = 0;
    lucDoGy63TiepMs = millis();
  }
  else
  {


        lucDoGy63TiepMs = millis() + 5000;
    }
}

// Đọc kết quả ADC 24-bit (sốđo thô của áp suấ t hoặc nhiệt độ)
bool gy63DocAdc(uint32_t &val)
{
  uint8_t b[3];
  if (!gy63Doc(0x00, b, 3))
    return false;
  val = (uint32_t(b[0]) << 16) | (uint32_t(b[1]) << 8) | b[2];
  return val != 0 && val != 0xFFFFFF;
}

/*
  * Máy trạng thái đọc GY-63 — không dùng delay():
  *
  * RANH         → gửi lệnh đo nhiệt độ → CHO_NHIET_DO
  * CHO_NHIET_DO → đợi 10ms → đọc nhiệt độ → gử  i lệnh đo áp suấ t → CHO_AP_SUAT
  * CHO_AP_SUAT → đợi 10ms → đọc áp suấ   t → tính kết quả → RANH
  *
  * Tại sao không dùng delay()? Vì delay() làm cả chương trình dừng lại,
  * khiế n cảm biến MPU không được đọc đúng giờ!
  */
void docGy63()
{
    uint32_t t = millis();
    if (!gy63Ok && int32_t(t - lucDoGy63TiepMs) >= 0)
    {
      khoiTaoGy63();
      t = millis();
    }
    if (!gy63Ok)
    {
      gy63Mau.ok = false;
      return;
    }

    if (gdGy63 == GdGy63::RANH)
    {
      if (int32_t(t - lucDoGy63TiepMs) < 0)
        return;
      // 0x58 = D2 OSR4096: đo nhiệt độ ở độ phân giải cao nhất
      if (gy63Lenh(0x58))
      {
        lucBatDauDoUs = micros();
        gdGy63 = GdGy63::CHO_NHIET_DO;
      }
      else
      {
        gy63Ok = gy63Mau.ok = false;
        lucDoGy63TiepMs = millis() + 5000;
        Serial.println("GY-63: loi lenh do nhiet do, thu lai sau 5s");
      }
    }
    else if (uint32_t(micros() - lucBatDauDoUs) < 10000)
    {
      return; // Chip cần ~9ms đểchuyể n đổi, đợi không chặn
    }
    else if (gdGy63 == GdGy63::CHO_NHIET_DO)
    {
      // 0x48 = D1 OSR4096: đo áp suất
      if (gy63DocAdc(nhietDoThoGy63) && gy63Lenh(0x48))
      {
        lucBatDauDoUs = micros();
        gdGy63 = GdGy63::CHO_AP_SUAT;
      }
      else
      {
        gdGy63 = GdGy63::RANH;
        gy63Ok = gy63Mau.ok = false;
        lucDoGy63TiepMs = millis() + 5000;
        Serial.println("GY-63: loi ADC/lenh ap suat, thu lai sau 5s");
      }
    }


    else
    {
      uint32_t apTho;
      if (!gy63DocAdc(apTho))
      {
         // Lỗi ADC → khởi tạo lại sau 5 giây
         gdGy63 = GdGy63::RANH;
         gy63Ok = false;
         lucDoGy63TiepMs = millis() + 5000;
         gy63Mau.ok = false;
         Serial.println("GY-63: loi ADC");
         return;
      }
      gdGy63 = GdGy63::RANH;
      lucDoGy63TiepMs = millis() + 20; // ~25 cặp mẫ
                                                   u/giây

        // ── Tính áp suất và nhiệt độ theo công thức của nhà sản xuất MS5611 ──
        // dT = sốđo - hệ sốC5*256 (sai lệch nhiệt độ so với 25°C)
        int64_t dT = int64_t(nhietDoThoGy63) - int64_t(promGy63[5]) * 256;
        // TEMP = 2000 + dT*C6/2^23 (nhiệt độ thực tính bằng 0.01°C)
        int64_t TEMP = 2000 + dT * promGy63[6] / 8388608;
        // OFF, SENS: độ lệch và độ nhạy theo nhiệt độ
        int64_t OFF = int64_t(promGy63[2]) * 65536 + int64_t(promGy63[4]) * dT / 128;
        int64_t SENS = int64_t(promGy63[1]) * 32768 + int64_t(promGy63[3]) * dT / 256;
        // Bù thêm khi nhiệt độ thấp (< 20°C)
        if (TEMP < 2000)
        {
          int64_t t2 = (TEMP - 2000) * (TEMP - 2000);
          OFF -= 5 * t2 / 2;
          SENS -= 5 * t2 / 4;
          if (TEMP < -1500)
          {
            int64_t t3 = (TEMP + 1500) * (TEMP + 1500);
            OFF -= 7 * t3;
            SENS -= 11 * t3 / 2;
          }
          TEMP -= dT * dT / 2147483648LL;
        }
        // P = (D1*SENS/2^21 - OFF) / 2^15 (áp suất tính bằng Pa)
        float p = float((int64_t(apTho) * SENS / 2097152 - OFF) / 32768);
        float T = float(TEMP) / 100.0f; // Đổi 0.01°C → °C

        // Bỏ qua mẫ u ngoài miề
                               n hợp lý (1000–120000 Pa, -40°C đến 85°C)
        if (p < 1000 || p > 120000 || T < -40 || T > 85)
        {
          gy63Mau.ok = false;
          return;
        }
        ++soMauGy63;

        // Thu thập 25 mẫu đầu đểtính mố c áp suất ban đầu (tránh nhiễu)
        if (soMauKhoiDong < 25)
        {
          tongApKhoiDong += p;
          if (++soMauKhoiDong == 25)
          {
            apSuatMocPa = float(tongApKhoiDong / 25);
            apSuatLocPa = p;
            Serial.printf("GY-63: moc P0=%.1f Pa\n", apSuatMocPa);
          }
          else
          {
            gy63Mau.ok = false;
            return;
          }
        }
        // Lọc EMA 20% đểgiảm nhiễ u (EMA = bộ lọc trung bình theo hàm mũ)
        apSuatLocPa += 0.20f * (p - apSuatLocPa);
        // Công thức đo độ cao qua áp suất (barometric formula)
        float dH = 44330.0f * (1.0f - powf(apSuatLocPa / apSuatMocPa, 0.19029495f));
        gy63Mau = {millis(), p, T, dH, true};
    }
}

// ================================================================


// PHẦ N 7: BLUETOOTH LOW ENERGY (BLE)
// [KẾ T HỢP: Thư viện BLE ESP32 + NHÓM TỰ CODE BẮT TAY AN TOÀN]
// (Mượn hạ tầ ng kết nối BLE của Espressif; nhóm TỰ CODE các lớp Callback
//   và cơ chếbắ t tay xác nhận ACK từ điện thoại Android)
// ================================================================

/*
  * Hàm gọi lại BLE: được gọi tự động khi điện thoại kết nối/ngắ t.
  * Chỉ được phép thay đổi cờ atomic, KHÔNG làm việc nặng ở đây!
  * (Vì hàm này chạy trong luồng riêng của thư viện BLE)
  */
class XuLyKetNoi : public BLEServerCallbacks
{
   void onConnect(BLEServer *) override
   {
     mtuBle.store(23);
     daDangKySuKien.store(false);
     bleDaKetNoi.store(true);
   }
   void onDisconnect(BLEServer *) override
   {
     bleDaKetNoi.store(false);
     daDangKySuKien.store(false);
     canGuiDuLieu.store(false);
     canGuiLaiSos.store(true);
     canQuangBaLai.store(true);
   }
#if defined(CONFIG_NIMBLE_ENABLED)
   void onMtuChanged(BLEServer *, ble_gap_conn_desc *, uint16_t mtu) override { mtuBle.store(mtu); }
#elif defined(CONFIG_BLUEDROID_ENABLED)
   void onMtuChanged(BLEServer *, esp_ble_gatts_cb_param_t *p) override { mtuBle.store(p->mtu.mtu); }
#endif
};

// Khi Android bật/tắ t nhận thông báo SOS (ghi vào CCCD descriptor)
class XuLyDangKySuKien : public BLEDescriptorCallbacks
{
   void onWrite(BLEDescriptor *d) override
   {
     const uint8_t *v = d->getValue();
     daDangKySuKien.store(d->getLength() >= 2 && (v[0] & 1));
   }
};

// Theo dõi kế t quả notify (thành công hay thất bại)
class XuLyKenhSuKien : public BLECharacteristicCallbacks
{
   void onStatus(BLECharacteristic *, Status s, uint32_t) override
   {
     if (s != SUCCESS_NOTIFY)
     {
       guiSosLoi.store(true);
       if (s == ERROR_NOTIFY_DISABLED || s == ERROR_NO_SUBSCRIBER)
         daDangKySuKien.store(false);
     }
   }
#if defined(CONFIG_NIMBLE_ENABLED)
   void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *, uint16_t v) override
   {
     daDangKySuKien.store(v & 1);
   }
#endif
};

// Nhận lệnh từ Android qua kênh WRITE
class XuLyLenhAndroid : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *c) override
  {
    String v = c->getValue();
    if (v == "GET_TELEMETRY")
    {
      canGuiDuLieu.store(true); // Android muốn nhận dữ liệu cảm biến
    }
    else if (v.startsWith("SOS_ACK:"))


        {
            long m = v.substring(8).toInt();
            if (m > 0 && m <= 65535)
              maAckSos.store(uint16_t(m)); // Android đã nhận SOS
        }
        else if (!coCauHinhMoi.load() && v.length() > 0 &&
                 v.length() < sizeof(bufCauHinh) && v[0] == '{')
        {
          // Android gửi bộ thông sốmới dạng JSON
          memcpy(bufCauHinh, v.c_str(), v.length());
          bufCauHinh[v.length()] = 0;
          daiCauHinh = v.length();
          coCauHinhMoi.store(true);
        }
    }
};

// Tạo toàn bộ dịch vụ BLE và bắt đầu quảng bá
void khoiTaoBle()
{
  char ten[20];
  snprintf(ten, sizeof(ten), "FALLSAFE-%04X", (unsigned)(ESP.getEfuseMac() & 0xFFFF));
  BLEDevice::init(ten);
  BLEDevice::setMTU(512);
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new XuLyKetNoi());
  BLEService *svc = srv->createService(UUID_DICH_VU);

    bleKenhDuLieu = svc->createCharacteristic(UUID_DU_LIEU, BLECharacteristic::PROPERTY_NOTIFY);
    #if !defined(CONFIG_NIMBLE_ENABLED)
    bleKenhDuLieu->addDescriptor(new BLE2902());
    #endif

    bleKenhSuKien = svc->createCharacteristic(UUID_SU_KIEN, BLECharacteristic::PROPERTY_NOTIFY);
    bleKenhSuKien->setCallbacks(new XuLyKenhSuKien());
    #if !defined(CONFIG_NIMBLE_ENABLED)
    auto *cccd = new BLE2902();
    cccd->setCallbacks(new XuLyDangKySuKien());
    bleKenhSuKien->addDescriptor(cccd);
    #endif // NimBLE tự tạo CCCD; nhận đăng ký qua onSubscribe().

    auto *yc = svc->createCharacteristic(UUID_YEU_CAU, BLECharacteristic::PROPERTY_WRITE);
    yc->setCallbacks(new XuLyLenhAndroid());

    bleKenhXacNhan = svc->createCharacteristic(UUID_XAC_NHAN, BLECharacteristic::PROPERTY_NOTIFY);
    #if !defined(CONFIG_NIMBLE_ENABLED)
    bleKenhXacNhan->addDescriptor(new BLE2902());
    #endif

    svc->start();
    auto *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(UUID_DICH_VU);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.printf("BLE: quang ba %s\n", ten);
}

// ================================================================
// PHẦ N 8: GIAO THỨC ĐÓNG GÓI DỮ LIỆU JSON
// [KẾ T HỢP: Thư viện cJSON + NHÓM TỰ THIẾT KẾCẤ         ẢN TIN]
                                                  U TRÚC BẢ
// (Nhóm tự thiế t kếcú pháp gói tin JSON và lập trình chia nhỏ khung MTU BLE,
//   dùng cJSON phân tích lệnh gửi xuống từ điện thoại)
// ================================================================

/*
 * Giao thức khung IF-003:
 *   Byte 0-1: "FS" (0x46, 0x53) = chữ ký đểAndroid nhận ra
 *   Byte 2:   Phiên bản = 1
 *   Byte 3:   Loại: 1=dữ liệu cảm biến, 2=SOS
 *   Byte 4-7: Mã số32-bit
 *   Byte 8-9: Sốthứ tự khung hiện tại
 *   Byte 10-11: Tổng sốkhung
 *   Byte 12-13: Tổng độ dài dữ liệu
 *   Byte 14-15: Vị trí byte trong dữ liệu gốc
 *   Byte 16+: Nội dung JSON


 */

// Ghi đầu khung vào mảng (16 byte đầ u)
static void vietDauKhung(uint8_t *f, uint8_t loai, uint32_t ma,
                          uint16_t soKhung, uint16_t tongKhung,
                          uint16_t doDai, uint16_t viTri)
{
  f[0] = 0x46;
  f[1] = 0x53;
  f[2] = 1;
  f[3] = loai;
  f[4] = ma;
  f[5] = ma >> 8;
  f[6] = ma >> 16;
  f[7] = ma >> 24;
  f[8] = soKhung;
  f[9] = soKhung >> 8;
  f[10] = tongKhung;
  f[11] = tongKhung >> 8;
  f[12] = doDai;
  f[13] = doDai >> 8;
  f[14] = viTri;
  f[15] = viTri >> 8;
}

// Tính tổng sốkhung cầ n thiết dựa trên kích thước MTU
static uint16_t tinhTongKhung(size_t doDai, uint16_t mtu)
{
  if (doDai <= (size_t)(mtu - 3))
    return 0; // Vừa 1 gói, không cần chia
  return (doDai + (mtu - 19) - 1) / (mtu - 19);
}

// ================================================================
// PHẦ N 9: XỬLÝ SOS VÀ CỨU HỘ KHẨ N CẤP
// [NHÓM TỰ CODE 100% - LOGIC BÁO ĐỘNG VÀ ĐIỀ U PHỐI KHẨN CẤP]
// (Tự lập trình hàng đợi SOS, cơ chếtimeout 15s, và thuật toán nghỉ 5 phút
//   thông minh: chỉ nghỉ khi điện thoại đã nhận được ACK)
// ================================================================

// Tạo JSON SOS và đưa vào hàng đợi
void xepSos(uint16_t maSuKien, bool laThu)
{
  if (dangNghi || (daiSos && !(sosLaThu && !laThu)))
    return; // SOS thật được ưu tiên thay gói thử; không ghi đè SOS thật đang chờ.
  maAckSos.store(0); // ACK cũ không được xác nhận cho sự kiện mới.
  int n = snprintf(bufSos, sizeof(bufSos),
                   "{\"eventId\":\"%u\",\"deviceId\":\"FALLSAFE-%04X\","
                   "\"eventType\":\"SOS_PRESSED\",\"test\":%s}",
                   maSuKien, (unsigned)(ESP.getEfuseMac() & 0xFFFF), laThu ? "true" : "false");
  if (n <= 0 || size_t(n) >= sizeof(bufSos))
    return;

    sosLaThu = laThu;
    maSuKienSos = maSuKien;
    daiSos = n;
    viTriSos = soKhungSos = 0;
    sosDangChoAck = false;
    lucGuiSosCuoiMs = 0;
    lucXepSosMs = millis();
    // Tiếp tục lấ y mẫ
                      u trong khi chờ; chỉ nghỉ sau ACK của SOS thật.
    daiDuLieu = 0;
    canGuiDuLieu.store(false); // Huỷ gói dữ liệu đang dở
}

/*
 * Gửi từng bước SOS — mỗi lần gọi chỉ gửi 1 khung.
 * Trả vềTRUE nế u đang trong quá trình gửi hoặc chờ ACK.
 *
 * Quy trình:
 * 1. Nế u Android gửi ACK → xoá SOS khỏi hàng đợi
 * 2. Nế u quá thời hạn 5 phút → bỏ qua
 * 3. Gử i từng khung, đợi ACK sau khi gửi xong
 * 4. Nế u 1 giây chưa có ACK → gửi lại từ đầu
 */


bool guiSos(uint32_t t)
{
  static uint32_t lucGuiKhungMs = 0;

  // Kiểm tra ACK từ Android
  if (daiSos && maAckSos.exchange(0) == maSuKienSos)
  {
    Serial.printf("SOS: Android xac nhan id=%u\n", maSuKienSos);
    if (!sosLaThu)
    {
      lucBatDauNghiMs = t;
      dangNghi = true;
      Serial.println("SOS: bat dau nghi 5 phut sau ACK");
    }
    daiSos = 0;
    sosDangChoAck = false;
    return false;
  }
  // Hết hạn chờ
  if (daiSos && uint32_t(t - lucXepSosMs) >= HAN_ACK_SOS_MS)
  {
    Serial.printf("SOS: het han, chua duoc Android xac nhan id=%u; tiep tuc theo doi\n", maSuKienSos);
    boNhanDang.trangThai = TrangThaiNga::THEO_DOI;
    boNhanDang.xoaUngVienNga();
    boNhanDang.daCoMau = false;
    daiSos = 0;
    sosDangChoAck = false;
    return false;
  }
  // Reset tiến trình gửi khi vừa kết nối lại
  if (canGuiLaiSos.exchange(false))
  {
    viTriSos = soKhungSos = 0;
    sosDangChoAck = false;
  }
  if (!daiSos || !bleDaKetNoi.load() || !daDangKySuKien.load() || !bleKenhSuKien)
    return false;

  // Đang chờ ACK → thửlại sau 1 giây
  if (sosDangChoAck)
  {
    if (uint32_t(t - lucGuiSosCuoiMs) < 1000)
      return false;
    viTriSos = soKhungSos = 0;
    sosDangChoAck = false;
    Serial.printf("SOS: gui lai id=%u\n", maSuKienSos);
  }
  if (lucGuiKhungMs && uint32_t(t - lucGuiKhungMs) < 10)
    return true;

  uint16_t mtu = max(mtuBle.load(), (uint16_t)23);
  if (viTriSos == 0)
    tongKhungSos = tinhTongKhung(daiSos, mtu);

  if (tongKhungSos == 0)
  {
    // Toàn bộ SOS vừa 1 gói BLE → gửi thẳng
    guiSosLoi.store(false);
    bleKenhSuKien->setValue((uint8_t *)bufSos, daiSos);
    bleKenhSuKien->notify();
    if (!guiSosLoi.load() && bleDaKetNoi.load() && daDangKySuKien.load())
       sosDangChoAck = true;
  }
  else
  {
    // Chia thành nhiề u khung
    size_t doDai = min(size_t(mtu - 19), daiSos - viTriSos);
    uint8_t khung[112];
    vietDauKhung(khung, 2, maSuKienSos, soKhungSos, tongKhungSos,
                  daiSos, viTriSos);
    memcpy(khung + 16, bufSos + viTriSos, doDai);
    guiSosLoi.store(false);
    bleKenhSuKien->setValue(khung, 16 + doDai);
    bleKenhSuKien->notify();
    if (!guiSosLoi.load() && bleDaKetNoi.load() && daDangKySuKien.load())


      {
          viTriSos += doDai;
          ++soKhungSos;
          if (viTriSos == daiSos)
            sosDangChoAck = true;
      }
    }
    lucGuiKhungMs = t;
    if (sosDangChoAck)
    {
      lucGuiSosCuoiMs = t;
      Serial.printf("SOS: da gui, cho ACK id=%u thu=%u\n", maSuKienSos, sosLaThu);
    }
    return true;
}

// ================================================================
// PHẦ N 10: GỬ           ẢM BIẾ
               I DỮ LIỆU CẢ     N ĐỊNH KỲ
// [NHÓM TỰ CODE - ĐIỀ U TIẾT DÒNG DỮ LIỆU BLUETOOTH]
// (Tự lập trình nhịp gử i định kỳ đểkhông làm nghẽn sóng BLE)
// ================================================================

// Đóng gói JSON từ mẫu MPU và GY-63 hiện tại
void chuanBiDuLieu(uint32_t t)
{
  bool mpuOkMau = mpuMau.ok && uint32_t(t - mpuMau.tMs) <= 150;
  bool gy63OkMau = gy63Mau.ok && uint32_t(t - gy63Mau.tMs) <= 200;
  char sP[24] = "null", sT[24] = "null", sH[24] = "null", sId[20];
  if (gy63OkMau)
  {
    snprintf(sP, sizeof(sP), "%.1f", gy63Mau.apSuatPa);
    snprintf(sT, sizeof(sT), "%.2f", gy63Mau.nhietDoC);
    snprintf(sH, sizeof(sH), "%.3f", gy63Mau.doCaoM);
  }
  snprintf(sId, sizeof(sId), "FALLSAFE-%04X", (unsigned)(ESP.getEfuseMac() & 0xFFFF));
  uint32_t seq = ++soThuTuDuLieu;
  int n;
  if (mpuOkMau)
  {
    constexpr float R2D = 57.2957795f; // rad/s → °/s
    n = snprintf(bufDuLieu, sizeof(bufDuLieu),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%lu,"
                 "\"timestampMs\":%lu,\"accelXMs2\":%.3f,\"accelYMs2\":%.3f,\"accelZMs2\":%.3f,"
                 "\"gyroXDps\":%.2f,\"gyroYDps\":%.2f,\"gyroZDps\":%.2f,"
                 "\"pressurePa\":%s,\"temperatureC\":%s,\"altitudeDeltaM\":%s,"
                 "\"batteryPercent\":-1,\"batteryVoltageMv\":null,\"isCharging\":false,"
                 "\"sosButtonPressed\":false,\"sensorQuality\":%u}",
                 sId, (unsigned long)seq, (unsigned long)t,
                 mpuMau.ax, mpuMau.ay, mpuMau.az,
                 mpuMau.gx * R2D, mpuMau.gy * R2D, mpuMau.gz * R2D,
                 sP, sT, sH, gy63OkMau ? 100u : 50u);
  }
  else
  {
    n = snprintf(bufDuLieu, sizeof(bufDuLieu),
                 "{\"protocolVersion\":1,\"deviceId\":\"%s\",\"sequenceNumber\":%lu,"
                 "\"timestampMs\":%lu,\"accelXMs2\":null,\"accelYMs2\":null,\"accelZMs2\":null,"
                 "\"gyroXDps\":null,\"gyroYDps\":null,\"gyroZDps\":null,"
                 "\"pressurePa\":%s,\"temperatureC\":%s,\"altitudeDeltaM\":%s,"
                 "\"batteryPercent\":-1,\"batteryVoltageMv\":null,\"isCharging\":false,"
                 "\"sosButtonPressed\":false,\"sensorQuality\":0}",
                 sId, (unsigned long)seq, (unsigned long)t, sP, sT, sH);
  }
  if (n <= 0 || size_t(n) >= sizeof(bufDuLieu))
  {
    daiDuLieu = 0;
    return;
  }
  daiDuLieu = n;
  viTriDuLieu = soKhungDuLieu = 0;
  uint16_t mtu = max(mtuBle.load(), (uint16_t)23);
  tongKhungDuLieu = tinhTongKhung(daiDuLieu, mtu);
}

// Gửi từng bước dữ liệu cảm biế
                               n (tương tự guiSos nhưng không cần ACK)


void guiDuLieu(uint32_t t)
{
  static uint32_t lucGuiKhungMs = 0;
  if (!bleDaKetNoi.load())
  {
    daiDuLieu = 0;
    canGuiDuLieu.store(false);
    return;
  }
  if (!daiDuLieu && canGuiDuLieu.exchange(false))
    chuanBiDuLieu(t);
  if (!daiDuLieu || !bleKenhDuLieu ||
      (soKhungDuLieu > 0 && uint32_t(t - lucGuiKhungMs) < 10))
    return;

    if (tongKhungDuLieu == 0)
    {
      bleKenhDuLieu->setValue((uint8_t *)bufDuLieu, daiDuLieu);
      bleKenhDuLieu->notify();
      daiDuLieu = 0;
    }
    else
    {
      uint16_t mtu = max(mtuBle.load(), (uint16_t)23);
      size_t doDai = min(size_t(mtu - 19), daiDuLieu - viTriDuLieu);
      uint8_t khung[512];
      vietDauKhung(khung, 1, soThuTuDuLieu,
                    soKhungDuLieu, tongKhungDuLieu,
                    daiDuLieu, viTriDuLieu);
      memcpy(khung + 16, bufDuLieu + viTriDuLieu, doDai);
      bleKenhDuLieu->setValue(khung, 16 + doDai);
      bleKenhDuLieu->notify();
      viTriDuLieu += doDai;
      ++soKhungDuLieu;
      if (viTriDuLieu == daiDuLieu)
         daiDuLieu = 0;
    }
    lucGuiKhungMs = t;
}

// ================================================================
// PHẦ N 11: CẤU HÌNH QUA BLE VÀ LƯU VÀO BỘ NHỚ FLASH (NVS)
// [KẾ                                             ẢO VỆ CẤ
       T HỢP: Thư viện Preferences + NHÓM TỰ CODE BẢ       U HÌNH]
// (Mượn Preferences của hệ thố ng; nhóm tự code hàm giải mã JSON và kiểm tra
//   tính hợp lệ hopLe() trước khi nạp tham sốmới)
// ================================================================

// Phân tích JSON cấu hình từ Android, kiểm tra từng trường
bool phanTichCauHinh(const char *json, size_t doDai, CauHinhNga &out)
{
  const char *end = nullptr;
  cJSON *root = cJSON_ParseWithLengthOpts(json, doDai + 1, &end, true);
  if (!root)
    return false;

    // Tên trường phải khớp chính xác với giao thức JSON của Android
    const char *fields[] = {
        "impactAccelerationMs2", "stillnessTargetAccelerationMs2", "stillnessToleranceMs2",
        "postImpactWindowMs", "postImpactStillnessDurationMs", "minimumStillnessSamples",
        "maximumSampleGapMs", "freeFallThresholdMs2", "freeFallMinDurationMs",
        "gyroTurnThresholdDps", "pressureEvidenceMinRisePa", "pressureWindowMs",
        "altitudeDropMinM", "sampleWatchdogMs"};
    double v[14] = {};
    int cnt = 0;
    for (auto *f = root->child; f; f = f->next)
      ++cnt;
    bool ok = cJSON_IsObject(root) && end == json + doDai && (cnt == 14 || cnt == 15);

    for (int i = 0; ok && i < 14; ++i)
    {
      auto *f = cJSON_GetObjectItemCaseSensitive(root, fields[i]);
      ok = cJSON_IsNumber(f) && isfinite(f->valuedouble);
      if (ok)
        v[i] = f->valuedouble;
    }


    // Trường tùy chọn thứ 15 "strongImpactFactor" (0 = tắt, hoặc 1..10). Không gửi thì dùng mặc định.
    double heSo = out.heSoVaDapManh;
    if (ok && cnt == 15)
    {
      auto *f = cJSON_GetObjectItemCaseSensitive(root, "strongImpactFactor");
      ok = cJSON_IsNumber(f) && isfinite(f->valuedouble);
      if (ok)
        heSo = f->valuedouble;
    }
    // Các trường thời gian phải là sốnguyên không âm
    for (int i : {3, 4, 5, 6, 8, 11, 13})
      if (ok && (v[i] < 0 || v[i] > 10000 || floor(v[i]) != v[i]))
        ok = false;

    if (ok)
    {
      out.nguongVaDapMs2 = v[0];
      out.giaTocNamYenMs2 = v[1];
      out.saiSoNamYenMs2 = v[2];
      out.cuaSoSauVaDapMs = v[3];
      out.thoiGianNamYenSauVaDapMs = v[4];
      out.soMauNamYenToiThieu = v[5];
      out.khoangDutMauToiDaMs = v[6];
      out.nguongRoiTuDoMs2 = v[7];
      out.thoiGianRoiTuDoToiThieuMs = v[8];
      out.nguongXoayManhDps = v[9];
      out.mucTangApSuatToiThieuPa = v[10];
      out.cuaSoApSuatMs = v[11];
      out.mucGiamDoCaoToiThieuM = v[12];
      out.thoiHanMauMs = v[13];
      out.heSoVaDapManh = float(heSo);
      ok = out.hopLe();
    }
    cJSON_Delete(root);
    return ok;
}

// Gửi JSON xác nhận kết quả cấu hình cho Android
void guiXacNhanCauHinh(const char *trangThai, const char *maLoi, const char *msg)
{
  if (!bleDaKetNoi.load() || !bleKenhXacNhan)
    return;
  char buf[240];
  int n = snprintf(buf, sizeof(buf),
                   "{\"protocolVersion\":1,\"commandId\":\"profile\",\"deviceId\":\"FALLSAFE-%04X\","
                   "\"timestampMs\":%lu,\"commandStatus\":\"%s\",\"errorCode\":%s,\"message\":\"%s\"}",
                   (unsigned)(ESP.getEfuseMac() & 0xFFFF), (unsigned long)millis(), trangThai, maLoi, msg);
  if (n > 0 && size_t(n) < sizeof(buf) && n <= mtuBle.load() - 3)
  {
    bleKenhXacNhan->setValue((uint8_t *)buf, n);
    bleKenhXacNhan->notify();
  }
}

// Nạp cấu hình từ bộ nhớ NVS khi khởi động
void napCauHinhNvs()
{
  if (!nvs.begin("fallsafe", false))
  {
    Serial.println("NVS: loi, dung mac dinh");
    return;
  }
  if (nvs.getBytesLength("profile") != sizeof(CauHinhNga))
    return;
  CauHinhNga ch;
  if (nvs.getBytes("profile", &ch, sizeof(ch)) == sizeof(ch) && ch.hopLe())
  {
    boNhanDang.apDungCauHinh(ch);
    Serial.println("NVS: da nap cau hinh");
    inImpactAcceleration();
  }
}

// Xửlý JSON cấ u hình mới từ Android
void xuLyCauHinhMoi()


{
    if (!coCauHinhMoi.load())
      return;
    if (!bleDaKetNoi.load())
    {
      coCauHinhMoi.store(false);
      return;
    }
    CauHinhNga ch;
    if (!phanTichCauHinh(bufCauHinh, daiCauHinh, ch))
    {
      guiXacNhanCauHinh("REJECTED", "\"INVALID_PROFILE\"", "Invalid profile");
    }
    else
    {
      if (nvs.putBytes("profile", &ch, sizeof(ch)) != sizeof(ch))
         guiXacNhanCauHinh("FAILED", "\"STORAGE_ERROR\"", "Could not save profile");
      else
      {
         boNhanDang.apDungCauHinh(ch);
         guiXacNhanCauHinh("COMPLETED", "null", "Applied and saved");
         Serial.println("NVS: da luu cau hinh moi");
         inImpactAcceleration();
      }
    }
    coCauHinhMoi.store(false);
}

// ================================================================
// PHẦ N 12: LỆNH SERIAL (DÒNG LỆNH USB DÀNH CHO KỸ SƯ KIỂM THỬ)
// [NHÓM TỰ CODE - CÔNG CỤ TỰ THỬNGHIỆM VÀ GỠ LỖ     ỦA THI & HÂN]
                                                  I CỦ
// ================================================================

const char *tenTrangThai(TrangThaiNga s)
{
  switch (s)
  {
  case TrangThaiNga::THEO_DOI:
    return "MONITORING";
  case TrangThaiNga::NGHI_NGO:
    return "SUSPECTED";
  case TrangThaiNga::XAC_MINH:
    return "VERIFYING";
  case TrangThaiNga::CANH_BAO:
    return "ALERT";
  }
  return "UNKNOWN";
}

// In ngưỡng va đập hiện đang dùng (m/s²) ra Serial đểkiể m tra cấu hình
void inImpactAcceleration()
{
  Serial.printf("IMPACT_ACCELERATION: %.2f m/s^2 | STRONG_FACTOR: %.2f\n",
                boNhanDang.cauHinh.nguongVaDapMs2, boNhanDang.cauHinh.heSoVaDapManh);
}

void xuLySerial()
{
  if (!Serial.available())
    return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

    if (cmd == "DUMP_MPU")
    {
      // In toàn bộ nhật ký CSV ra màn hình Serial
      if (!fsOk)
      {
        Serial.println("LittleFS chua san sang");
        return;
      }
      File f = LittleFS.open(FILE_CSV, "r");
      if (!f)
      {
        Serial.println("Chua co du lieu");


        return;
      }
      Serial.println("BEGIN_MPU_CSV");
      while (f.available())
        Serial.write(f.read());
      f.close();
      Serial.println("END_MPU_CSV");
    }
    else if (cmd == "TEST_SOS")
    {
      if (daiSos)
      {
        Serial.println("Dang co SOS trong hang doi");
        return;
      }
      static uint16_t id = 40000;
      if (++id == 0)
        id = 40000;
      xepSos(id, true);
      Serial.printf("TEST_SOS: id=%u\n", id);
    }
    else if (cmd == "TEST_SOS_REAL")
    {
      if (!bleDaKetNoi || !daDangKySuKien)
      {
        Serial.println("Android chua ket noi");
        return;
      }
      if (daiSos)
      {
        Serial.println("Dang co SOS trong hang doi");
        return;
      }
      static uint16_t id = 50000;
      if (++id == 0)
        id = 50000;
      xepSos(id);
      Serial.printf("TEST_SOS_REAL: id=%u\n", id);
    }
    else if (cmd == "FORMAT_MPU" && !fsOk)
    {
      if (LittleFS.format())
      {
        fsOk = LittleFS.begin(false);
      }
      Serial.println(fsOk ? "LittleFS: OK" : "LittleFS: loi");
    }
}

// ================================================================
// PHẦ         Ở I ĐỘNG VÀ VÒNG LẶP CHÍNH (BỘ ĐIỀ
       N 13: KHỞ                                 U PHỐI ĐA NHIỆM THỜI GIAN THỰC)
// [KẾ T HỢP: Khung setup/loop Arduino + NHÓM TỰ CODE ĐIỀ U PHỐI MILLIS]
// (Không dùng lệnh delay() làm treo chip; nhóm tự code bộ điề  u phối thời gian
//   thực đểcác cảm biế  n và Bluetooth chạy song song nhịp nhàng)
// ================================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

    // Bật 2 bus I2C
    busMpu.begin(MPU_SDA, MPU_SCL, 100000);
    busMpu.setTimeOut(5);
    busGy63.begin(GY63_SDA, GY63_SCL, 100000);
    busGy63.setTimeOut(5);
    Serial.println("\n=== FallSafe v13 khoi dong ===");

    // Quét I2C đểkiể m tra kết nối phần cứng
    Serial.printf("MPU (0x68): %s\n", i2cPing(busMpu, DIA_CHI_MPU) ? "OK" : "KHONG THAY");
    Serial.printf("GY63(0x77): %s\n", i2cPing(busGy63, DIA_CHI_GY63) ? "OK" : "KHONG THAY");

    // Nạp cấu hình → khởi tạo cảm biế
                                     n → bật Bluetooth
    napCauHinhNvs();
    khoiTaoMpu();


    khoiTaoGy63();
    fsOk = LittleFS.begin(false);
    if (!fsOk)
      Serial.println("LittleFS: gui 'FORMAT_MPU' de khoi tao");
    khoiTaoBle();
    Serial.println("=== San sang ===\n");
}

void loop()
{
  // Biến static: chỉ khởi tạo 1 lần, nhớ giá trị giữa các lần gọi
  static uint32_t lucInMs = 0;
  static uint32_t lucNgaTiepMs = 0;
  static uint32_t mpuTruoc = 0, gy63Truoc = 0;

    // Khởi động lại quảng bá BLE sau khi điện thoại ngắ
                                                       t kế
                                                          t nối
    if (canQuangBaLai.exchange(false))
    {
      BLEDevice::startAdvertising();
      Serial.println("BLE: quang ba lai");
    }

    uint32_t t = millis();

    // ── Hết giờ nghỉ sau SOS → tiếp tục theo dõi ────────────────
    if (dangNghi && uint32_t(t - lucBatDauNghiMs) >= NGHI_SAU_SOS_MS)
    {
      dangNghi = false;
      boNhanDang.trangThai = TrangThaiNga::THEO_DOI;
      boNhanDang.xoaUngVienNga();
      boNhanDang.daCoMau = false;
      mpuMau.ok = gy63Mau.ok = false;
      gdGy63 = GdGy63::RANH;
      lucNgaTiepMs = t + 10;
      Serial.println("SOS: het 5 phut nghi, tiep tuc doc cam bien");
    }

    // ── Lấy mẫu phát hiện ngã mỗi 10ms ──────────────────────────
    if (!dangNghi && int32_t(t - lucNgaTiepMs) >= 0)
    {
      lucNgaTiepMs = t + 10;
      float giaToc = 0, xoay = 0;
      bool imuOk = docMpuNhanh(giaToc, xoay);
      t = millis(); // Chốt giờ SAU khi đọc MPU, tránh trừ giờ cũ cho dấu thời gian mới.
      bool mpuMoi = mpuMau.ok && uint32_t(t - mpuMau.tMs) <= boNhanDang.cauHinh.thoiHanMauMs;
      bool gy63Moi = gy63Mau.ok && uint32_t(t - gy63Mau.tMs) <= boNhanDang.cauHinh.thoiHanMauMs;

        TrangThaiNga truoc = boNhanDang.trangThai;
        bool ngaDuocXacNhan = boNhanDang.xuLyMau(
            t, giaToc, xoay, gy63Moi ? gy63Mau.doCaoM : NAN,
            imuOk && mpuMoi, gy63Moi ? gy63Mau.apSuatPa : NAN);

        if (ngaDuocXacNhan)
        {
          Serial.println("FALL: ALERT -> gui SOS");
          xepSos(boNhanDang.maSuKien); // Báo động!
        }
        else if (boNhanDang.trangThai != truoc)
        {
          Serial.printf("FALL: %s\n", tenTrangThai(boNhanDang.trangThai));
        }
        if (boNhanDang.ghiChu[0])
        {
          Serial.printf("FALL: %s\n", boNhanDang.ghiChu);
          boNhanDang.ghiChu[0] = 0;
        }
    }

    // ── Đọc GY-63 (máy trạng thái, không chặn) ───────────────────
    if (!dangNghi)
      docGy63();

    // ── Xửlý SOS (ưu tiên cao nhất) ────────────────────────────
    t = millis(); // docGy63 có thểtạo mẫu mới hoặc mất thời gian I2C.
    bool sosDangGui = guiSos(t);


    // ── Trong giờ nghỉ: không làm gì thêm ───────────────────────
    if (dangNghi)
    {
      canGuiDuLieu.store(false);
      delay(1);
      return;
    }

    // ── Khi không có SOS: xửlý lệnh và gửi dữ liệu thường ──────
    if (!sosDangGui)
    {
      xuLySerial();
      if (dangNghi)
        return; // xuLySerial có thểtrigger SOS test
      xuLyCauHinhMoi();
      t = millis();
      guiDuLieu(t);
    }

    // ── In thống kê mỗi 3 giây ───────────────────────────────────
    if (uint32_t(t - lucInMs) >= 3000)
    {
      uint32_t dt = t - lucInMs;
      lucInMs = t;

      // Đọc và lưu CSV khi hệ thống đang bình thường (không gửi SOS)
      if (!daiSos && boNhanDang.trangThai == TrangThaiNga::THEO_DOI && !boNhanDang.dangRoiTuDo)
        docVaLuuMpuDayDu();

      if (gy63Mau.ok)
        Serial.printf("GY-63: P=%.1fPa T=%.1fC dH=%+.3fm\n",
                      gy63Mau.apSuatPa, gy63Mau.nhietDoC, gy63Mau.doCaoM);
      else
        Serial.println("GY-63: chua co mau");

      Serial.printf("FALL: %s | loi_mau=%lu\n",
                    tenTrangThai(boNhanDang.trangThai), (unsigned long)boNhanDang.soLanDutMau);
      inImpactAcceleration();
      Serial.printf("TAN_SO: MPU=%lu Hz GY63=%lu Hz\n",
                    (unsigned long)((soMauMpu - mpuTruoc) * 1000 / dt),
                    (unsigned long)((soMauGy63 - gy63Truoc) * 1000 / dt));
      mpuTruoc = soMauMpu;
      gy63Truoc = soMauGy63;
      Serial.printf("BLE: %s | SOS: %s\n\n",
                    bleDaKetNoi.load() ? "ket noi" : "quang ba",
                    !daiSos ? "khong" : (sosDangChoAck ? "cho ACK" : "dang gui"));
    }
    delay(1);
}
