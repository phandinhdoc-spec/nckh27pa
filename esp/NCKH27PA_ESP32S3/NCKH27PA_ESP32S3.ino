/*
 ==============================================================================
  DE TAI NCKH: HE THONG PHAT HIEN TE NGA VA CANH BAO KHAN CAP
  Nhom tac gia: Hoc sinh lop 8 (Viet Nam)
  Bo mach su dung: ESP32-S3 Super Mini
  Cam bien:
    1. MPU-6050 (GY-521): Do gia toc 3 truc va van toc goc 3 truc
    2. MS5611 (GY-63)  : Do ap suat khi quyen va nhiet do (tinh do cao tuong doi)

  Phan bo chan giao tiep I2C (ESP32-S3 co 2 bo dieu khien I2C phan cung doc lap):
    - Bus 0 (Wire)  cho MPU-6050: Chan SDA = GPIO 6, Chan SCL = GPIO 7
    - Bus 1 (Wire1) cho GY-63   : Chan SDA = GPIO 3, Chan SCL = GPIO 2

  Ghi chu ve thu vien:
    - Su dung Adafruit_MPU6050 va Adafruit_Sensor cho MPU-6050.
    - Su dung thu vien MS5611 cua tac gia Rob Tillaart cho GY-63.
 ==============================================================================
*/

// ==============================================================================
// 0. CHON CHE DO CHAY CUA CHUONG TRINH
// ==============================================================================
// CHE_DO_KIEM_TRA = 1 : Man hinh kiem tra truc quan de doc tren Serial Monitor (~2 Hz)
// CHE_DO_KIEM_TRA = 0 : Che do ghi du lieu sach 13 cot CSV de huan luyen mo hinh (50 Hz)
#define CHE_DO_KIEM_TRA 1

// ==============================================================================
// 1. KHAI BAO THU VIEN
// ==============================================================================
#include <Wire.h>               // Thu vien giao tiep I2C mac dinh cua Arduino
#include <Adafruit_MPU6050.h>   // Thu vien cam bien chuyen dong MPU-6050
#include <Adafruit_Sensor.h>    // Thu vien chuan cam bien cua Adafruit
#include <MS5611.h>             // Thu vien cam bien ap suat MS5611 cua Rob Tillaart
#include <math.h>               // Thu vien toan hoc (dung cho can bac hai, luy thua, fabsf)

// ==============================================================================
// 2. CAU HINH PHAN CUNG VA CAC HANG SO
// ==============================================================================

// --- Cau hinh chan I2C cho ESP32-S3 ---
const int CHAN_MPU_SDA  = 6;  // Chan du lieu SDA cua MPU6050 noi vao GPIO 6
const int CHAN_MPU_SCL  = 7;  // Chan xung nhip SCL cua MPU6050 noi vao GPIO 7
const int CHAN_GY63_SDA = 3;  // Chan du lieu SDA cua MS5611 noi vao GPIO 3
const int CHAN_GY63_SCL = 2;  // Chan xung nhip SCL cua MS5611 noi vao GPIO 2

// Toc do xung nhip I2C (100 kHz giup tin hieu on dinh nhat khi dung day noi jumper)
const uint32_t TAN_SO_I2C = 100000;

// Che do sua loi chia doi ap suat cua MS5611 (mathMode = 1 theo tai lieu Rob Tillaart)
const uint8_t CHE_DO_TOAN_MS5611 = 1;

// Dia chi I2C cua cac cam bien tren bus
const uint8_t DIA_CHI_MPU_CHINH  = 0x68;  // Dia chi mac dinh khi chan AD0 noi GND
const uint8_t DIA_CHI_MPU_PHU    = 0x69;  // Dia chi du phong khi chan AD0 noi VCC
const uint8_t DIA_CHI_GY63_CHINH = 0x77;  // Dia chi mac dinh cua GY-63 khi chan CSB noi GND
const uint8_t DIA_CHI_GY63_PHU   = 0x76;  // Dia chi du phong cua GY-63 khi chan CSB noi VCC

// --- Cau hinh tan so va chu ky thoi gian ---
#if CHE_DO_KIEM_TRA
const uint32_t TAN_SO_LAY_MAU_HZ = 100;  // Che do kiem tra: 100 Hz de bat nhanh moi cu dong
#else
const uint32_t TAN_SO_LAY_MAU_HZ = 50;   // Che do ghi CSV: 50 Hz chuan de gui khong nghen cong Serial
#endif
// Chu ky giua hai lan lay mau (tinh bang micro giay - microsecond)
const uint32_t CHU_KY_LAY_MAU_US = 1000000UL / TAN_SO_LAY_MAU_HZ;

// Tan so in man hinh kiem tra ra Serial (2 Hz = moi nua giay in mot lan)
const uint32_t TAN_SO_HIEN_THI_HZ = 2;
const uint32_t SO_MAU_MOI_LAN_HIEN_THI = TAN_SO_LAY_MAU_HZ / TAN_SO_HIEN_THI_HZ;

// Chu ky doc cam bien khi ap MS5611 (40 mili giay ~ 25 Hz, vi MS5611 can ~4.6 ms de bien doi)
const uint32_t CHU_KY_DOC_KHI_AP_MS = 40;

// Thoi gian cho thu lai neu cam bien bi long day hoac mat ket noi (5000 ms = 5 giay)
const uint32_t CHU_KY_THU_LAI_MS = 5000;

// Neu bi loi doc lien tiep 10 lan thi danh dau cam bien bi mat ket noi de thu lai
const uint8_t GIOI_HAN_LOI_LIEN_TIEP = 10;

// Thoi gian cho ket noi Serial khoi dong (toi da 3 giay)
const uint32_t THOI_GIAN_CHO_SERIAL_MS = 3000;

// --- Thiet lap moc ap suat chuan (Baseline Datum) ---
const uint16_t MUC_TIEU_MAU_CHUAN = 30;           // Can gom 30 mau khi ap luc khoi dong
const uint16_t SO_MAU_CHUAN_TOI_THIEU = 15;       // Toi thieu phai duoc 15 mau hop le
const uint32_t THOI_GIAN_CHO_MAU_CHUAN_MS = 3000; // Thoi gian gom mau toi da 3 giay
const float GIOI_HAN_DUOI_AP_SUAT_PA = 30000.0f;  // Ap suat nho hon 30000 Pa coi nhu loi
const float GIOI_HAN_TREN_AP_SUAT_PA = 110000.0f; // Ap suat lon hon 110000 Pa coi nhu loi
const float GIA_TRI_TRONG_LUC = 9.80665f;         // 1g = 9.80665 m/s^2

