# KẾ HOẠCH KIỂM THỬ THUẬT TOÁN PHÁT HIỆN TÉ NGÃ (esp32-test)

Tài liệu này quy định chi tiết quy trình thực nghiệm, các kịch bản kiểm thử vận động (T0–T12) và tiêu chuẩn nghiệm thu cho firmware bộ test `esp32-test` (**TEST_RIG**: ESP32-WROOM-32 + MPU9250 + GY-63/MS5611).

> **CẢNH BÁO AN TOÀN TUYỆT ĐỐI:**
> - Toàn bộ các thử nghiệm ngã mô phỏng (T6–T11) **BẮT BUỘC** phải thực hiện trên đệm mút thể dục dày (tối thiểu 15–20 cm) hoặc nệm lò xo có người hỗ trợ giám sát bảo hộ.
> - **TUYỆT ĐỐI KHÔNG** để người cao tuổi, người có bệnh lý tim mạch, xương khớp hoặc học sinh thực hiện các động tác ngã thật trên sàn cứng.
> - Khuyến khích sử dụng bao cát / hình nộm có trọng lượng (dummy 10–20 kg) mô phỏng khi thử nghiệm rơi và va đập ở độ cao lớn.

---

## 1. Mục tiêu và Nguyên tắc kiểm thử

1. **Xác nhận tính chính xác (Sensitivity & Specificity):**
   - Không được báo động nhầm (False Positive - FP = 0) trong các hoạt động sinh hoạt hàng ngày thông thường (ADL: T0–T5, T12).
   - Bắt trọn vẹn và xác nhận đúng (True Positive - TP) trong các tình huống ngã nguy hiểm có va đập và nằm bất động (T7–T11).
2. **Kiểm tra tính nhất quán (Parity) với Android:**
   - Xác nhận ngã phải tuân thủ đúng chuỗi: Va đập $a \ge 25.0 m/s^2$ $\rightarrow$ Bất động liên tục $\ge 1000 ms$ với $|a - 9.81| \le 1.0 m/s^2$.
   - Rơi tự do (Free-fall) và áp suất khí quyển ($\Delta P \ge 12 Pa$) là bằng chứng bổ trợ (corroboration), không được phủ quyết quyết định nếu thiếu.
3. **Độ ổn định hệ thống:**
   - Tần số lấy mẫu IMU duy trì chuẩn xác 100 Hz (chu kỳ 10 ms ± 1 ms).
   - Chu kỳ khí áp MS5611 không chặn 25 Hz. Không phát sinh tràn bộ đệm (stack overflow) hay kẹt bus I2C.

---

## 2. Danh mục 13 Ca kiểm thử (T0 – T12)

---

### T0: Đứng yên / Nằm yên (Baseline Stillness & Sensor Noise Test)
- **Mục tiêu:** Xác định độ ổn định của cảm biến, độ nhiễu nền gia tốc, độ lệch tĩnh (gyro drift) và tính ổn định của đường chuẩn áp suất $P_0$.
- **Cách thực hiện an toàn:** Đặt thiết bị cố định trên mặt bàn phẳng hoặc người thử nghiệm đeo thắt lưng đứng yên / nằm yên trong 60 giây.
- **Expected State:** Duy trì liên tục `NORMAL`.
- **Dữ liệu cần quan sát:**
  - Gia tốc tổng: $a_{mag} \approx 9.75 \dots 9.85 m/s^2$ (dung sai $< 0.1 m/s^2$).
  - Tốc độ góc tổng: $g_{mag} < 5.0^\circ/s$.
  - Biến thiên áp suất: $|\Delta P| < 3 Pa$; $|\Delta H| < 0.25 m$.
  - Cờ tĩnh: `stationary = true`.
- **Tiêu chí PASS:** Không phát sinh bất kỳ sự kiện chuyển trạng thái nào ngoài `NORMAL`. Điểm rủi ro (risk score) duy trì $\le 10\%$.
- **Dự kiến FP/FN:** Không có. Nếu báo va đập là do cảm biến lỗi hoặc xung đột I2C.
- **Threshold cần xem xét:** `stillnessTargetAccelerationMs2` (9.81), `stillnessToleranceMs2` (1.0).

---

