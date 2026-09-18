# Current Task

## Goal
Sửa và hoàn thiện Android Vị trí/Gửi vị trí/Gọi người thân/Thiết bị, điều phối SOS tự động dùng backend hiện có, SMS SIM có biên nhận thật và adapter cuộc gọi thoại backend có DTMF/callback/idempotency; giữ nguyên UI trung tâm, contacts và profile ngưỡng hiện có.

## Status
IN_PROGRESS — permission and optional-AI completion started on the existing local SOS implementation. Existing uncommitted changes remain protected.

## Existing Working Tree
Các thay đổi Android profile/calibration từ task trước đang uncommitted và phải được giữ nguyên. Không revert hoặc ghi đè.

## Discovery Findings
- Handler sai nằm tại `HomeScreen.kt:506-517`: ô VỊ TRÍ gắn `showCallConfirmDialog`; dialog chỉ báo gọi demo. Chưa có Android location/SMS/call implementation hay quyền tương ứng.
- Countdown thật ở `AlertCore`/`DemoController`; timeout 10 giây đi qua cùng SOS state và `SyncCoordinator` ghi EVENT/SOS bền vững. Core ID nội bộ không bền nhưng `SyncOutbox` hiện giữ thao tác API; backend đã idempotent theo `eventId` và unique outbox event.
- Backend Node/SQLite có contacts, watchdog, SOS outbox, ACK/resolve; chỉ ghi `RECORDED`, chưa có SMS/call provider/worker/callback. Không được báo đã gọi/gửi ngoài.
- Android contacts có `receiveSos` và `isPrimary` nhưng chưa có thứ tự ưu tiên riêng; cần contract ưu tiên ổn định.
- ESP32 v1 chưa cung cấp tọa độ GNSS; BMP390 chỉ là áp suất. Device status backend có pin/kết nối thật khi heartbeat tồn tại; UI hiện đang dùng giá trị giả `deviceConnected=true`, `batteryStatus="Pin tốt"`.
- API Android không cho bơm audio vào uplink cuộc gọi SIM; TTS cục bộ không chứng minh người nhận nghe. Cuộc gọi có lời nói/DTMF phải qua adapter thoại backend. SMS SIM có `sentIntent`/`deliveryIntent`; multi-SIM phải chọn subscription hợp lệ.
- Twilio được chọn làm adapter mẫu có thể tắt: Voice API có trạng thái busy/no-answer/failed/completed, Gather DTMF và Play; Việt Nam có outbound nhưng không có số voice nội địa, giá công khai hiện tại khoảng USD 0.1777/phút mobile và 0.1947/phút local. Chưa có tài khoản/khóa/số gọi đi nên chỉ mock/sandbox.
- Android target 36: location nền cần permission/FGS type location và phải được thiết lập khi Activity đang hiển thị; background FGS start bị hạn chế. Không đợi tới SOS mới xin quyền.

## Contract Decisions To Freeze
- Mở rộng duy nhất `docs/api-contract.md`; không tạo REST contract cạnh tranh.
- Một `eventId` bền vững là khóa idempotency cho EVENT/SOS/SMS/call/callback; cancel trước timeout chặn mọi dispatch chưa bắt đầu.
- Location gồm lat/lon hợp lệ (không 0,0), accuracy/timestamp/source `PHONE|ESP32_GNSS`; freshness được ghi rõ. Không gọi reverse geocode trên critical path.
- Manual share dùng nội dung trung tính và người nhận được chọn; emergency message có tên người dùng, nghi ngã/cần hỗ trợ, event time, tọa độ/link/fix time/accuracy/source hoặc cảnh báo chưa có vị trí. Late location supplement tối đa một lần/event.
- Trạng thái SMS tách `QUEUED/SENDING/SENT/DELIVERED/FAILED`; SENT không phải đọc. Call tách provider states và `acknowledged`; completed/voicemail không phải ACK.
- Android SIM SMS/call là fallback; SIM call không tuyên bố phát lời. Backend voice adapter mặc định disabled, secrets chỉ env, callback xác thực, số lượt/liên hệ/timeout hữu hạn.
- DTMF 1 dừng mở rộng sang liên hệ mới nhưng không tự cúp cuộc gọi đang kết nối; lời nói tiếp tục lặp có giới hạn tới khi cuộc gọi kết thúc/provider timeout.