#if CHE_DO_KIEM_TRA
// Cac nguong dung de hien thi nhan xet truc quan (chi dung de xem tren man hinh test)
const float NGUONG_GIA_TOC_CHUAN_G        = 1.0f;   // Gia toc chuan khi dung yen la ~1g
const float DUNG_SAI_GIA_TOC_CHUAN_G     = 0.15f;  // Cho phep sai lech 0.15g quanh 1g
const float NGUONG_DAO_DONG_DANG_YEN_G    = 0.05f;  // Bien do dao dong nho hon 0.05g la rat it thay doi
const float NGUONG_DAO_DONG_CHUYEN_DONG_NHE_G = 0.35f; // Nho hon 0.35g la chuyen dong nhe
const float NGUONG_ROI_TU_DO_G            = 0.5f;   // Nho hon 0.5g la co dau hieu giam tai / roi
const float NGUONG_VA_DAP_MANH_G          = 2.0f;   // Lon hon 2.0g la co va dap manh
const float NGUONG_XOAY_NHE_DPS           = 10.0f;  // Xoay tren 10 do/giay
const float NGUONG_XOAY_MANH_DPS          = 60.0f;  // Xoay tren 60 do/giay
const float DO_TRE_XU_HUONG_CAO_DO_M      = 0.15f;  // Doi it nhat 0.15 m de xac nhan xu huong len/xuong
const uint8_t DO_DAI_BO_LOC_TRUOT         = 5;      // Tinh trung binh 5 mau cao do gan nhat
const uint8_t SO_O_THANH_TIEN_DO          = 10;     // Thanh tien do 10 o
const uint32_t CUA_SO_DO_NHIP_THUC_US     = 1000000UL; // Cua so 1 giay de do tan so thuc te
#endif

// ==============================================================================
// 3. KHAI BAO BIEN TOAN CUC
// ==============================================================================

// --- Doi tuong cam bien ---
Adafruit_MPU6050 cam_bien_mpu;                  // Doi tuong MPU6050 (chay tren Wire - Bus 0)
MS5611 cam_bien_baro_chinh(DIA_CHI_GY63_CHINH, &Wire1); // GY-63 dia chi chinh 0x77 tren Wire1
MS5611 cam_bien_baro_phu(DIA_CHI_GY63_PHU, &Wire1);     // GY-63 dia chi phu 0x76 tren Wire1
MS5611 *cam_bien_baro = &cam_bien_baro_chinh;           // Con tro tro toi cam bien dang hoat dong

// --- Bien trang thai cam bien MPU-6050 ---
bool mpu_san_sang = false;             // Co bao MPU6050 da ket noi thanh cong
bool mpu_tung_ket_noi = false;         // Ghi nho MPU6050 da tung duoc tim thay chua
uint8_t dia_chi_mpu_dang_dung = 0;     // Luu dia chi 0x68 hoac 0x69 dang dung
uint8_t dem_loi_mpu_lien_tiep = 0;     // Dem so lan doc loi lien tiep
uint32_t tong_so_loi_mpu = 0;          // Tong so lan doc MPU bi loi tu luc bat nguon
uint32_t so_lan_ket_noi_lai_mpu = 0;   // So lan tim thay lai cam bien sau khi bi long day

// --- Bien trang thai cam bien MS5611 (GY-63) ---
bool baro_san_sang = false;            // Co bao MS5611 da ket noi thanh cong
uint8_t dia_chi_baro_dang_dung = 0;    // Luu dia chi 0x77 hoac 0x76 dang dung

// --- Bien quan ly moc ap suat chuan (Baseline) ---
bool co_ap_suat_chuan = false;         // Co bao ap suat chuan da duoc xac lap hop le
float ap_suat_chuan_pa = 0.0f;         // Gia tri ap suat chuan (Pa) lam moc tinh do cao
uint16_t dem_so_mau_chuan = 0;         // So mau hop le thu duoc de tinh ap suat chuan
bool dang_thu_thap_lai_chuan = false;  // Co bao dang gom mau chuan moi khi cam bien hoi phuc
float tong_ap_suat_thu_thap_lai = 0.0f;// Tong ap suat trong qua trinh gom lai
uint32_t thoi_diem_bat_dau_gom_chuan_ms = 0; // Thoi diem bat dau gom lai mau chuan

// --- Bien luu du lieu thu thap o moi slot (don vi chuan quoc te SI) ---
float dong_ax, dong_ay, dong_az;       // Gia toc 3 truc X, Y, Z (m/s^2)
float dong_a_tong;                     // Do lon vector gia toc tong A = sqrt(ax^2 + ay^2 + az^2) (m/s^2)
float dong_gx, dong_gy, dong_gz;       // Van toc goc 3 truc X, Y, Z (do/giay - deg/s)
float dong_g_tong;                     // Do lon van toc goc tong G = sqrt(gx^2 + gy^2 + gz^2) (deg/s)
float dong_ap_suat_pa;                 // Ap suat khi quyen tuc thoi (Pa)
float dong_chenh_ap_pa;                // Do lech ap suat so voi moc chuan Delta P = P - P_chuan (Pa)
float dong_nhiet_do_c;                 // Nhiet do moi truong tu cam bien (do C)
float dong_do_cao_tuong_doi;           // Do cao tuong doi tinh tu cong thuc barometric (met)

// --- Bien quan ly nhip thoi gian ---
uint32_t moc_thoi_gian_slot_us = 0;    // Moc microsecond cho lan lay mau ke tiep
uint32_t thoi_diem_doc_baro_gan_nhat_ms = 0; // Thoi diem doc baro gan nhat
uint32_t thoi_diem_thu_lai_mpu_ms = 0;       // Thoi diem thu ket noi lai MPU
uint32_t thoi_diem_thu_lai_baro_ms = 0;      // Thoi diem thu ket noi lai MS5611
bool den_luot_doc_baro = true;               // Co bao da den luc doc baro

