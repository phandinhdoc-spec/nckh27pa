# Luồng quyền Android cho SOS (FallSafe)

Tài liệu này chốt mô hình quyền, hành vi suy giảm (fail-safe) của SOS và cách kiểm thử.
Quyết định tương ứng: `docs/decisions.md` D07. Baseline kỹ thuật: `.ai/architecture.md`.

## 1. Ba quyền độc lập

| Khả năng | Quyền Android | Vì sao |
|---|---|---|
| Vị trí | `ACCESS_FINE_LOCATION` và/hoặc `ACCESS_COARSE_LOCATION` (chỉ khi đang dùng ứng dụng) | Gửi kèm tọa độ/link bản đồ trong cảnh báo |
| SMS | `SEND_SMS` | Gửi cảnh báo SOS tới người thân đã bật `receiveSos` |
| Điện thoại | `CALL_PHONE` | Mở cuộc gọi SIM khi người dùng chủ động chọn "Gọi người thân" |

- `READ_PHONE_STATE` vẫn khai báo trong manifest cho việc chọn SIM khi máy có nhiều SIM, nhưng **không**
  là điều kiện của khả năng SMS: thiếu nó chỉ hiện ghi chú "Chưa cho phép kiểm tra SIM; nếu máy có nhiều
  SIM, ứng dụng cần chọn SIM để gửi".
- **Không** xin `ACCESS_BACKGROUND_LOCATION`. Ứng dụng không lấy vị trí khi chạy nền ngoài cơ chế
  foreground service `health|location` đã có; FGS này chỉ được bật sau khi đã có quyền vị trí và chỉ được
  khởi động khi Activity đang hiển thị. Nếu sau này cần lấy vị trí nền liên tục thì phải đánh giá và xin
  quyết định trước (xem `docs/next-gate.md`).
- **Không** có "quyền Google Maps". Mở bản đồ chỉ dùng Intent (`docs/decisions.md` D07).

## 2. Trạng thái hiển thị và luồng xin quyền

Mỗi khả năng có ba trạng thái UI (`CapabilityDisplayState`), không lộ tên hằng quyền cho người dùng:

| Trạng thái | Nhãn UI | Nút |
|---|---|---|
| `GRANTED` | `Đã cấp` (vị trí gần đúng: `Đã cấp (vị trí gần đúng)`) | không có nút |
| `CAN_REQUEST` | `Chưa cấp` | `CẤP QUYỀN` → mở hộp thoại hệ thống |
| `NEEDS_SETTINGS` | `Bị từ chối — cần mở Cài đặt ứng dụng` | `MỞ CÀI ĐẶT ỨNG DỤNG` → mở App Settings |

Quy tắc bắt buộc:

0. **Không có thao tác nào được im lặng.** Mỗi lần người dùng bấm nút xin quyền, kết quả trả về
   (`PermissionRequestResult`) phải dẫn tới đúng một trong hai việc: mở hộp thoại hệ thống Android thật,
   hoặc hiện thông báo rõ ràng kèm nút mở Cài đặt. Trạng thái không thể hỏi lại hiển thị đúng câu
   "Quyền này đang bị tắt. Hãy bật trong Cài đặt." và nút `MỞ CÀI ĐẶT ỨNG DỤNG`
   (`Settings.ACTION_APPLICATION_DETAILS_SETTINGS`).
1. Không xin quyền nào lúc khởi động ứng dụng. Chỉ xin sau một thao tác rõ ràng của người dùng, và mỗi
   lần chỉ xin **một** khả năng, theo thứ tự Vị trí → SMS → Điện thoại.
2. Lần đầu dùng ứng dụng ở trạng thái không khẩn cấp (SAFE hoặc MẤT KẾT NỐI thiết bị), ứng dụng hiện một
   hộp thoại giải thích ngắn: "Để gửi cảnh báo khi phát hiện té ngã, ứng dụng cần quyền định vị, gửi tin
   nhắn và gọi người thân." với hai lựa chọn `TIẾP TỤC CẤP QUYỀN` (xin đúng một quyền còn thiếu) và `ĐỂ SAU`.
   Hộp thoại tự biến mất khi chuyển sang trạng thái nguy hiểm (đếm ngược/SOS) và **không** chặn nút SOS.
