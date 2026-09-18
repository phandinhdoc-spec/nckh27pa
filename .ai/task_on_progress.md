# Current Task

## Goal
Hoàn thiện toàn bộ hệ thống quyền Android phục vụ SOS (điện thoại, SMS, định vị, mở bản đồ), Permission
Center trong Cài đặt, luồng xin quyền lần đầu và suy giảm từng bước của SOS — tích hợp vào kiến trúc hiện
có, không viết lại ứng dụng.

## Status
IN_PROGRESS — xác minh hộp thoại quyền hệ thống trên thiết bị (T5). Phần permission flow/Permission
Center/fail-safe SOS đã xong và được kiểm chứng; T5 bổ sung bảo đảm "không thao tác nào im lặng".
Việc còn lại: cài APK mới trên thiết bị thật của chủ dự án và chạy lại checklist §6 `docs/permission-flow.md`.

## T5 — Hộp thoại quyền hệ thống (báo cáo của chủ dự án: "vẫn không hiện")
- Kiểm tra 3 lớp bằng `rg`: (1) manifest có đủ `ACCESS_COARSE_LOCATION`, `ACCESS_FINE_LOCATION`,
  `SEND_SMS`, `CALL_PHONE`; (2) UI có nút `CẤP QUYỀN` trong `PermissionCenterSection` và nút
  `TIẾP TỤC CẤP QUYỀN` trong hộp thoại lần đầu; (3) trigger gọi API hệ thống thật qua
  `registerForActivityResult(ActivityResultContracts.RequestPermission/RequestMultiplePermissions)` trong
  `MainActivity.launchCapabilityRequest`.
- Trên APK cài sạch (uninstall → install): **cả ba hộp thoại hệ thống thật đều hiện**
  (`REQUEST_PERMISSIONS` → `com.google.android.permissioncontroller/...GrantPermissionsActivity`):
  Vị trí ("access this device's location?"), Điện thoại ("make and manage phone calls?"),
  SMS ("send and view SMS messages?"). Bằng chứng: `docs/evidence/permission-dialog/`.
- Lỗi thật tìm thấy và đã sửa (T5): UI bỏ qua `PermissionRequestResult` nên khi trạng thái là
  `NEEDS_SETTINGS` (Android không cho hỏi lại) thì bấm nút **không có gì xảy ra**. Nay hiện đúng câu
  "Quyền này đang bị tắt. Hãy bật trong Cài đặt." kèm nút `MỞ CÀI ĐẶT ỨNG DỤNG`.
- Nguyên nhân phụ rất có thể gặp ở phía người dùng: bản APK cũ. Cả hai bản trước đều là
  `versionCode=1` / `versionName=0.1-demo` nên không phân biệt được. Đã tăng lên
  `versionCode=3` / `versionName=0.3-permission`.
- Bộ kiểm thử cài sạch: `python3 android/scripts/permission-dialog-acceptance.py`
  (bắt buộc `adb uninstall` trước khi `adb install`).

## Kết quả theo yêu cầu

### Permission flow
- Ba khả năng độc lập: Vị trí (fine/coarse foreground), SMS (`SEND_SMS`), Điện thoại (`CALL_PHONE`).
  `READ_PHONE_STATE` chỉ còn phục vụ đa SIM và không chặn SMS khi thiếu.
- `NEEDS_SETTINGS` khi đã từ chối vĩnh viễn (`wasAttempted && !shouldShowRequestPermissionRationale`):
  không lặp lại hộp thoại hệ thống, chỉ mở App Settings bằng một thao tác riêng.
- Trạng thái đọc lại từ Android trong `onResume`; cấp lại quyền trong App Settings được phản ánh ngay.
- Chỉ cấp approximate vẫn là trạng thái dùng được và được hiển thị riêng.

### First-run & Permission Center
- Hộp thoại giải thích hiện một lần ở trạng thái không khẩn cấp (SAFE **và** MẤT KẾT NỐI THIẾT BỊ, tức là
  dùng được cả ở chế độ PHONE_ONLY), tự ẩn khi chuyển sang đếm ngược/SOS, không chặn nút SOS.
- `Cài đặt → QUYỀN ỨNG DỤNG`: 📍 Vị trí / 📞 Điện thoại / 💬 SMS với nhãn "Đã cấp", "Đã cấp (vị trí gần
  đúng)", "Chưa cấp", "Bị từ chối — cần mở Cài đặt ứng dụng" và nút `CẤP QUYỀN` / `MỞ CÀI ĐẶT ỨNG DỤNG`;
  không hiển thị tên hằng quyền, chữ ≥16sp, nút ≥56dp.