#if CHE_DO_KIEM_TRA
// --- Cac bien phuc vu man hinh kiem tra truc quan (~2 Hz) ---
uint32_t dem_chu_ky_hien_thi = 0;      // Dem so mau de du 500 ms thi in man hinh
bool cua_so_co_mpu = false;            // Cua so hien tai co mau MPU hop le khong
float cua_so_ax_g = 0.0f, cua_so_ay_g = 0.0f, cua_so_az_g = 0.0f, cua_so_a_tong_g = 0.0f;
float cua_so_a_min_g = 0.0f, cua_so_a_max_g = 0.0f;
bool cua_so_chuyen_dong_manh = false;
float cua_so_gx_max = 0.0f, cua_so_gy_max = 0.0f, cua_so_gz_max = 0.0f, cua_so_g_max = 0.0f;

bool cua_so_co_baro = false;           // Cua so hien tai co mau MS5611 hop le khong
float cua_so_ap_suat_pa = 0.0f;
float cua_so_chenh_ap_pa = 0.0f;
float cua_so_nhiet_do_c = 0.0f;
float cua_so_do_cao_m = 0.0f;

float lich_su_do_cao[DO_DAI_BO_LOC_TRUOT]; // Mang luu 5 gia tri do cao gan nhat
uint8_t chi_so_lich_su_do_cao = 0;
uint8_t so_mau_lich_su_do_cao = 0;
float do_cao_tham_chieu_xu_huong_m = 0.0f;
bool da_co_do_cao_tham_chieu = false;
uint8_t trang_thai_xu_huong = 0;       // 0: On dinh, 1: Dang len, 2: Dang xuong

uint32_t dem_so_slot_thuc_te = 0;
uint32_t so_slot_da_chay = 0;
uint32_t so_slot_ky_vong = 0;
uint32_t moc_bat_dau_do_nhip_us = 0;
float tan_so_thuc_te_mpu_hz = 0.0f;
bool da_do_xong_tan_so = false;
#endif

// ==============================================================================
// 4. CAC HAM KHOI TAO VA DOC CAM BIEN
// ==============================================================================

// Ham thu khoi tao MPU6050 tai mot dia chi nhat dinh
bool khoi_tao_mpu(uint8_t dia_chi) {
  // Khoi tao giao tiep qua con tro &Wire (Bus 0)
  if (!cam_bien_mpu.begin(dia_chi, &Wire)) {
    return false;
  }
  // Cau hinh thang do gia toc +-8g (chong bao hoa khi hoc sinh chay nhay hoac bi te nga)
  cam_bien_mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  // Cau hinh thang do van toc goc +-500 do/giay
  cam_bien_mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  // Cau hinh bo loc thong thap phan cung DLPF 184 Hz de loc nhieu rung
  cam_bien_mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);
  return true;
}

// Ham tu dong do tim MPU6050 (thu dia chi 0x68 truoc, neu khong duoc thu 0x69)
uint8_t do_tim_mpu() {
  if (khoi_tao_mpu(DIA_CHI_MPU_CHINH)) return DIA_CHI_MPU_CHINH;
  if (khoi_tao_mpu(DIA_CHI_MPU_PHU))   return DIA_CHI_MPU_PHU;
  return 0; // Khong tim thay cam bien
}

// Ham thu khoi tao MS5611 (GY-63)
bool khoi_tao_baro(MS5611 *cam_bien) {
  if (!cam_bien->begin()) {
    return false;
  }
  // Goi reset(1) de chon mathMode 1 (factor 2 fix), tranh loi mat 50% ap suat
  if (!cam_bien->reset(CHE_DO_TOAN_MS5611)) {
    return false;
  }
  // Dat muc lay mau qua (oversampling) tieu chuan OSR_STANDARD
  cam_bien->setOversampling(OSR_STANDARD);
  return true;
}

// Ham tu dong do tim MS5611 (thu dia chi 0x77 truoc, neu khong duoc thu 0x76)
uint8_t do_tim_baro() {
  if (khoi_tao_baro(&cam_bien_baro_chinh)) return DIA_CHI_GY63_CHINH;
  if (khoi_tao_baro(&cam_bien_baro_phu))   return DIA_CHI_GY63_PHU;
  return 0; // Khong tim thay cam bien
}

// Ham doc du lieu tu MPU6050 va kiem tra loi minh bach
bool doc_cam_bien_mpu(float *ax, float *ay, float *az, float *gx, float *gy, float *gz) {
  if (!mpu_san_sang) return false;

  sensors_event_t bien_co_gia_toc, bien_co_con_quay, bien_co_nhiet_do;
  if (!cam_bien_mpu.getEvent(&bien_co_gia_toc, &bien_co_con_quay, &bien_co_nhiet_do)) {
    tong_so_loi_mpu++;
    if (dem_loi_mpu_lien_tiep < 255) dem_loi_mpu_lien_tiep++;
    if (dem_loi_mpu_lien_tiep >= GIOI_HAN_LOI_LIEN_TIEP) mpu_san_sang = false;
    return false;
  }

  // Kiem tra gia tri co bi vo cuc (inf) hoac khong phai so (NaN) do loi I2C khong
  if (!isfinite(bien_co_gia_toc.acceleration.x) ||
      !isfinite(bien_co_gia_toc.acceleration.y) ||
      !isfinite(bien_co_gia_toc.acceleration.z) ||
      !isfinite(bien_co_con_quay.gyro.x) ||
      !isfinite(bien_co_con_quay.gyro.y) ||
      !isfinite(bien_co_con_quay.gyro.z)) {
    tong_so_loi_mpu++;
    if (dem_loi_mpu_lien_tiep < 255) dem_loi_mpu_lien_tiep++;
    if (dem_loi_mpu_lien_tiep >= GIOI_HAN_LOI_LIEN_TIEP) mpu_san_sang = false;
    return false;
  }

  // Doc thanh cong, reset chuoi loi
  dem_loi_mpu_lien_tiep = 0;

  // Gia toc duoc thu vien tra ve theo don vi m/s^2
  *ax = bien_co_gia_toc.acceleration.x;
  *ay = bien_co_gia_toc.acceleration.y;
  *az = bien_co_gia_toc.acceleration.z;

  // Thu vien tra ve radian/giay -> nhan SENSORS_RADS_TO_DPS (x 180 / pi) de doi sang do/giay
  *gx = bien_co_con_quay.gyro.x * SENSORS_RADS_TO_DPS;
  *gy = bien_co_con_quay.gyro.y * SENSORS_RADS_TO_DPS;
  *gz = bien_co_con_quay.gyro.z * SENSORS_RADS_TO_DPS;
  return true;
}

