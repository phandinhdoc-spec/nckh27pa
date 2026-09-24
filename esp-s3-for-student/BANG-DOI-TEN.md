# BẢNG ĐỐI TÊN GLOSSARY — VIẾT LẠI FIRMWARE ESP-S3

Tài liệu đối chiếu tên định danh giữa bản chính thức (esp-s3/esp-s3.ino) và phiên bản viết lại theo văn phong tiếng Việt không dấu. Mọi định danh từ danh sách hợp đồng PHẢI đúng tên mới trongGLOSSARY trước khi cập nhật mã nguồn.

## Quy tắc đặt tên
- Chỉ chữ thường, số, gạch dưới
- Bắt đầu bằng chữ cái
- Không dấu tiếng Việt
- Gợi nhớ hành vi/ngữ nghĩa
- Ưu tiên dùng từ vựng đã có trong esp-s3-hocsinh.ino nếu cùng khái niệm

---

## Bang doi ten chinh

| Ten cu | Ten moi (tieng Viet KHONG DAU) | Loai | Y nghia/hanh vi | Ghi chu |
|--------|-------------------------------|------|-----------------|---------|
| s_baro | camBienKhiAp | global | Bien toan cuc cho cam bien khi ap MS5611 |  |
| s_fallState | trangThaiPhatHienNga | global | Bien toan cuc luu trang thai xac minh nga |  |
| s_mpu | camBienMpu | global | Bien toan cuc cho cam bien gia toc MPU6050 |  |
| buffer | boDem | member | Vung dem luu du lieu doc tu cam bien |  |
| address | diaChi | member | Dia chi I2C cua thiet bi |  |
| s_state | trangThaiThietBi | global | Trang thai chinh cua thiet bi |  |
| cmdId | cmdId | member | Ma lenh nhan duoc qua BLE (giu nguyen) |  |
| online | dangHoatDong | member | Co ket noi duoc voi cam bien khong |  |
| az | az | member | Gia toc truc Z |  |
| ax | ax | member | Gia toc truc X |  |
| altitudeDeltaM | chenhLechDoCaoM | member | Chenh lech do cao so voi moc tham chieu |  |
| PROFILE_DEFAULT | NGUONG_PHAT_HIEN_TE_NGA | global | Cau hinh mac dinh cho phat hien nga |  |
| gz | gz | member | Van toc goc truc Z |  |
| sendBleAck | guiBleAck | function | Gui ban tin xac nhan lenh qua BLE |  |
| pressurePa | apSuatPa | member | Ap suat khong khi (Pascal) |  |
| s_counters | boDemHeThong | global | Bien dem su kien he thong |  |
| samples | soMauDaDoc | member | So mau cam bien da doc |  |
| s_lastErrorCode | maLoiCuoiCung | global | Ma loi cuoi cung xay ra |  |
| cmdName | tenLenh | member | Ten lenh BLE nhan duoc |  |
| ENABLE_BLE | CO_BLE | define | Bat/tat tinh nang BLE (macro cua du an, doi ten theo yeu cau "doi het ma tu viet") |  |
| raw | duLieuTho | member | Du lieu tho chua xu ly tu cam bien |  |
| hasReadError | coLoiDoc | member | Co loi khi doc cam bien |  |
| state | giaTriTrangThai | member | Gia tri numer cua trang thai |  |
| s_ringHead | viTriGhiBoDemVong | global | Chi so ghi hien tai trong bo dem vong |  |
| s_batteryPercent | phanTramPin | global | Phan tram pin con lai |  |
| preImpactGravity | trongLucTruocVaCham | member | Huong trong luc truoc khi va cham |  |
| s_bleDeviceName | tenThietBiBle | global | Ten thiet bi BLE hien thi |  |
| c | c | member | Mang he so PROM cua MS5611 |  |
| errBuf | boDemLoi | member | Vung dem thong bao loi |  |
| timestampMs | thoiGianMs | member | Thoi diem lay mau (milli giay) |  |
| s_ringBuffer | boDemVong | global | Bo dem vong SensorSample |  |
| n_rem | soMauConLai | member | So mau con lai can ghi |  |
| sendBleDeviceStatus | guiBleTrangThaiThietBi | function | Gui trang thai thiet bi qua BLE |  |
| blePacketsDropped | goiBleBiBo | member | So goi BLE bi huy do qua tai |  |
| sendBleEvent | guiBleSuKien | function | Gui su kien canh bao qua BLE |  |
| PIN_BUZZER | CHAN_CANH_BAO | global | Chan GPIO dieu khien canh bao (coi) |  |
| s_referencePressurePa | apSuatThamChieuPa | global | Ap suat tham chieu de tinh do cao |  |
| inVerificationWindow | dangTrongCuaSoXacMinh | member | Co dang trong cua so xac minh nga |  |
| triggerReasons | nguyenNhaKichHoat | member | Danh sach nguyen nha phat hien nga |  |
| s_bleConnected | ketNoiBle | global | Trang thai ket noi BLE |  |
| s_blePeerMtu | mtuBle | global | Kich thuoc goi BLE toi da (MTU) |  |
| confidence | doTinCay | member | Muc do tin cay ket qua phat hien |  |
| saturated | biBaoHoa | member | Gia tri cam bien qua gioi han do |  |
| requestedRate | tanSoYeuCau | member | Tan so lay mau yeu cau (Hz) |  |
| RING_BUFFER_SIZE | SO_LUONG_BO_DEM_VONG | define | Kich thuoc bo dem vong |  |
| i2cWriteByte | ghiI2cMotByte | function | Ghi 1 byte vao thanh ghi I2C |  |
| temperatureC | nhietDoC | member | Nhiet do thiet bi |  |
| promBackup | SaoLuuProm | member | Sao luu du lieu PROM cua MS5611 |  |
| stillnessSampleCount | soMauDungYen | member | So mau dung yen lien tiep |  |
| s_bleStreaming | truyenBle | global | Trang thai truyen du lieu BLE |  |
| s_pendingCmdPayload | lenhBleChoXuLy | global | Du lieu lenh BLE dang cho xu ly |  |
| quality | chatLuong | member | Chat luong tin hieu tin hieu |  |
| s_rawCsvMode | cheDoCsvTho | global | Che do xuat CSV tho |  |
| s_stablePressureStartMs | thoiDiemApSuatOnDinhMs | global | Thoi diem ap suat bat dau on dinh |  |
| ep | thoiGianUnix | member | Thoi gian Unix (epoch) milliseconds |  |
| DeviceState | TrangThaiThietBi | enum | Cac trang thai cua thiet bi |  |
| PIN_LED_STATUS | CHAN_LED_TRANG_THAI | global | Chan GPIO LED trang thai |  |
| HW_HAS_FUEL_GAUGE | CO_DONG_HO_NHIEN_LIEU | define | Co cam bien do pin (macro cua du an) |  |
| impactAccelerationMs2 | giaTocVaChamMs2 | member | Nguong gia toc va cham | Nguon: 25.0 [Android FallDetectionConfig.DEFAULT] |
| freeFallThresholdMs2 | nguongTuDoMs2 | member | Nguong phat hien roi tu do | Nguon: 3.0 [Suy luan he thong - logging] |
| deviceStateToString | tenTrangThaiThietBi | function | Chuyen trang thai thanh chuoi hien thi |  |
| s_ringCount | soMauTrongBoDem | global | So mau hien co trong bo dem vong |  |
| s_lowBatWarned | daCanhBaoPinYeu | global | Da canh bao pin yeu |  |
| s_criticalBatWarned | daCanhBaoPinKiet | global | Da canh bao pin kiet |  |
| convStartUs | thoiDiemBatDauChuyenDoiUs | member | Thoi diem bat dau chuyen doi ADC (micro giay) |  |
| preImpactMinAcc | giaTocNhoNhatTruocVaCham | member | Gia toc nho nhat truoc thoi diem va cham |  |
| stillnessStartMs | thoiDiemBatDauDungYenMs | member | Thoi diem bat dau dung yen |  |
| s_pCharStream | dacTruTruyenDuLieu | global | Dac tru BLE truyen du lieu cam bien |  |
| s_pCharEvent | dacTruSuKien | global | Dac tru BLE gui su kien |  |
| s_pCharStatus | dacTruTrangThai | global | Dac tru BLE doc/trang thai |  |
| s_pCharAck | dacTruXacNhan | global | Dac tru BLE xac nhan lenh |  |
| msgBuf | boDemTinNhan | member | Vung dem tin nhan |  |
| peakBuf | boDemGiaTriCucDai | member | Vung dem gia tri cuc dai |  |
| orientBuf | boDemHuong | member | Vung dem du lieu huong |  |
| altBuf | boDemDoCao | member | Vung dem du lieu do cao |  |
| inactBuf | boDemKhongHoatDong | member | Vung dem che do khong hoat dong |  |
| baroStatusStr | chuoiTrangThaiKhiAp | member | Chuoi hien thi trang thai cam bien khi ap |  |
| PIN_BUTTON_SOS | CHAN_NUT_SOS | global | Chan GPIO nut nhan khan cap SOS |  |
| WIFI_SSID | WIFI_SSID | define | Ten mang WiFi (giu nguyen ROM) |  |
| getCurrentTimestampMs | layThoiGianHienTaiMs | function | Lay thoi gian hien tai (ms) |  |
| stillnessTargetAccelerationMs2 | nguongDungYenMs2 | member | Muc gia toc muc tieu khi dung yen | Nguon: 9.81 [Android FallDetectionConfig.DEFAULT] |
| stillnessToleranceMs2 | nguongSaiSoDungYenMs2 | member | Sai so cho phep cua nguong dung yen | Nguon: 1.0 [Android FallDetectionConfig.DEFAULT] |
| postImpactWindowMs | cuaSoSauVaChamMs | member | Thoi gian theo doi sau va cham (ms) | Nguon: 3000 [Android FallDetectionConfig.DEFAULT] |
| postImpactStillnessDurationMs | thoiGianDungYenSauVaChamMs | member | Thoi gian dung yen toi thieu sau va cham | Nguon: 1000 [Android FallDetectionConfig.DEFAULT] |
| highAngularSpeedDps | vanTocGocCaoDps | member | Van toc goc cao nhat ghi nhan | Nguon: 200.0 [Suy luan he thong - logging] |
| s_imuSampleRateHz | tanSoLayMauImuHz | global | Tan so lay mau cam bien IMU |  |
| s_isCharging | dangSacPin | global | Trang thai sac pin |  |
| who | maChip | member | Ma dinh danh thanh ghi WHO_AM_I |  |
| crcValid | crcHopLe | member | CRC hop le (true/false) |  |
| s_currentEventId | maSuKienHienTai | global | Ma su kien canh bao hien tai |  |
| imuStatusStr | chuoiTrangThaiImu | member | Chuoi trang thai IMU |  |
| s_sosPressStartMs | thoiDiemBamSosMs | global | Thoi diem bat dau nhan nut SOS |  |
| s_cancelPressStartMs | thoiDiemBamHuyMs | global | Thoi diem bat dau nhan nut huy |  |
| angleChangeDeg | thayDoiGocDo | member | Thay doi goc do phat hien |  |
| s_lastImuSampleUs | lanDocImuCuoiUs | global | Thoi diem doc IMU cuoi (micro giay) |  |
| phonePressureMinimumRisePa | nguongTangApSuatPinPa | member | Nguong tang ap suat cho phat hien nga | Nguon: 12.0 [Android - pressure flag disabled] |
| s_sampleDropCount | soMauBiBo | global | So mau bi huy do qua tai xu ly |  |
| s_imuSamplePeriodUs | chuKyLayMauImuUs | global | Chu ky lay mau IMU (micro giay) |  |
| gyroBiasX | saiSoVanTocGocX | member | Sai lech con quay truc X |  |
| gyroBiasY | saiSoVanTocGocY | member | Sai lech con quay truc Y |  |
| gyroBiasZ | saiSoVanTocGocZ | member | Sai lech con quay truc Z |  |
| baselineGravityX | trongLucGocX | member | Huong trong luc goc truc X (dung yen) |  |
| baselineGravityY | trongLucGocY | member | Huong trong luc goc truc Y (dung yen) |  |
| baselineGravityZ | trongLucGocZ | member | Huong trong luc goc truc Z (dung yen) |  |
| i2cReadBytes | docI2cNhieuByte | function | Doc nhieu byte tu thanh ghi I2C |  |
| readMPU6050 | docMpu6050 | function | Doc du lieu tu cam bien MPU6050 |  |
| accelScale | thangDoGiaToc | member | He so chia thang do gia toc |  |
| gyroScale | thangDoVanTocGoc | member | He so chia thang do van toc goc |  |
| d1Raw | duLieuThoApSuat | member | Du lieu tho ap suat tram MS5611 |  |
| d2Raw | duLieuThoNhietDo | member | Du lieu tho nhiet do tram MS5611 |  |
| lastSampleMs | mauCuoiCungMs | member | Thoi diem mau cuoi cung (milli giay) |  |
| s_alertStartMs | thoiDiemBatDauCanhBaoMs | global | Thoi diem bat dau canh bao |  |
| s_buzzerPatternDeadlineMs | hanChoTruongCoiMs | global | Thoi gian ket thuc choi coi |  |
| s_pServer | mayChuBle | global | Tham chieu may chu BLE |  |
| s_lastCommandId | lenhCuoiCung | global | Ma lenh BLE cuoi cung nhan duoc |  |
| s_pendingCommand | choXuLyLen | global | Co lenh BLE dang cho xu ly |  |
| performSelfTestAndCalibration | tuKiemTraVaHieuChuan | function | Thuc hien tu kiem tra va hieu chuan |  |
| s_lastStableCheckPressurePa | apSuatKiemTraOnDinhCuoiPa | global | Ap suat cuoi cung kiem tra on dinh |  |
| s_sosTriggered | daKichHoatSos | global | Da nhan nut SOS |  |
| stillDuration | thoiGianDungYen | member | Thoi gian dung yen lien tiep |  |
| msg | thongDiep | member | Thong diep su kien |  |
| mac | diaChiMac | member | Dia chi MAC thiet bi |  |
| eventLabel | nhanSuKien | member | Nhan loai su kien phat hien |  |
| curAx | giaTocHienTaiX | member | Gia toc truc X hien tai |  |
| PIN_BUTTON_CANCEL | CHAN_NUT_HUY | global | Chan GPIO nut nhan huy canh bao |  |
| FIRMWARE_VERSION | FIRMWARE_VERSION | define | Phien ban firmware (giu nguyen ROM) |  |
| UUID_SERVICE | UUID_SERVICE | define | UUID dich vu BLE (giu nguyen) |  |
| s_globalSequenceNumber | soThuTuToanCuc | global | So thu tu tang dan toan cuc |  |
| s_timeSynced | daDongBoThoiGian | global | Trang thai dong bo thoi gian |  |
| s_deviceEpochTimeMs | thoiGianEpochThietBiMs | global | Thoi gian epoch cua thiet bi (ms) |  |
| s_epochSyncLocalMs | thoiDiemDongBoCuoiMs | global | Thoi diem dong bo thoi gian cuoi cung |  |
| postureChangeThresholdDeg | nguongThayDoiTuTheDo | member | Nguong thay doi tu the | Nguon: 45.0 [Suy luan he thong - logging] |
| SensorSample | MauCamBien | struct | Cau truc luu du lieu 1 lan do cam bien |  |
| scanI2C | quetI2c | function | Quet bus I2C de tim thiet bi |  |
| anyFound | timThayThietBi | member | Tim thay it nhat 1 thiet bi |  |
| pollMS5611 | docMs5611KhongChan | function | Doc MS5611 theo che do non-blocking |  |
| imuSamplesRead | soMauImuDaDoc | member | So mau IMU da doc |  |
| lateLoopSlots | soLanTreChuKy | member | So lan tre chu ky xu ly |  |
| fallEventsConfirmed | suKienNgaDaXacMinh | member | So su kien nga da xac minh |  |
| sosTriggers | lanBamSos | member | So lan nhan nut SOS |  |
| impactTimestampMs | thoiDiemVaChamMs | member | Thoi diem va cham |  |
| peakImpactAcc | giaTocVaChamCucDai | member | Gia toc cuc dai tai thoi diem va cham |  |
| s_pCharCommand | dacTruLenh | global | Dac tru BLE nhan lenh |  |
| s_currentEventSeq | thuTuSuKienHienTai | global | Thu tu su kien hien tai |  |
| seq | thuTu | member | So thu tu mau trong bo dem vong |  |
| s_lastHumanLogMs | thoiDiemInDeDocCuoiMs | global | Thoi diem in man hinh cuoi cung |  |
| printProfile | inBangNguong | function | In bang thong so cau hinh ra Serial |  |
| printCsvHeader | inTieuDeCsv | function | In tieu de file CSV ra Serial |  |
| sumGx | tongGx | member | Tong van toc goc truc X |  |
| sumAx | tongAx | member | Tong gia toc truc X trong cua so |  |
| sumP | tongApSuat | member | Tong ap suat trung binh |  |
| vibrationWarnings | canhBaoRungDong | member | So canh bao rung dong |  |
| durationMs | khoangThoiGianMs | member | Thoi gian tinh bang milli giay |  |
| mpuFound | timThayMpu | member | Tim thay cam bien MPU6050 |  |
| s_imuConsecutiveFailures | soLanLoiImuLienTiep | global | So lan loi doc IMU lien tiep |  |
| s_streamDecimator | boDemLocTruyen | global | Bo dem loc giam tan so truyen BLE |  |
| s_lastBatCheckMs | lanKiemTraPinCuoiMs | global | Thoi diem kiem tra pin cuoi |  |
| s_lastWifiCheckMs | lanKiemTraWifiCuoiMs | global | Thoi diem kiem tra WiFi cuoi |  |
| s_wifiWasConnected | wifiDaKetNoi | global | Trang thai ket noi WiFi truoc do |  |
| s_lastBleStatusMs | lanGuiTrangThaiBleCuoiMs | global | Thoi diem gui trang thai BLE cuoi |  |
| PIN_I2C0_SDA | CHAN_I2C0_SDA | global | Chan SDA bus I2C so 0 |  |
| PIN_I2C0_SCL | CHAN_I2C0_SCL | global | Chan SCL bus I2C so 0 |  |
| PIN_I2C1_SDA | CHAN_I2C1_SDA | global | Chan SDA bus I2C so 1 |  |
| PIN_I2C1_SCL | CHAN_I2C1_SCL | global | Chan SCL bus I2C so 1 |  |
| PIN_LED_BAT_1 | CHAN_LED_PIN_1 | global | Chan GPIO LED phan tram pin muc 1 |  |
| PIN_LED_BAT_2 | CHAN_LED_PIN_2 | global | Chan GPIO LED phan tram pin muc 2 |  |
| PIN_LED_BAT_3 | CHAN_LED_PIN_3 | global | Chan GPIO LED phan tram pin muc 3 |  |
| PROTOCOL_VERSION | PROTOCOL_VERSION | define | Phien ban giao thuc (giu nguyen) |  |
| UUID_CHAR_STREAM | UUID_CHAR_STREAM | define | UUID dac tru truyen du lieu (giu nguyen) |  |
| UUID_CHAR_EVENT | UUID_CHAR_EVENT | define | UUID dac tru su kien (giu nguyen) |  |
| UUID_CHAR_STATUS | UUID_CHAR_STATUS | define | UUID dac tru trang thai (giu nguyen) |  |
| UUID_CHAR_COMMAND | UUID_CHAR_COMMAND | define | UUID dac tru lenh (giu nguyen) |  |
| UUID_CHAR_ACK | UUID_CHAR_ACK | define | UUID dac tru xac nhan (giu nguyen) |  |
| WIFI_PASS | WIFI_PASS | define | Mat khau WiFi (giu nguyen ROM) |  |
| getMonotonicTimeMs | layThoiGianChayMs | function | Lay thoi gian chay he thong (ms) |  |
| FallDetectionProfile | CauHinhPhatHienNga | struct | Cau truc cau hinh nguong phat hien nga |  |
| usePressureFallFilter | suDungBoLocApSuat | member | Su dung bo loc ap suat de phat hien nga | Nguon: false [Android - pressure flag disabled] |
| magnitude | doLonGiaToc | member | Do lon vecto gia toc tong hop |  |
| Mpu6050Driver | TrangThaiCamBienMpu | struct | Cau truc trang thai cam bien MPU6050 |  |
| initMPU6050 | khoiTaoMpu6050 | function | Khoi tao cam bien MPU6050 |  |
| Ms5611State | TrangThaiMs5611 | enum | Trang thai cam bien ap suat MS5611 |  |
| Ms5611Driver | TrangThaiCamBienKhiAp | struct | Cau truc trang thai cam bien khi ap |  |
| checkMS5611CRC4 | kiemTraCrc4Ms5611 | function | Kiem tra checksum CRC4 cua MS5611 |  |
| crcRead | crcDocDuoc | member | Gia tri CRC doc duoc tu cam bien |  |
| initMS5611 | khoiTaoMs5611 | function | Khoi tao cam bien MS5611 |  |
| calculateMS5611 | tinhToanMs5611 | function | Tinh toan ap suat/nhiet do tu du lieu tho |  |
| SystemCounters | BoDemHeThong | struct | Cau truc dem su kien he thong |  |
| baroSamplesRead | soMauKhiApDaDoc | member | So mau cam bien khi ap da doc |  |
| s_lastState | trangThaiTruocDo | global | Trang thai thiet bi lan cuoi |  |
| FallVerificationState | TrangThaiXacMinhNga | struct | Cau truc trang thai xac minh nga |  |
| s_eventCounter | boDemSuKien | global | Bien dem so luong su kien |  |
| hasPeakAcc | coGiaTocCucDai | member | Co phat hien gia toc cuc dai |  |
| isRetransmit | laTruyenLai | member | Co phai goi truyen lai khong |  |
| sendBleSensorStream | guiBleDuLieuCamBien | function | Gui du lieu cam bien theo thoi gian thuc |  |
| SERIAL_RAW_MODE | CHE_DO_SERIAL_THO | define | Che do xuat Serial tho (giu nguyen) |  |
| printSystemStatus | inTrangThaiHeThong | function | In trang thai he thong ra Serial |  |
| updatePressureBaselineTracking | capNhatApSuatThamChieu | function | Cap nhat moc ap suat tham chieu |  |
| updateBatteryStatus | capNhatTrangThaiPin | function | Cap nhat trang thai pin |  |
| updateButtons | capNhatNutBam | function | Kiem tra trang thai nut nhan |  |
| updateBuzzer | capNhatCoi | function | Dieu khien coi canh bao |  |
| processFallDetection | xuLyPhatHienTeNga | function | Xu ly thuat toan phat hien nga |  |
| elapsedMs | thoiGianQuaMs | member | Thoi gian troi qua (ms) |  |
| dispatchBleCommand | xuLyLenhBle | function | Xu ly lenh BLE nhan duoc |  |
| twdt_config | cauHinhWatchdog | member | Cau hinh bo giam sat chuong trinh |  |
| PIN_BAT_ADC | CHAN_ADC_PIN | global | Chan GPIO doc dien ap pin |  |
| HW_HAS_GNSS | CO_GNSS | define | Co GPS/GNSS (macro cua du an) |  |
| HW_HAS_CELLULAR | CO_4G | define | Co module 4G (macro cua du an) |  |
| postImpactGravity | trongLucSauVaCham | member | Huong trong luc sau khi va cham |  |
| baselineAltitude | doCaoMoc | member | Do cao moc tham chieu |  |
| s_buzzerPatternId | cheDoCanhBao | global | Che do canh bao hien tai |  |