### T1: Đi bộ bình thường (Normal Walking ADL)
- **Mục tiêu:** Kiểm tra khả năng chống báo giả khi người đeo thực hiện chuyển động tuần hoàn nhịp nhàng.
- **Cách thực hiện an toàn:** Người đeo gắn thiết bị chắc chắn ở thắt lưng (vị trí hông bên phải), đi bộ tự nhiên với tốc độ bình thường (khoảng 3–4 km/h) trên hành lang phẳng trong 2 phút (khoảng 100–120 bước chân).
- **Expected State:** `NORMAL` trong toàn bộ thời gian.
- **Dữ liệu cần quan sát:**
  - Đỉnh gia tốc bước chân: $a_{peak} \approx 12.0 \dots 17.0 m/s^2$ (hiếm khi vượt quá $19 m/s^2$).
  - Tốc độ góc: $20 \dots 60^\circ/s$.
  - Không có pha rơi tự do ($a_{mag}$ không xuống dưới $6.0 m/s^2$).
- **Tiêu chí PASS:** Tuyệt đối không kích hoạt `IMPACT` ($a_{peak} < 25.0 m/s^2$). Duy trì trạng thái `NORMAL`.
- **Dự kiến FP/FN:** Nếu bước chân dậm quá mạnh trên nền đá có thể tiệm cận $20 m/s^2$, nhưng vẫn dưới ngưỡng $25 m/s^2$.
- **Threshold cần xem xét:** `impactAccelerationMs2` (25.0).

---

### T2: Ngồi xuống nhanh trên ghế cứng (Fast Sitting on Hard Chair ADL)
- **Mục tiêu:** Kiểm tra chống báo giả khi có lực va chạm phần mông/thắt lưng với bề mặt ghế cứng không đệm.
- **Cách thực hiện an toàn:** Người thử nghiệm đang đứng, ngồi nhanh và dứt khoát xuống một chiếc ghế gỗ hoặc ghế nhựa cứng (không dùng lực nhảy đập mông quá mức gây chấn thương cột sống). Lặp lại 5 lần.
- **Expected State:** Có thể xuất hiện đỉnh xung lực nhưng không đủ điều kiện xác nhận ngã: `NORMAL` (nếu xung $< 25 m/s^2$) hoặc `NORMAL` $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `NORMAL` (nếu người đó sau khi ngồi vẫn tiếp tục cử động, đổi tư thế hoặc không tĩnh).
- **Dữ liệu cần quan sát:**
  - Đỉnh gia tốc chạm ghế: $a_{peak} \approx 18.0 \dots 24.5 m/s^2$. Đôi khi có thể chạm $25.5 m/s^2$ trong vài mili giây.
  - Vận tốc góc: thấp ($< 40^\circ/s$).
  - Độ cao: giảm khoảng $0.4 \dots 0.5 m$.
- **Tiêu chí PASS:** Không chuyển sang `FALL_CONFIRMED` (người ngồi thường vẫn có vi chuyển động thở, nhúc nhích hoặc đứng lên, hoặc đỉnh va chạm $< 25 m/s^2$).
- **Dự kiến FP/FN:** Nguy cơ FP nếu người ngồi xuống ghế rồi lập tức nín thở, bất động tuyệt đối trong $> 1000 ms$ và cú ngồi có đỉnh $\ge 25 m/s^2$.
- **Threshold cần xem xét:** `impactAccelerationMs2` (nếu quan sát false-positive, ngưỡng cần chỉnh ĐẦU TIÊN là `impactAccelerationMs2` từ 25 → 26–28 $m/s^2$, KHÔNG phải `minimumStillnessSamples`), `stillnessToleranceMs2`.

---

### T3: Nằm xuống chủ động / Thả người lên giường (Deliberate Lying Down onto Bed ADL)
- **Mục tiêu:** Phân biệt hành vi ngã và hành vi nằm nghỉ có kiểm soát lực.
- **Cách thực hiện an toàn:** Người thử nghiệm đang đứng cạnh giường đệm, chủ động ngả lưng hoặc thả người nằm xuống đệm mềm. Nằm yên thư giãn. Lặp lại 3 lần.
- **Expected State:** `NORMAL`. Nếu có xung lực nhẹ khi chạm đệm: có thể vào `POST_IMPACT` nhưng không đủ đỉnh va đập.
- **Dữ liệu cần quan sát:**
  - Đỉnh gia tốc chạm đệm mềm: rất êm, thường chỉ $13.0 \dots 20.0 m/s^2$ (do đệm hấp thụ xung lực kéo dài thời gian giảm tốc).
  - Tốc độ góc ngả lưng: $40 \dots 90^\circ/s$.
  - Pha sau đó: Tĩnh hoàn toàn ($|a - 9.81| \le 0.5 m/s^2$).