// Ham doc du lieu tu MS5611 (GY-63)
bool doc_cam_bien_baro(float *ap_suat_pa, float *nhiet_do_c) {
  if (!baro_san_sang) return false;

  // Ham read() doc ca ap suat va nhiet do
  if (cam_bien_baro->read() != MS5611_READ_OK) {
    return false;
  }

  float p = cam_bien_baro->getPressurePascal(); // Doc ap suat don vi Pascal (Pa)
  float t = cam_bien_baro->getTemperature();    // Doc nhiet do don vi do C

  // Kiem tra gia tri hop le va nam trong gioi han khi quyen
  if (!isfinite(p) || !isfinite(t)) return false;
  if (p < GIOI_HAN_DUOI_AP_SUAT_PA || p > GIOI_HAN_TREN_AP_SUAT_PA) return false;

  *ap_suat_pa = p;
  *nhiet_do_c = t;
  return true;
}

// ==============================================================================
// 5. CAC HAM QUAN LY MOC AP SUAT CHUAN (BASELINE DATUM)
// ==============================================================================

#if CHE_DO_KIEM_TRA
// In thanh tien do thu thap mau chuan tren Serial Monitor
void in_thanh_tien_do_mau_chuan(uint16_t so_luong) {
  if (so_luong > MUC_TIEU_MAU_CHUAN) so_luong = MUC_TIEU_MAU_CHUAN;
  uint8_t so_o_day = (uint8_t)((uint32_t)so_luong * (uint32_t)SO_O_THANH_TIEN_DO / (uint32_t)MUC_TIEU_MAU_CHUAN);
  if (so_o_day > SO_O_THANH_TIEN_DO) so_o_day = SO_O_THANH_TIEN_DO;
  Serial.print(F("\r["));
  for (uint8_t i = 0; i < SO_O_THANH_TIEN_DO; i++) {
    Serial.print(i < so_o_day ? '#' : '-');
  }
  Serial.printf("] %u/%u mau khi ap chuan", (unsigned)so_luong, (unsigned)MUC_TIEU_MAU_CHUAN);
}
#endif

// Thu thap ap suat chuan luc khoi dong chuong trinh trong setup()
void thu_thap_ap_suat_chuan() {
  if (!baro_san_sang) return;

  uint32_t thoi_diem_bat_dau_ms = millis();
  uint32_t thoi_diem_mau_truoc_ms = 0;
  bool da_co_mau_dau_tien = false;
  float tong_ap_suat = 0.0f;
  uint16_t so_mau_doc_duoc = 0;

  // Gom toi da 30 mau trong vong khong qua 3 giay
  while (so_mau_doc_duoc < MUC_TIEU_MAU_CHUAN && (millis() - thoi_diem_bat_dau_ms) < THOI_GIAN_CHO_MAU_CHUAN_MS) {
    if (da_co_mau_dau_tien && (millis() - thoi_diem_mau_truoc_ms) < CHU_KY_DOC_KHI_AP_MS) {
      yield(); // Nhuong quyen cho he thong de tranh reset Watchdog Timer
      continue;
    }
    da_co_mau_dau_tien = true;
    thoi_diem_mau_truoc_ms = millis();

    float p, t;
    if (doc_cam_bien_baro(&p, &t)) {
      tong_ap_suat += p;
      so_mau_doc_duoc++;
#if CHE_DO_KIEM_TRA
      in_thanh_tien_do_mau_chuan(so_mau_doc_duoc);
#endif
    }
  }

  dem_so_mau_chuan = so_mau_doc_duoc;
  if (so_mau_doc_duoc >= SO_MAU_CHUAN_TOI_THIEU) {
    ap_suat_chuan_pa = tong_ap_suat / (float)so_mau_doc_duoc;
    co_ap_suat_chuan = true;
  }

#if CHE_DO_KIEM_TRA
  in_thanh_tien_do_mau_chuan(so_mau_doc_duoc);
  Serial.println();
#endif
}

// Bat dau gom lai mau chuan khi cam bien MS5611 hoi phuc (chay nen, khong lam dung chuong trinh)
void bat_dau_thu_thap_lai_chuan() {
  dang_thu_thap_lai_chuan = true;
  dem_so_mau_chuan = 0;
  tong_ap_suat_thu_thap_lai = 0.0f;
  thoi_diem_bat_dau_gom_chuan_ms = millis();
#if CHE_DO_KIEM_TRA
  Serial.println(F("MS5611 da hoi phuc: dang tu dong lay lai ap suat chuan..."));
#endif
}

// Chot ket qua thu thap lai ap suat chuan
void ket_thuc_thu_thap_lai_chuan(bool thanh_cong) {
  dang_thu_thap_lai_chuan = false;
  if (thanh_cong) {
    ap_suat_chuan_pa = tong_ap_suat_thu_thap_lai / (float)dem_so_mau_chuan;
    co_ap_suat_chuan = true;
#if CHE_DO_KIEM_TRA
    Serial.printf("Ap suat chuan moi: %.1f Pa (%u mau hop le)\n", ap_suat_chuan_pa, (unsigned)dem_so_mau_chuan);
#endif
  } else {
    co_ap_suat_chuan = false;
#if CHE_DO_KIEM_TRA
    Serial.println(F("Khong du mau de dat lai ap suat chuan moi."));
#endif
  }
}

// Nap them 1 mau khi ap vao bo dem gom mau chuan
void nap_mau_khi_ap_vao_chuan(float p) {
  if (!dang_thu_thap_lai_chuan) return;
  tong_ap_suat_thu_thap_lai += p;
  dem_so_mau_chuan++;
  if (dem_so_mau_chuan >= MUC_TIEU_MAU_CHUAN) {
    ket_thuc_thu_thap_lai_chuan(true);
  }
}

// Kiem tra het han cua so thu thap lai chuan
void xu_ly_thu_thap_lai_chuan() {
  if (!dang_thu_thap_lai_chuan) return;
  if ((millis() - thoi_diem_bat_dau_gom_chuan_ms) >= THOI_GIAN_CHO_MAU_CHUAN_MS) {
    ket_thuc_thu_thap_lai_chuan(dem_so_mau_chuan >= SO_MAU_CHUAN_TOI_THIEU);
  }
}