---

## CAM DOI (KHONG DUOC PHEP DOI TEN)

Cac dinh danh duoi day la thu vien, ham he thong, define cung cap hoac cau truc API quoc te. TUYET DOI KHONG doi ten chung.

| Ten cu (KHONG DOI) | Ly do ghi chu | Loai |
|------------------------|--------------|------|
| false | Gia tri Boolean nguyen thuy C/Arduino | member |
| true | Gia tri Boolean nguyen thuy C/Arduino | member |
| minimumStillnessSamples | Ten trong struct FallDetectionProfile (do Android/Java dat) | member |
| maximumSampleGapMs | Ten trong struct FallDetectionProfile (do Android/Java dat) | member |
| MPU6050_ADDR_A | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_SMPLRT_DIV | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_WHO_AM_I | Define thanh ghi I2C cua MPU6050 | define |
| MS5611_ADDR_A | Define thanh ghi I2C cua MS5611 | define |
| MS5611_CMD_ADC_RD | Define lenh doc ADC cua MS5611 | define |
| MPU6050_ADDR_B | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_CONFIG | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_GYRO_CONFIG | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_ACCEL_CONFIG | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_ACCEL_XOUT_H | Define thanh ghi I2C cua MPU6050 | define |
| MPU6050_PWR_MGMT_1 | Define thanh ghi I2C cua MPU6050 | define |
| MS5611_ADDR_B | Define thanh ghi I2C cua MS5611 | define |
| MS5611_CMD_RESET | Define lenh reset cua MS5611 | define |
| MS5611_CMD_PROM_RD | Define lenh doc PROM cua MS5611 | define |
| MS5611_CMD_CONV_D1 | Define lenh chuyen doi ap suat cua MS5611 | define |
| MS5611_CMD_CONV_D2 | Define lenh chuyen doi nhiet do cua MS5611 | define |
| ServerCallbacks | Lop callback BLE server | class |
| CommandCallbacks | Lop callback BLE lenh | class |
| loop | Ham loop Arduino (mac dinh) | function |
| onConnect | Ham callback ket noi BLE | function |
| onDisconnect | Ham callback ngat ket noi BLE | function |
| onWrite | Ham callback ghi dac tru BLE | function |
| setup | Ham setup Arduino (mac dinh) | function |

---

SO_DONG_BANG=210
TONG_DINH_DANH=210