- **Tiêu chí PASS:** Không kích hoạt `FALL_CONFIRMED` vì đỉnh va chạm không đạt ngưỡng $25.0 m/s^2$.
- **Dự kiến FP/FN:** FN không áp dụng vì đây là ADL. Không được có FP.
- **Threshold cần xem xét:** `impactAccelerationMs2` (25.0).

---

### T4: Cúi người nhặt đồ / Buộc dây giày (Bending Over to Pick Up Items ADL)
- **Mục tiêu:** Kiểm tra ảnh hưởng của việc đổi hướng vector trọng lực mạnh mà không có va chạm.
- **Cách thực hiện an toàn:** Đang đứng thẳng, nhanh chóng cúi gập lưng $90^\circ$ chạm tay xuống sàn để nhặt vật phẩm, giữ nguyên tư thế cúi 2 giây, rồi đứng thẳng trở lại. Lặp lại 5 lần.
- **Expected State:** Duy trì liên tục `NORMAL`.
- **Dữ liệu cần quan sát:**
  - Trọng lực dịch chuyển từ trục Z sang trục X/Y: $a_z$ giảm về $\approx 0$, $a_x$ hoặc $a_y$ tăng lên $\approx 9.81 m/s^2$.
  - Gia tốc tổng: Luôn dao động ổn định quanh $9.0 \dots 11.5 m/s^2$ (không có đỉnh đột biến).
  - Tốc độ góc xoay lưng: $60 \dots 110^\circ/s$.
- **Tiêu chí PASS:** Tuyệt đối không kích hoạt `IMPACT` hay `FALL_CONFIRMED`.
- **Dự kiến FP/FN:** FP = 0.
- **Threshold cần xem xét:** Bộ lọc tư thế IIR và `impactAccelerationMs2`.

---

### T5: Rung lắc mạnh / Nhảy tại chỗ (Strong Vibration / Jumping in Place ADL)
- **Mục tiêu:** Kiểm tra phản ứng của bộ phát hiện va chạm trước các xung lực tuần hoàn liên tục.
- **Cách thực hiện an toàn:** Người đeo thực hiện động tác nhảy dây tại chỗ hoặc chạy nâng cao đùi nhanh trong 10 giây trên sàn có giày thể thao giảm chấn.
- **Expected State:** Có thể kích hoạt `IMPACT` trong tích tắc, nhưng lập tức bị hủy do chuỗi tĩnh bị phá vỡ liên tục: `NORMAL` $\leftrightarrow$ `POST_IMPACT` (chỉ duy trì vài mili giây) $\rightarrow$ `NORMAL`.
- **Dữ liệu cần quan sát:**
  - Đỉnh tiếp đất khi nhảy: có thể đạt $22.0 \dots 35.0 m/s^2$ (vượt ngưỡng impact).
  - Ngay sau đỉnh tiếp đất: Cơ thể tiếp tục nhún nhảy, $a_{mag}$ dao động mạnh $5 \dots 20 m/s^2$, $g_{mag} > 50^\circ/s$.
  - Số mẫu tĩnh `stillnessSampleCount` luôn bị reset về 0 liên tục.
- **Tiêu chí PASS:** Không bao giờ vào `FALL_CONFIRMED` vì không có pha bất động tĩnh kéo dài $\ge 1000 ms$. Sau khi ngừng nhảy 3 giây, thiết bị trở về `NORMAL`.
- **Dự kiến FP/FN:** Nguy cơ FP nếu sau khi nhảy cú cuối cùng người thử nghiệm lập tức đứng cứng đờ như tượng. Tuy nhiên trong thực tế sinh lý, người nhảy xong luôn thở dốc và rung lắc cơ thể.
- **Threshold cần xem xét:** `postImpactStillnessDurationMs` (1000 ms), `stillnessToleranceMs2` (1.0). (Ghi chú: nếu quan sát false-positive, ngưỡng cần chỉnh ĐẦU TIÊN là `impactAccelerationMs2` từ 25 → 26–28 $m/s^2$, KHÔNG phải `minimumStillnessSamples`).

---