// ==============================================================================
// 6. CAC HAM IN DU LIEU VA GIAO DIEN SERIAL
// ==============================================================================

// In bang thong tin khoi dong he thong
void in_thong_tin_khoi_dong() {
  Serial.println();
  Serial.println(F("=================================================="));
  Serial.println(F("   DE TAI NCKH: HE THONG PHAT HIEN TE NGA         "));
  Serial.println(F("   BO MACH: ESP32-S3 SUPER MINI SENSOR NODE       "));
  Serial.println(F("   MPU-6050 (Wire: 6/7) + MS5611 (Wire1: 3/2)     "));
  Serial.println(F("=================================================="));

  if (mpu_san_sang) {
    Serial.printf("MPU-6050 : KET NOI TOT (Dia chi: 0x%02X | SDA=GPIO%d, SCL=GPIO%d | Thang do: +-8g, 500 dps)\n",
                  dia_chi_mpu_dang_dung, CHAN_MPU_SDA, CHAN_MPU_SCL);
  } else {
    Serial.printf("MPU-6050 : KHONG TIM THAY (Kiem tra day SDA=GPIO%d, SCL=GPIO%d va nguon 3.3V)\n",
                  CHAN_MPU_SDA, CHAN_MPU_SCL);
  }

  if (baro_san_sang) {
    Serial.printf("MS5611   : KET NOI TOT (Dia chi: 0x%02X | SDA=GPIO%d, SCL=GPIO%d | mathMode=1 Factor-2 Fix)\n",
                  dia_chi_baro_dang_dung, CHAN_GY63_SDA, CHAN_GY63_SCL);
  } else {
    Serial.printf("MS5611   : KHONG TIM THAY (Kiem tra day SDA=GPIO%d, SCL=GPIO%d va chan CSB)\n",
                  CHAN_GY63_SDA, CHAN_GY63_SCL);
  }

  Serial.printf("Tan so IMU: %lu Hz | Tan so Khi ap: ~25 Hz\n", (unsigned long)TAN_SO_LAY_MAU_HZ);
  if (co_ap_suat_chuan) {
    Serial.printf("Ap suat chuan (Baseline): %.1f Pa (trung binh cua %u mau hop le)\n",
                  ap_suat_chuan_pa, (unsigned)dem_so_mau_chuan);
  } else {
    Serial.println(F("Ap suat chuan: CHUA HOP LE (se tu dong lay lai khi cam bien ket noi lai)"));
  }
}

// In dong tieu de cho file CSV (13 cot dung chuan de nap vao may hoc)
void in_tieu_de_csv() {
  Serial.print(F("timestamp_ms,ax,ay,az,a_mag,gx,gy,gz,g_mag,pressure_pa,pressure_delta_pa,temperature_c,relative_altitude_m\n"));
}

// Gui 1 dong du lieu CSV (neu cot nao khong co cam bien thi de trong)
void gui_dong_csv(uint32_t thoi_gian_ms, bool co_mpu, bool co_ap_suat, bool co_chenh_ap, bool co_nhiet_do, bool co_do_cao) {
  Serial.printf("%lu", (unsigned long)thoi_gian_ms);

  if (co_mpu) {
    Serial.printf(",%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.2f",
                  dong_ax, dong_ay, dong_az, dong_a_tong,
                  dong_gx, dong_gy, dong_gz, dong_g_tong);
  } else {
    Serial.print(F(",,,,,,,,"));
  }

  if (co_ap_suat)  Serial.printf(",%.1f", dong_ap_suat_pa);      else Serial.print(',');
  if (co_chenh_ap) Serial.printf(",%.1f", dong_chenh_ap_pa);     else Serial.print(',');
  if (co_nhiet_do) Serial.printf(",%.2f", dong_nhiet_do_c);      else Serial.print(',');
  if (co_do_cao)   Serial.printf(",%.3f", dong_do_cao_tuong_doi); else Serial.print(',');

  Serial.print('\n');
}

#if CHE_DO_KIEM_TRA
// --- Cac ham ho tro nhan dinh truc quan cho che do kiem tra ---

const char *nhan_dinh_gia_toc() {
  float do_bien_thien = cua_so_a_max_g - cua_so_a_min_g;
  if (do_bien_thien < NGUONG_DAO_DONG_DANG_YEN_G) {
    if (fabsf(cua_so_a_tong_g - NGUONG_GIA_TOC_CHUAN_G) <= DUNG_SAI_GIA_TOC_CHUAN_G) {
      return "BINH THUONG (~1g)";
    }
    return "BAT THUONG (|a| xa 1g)";
  }
  if (cua_so_chuyen_dong_manh) return "CHUYEN DONG MANH";
  return "DANG CHUYEN DONG";
}

const char *nhan_dinh_chuyen_dong() {
  float do_bien_thien = cua_so_a_max_g - cua_so_a_min_g;
  if (do_bien_thien < NGUONG_DAO_DONG_DANG_YEN_G) {
    if (fabsf(cua_so_a_tong_g - NGUONG_GIA_TOC_CHUAN_G) <= DUNG_SAI_GIA_TOC_CHUAN_G) {
      return "DANG YEN";
    }
    return "BAT THUONG";
  }
  if (cua_so_chuyen_dong_manh) return "CHUYEN DONG MANH";
  if (do_bien_thien < NGUONG_DAO_DONG_CHUYEN_DONG_NHE_G) return "CHUYEN DONG NHE";
  return "DANG CHUYEN DONG";
}

const char *nhan_dinh_xoay(float van_toc_xoay_dps) {
  if (van_toc_xoay_dps >= NGUONG_XOAY_MANH_DPS) return "MANH";
  if (van_toc_xoay_dps >= NGUONG_XOAY_NHE_DPS)  return "NHE";
  return "RAT NHO";
}

const char *nhan_dinh_xu_huong_do_cao() {
  if (trang_thai_xu_huong == 1) return "DANG LEN (+)";
  if (trang_thai_xu_huong == 2) return "DANG XUONG (-)";
  return "ON DINH";
}