3. Từ chối một lần: trạng thái vẫn `Chưa cấp`, được phép xin lại.
4. Từ chối vĩnh viễn (Don't ask again): ứng dụng **không** tự mở lại hộp thoại hệ thống; chỉ mở App Settings
   khi người dùng bấm nút riêng.
5. Trạng thái được đọc lại từ Android mỗi lần Activity `onResume`, nên khi người dùng quay về từ App
   Settings giao diện cập nhật ngay.
6. Chỉ cấp vị trí gần đúng (approximate) là trạng thái **dùng được**, không phải lỗi; UI ghi rõ
   "Vị trí gần đúng vẫn dùng được; người thân có thể thấy khu vực thay vì điểm chính xác."

## 3. Suy giảm từng bước của SOS (fail-safe)

Một sự kiện SOS luôn tạo báo cáo từng bước (`SosDispatchReport`) gồm bốn bước độc lập, trạng thái riêng:
`LOCATION` (Vị trí), `SMS` (Tin nhắn), `VOICE_CALL` (Cuộc gọi trợ giúp), `MAP_LINK` (Liên kết bản đồ).
Mỗi trạng thái là `SUCCESS`, `PARTIAL`, `PERMISSION_MISSING`, `UNAVAILABLE`, `FAILED` hoặc `SKIPPED`.

| Tình huống | Hành vi bắt buộc |
|---|---|
| Không có quyền nào | SOS vẫn chạy; báo cáo ghi từng bước thiếu quyền/không khả dụng; ứng dụng không crash |
| Có Vị trí + SMS, thiếu Điện thoại | Tin nhắn vẫn được gửi; bước gọi ghi "Chưa cho phép gọi điện; không thể tự động gọi" |
| Có Vị trí + Điện thoại, thiếu SMS | Bước gọi/hỗ trợ vẫn chạy được; bước SMS ghi thiếu quyền; nút "Gọi người thân" vẫn mở cuộc gọi SIM |
| Không có vị trí (chưa cấp, GPS tắt, quá hạn 8 giây, tọa độ không hợp lệ) | Tin nhắn ghi "Chưa xác định được vị trí." — **không** tạo tọa độ giả, không chặn các bước khác |
| Không có Google Maps | Mở bản đồ chuyển sang ứng dụng bản đồ khác, rồi tới trình duyệt bằng link `https://www.google.com/maps/...`; tin nhắn luôn chứa link này |
| Máy chủ/backend lỗi | SOS cục bộ (vị trí + SMS) không phụ thuộc máy chủ |
| SMS hoặc cuộc gọi lỗi | Bước đó ghi `FAILED` với lý do thật; các bước khác không bị ảnh hưởng |

Thứ tự mở bản đồ: (1) `geo:` nhắm đúng gói `com.google.android.apps.maps`, (2) `geo:` chung cho ứng dụng
bản đồ bất kỳ, (3) link `https` bằng trình duyệt. Không có ứng dụng nào xử lý thì hiện thông báo thật,
không crash. Không có tọa độ thì báo "Chưa có vị trí để mở bản đồ."

## 4. Ghi chú kiến trúc

- Gọi tự động trong SOS có hai nhánh tách biệt: adapter thoại qua máy chủ backend (D03) và **Cuộc gọi SIM**
  bằng `ACTION_CALL` tới người nhận ưu tiên (D09) — một lần duy nhất, chỉ sau khi SMS đã được chuyển cho thiết
  bị gửi. Adapter máy chủ là nhánh PHỤ TRỢ best-effort: máy chủ đã tiếp nhận (`STARTED`), máy chủ lỗi hay máy
  chủ vắng mặt đều KHÔNG chặn cuộc gọi SIM; SMS và vị trí cũng không quyết định cuộc gọi đó. Thiếu
  `CALL_PHONE` → báo thiếu quyền, không crash, SMS/backend vẫn chạy. Thao tác gọi tay trong thẻ
  "GỌI NGƯỜI THÂN" không đổi.
- Vị trí được lấy qua MỘT abstraction `LocationRepository.getBestAvailableLocation(timeoutMs)` trong cửa sổ có hạn
  (mặc định 8 giây) khi sự kiện bắt đầu: permission → cache/last-known còn mới (dùng ngay) → current theo
  FUSED → GPS → NETWORK → cache cũ → thất bại rõ ràng. `fused` (kết hợp GNSS + Wi-Fi + cell + sensor) là nguồn
  chính; `LocationManager` là dự phòng khi máy không có Google Play Services. **Không cần satellite fix**,
  **không ngưỡng accuracy** (100–200 m vẫn gửi), toàn bộ không bao giờ throw. GPS tắt/không fix **không** chặn
  cảnh báo (D03/D08), và câu "ra nơi thoáng" chỉ còn là GỢI Ý cải thiện độ chính xác, không phải lỗi chặn.
- Bước SOS gồm 5 nhánh độc lập: Vị trí, Tin nhắn, Cuộc gọi trợ giúp (adapter máy chủ), **Cuộc gọi SIM**
  (`ACTION_CALL` một lần tới người nhận ưu tiên — độc lập với máy chủ, SMS và vị trí, D09), Liên kết bản đồ.
  Thiếu vị trí chỉ đổi nội dung tin nhắn; SMS/CALL/backend luôn được thử.
- Không đưa tên hằng quyền vào chuỗi hiển thị; mọi câu chữ tiếng Việt, ngắn, chữ lớn, nút tối thiểu 56dp.

## 5. Kiểm thử tự động

Trạng thái kiểm chứng hiện tại (2026-09-18, sau task sửa luồng SOS): **170 test đơn vị PASS, 0 lỗi
(`assembleDebug` PASS)**; 11/11 check emulator PASS cho quyền (`docs/evidence/permission-flow/results.json`);
**2/2 check emulator PASS cho luồng SOS + vị trí** (`docs/evidence/sos-location/results.json`);
backend 55/55 PASS.

- Test đơn vị (không cần thiết bị), chạy bằng
  `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --rerun-tasks --no-daemon`:
  xác nhận đọc số thật từ `app/build/test-results/testDebugUnitTest/*.xml`, không tin exit code/báo cáo worker.
- Test đơn vị mới cho luồng này: `location/BestAvailableLocationRepositoryTest` (15),
  `emergency/SosCallStepTest` (9), `location/LocationIntentAndProviderTest` (4),
  `emergency/SosDispatchReportTest` (7), `emergency/EmergencyLogicTest` (10), `HomeScreenPureUiTest` (4).
  `emergency/BoundedLocationResolutionTest` đã bị xoá cùng class không còn dùng; coverage thay thế nằm trong
  `BestAvailableLocationRepositoryTest`. Không test nào gửi SMS thật hoặc gọi số thật (JVM thuần).
- Kiểm thử trên emulator (không dùng thiết bị thật), ba bộ độc lập:
  - `python3 android/scripts/permission-dialog-acceptance.py` — **bắt buộc sau mỗi lần cài sạch**
    (`adb uninstall vn.nckh27pa.fallsafe` → `adb install …`): xác nhận hộp thoại hệ thống Android thật sự
    xuất hiện cho Vị trí (từ hộp thoại giải thích lần đầu), Điện thoại và SMS; từ chối không crash;
    từ chối vĩnh viễn dẫn tới App Settings; quay lại ứng dụng thì trạng thái tự cập nhật.
    Bằng chứng: `docs/evidence/permission-dialog/` (`acceptance-results.json` + dump UI từng hộp thoại).
  - `python3 android/scripts/permission-flow-check.py` — Permission Center, suy giảm SOS, GPS tắt, bản đồ.
    Bằng chứng: `docs/evidence/permission-flow/`.
  - `python3 android/scripts/sos-location-acceptance.py` — **luồng SOS thật**: dựng liên hệ người thân trong
    dữ liệu người dùng, bơm `adb emu geo fix 106.6297 10.8231`, gây ngã THẬT bằng
    `adb emu sensor set acceleration` (không dùng replay), xác nhận: fix được lấy trong lúc đếm ngược, thẻ vị
    trí đạt "Đã xác định ± N m • vừa xong", SMS thật được gửi tới số của người thân (đọc lại từ
    `content://sms/sent`), `ACTION_CALL` thật được phát (màn hình gọi điện hiện số), và bước "Cuộc gọi SIM"
    trong báo cáo SOS. Bằng chứng: `docs/evidence/sos-location/` (`results.json`, `logcat-dispatch.txt`,
    `sent-sms-provider.txt`, `location-card.txt`, `countdown.txt`, `sos-state.txt`).
  Cả ba runner chỉ chạy trên emulator và từ chối thiết bị thật.
  Các check: `first-run`, `permission-center`, `request-deny`, `permanent-denial`, `grant-on-resume`,
  `approximate-only`, `sos-degradation`, `gps-off`, `maps-intents`, `maps-opens-google-maps`,
  `maps-fallback-browser`.

## 6. Kiểm thử thủ công trên điện thoại thật (bắt buộc trước khi coi là hoàn tất thực địa)

1. Cài bản debug, xoá dữ liệu ứng dụng, mở lần đầu: hộp thoại giải thích hiện ra; `ĐỂ SAU` đóng được;
   `TIẾP TỤC CẤP QUYỀN` chỉ mở một hộp thoại quyền.
2. Cài đặt → QUYỀN ỨNG DỤNG: ba dòng hiện đúng trạng thái; bấm `CẤP QUYỀN` với từng quyền; sau khi cấp
   trạng thái đổi thành "Đã cấp" ngay khi quay lại ứng dụng.
3. Từ chối hai lần (hoặc chọn "Không hỏi lại") cho Vị trí: dòng quyền chuyển thành "Bị từ chối — cần mở
   Cài đặt ứng dụng"; bấm nút mở App Settings; bật quyền trong Android; quay lại ứng dụng thấy "Đã cấp".
4. Chỉ cấp vị trí gần đúng (Approximate) và kiểm tra dòng hiển thị "Đã cấp (vị trí gần đúng)".
5. Tắt Vị trí/GPS trong cài đặt hệ thống rồi kích hoạt SOS: ứng dụng không crash, báo cáo ghi bước Vị trí
   không thành công và tin nhắn ghi "Hiện chưa xác định được vị trí chính xác."; SMS và cuộc gọi SIM vẫn chạy.
6. Thu hồi từng quyền (SMS, Điện thoại) rồi kích hoạt SOS: kiểm tra đúng hành vi suy giảm ở §3, bao gồm
   thông báo "không thể tự động gọi" khi thiếu `CALL_PHONE`.
7. Với máy có SIM thật: kiểm tra nhận SMS trên máy người nhận được phép, biên nhận `DELIVERED`, chọn SIM
   khi máy hai SIM, và cuộc gọi SIM thủ công.
8. Kiểm tra bản đồ: có Google Maps → mở đúng ứng dụng Maps; gỡ/tắt Google Maps → mở bằng trình duyệt với
   cùng tọa độ; không có cả hai → thông báo thật, không crash.
9. Giám sát nền: bật "Giám sát nền" từ màn hình đang hiển thị, khoá màn hình, kiểm tra hành vi FGS và
   giới hạn tiết kiệm pin của từng hãng máy.

## 7. Giới hạn đã biết

- Emulator không có SIM/GSM thật: SMS được ghi vào `content://sms/sent` của thiết bị (đã xác minh) và
  `ACTION_CALL` mở được màn hình gọi điện của emulator (đã xác minh), nhưng **biên nhận giao SMS của nhà mạng
  và cuộc gọi qua mạng di động thật vẫn chưa kiểm chứng**. Hộp thoại quyền theo hãng máy, đa SIM, GPS ngoài
  trời và hành vi FGS khi khoá màn hình vẫn cần thiết bị thật.
- Trên emulator, vị trí chỉ có khi có client đang yêu cầu và toạ độ được "bơm" bằng `adb emu geo fix`.
  Đã xác minh cả hai nhánh: (a) fix FUSED mới trong lúc đếm ngược (`source=FUSED accuracy=5.0 cause=none`), và
  (b) current timeout → dùng cache CŨ (`source=CACHED accuracy=5.0 age=117559 cause=TIMEOUT`) và **SOS vẫn
  được gửi** kèm câu "Vị trí gần nhất, cập nhật lúc …". Trên điện thoại thật cần xác nhận thời gian bắt fix
  ngoài trời.
- Giới hạn của bản DEMO (không phải lỗi luồng SOS): bộ đếm sự kiện cục bộ khởi động lại từ 1 sau mỗi lần mở
  app, nên id sự kiện từ xa được tái sử dụng; `EmergencyCoordinator.dispatch` khi đó thấy bản ghi đã
  `dispatched` và bỏ qua, UI hiển thị báo cáo cũ. Runner `sos-location-acceptance.py` xoá
  `fallsafe_emergency_records.xml` + `fallsafe_emergency_identity.xml` để bảo đảm sự kiện mới. Cần theo dõi
  khi làm phần bền vững hoá sự kiện.