### Fail-safe SOS
- `SosDispatchReport` bốn bước độc lập (Vị trí, Tin nhắn, Cuộc gọi trợ giúp, Liên kết bản đồ) được lưu theo
  sự kiện và hiển thị trong hộp thoại "Tiến trình gửi SOS".
- Thiếu bất kỳ quyền nào cũng không crash và không bỏ im lặng: mỗi bước có trạng thái + lý do thật
  (ví dụ "Chưa cho phép gọi điện; không thể tự động gọi").
- Không có vị trí (chưa cấp / GPS tắt / quá hạn 8 giây / tọa độ sai) → tin nhắn ghi "Chưa xác định được vị
  trí.", không có tọa độ giả, các bước khác vẫn chạy.
- Không có Google Maps → mở bằng trình duyệt với cùng link; có Maps → mở đúng Google Maps.

### Định vị nền (đã đánh giá, KHÔNG thêm gì)
- Ứng dụng không lấy vị trí nền ngoài FGS `health|location` đã có; FGS này chỉ bật khi đã có quyền vị trí
  và Activity đang hiển thị. Vì vậy **không** xin `ACCESS_BACKGROUND_LOCATION`. Nếu sau này cần vị trí nền
  liên tục phải xin quyết định mới.

## Kiểm thử đã chạy (bằng chứng thật)
- `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --rerun-tasks --no-daemon`:
  147 test, 0 lỗi (28 test mới cho quyền/SOS: `CapabilityHardeningTest`, `SosDispatchReportTest`,
  `LocationIntentAndProviderTest`, `PermissionCenterUiTest`).
- `./gradlew assembleDebug --no-daemon`: PASS → `android/app/build/outputs/apk/debug/app-debug.apk`.
- `cd backend && npm test`: 55/55 PASS (không đổi backend).
- Emulator NCKH27PA (API 36, x86_64): `python3 android/scripts/permission-flow-check.py` — 11/11 check PASS,
  bằng chứng ở `docs/evidence/permission-flow/` (results.json + dump UI + trạng thái quyền).
  Đã xác minh trên APK thật: luồng xin quyền, từ chối một lần, từ chối hai lần (Don't ask again) →
  mở App Settings → quay lại ứng dụng, cấp lại quyền trong Settings, vị trí gần đúng, tắt dịch vụ vị trí,
  SOS không quyền không crash có báo cáo từng bước, mở Google Maps, và fallback trình duyệt khi Maps bị tắt.
- Bằng chứng SOS thật trên emulator: `Trạng thái SOS: Vị trí: Thành công • Tin nhắn: Không khả dụng •
  Cuộc gọi trợ giúp: Chưa cấp quyền • Liên kết bản đồ: Bỏ qua` khi thiếu `CALL_PHONE`.
- `git diff --check`: sạch. Không commit.

## Problems/Blockers
- Không có điện thoại thật/SIM/GPS: hộp thoại quyền theo hãng máy, đa SIM + biên nhận SMS thật, cuộc gọi
  SIM thật, thời gian bắt fix ngoài trời, hành vi FGS khi khoá màn hình chưa được kiểm chứng.
- Adapter thoại backend vẫn chưa có tài khoản/số gọi đi/public callback → bước "Cuộc gọi trợ giúp" tự động
  vẫn chỉ báo "Không khả dụng"; muốn SOS tự gọi SIM thì cần quyết định mới (`docs/next-gate.md` §4).
- Trên emulator, sự kiện SOS đầu tiên khi chưa từng có fix sẽ hết hạn 8 giây và đi vào nhánh "không có vị
  trí" (đúng thiết kế D03); cần xác nhận lại trên thiết bị thật.

## Next Action
1. Kiểm thử thủ công theo `docs/permission-flow.md` §6 trên điện thoại thật có SIM (chờ chủ dự án cấp phép
   thiết bị/số người nhận thử).
2. Chủ dự án chốt `docs/next-gate.md` §4 (auto-dial SIM) và §5 (phạm vi kiểm thử thực địa).
3. Chỉ commit/push khi chủ dự án yêu cầu.

## Lịch sử phân công phiên này
- T2 — Codex Sol: logic quyền/location/SMS/call/maps + báo cáo từng bước + test (`.ai/T2-codex-sos-permissions.md`, DONE).
- T3 — AGY Gemini (`gemini-3.8-flash-medium`): Permission Center + hộp thoại lần đầu + phản hồi SOS (`.ai/T3-agy-permission-ui.md`, DONE, có 1 defect được trả lại và sửa).
- Hermes: chốt contract, review, chạy Gradle/emulator, tài liệu.