// Cap nhat xu huong bien doi do cao qua bo loc trung binh truot
void cap_nhat_xu_huong_do_cao(float do_cao_m) {
  lich_su_do_cao[chi_so_lich_su_do_cao] = do_cao_m;
  chi_so_lich_su_do_cao = (uint8_t)((chi_so_lich_su_do_cao + 1) % DO_DAI_BO_LOC_TRUOT);
  if (so_mau_lich_su_do_cao < DO_DAI_BO_LOC_TRUOT) so_mau_lich_su_do_cao++;

  float tong = 0.0f;
  for (uint8_t i = 0; i < so_mau_lich_su_do_cao; i++) {
    tong += lich_su_do_cao[i];
  }
  float trung_binh = tong / (float)so_mau_lich_su_do_cao;

  if (!da_co_do_cao_tham_chieu) {
    do_cao_tham_chieu_xu_huong_m = trung_binh;
    da_co_do_cao_tham_chieu = true;
    trang_thai_xu_huong = 0;
    return;
  }

  if ((trung_binh - do_cao_tham_chieu_xu_huong_m) >= DO_TRE_XU_HUONG_CAO_DO_M) {
    trang_thai_xu_huong = 1; // Dang len cao
    do_cao_tham_chieu_xu_huong_m = trung_binh;
  } else if ((do_cao_tham_chieu_xu_huong_m - trung_binh) >= DO_TRE_XU_HUONG_CAO_DO_M) {
    trang_thai_xu_huong = 2; // Dang ha thap do cao (co the nga hoac di xuong)
    do_cao_tham_chieu_xu_huong_m = trung_binh;
  } else {
    trang_thai_xu_huong = 0; // Do cao on dinh
  }
}

// In thoi gian he thong da hoat dong (Gio:Phut:Giay)
void in_thoi_gian_chay() {
  uint32_t tong_giay = millis() / 1000UL;
  uint32_t gio = tong_giay / 3600UL;
  uint32_t phut = (tong_giay / 60UL) % 60UL;
  uint32_t giay = tong_giay % 60UL;
  Serial.printf("%02lu:%02lu:%02lu\n", (unsigned long)gio, (unsigned long)phut, (unsigned long)giay);
}

// Xoa bo dem thong ke sau moi lan in man hinh
void xoa_bo_dem_hien_thi() {
  cua_so_co_mpu = false;
  cua_so_co_baro = false;
  cua_so_a_min_g = 0.0f;
  cua_so_a_max_g = 0.0f;
  cua_so_chuyen_dong_manh = false;
  cua_so_g_max = 0.0f;
  cua_so_gx_max = 0.0f;
  cua_so_gy_max = 0.0f;
  cua_so_gz_max = 0.0f;
  cua_so_ax_g = 0.0f;
  cua_so_ay_g = 0.0f;
  cua_so_az_g = 0.0f;
  cua_so_a_tong_g = 0.0f;
}

// In man hinh giam sat truc quan de doc (~2 Hz)
void in_man_hinh_kiem_tra() {
  Serial.println(F("=================================================="));
  Serial.println(F("     KIEM TRA CAM BIEN NCKH (ESP32-S3)           "));
  Serial.println(F("=================================================="));

  // --- KHOI 1: THONG SO MPU-6050 ---
  Serial.println(F("[1. CAM BIEN GIA TOC & GOC QUAY (MPU-6050)]"));
  if (mpu_san_sang) {
    Serial.printf("  Trang thai    : HOAT DONG TOT (0x%02X | SDA=%d, SCL=%d)\n",
                  dia_chi_mpu_dang_dung, CHAN_MPU_SDA, CHAN_MPU_SCL);
  } else {
    Serial.printf("  Trang thai    : MAT KET NOI (%lu loi burst, da noi lai %lu lan)\n",
                  (unsigned long)tong_so_loi_mpu, (unsigned long)so_lan_ket_noi_lai_mpu);
  }

  if (cua_so_co_mpu) {
    Serial.printf("  Gia toc 3 truc: X=%+6.2f | Y=%+6.2f | Z=%+6.2f m/s^2\n",
                  cua_so_ax_g * GIA_TRI_TRONG_LUC,
                  cua_so_ay_g * GIA_TRI_TRONG_LUC,
                  cua_so_az_g * GIA_TRI_TRONG_LUC);
    Serial.printf("  Gia toc tong A: %5.2f m/s^2 (%4.2f g) -> %s\n",
                  cua_so_a_tong_g * GIA_TRI_TRONG_LUC,
                  cua_so_a_tong_g,
                  nhan_dinh_gia_toc());
    Serial.printf("  Nhan dinh     : %s\n", nhan_dinh_chuyen_dong());
    Serial.printf("  Toc do goc    : X=%+6.1f | Y=%+6.1f | Z=%+6.1f deg/s\n",
                  cua_so_gx_max, cua_so_gy_max, cua_so_gz_max);
    Serial.printf("  Tong toc do xoay: %5.1f deg/s (%s)\n",
                  cua_so_g_max, nhan_dinh_xoay(cua_so_g_max));
  } else {
    Serial.println(F("  Gia toc       : -- (khong co du lieu)"));
    Serial.println(F("  Toc do goc    : -- (khong co du lieu)"));
  }

  Serial.println();
  Serial.println(F("--------------------------------------------------"));
  Serial.println();

  // --- KHOI 2: THONG SO MS5611 ---
  Serial.println(F("[2. CAM BIEN AP SUAT & DO CAO (MS5611 / GY-63)]"));
  if (baro_san_sang) {
    Serial.printf("  Trang thai    : HOAT DONG TOT (0x%02X | SDA=%d, SCL=%d)\n",
                  dia_chi_baro_dang_dung, CHAN_GY63_SDA, CHAN_GY63_SCL);
  } else {
    Serial.println(F("  Trang thai    : MAT KET NOI"));
  }

  if (cua_so_co_baro) {
    Serial.printf("  Ap suat       : %7.2f hPa (%.0f Pa)\n",
                  cua_so_ap_suat_pa / 100.0f, cua_so_ap_suat_pa);
    Serial.printf("  Nhiet do      : %5.1f degC\n", cua_so_nhiet_do_c);
    if (co_ap_suat_chuan) {
      Serial.printf("  Moc chuan     : %.0f Pa\n", ap_suat_chuan_pa);
      Serial.printf("  Do cao tuong doi: %+.2f m (Chenh lech ap: %+.1f Pa)\n",
                    cua_so_do_cao_m, cua_so_chenh_ap_pa);
      Serial.printf("  Xu huong      : %s\n", nhan_dinh_xu_huong_do_cao());
    } else {
      Serial.println(F("  Moc chuan     : -- (chua hop le)"));
      Serial.println(F("  Do cao        : --"));
    }
  } else {
    Serial.println(F("  Ap suat       : -- (khong co du lieu)"));
    Serial.println(F("  Nhiet do      : -- (khong co du lieu)"));
  }

  Serial.println();
  Serial.println(F("--------------------------------------------------"));
  Serial.println();

  // --- KHOI 3: THONG SO HE THONG ---
  Serial.println(F("[3. THONG SO HE THONG]"));
  if (da_do_xong_tan_so) {
    Serial.printf("  Tan so thuc te: %.1f Hz (chay duoc %lu/%lu slot lay mau)\n",
                  tan_so_thuc_te_mpu_hz, (unsigned long)so_slot_da_chay, (unsigned long)so_slot_ky_vong);
  } else {
    Serial.println(F("  Tan so thuc te: Dang do..."));
  }
  Serial.printf("  Tan so man hinh: %lu Hz | Thoi gian chay: ", (unsigned long)TAN_SO_HIEN_THI_HZ);
  in_thoi_gian_chay();
  Serial.println(F("=================================================="));
  Serial.println();
}
#endif

