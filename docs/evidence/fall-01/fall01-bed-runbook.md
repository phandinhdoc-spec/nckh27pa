# FALL-01 — Runbook thí nghiệm trên điện thoại thật (FALL01-BED-01/02/03)

Mục đích: thu log cảm biến thật của pipeline FALL-01 hiện tại. **Không** kết luận ngưỡng trong bước này.
Chưa có file log nào trong thư mục này — các file `fall01-bed-0N.log` do chủ dự án tạo bằng lệnh dưới đây.

Thông tin cần biết trước:

- Tag log duy nhất: `FALL01`, mức `I`. Một lệnh `adb logcat` là lấy được toàn bộ dữ liệu FALL-01.
- `t` trong log = ms kể từ lúc cảm biến bắt đầu chạy (`FALL01_SESSION,event=START`), nên cứ thu liền mạch
  từ lúc bật Giám sát là dữ liệu tự căn gốc.
- Nếu detector xác nhận ngã, app vào đếm ngược 10 giây rồi **gửi SOS thật** (SMS/cuộc gọi) tới liên hệ đã
  cấu hình. Muốn tránh cuộc gọi/tin nhắn thật thì bấm **An toàn** trong lúc đếm ngược. Không sửa luồng SOS.
- Log phải để nguyên, không chỉnh sửa tay: đây là bằng chứng thô.

## Chuẩn bị (một lần)

```bash
cd /Users/phananh/TEMP/nckh27pa/nckh27pa
ADB=~/Library/Android/sdk/platform-tools/adb     # hoặc dùng `adb` nếu đã có trong PATH
OUT=docs/evidence/fall-01

$ADB devices                                     # phải thấy đúng 1 thiết bị ở trạng thái `device`
(cd android && ./gradlew --offline --no-daemon --max-workers=2 assembleDebug)
$ADB install -r android/app/build/outputs/apk/debug/app-debug.apk
```

## Một lượt ghi (làm lần lượt cho BED-01, BED-02, BED-03)

```bash
# (1) xoá buffer log cũ
$ADB logcat -c

# (2) BẮT ĐẦU thu log — để terminal này chạy suốt lượt thí nghiệm
$ADB logcat -v threadtime FALL01:V '*:S' | tee $OUT/fall01-bed-01.log
```

Sau khi lệnh (2) đã chạy, **trong lúc nó vẫn đang thu**:

```
(3) Trong app FallSafe: mở màn hình chính → bật Giám sát (nguồn "PHONE_ONLY • cảm biến thật")
    → bật "giám sát nền" để việc ghi không phụ thuộc màn hình sáng/tối.
    Dòng FALL01_SESSION,event=START và FALL01_PROFILE phải xuất hiện ngay sau đó.

(4) Thao tác FALL01-BED-01:
    a. đặt máy đứng yên trên giường khoảng 3 giây
    b. nhấc máy lên đúng độ cao thực tế như khi người dùng cầm máy (~0,7–1,0 m), giữ nguyên tư thế thân máy
    c. thả máy rơi tự nhiên xuống GIƯỜNG mềm
    d. KHÔNG can thiệp vào chuyển động sau khi máy chạm giường
    e. để nguyên thêm khoảng 5 giây

(5) Trong app: dừng Giám sát  → dòng FALL01_SESSION,event=STOP với `confirmed=` sẽ chốt phán quyết lượt này
(6) Ở terminal logcat: Ctrl-C để kết thúc ghi
```

Lặp lại (1)→(6) với `fall01-bed-02.log` và `fall01-bed-03.log`.

## Kiểm tra ngay sau mỗi lượt (không sửa file log)

```bash
grep -c 'FALL01_SAMPLE' $OUT/fall01-bed-01.log                      # số mẫu đã ghi
grep -E 'FALL01_(SESSION|PROFILE|DECISION|STATE|ALERT_STATE)' $OUT/fall01-bed-01.log
awk -F',' '/FALL01_SAMPLE/ {for(i=1;i<=NF;i++) if($i ~ /^amag=/){split($i,a,"="); if(a[2]+0>m) m=a[2]+0}} END {print "peak amag =", m, "m/s^2"}' $OUT/fall01-bed-01.log
```

## Cách đọc kết quả (chỉ mô tả, không tuning)

- `FALL01_SESSION,event=STOP,...,confirmed=false` và không có dòng `FALL01_DECISION,...,detected=true`
  ⇒ lượt đó không xác nhận ngã; xem `amag` cực đại trong `FALL01_SAMPLE` để biết đỉnh va chạm thực tế
  trên nệm so với ngưỡng `impact=` in trong `FALL01_PROFILE`/`FALL01_STATE`.
- Có `FALL01_STATE,...,IMPACT_THRESHOLD_REACHED` ⇒ đã vượt ngưỡng va chạm; các dòng
  `FALL01_DECISION,reason=STILLNESS_INTERRUPTED` / `POST_IMPACT_WINDOW_EXPIRED` cho biết vì sao chưa xác nhận.
- `dtMs` lớn (> 250) giải thích được các lần `reason=SAMPLE_GAP_RESET`.
- `gtns` so với `tns` cho biết độ lệch thời gian giữa mẫu accel và mẫu gyro gần nhất (không nội suy).

Phân tích định lượng đầy đủ (magnitude vs time, đỉnh impact, khoảng trước va chạm, độ tĩnh sau va chạm,
các chuyển pha sai/bỏ sót) chỉ thực hiện **sau khi** có đủ 3 file log thật, và chỉ khi đó mới bàn tới
ngưỡng FALL-01.