### T6: Rơi tự do thật / Mô phỏng rơi an toàn (Simulated Free-Fall Drop Test)
- **Mục tiêu:** Kiểm tra tính năng nhận diện rơi tự do `POSSIBLE_FREE_FALL` và bằng chứng giảm tải trọng lực.
- **Cách thực hiện an toàn:**
  - Phương án A (Thiết bị độc lập): Đặt thiết bị vào hộp bảo vệ có đệm xốp chống sốc, thả rơi tự do từ độ cao $1.0 m$ xuống đệm mút dày mềm.
  - Phương án B (Mô phỏng người): Người thử nghiệm đứng trên bục cao 30 cm bước hụt rơi xuống đệm thể dục.
- **Expected State:** `NORMAL` $\rightarrow$ `POSSIBLE_FREE_FALL` $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED` (nếu hộp nằm yên trên đệm sau rơi).
- **Dữ liệu cần quan sát:**
  - Pha rơi tự do: $a_{mag} < 3.0 m/s^2$ (hoặc $< 4.9 m/s^2$) duy trì liên tục $100 \dots 200 ms$.
  - Dòng log xuất hiện: `[EVENT] FREE FALL detected`.
  - Đỉnh va chạm khi tiếp đệm: $a_{peak} \ge 25.0 m/s^2$.
- **Tiêu chí PASS:** Bắt được sự kiện `FREE FALL` và chuyển tiếp vào `IMPACT`.
- **Dự kiến FP/FN:** Nếu thả rơi từ độ cao quá thấp ($< 20 cm$), thời gian rơi $< 60 ms$ có thể không kịp kích hoạt ngưỡng `freeFallMinDurationMs = 80 ms`.
- **Threshold cần xem xét:** `freeFallThresholdMs2` (4.90), `freeFallMinDurationMs` (80).

---

### T7: Ngã chúi người về phía trước (Forward Fall onto Mattress)
- **Mục tiêu:** Kiểm tra nhận diện kịch bản ngã điển hình do vấp chân, mất thăng bằng chúi về phía trước.
- **Cách thực hiện an toàn:** Người thử nghiệm đứng thẳng trên mép đệm mút thể dục dày, đổ người về phía trước, hai tay co trước ngực (không chống mạnh bàn tay để tránh trật khớp cổ tay), toàn thân tiếp đệm mút và giữ nguyên tư thế nằm sấp bất động trong 5 giây.
- **Expected State:** `NORMAL` $\rightarrow$ (`POSSIBLE_FREE_FALL` tùy độ cao) $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED`.
- **Dữ liệu cần quan sát:**
  - Góc quay thân: Gyro $g_{mag} > 120^\circ/s$.
  - Đỉnh va đập: $a_{peak} = 26.0 \dots 45.0 m/s^2$ ($> 2.55 g$).
  - Thời gian nằm yên sau va chạm: duy trì liên tục $\ge 1000 ms$ với $|a - 9.81| \le 1.0 m/s^2$.
  - Áp suất: tăng $\Delta P \approx +5 \dots +10 Pa$ (tương ứng hạ thấp $\approx 0.5 - 0.8 m$).
- **Tiêu chí PASS:** Xuất hiện dòng cảnh báo: `[ALERT] FALL CONFIRMED` trong vòng 1.5 đến 2.0 giây sau cú va chạm.
- **Dự kiến FP/FN:** Nguy cơ FN nếu đệm quá mềm làm tiêu tán lực khiến đỉnh va chạm không chạm mốc $25 m/s^2$ (khi đó cần xem xét giảm threshold va chạm cho đệm mềm).
- **Threshold cần xem xét:** `impactAccelerationMs2` (25.0), `postImpactStillnessDurationMs` (1000).

---