// ==============================================================================
// 7. HAM SETUP (CHAY 1 LAN KHI KHOI DONG HOAC RESET)
// ==============================================================================
void setup() {
  // Khoi tao giao tiep Serial toc do 115200 baud
  Serial.begin(115200);

  // Cho cong Serial san sang (toi da 3 giay de tranh treo chuong trinh khi cap nguon pin)
  uint32_t thoi_diem_cho_serial_ms = millis();
  while (!Serial && (millis() - thoi_diem_cho_serial_ms) < THOI_GIAN_CHO_SERIAL_MS) {
    yield();
  }
  delay(1000); // Cho dien ap nguon va duong truyen I2C on dinh

  // Khoi tao 2 bus I2C phan cung doc lap tren ESP32-S3
  Wire.begin(CHAN_MPU_SDA, CHAN_MPU_SCL, TAN_SO_I2C);     // Bus 0 cho MPU-6050
  Wire1.begin(CHAN_GY63_SDA, CHAN_GY63_SCL, TAN_SO_I2C);  // Bus 1 cho MS5611 (GY-63)

  // Do tim va khoi tao MPU-6050
  uint8_t dia_chi_mpu = do_tim_mpu();
  if (dia_chi_mpu != 0) {
    mpu_san_sang = true;
    mpu_tung_ket_noi = true;
    dia_chi_mpu_dang_dung = dia_chi_mpu;
  }

  // Do tim va khoi tao MS5611
  uint8_t dia_chi_baro = do_tim_baro();
  if (dia_chi_baro != 0) {
    baro_san_sang = true;
    dia_chi_baro_dang_dung = dia_chi_baro;
  }

  // Thu thap ap suat chuan ban dau
  thu_thap_ap_suat_chuan();

  // In thong tin khoi dong
  in_thong_tin_khoi_dong();

#if !CHE_DO_KIEM_TRA
  // Neu chay che do ghi CSV thi in tieu de cot truoc tien
  in_tieu_de_csv();
#endif

  // Khoi tao moc thoi gian cho vong lap loop()
  moc_thoi_gian_slot_us = micros() + CHU_KY_LAY_MAU_US;
  thoi_diem_doc_baro_gan_nhat_ms = millis();
  thoi_diem_thu_lai_mpu_ms = millis();
  thoi_diem_thu_lai_baro_ms = millis();

#if CHE_DO_KIEM_TRA
  moc_bat_dau_do_nhip_us = micros();
#endif
}