## Planned Ownership
- Codex Sol: `docs/api-contract.md`, backend provider/dispatch/callback/migrations/tests, Android non-UI location/SMS/emergency/device logic, manifest/MainActivity/controller wiring and tests. Không sửa `HomeScreen.kt`/Compose layout.
- AGY Gemini: `HomeScreen.kt` và UI file mới nếu cần, chỉ dùng interface Codex đã chốt; không sửa backend/domain/manifest/MainActivity.
- Hermes: contract review, integration, independent review, tests/build/APK/docs.

## Files Inspected
- `.ai/architecture.md`, `.ai/task_on_progress.md`, `android/android-plan.md`
- `docs/interface-contract.md` command/ACK sections
- `android/.../protocol/Esp32PacketDecoder.kt`
- `android/.../api/ApiService.kt`
- `android/.../PhoneSensorCollector.kt`, `FallDetectionProfiles.kt`
- `android/app/src/main/AndroidManifest.xml`
- `esp/core/local_alert.h`, `esp/core/README.md`
- `esp/esp32-plan.md` sensor, pressure, protocol and HTTP sections

## Tests/Commands Run
- `cd backend && npm test`: PASS — 46/46 Node tests, including dispatch idempotency, callback signature validation, DTMF acknowledgement, round-robin retry, SMS transport status and late callback ordering.
- `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon`: PASS.
- `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew assembleDebug --no-daemon`: PASS.
- `git diff --check`: PASS.
- Debug artifact verified: `android/app/build/outputs/apk/debug/app-debug.apk`.

## Problems/Blockers
- Không có Twilio account SID/auth token/số gọi đi/public HTTPS callback; adapter voice chỉ được kiểm thử fake/mock, mặc định disabled, không gọi số thật.
- Không có thiết bị Android/SIM/GPS hoặc emulator vận hành trong môi trường này; quyền, khóa màn hình, multi-SIM, carrier SMS delivery, Maps intent fallback và FGS background chỉ được unit/build-test.
- ESP32 v1 chưa cung cấp GNSS; BMP390 không được dùng như nguồn tọa độ. Thiết bị UI giữ UNKNOWN/stale khi backend chưa có heartbeat ESP thật.
- Giữ nguyên toàn bộ thay đổi profile/ESP config đang uncommitted từ task trước; không revert/ghi đè.

## Next Action
T1 (OWNER: Codex Sol; FILES: Android non-Compose permission/location/emergency wiring, MainActivity, manifest if needed, API/backend AI contract and tests; GOAL: replace startup permission spam with explicit, safe permission orchestration and add optional AI consent/abstraction; RESULT: pending; NEXT_ACTION: review handoff). T2 (OWNER: AGY Gemini; FILES: MainActivity Settings Compose-only UI and focused UI tests; GOAL: elderly-friendly permissions and AI consent/settings UI using the frozen T1 interface; RESULT: blocked on T1 interface; NEXT_ACTION: dispatch after T1 acceptance). Hermes owns contract review, integration, final tests/build, architecture and nhật ký updates.

## Permission & AI Contract (2026-09-17)
- Android runtime permissions are independent capabilities: `CALL_PHONE`, `SEND_SMS`, and foreground `ACCESS_FINE_LOCATION`/`ACCESS_COARSE_LOCATION`. No background-location permission is requested in this task. `READ_PHONE_STATE` remains only if the existing multi-SIM implementation requires it.
- No runtime permission is requested in `onCreate`. The user taps an explained, named SOS setup action; Android requests only the selected ungranted capability. A permanently denied capability opens App Settings only from a new explicit user tap.
- Permission state exposed to Compose must be refreshed on resume and distinguish granted, can request, and needs Settings. SOS remains best-effort: SMS, SIM call, location and backend/AI failures are isolated.
- Location acquisition must have a bounded timeout and optional last-known fallback; missing/timed-out GPS still dispatches its factual SMS warning.
- AI is opt-in persisted consent, not an Android permission. Android sends the minimum text-only request to a `/api/v1` backend endpoint only while enabled; no contacts, SMS history, call history or precise location is sent by default. Backend holds provider keys exclusively in environment variables and exposes a provider abstraction with disabled/unavailable results. AI is never invoked by or required for SOS/countdown/location/SMS/call.