### T8: Ngã ngửa người về phía sau (Backward Fall onto Mattress)
- **Mục tiêu:** Kiểm tra nhận diện kịch bản trượt chân ngã ngửa ra sau (nguy cơ chấn thương vùng gáy/lưng cao).
- **Cách thực hiện an toàn:** Người thử nghiệm đứng quay lưng vào đệm thể dục dày, ngả người rơi tự do ra sau, cằm thu sát ngực để bảo vệ gáy, lưng tiếp đệm phẳng và nằm yên bất động 5 giây.
- **Expected State:** `NORMAL` $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED`.
- **Dữ liệu cần quan sát:**
  - Vận tốc góc ngả sau: Gyro $g_{mag} > 130^\circ/s$.
  - Đỉnh va chạm lưng: rất rõ nét, thường $a_{peak} = 30.0 \dots 50.0 m/s^2$.
  - Bất động sau va chạm: $a_{mag} \approx 9.81 m/s^2$ trên trục Z/Y.
- **Tiêu chí PASS:** Phát hiện chính xác và in ra `[ALERT] FALL CONFIRMED`.
- **Dự kiến FP/FN:** Độ tin cậy cao, hầu như không bị sót mẫu vì va chạm ngã ngửa thường có xung lực dội mạnh.
- **Threshold cần xem xét:** `impactAccelerationMs2`, `stillnessToleranceMs2`.

---

### T9: Ngã nghiêng sang bên (Lateral / Sideways Fall onto Mattress)
- **Mục tiêu:** Kiểm tra nhận diện ngã nghiêng (thường gặp ở người già gãy cổ xương đùi).
- **Cách thực hiện an toàn:** Người thử nghiệm đứng nghiêng vai phải/trái vào đệm, đổ nghiêng người sang bên, hông và sườn tiếp đệm an toàn, nằm yên bất động.
- **Expected State:** `NORMAL` $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED`.
- **Dữ liệu cần quan sát:**
  - Gia tốc dồn mạnh vào trục Y (hoặc X tùy chiều gắn cảm biến).
  - Đỉnh va chạm: $a_{peak} \ge 25.0 m/s^2$.
  - Tư thế sau ngã: Trọng lực dồn sang trục bên sườn.
- **Tiêu chí PASS:** Kích hoạt thành công `FALL_CONFIRMED`.
- **Dự kiến FP/FN:** FN nếu người đeo ngã trúng vật cản làm giảm tốc từ từ.
- **Threshold cần xem xét:** `impactAccelerationMs2`.

---