// ==============================================================================
// 8. HAM LOOP (VONG LAP CHINH CUA CHUONG TRINH)
// ==============================================================================
void loop() {
  uint32_t thoi_gian_hien_tai_ms = millis();

  // --- 8.1. TU DONG KET NOI LAI CAM BIEN NEU BI MAT TIN HIEU ---
  if (!mpu_san_sang && (thoi_gian_hien_tai_ms - thoi_diem_thu_lai_mpu_ms) >= CHU_KY_THU_LAI_MS) {
    thoi_diem_thu_lai_mpu_ms = thoi_gian_hien_tai_ms;
    uint8_t dia_chi = do_tim_mpu();
    if (dia_chi != 0) {
      dia_chi_mpu_dang_dung = dia_chi;
      mpu_san_sang = true;
      if (mpu_tung_ket_noi) so_lan_ket_noi_lai_mpu++;
      mpu_tung_ket_noi = true;
      dem_loi_mpu_lien_tiep = 0;
    }
  }

  if (!baro_san_sang && (thoi_gian_hien_tai_ms - thoi_diem_thu_lai_baro_ms) >= CHU_KY_THU_LAI_MS) {
    thoi_diem_thu_lai_baro_ms = thoi_gian_hien_tai_ms;
    uint8_t dia_chi = do_tim_baro();
    if (dia_chi != 0) {
      dia_chi_baro_dang_dung = dia_chi;
      cam_bien_baro = (dia_chi == DIA_CHI_GY63_CHINH) ? &cam_bien_baro_chinh : &cam_bien_baro_phu;
      baro_san_sang = true;
      // Neu chua co moc ap suat chuan thi tu dong gom lai
      if (!co_ap_suat_chuan && !dang_thu_thap_lai_chuan) {
        bat_dau_thu_thap_lai_chuan();
      }
    }
  }

  // Kiem tra het han cua so thu thap lai ap suat chuan
  xu_ly_thu_thap_lai_chuan();

  // --- 8.2. DINH THOI LAY MAU CHINH XAC BANG MICROS() (KHONG DUNG DELAY) ---
  uint32_t thoi_gian_hien_tai_us = micros();
  // Neu chua den moc lay mau thi tiep tuc cho
  if ((int32_t)(thoi_gian_hien_tai_us - moc_thoi_gian_slot_us) < 0) return;

  // Tang moc thoi gian cho lan ke tiep
  moc_thoi_gian_slot_us += CHU_KY_LAY_MAU_US;
  // Xu ly tranh bi tre qua lau neu he thong bi ngat
  if ((int32_t)(moc_thoi_gian_slot_us - thoi_gian_hien_tai_us) <= 0) {
    moc_thoi_gian_slot_us = thoi_gian_hien_tai_us + CHU_KY_LAY_MAU_US;
  }

  uint32_t timestamp_ms = millis();
  bool da_doc_duoc_mpu = false;
  bool da_doc_duoc_ap_suat = false;
  bool da_tinh_duoc_chenh_ap = false;
  bool da_doc_duoc_nhiet_do = false;
  bool da_tinh_duoc_do_cao = false;

  // --- 8.3. DOC DU LIEU MPU-6050 ---
  float ax, ay, az, gx, gy, gz;
  if (doc_cam_bien_mpu(&ax, &ay, &az, &gx, &gy, &gz)) {
    dong_ax = ax;
    dong_ay = ay;
    dong_az = az;
    // Tinh tong do lon vector gia toc: A = sqrt(ax^2 + ay^2 + az^2)
    dong_a_tong = sqrtf(ax * ax + ay * ay + az * az);

    dong_gx = gx;
    dong_gy = gy;
    dong_gz = gz;
    // Tinh tong do lon van toc goc: G = sqrt(gx^2 + gy^2 + gz^2)
    dong_g_tong = sqrtf(gx * gx + gy * gy + gz * gz);

    if (isfinite(dong_a_tong) && isfinite(dong_g_tong)) {
      da_doc_duoc_mpu = true;
    }
  }

  // --- 8.4. DOC DU LIEU MS5611 (GY-63) (CHU KY ~40 MS = 25 HZ) ---
  if (baro_san_sang && (den_luot_doc_baro || (timestamp_ms - thoi_diem_doc_baro_gan_nhat_ms) >= CHU_KY_DOC_KHI_AP_MS)) {
    den_luot_doc_baro = false;
    thoi_diem_doc_baro_gan_nhat_ms = timestamp_ms;

    float p, t;
    if (doc_cam_bien_baro(&p, &t)) {
      dong_ap_suat_pa = p;
      dong_nhiet_do_c = t;
      da_doc_duoc_ap_suat = true;
      da_doc_duoc_nhiet_do = true;

      // Neu dang gom mau chuan hoi phuc thi nap them mau vao
      nap_mau_khi_ap_vao_chuan(p);

      // Neu da co ap suat chuan thi tinh chenh lech va do cao tuong doi
      if (co_ap_suat_chuan) {
        float delta_p = p - ap_suat_chuan_pa;
        // Cong thuc khi ap barometric de tinh do cao tuong doi: h = 44330.77 * (1 - (p / p0)^0.190263)
        float do_cao = 44330.77f * (1.0f - powf(p / ap_suat_chuan_pa, 0.190263f));

        if (isfinite(delta_p) && isfinite(do_cao)) {
          dong_chenh_ap_pa = delta_p;
          dong_do_cao_tuong_doi = do_cao;
          da_tinh_duoc_chenh_ap = true;
          da_tinh_duoc_do_cao = true;
        }
      }

#if CHE_DO_KIEM_TRA
      // Luu thong tin vao cua so hien thi
      cua_so_co_baro = true;
      cua_so_ap_suat_pa = p;
      cua_so_nhiet_do_c = t;
      if (da_tinh_duoc_do_cao) {
        cua_so_chenh_ap_pa = dong_chenh_ap_pa;
        cua_so_do_cao_m = dong_do_cao_tuong_doi;
        cap_nhat_xu_huong_do_cao(dong_do_cao_tuong_doi);
      }
#endif
    }
  }

#if CHE_DO_KIEM_TRA
  // --- 8.5. CHE DO KIEM TRA: GOM THONG KE VA IN RA MAN HINH THEO NHIP 2 HZ ---
  if (da_doc_duoc_mpu) {
    float a_g = dong_a_tong / GIA_TRI_TRONG_LUC;
    if (!cua_so_co_mpu) {
      cua_so_a_min_g = a_g;
      cua_so_a_max_g = a_g;
      cua_so_chuyen_dong_manh = false;
      cua_so_co_mpu = true;
    }
    if (a_g < cua_so_a_min_g) cua_so_a_min_g = a_g;
    if (a_g > cua_so_a_max_g) cua_so_a_max_g = a_g;
    if (a_g < NGUONG_ROI_TU_DO_G || a_g > NGUONG_VA_DAP_MANH_G) {
      cua_so_chuyen_dong_manh = true;
    }

    cua_so_ax_g = dong_ax / GIA_TRI_TRONG_LUC;
    cua_so_ay_g = dong_ay / GIA_TRI_TRONG_LUC;
    cua_so_az_g = dong_az / GIA_TRI_TRONG_LUC;
    cua_so_a_tong_g = a_g;

    if (dong_g_tong > cua_so_g_max) {
      cua_so_g_max = dong_g_tong;
      cua_so_gx_max = dong_gx;
      cua_so_gy_max = dong_gy;
      cua_so_gz_max = dong_gz;
    }
  }

  // Do tan so lay mau thuc te moi giay
  dem_so_slot_thuc_te++;
  uint32_t thoi_gian_cua_so_us = micros() - moc_bat_dau_do_nhip_us;
  if (thoi_gian_cua_so_us >= CUA_SO_DO_NHIP_THUC_US) {
    tan_so_thuc_te_mpu_hz = (float)dem_so_slot_thuc_te * 1000000.0f / (float)thoi_gian_cua_so_us;
    so_slot_da_chay = dem_so_slot_thuc_te;
    so_slot_ky_vong = thoi_gian_cua_so_us / CHU_KY_LAY_MAU_US;
    da_do_xong_tan_so = true;
    dem_so_slot_thuc_te = 0;
    moc_bat_dau_do_nhip_us = micros();
  }

  // Du so mau (50 mau @ 100 Hz = 500 ms) thi in man hinh 1 lan
  if (++dem_chu_ky_hien_thi >= SO_MAU_MOI_LAN_HIEN_THI) {
    dem_chu_ky_hien_thi = 0;
    in_man_hinh_kiem_tra();
    xoa_bo_dem_hien_thi();
  }
#else
  // --- 8.6. CHE DO GHI CSV: XUAT 13 COT DU LIEU SACH MOI SLOT ---
  gui_dong_csv(timestamp_ms, da_doc_duoc_mpu, da_doc_duoc_ap_suat,
               da_tinh_duoc_chenh_ap, da_doc_duoc_nhiet_do, da_tinh_duoc_do_cao);
#endif
}