### T10: Đang ngồi rồi trượt ngã xuống sàn (Slumping / Falling from Sitting Position)
- **Mục tiêu:** Đánh giá phát hiện ngã tầm thấp (low-height fall) từ ghế sofa hoặc giường xuống sàn.
- **Cách thực hiện an toàn:** Người thử nghiệm ngồi trên ghế thấp (30–40 cm), thả trượt người hoặc nghiêng người rơi khỏi ghế xuống đệm mút lót dưới chân, nằm yên.
- **Expected State:** `NORMAL` $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED`.
- **Dữ liệu cần quan sát:**
  - Quãng rơi ngắn ($< 40 cm$): Không có pha rơi tự do rõ rệt ($a_{mag}$ có thể chỉ xuống $6.0 m/s^2$).
  - Đỉnh va đập: Có thể dao động sát ngưỡng ($24.0 \dots 28.0 m/s^2$).
  - Độ chênh áp suất: nhỏ ($\Delta P \approx 3 \dots 5 Pa$).
- **Tiêu chí PASS:** Nếu đỉnh đạt $\ge 25 m/s^2$ và nằm yên $\ge 1000 ms$ thì PASS vào `FALL_CONFIRMED`. Nếu đỉnh va chạm $< 25 m/s^2$ do rơi thấp, ghi nhận FN để phục vụ tinh chỉnh ngưỡng.
- **Dự kiến FP/FN:** Nguy cơ FN cao nhất trong các loại ngã vì độ cao thấp và xung lực va chạm nhỏ.
- **Threshold cần xem xét:** `impactAccelerationMs2` (cân nhắc điều chỉnh ngưỡng xuống $22–24 m/s^2$ cho nhóm đối tượng nguy cơ ngã thấp).

---

### T11: Loạng choạng bám vật rồi trượt ngã (Stumbling, Grasping then Falling)
- **Mục tiêu:** Kiểm tra phản ứng trước chuỗi vận động phức tạp (mất thăng bằng $\rightarrow$ bám víu $\rightarrow$ trượt tay ngã).
- **Cách thực hiện an toàn:** Người thử nghiệm đứng, loạng choạng bước hụt, tay bám vào thành bàn/ghế giả định trong 1 giây, sau đó tuột tay ngã người xuống đệm mút bên cạnh, nằm yên bất động.
- **Expected State:** `NORMAL` $\rightarrow$ (Rung lắc) $\rightarrow$ `IMPACT` $\rightarrow$ `POST_IMPACT` $\rightarrow$ `FALL_CONFIRMED`.
- **Dữ liệu cần quan sát:**
  - Giai đoạn bám víu: Gyro dao động mạnh ($> 150^\circ/s$), gia tốc dao động $8 \dots 18 m/s^2$.
  - Giai đoạn chạm đệm cuối cùng: Đỉnh va chạm $a_{peak} \ge 25.0 m/s^2$.
  - Giai đoạn nằm yên: Đạt chuẩn tĩnh.
- **Tiêu chí PASS:** Xác nhận ngã thành công sau khi giai đoạn nằm yên hoàn tất.
- **Dự kiến FP/FN:** Nếu giai đoạn bám vật kéo dài làm đứt quãng thời gian giữa lúc mất thăng bằng và va chạm, thuật toán vẫn phải bắt đúng cú va chạm cuối cùng.
- **Threshold cần xem xét:** `maximumSampleGapMs` (250), `postImpactWindowMs` (3000).

---

### T12: Thay đổi độ cao không phải ngã: Đi thang máy / Leo cầu thang (Elevation Change ADL)
- **Mục tiêu:** Kiểm tra độc lập cảm biến khí áp GY-63/MS5611 và đảm bảo sự biến thiên áp suất lớn không gây kích hoạt ngã nhầm.
- **Cách thực hiện an toàn:** Người đeo đi vào thang máy di chuyển lên/xuống 2–3 tầng nhà, hoặc đi bộ nhanh lên/xuống cầu thang bộ.
- **Expected State:** Duy trì liên tục `NORMAL`.
- **Dữ liệu cần quan sát:**
  - Khi thang máy đi xuống: Áp suất tăng mạnh $\Delta P = +25 \dots +60 Pa$, độ cao tương đối $\Delta H$ giảm $-2.0 \dots -6.0 m$.
  - Gia tốc trong thang máy: Khởi động/dừng chỉ dao động nhẹ $9.2 \dots 10.5 m/s^2$ (hoàn toàn không có đỉnh va chạm).
- **Tiêu chí PASS:** Dù áp suất tăng mạnh và độ cao giảm sâu, thiết bị **KHÔNG ĐƯỢC** kích hoạt ngã vì không có va đập cơ học ($a_{peak} \ll 25.0 m/s^2$).
- **Dự kiến FP/FN:** FP = 0.
- **Threshold cần xem xét:** Khẳng định nguyên tắc: Áp suất chỉ là bằng chứng củng cố, không bao giờ tự kích hoạt ngã khi thiếu IMU.

---

## 3. Bảng Tổng hợp Kết quả Kiểm thử (Điền tay khi thực nghiệm)

| Mã ca | Mô tả kịch bản | Loại | Số lần thử | Số lần PASS | Số lần Báo giả (FP) | Số lần Bỏ sót (FN) | Đỉnh va đập đo được ($m/s^2$) | Thời lượng tĩnh ($ms$) | Kết luận (Đạt / Chưa) |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **T0** | Đứng / Nằm yên tĩnh | ADL | 3 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T1** | Đi bộ bình thường | ADL | 3 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T2** | Ngồi xuống nhanh ghế cứng | ADL | 5 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T3** | Nằm xuống giường chủ động | ADL | 3 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T4** | Cúi gập người nhặt đồ | ADL | 5 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T5** | Rung lắc / Nhảy tại chỗ | ADL | 3 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T6** | Rơi tự do mô phỏng | Fall | 3 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T7** | Ngã chúi về phía trước | Fall | 5 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T8** | Ngã ngửa ra phía sau | Fall | 5 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T9** | Ngã nghiêng sang bên | Fall | 5 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T10** | Trượt ngã từ ghế thấp | Fall | 5 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T11** | Bám vật rồi trượt ngã | Fall | 5 | ___ | - | ___ | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |
| **T12** | Thang máy / Cầu thang | ADL | 3 | ___ | ___ | - | ___ | ___ | [ ] ĐẠT &nbsp; [ ] HỎNG |

**Chỉ số tổng hợp:**
- **Độ nhạy (Sensitivity):** $\frac{\text{Tổng TP}}{\text{Tổng ca Fall (T6--T11)}} = \text{______} \%$
- **Độ đặc hiệu (Specificity):** $\frac{\text{Tổng TN}}{\text{Tổng ca ADL (T0--T5, T12)}} = \text{______} \%$
- **Tỷ lệ báo động giả mỗi giờ:** $\text{______}$ lần/giờ.
