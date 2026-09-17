# Current Task

## Goal
Chốt API contract chung Android ↔ Backend ↔ ESP32 dựa trên code và thuật ngữ hiện có, thiết kế backend monolith nhỏ đủ để Codex triển khai bằng TDD ở phase sau; bảo đảm luồng UI ↔ API ↔ backend cùng luồng sensor/event → countdown → cancel hoặc SOS và CRUD contacts.

## Status
PHASE 4 — ANDROID API INTEGRATION IN PROGRESS

## Relevant Architecture
Xem chi tiết tại `.ai/architecture.md`.
- **Android**: Compose + `AlertCore` + `DemoController` + `SharedPrefsContactRepository` + `MonitoringService`. 100% unit tests pass trên JDK 21.
- **Backend**: Đã chốt kiến trúc Local Monolith phân tầng (Route → Controller → Service → Repository) với **Node.js v26 thuần dùng built-ins (`node:http`, `node:sqlite`, `node:test`)**, **ZERO external dependencies**, test suite siêu tốc sub-50ms.
- **REST Contract**: Chuẩn hóa duy nhất tại `docs/api-contract.md` (`/api/v1`), cấu trúc envelope duy nhất (`success: true/false`, `data`/`error`, `timestamp`), map trực tiếp `AlertCore.State` (`MONITORING`, `SUSPECTED`, `VERIFYING`, `ALERTING`, `AWAITING_HELP`) và UI `MainScreenStatus`.
- **ESP32**: Đã chuẩn hóa tài liệu tích hợp Wi-Fi REST tại `docs/esp32-api.md`, tham chiếu canonical contract; giữ nguyên BLE packet contract tại `docs/interface-contract.md`.

## Files Inspected in Phase 2
- `.ai/architecture.md`
- `.ai/task_on_progress.md`
- `android/core/src/Core.kt` (`AlertCore`, state/status/event/response, AlertSink, Alert)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt` (`DemoController`, `MainScreenStatus`, `resolveMainScreenStatus`)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/ContactData.kt` (`EmergencyContact`, `ContactValidator`, VN phone regex, repo)
- `android/app/src/main/java/vn/nckh27pa/fallsafe/protocol/Esp32PacketDecoder.kt` (`Esp32SensorPacket`)
- `docs/interface-contract.md`, `docs/decisions.md`, `docs/next-gate.md`, `docs/fixtures/protocol-v1.json`
- `esp/core/local_alert.h` & `esp/core/local_alert.cpp` (`LocalAlertStateMachine`, buzzer, timing, state transitions)

## Post-Review Corrections & Architectural Decisions (Phase 2 Final)
1. **Xóa hoàn toàn Seed Data giả trong Production (§6.4)**:
   - Cơ sở dữ liệu runtime khởi đầu **hoàn toàn trống (clean empty state)**. Không hardcode người giả/thiết bị giả vào production DDL.
   - Fixture test chỉ được nạp khi chạy bộ kiểm thử (`NODE_ENV=test`) hoặc qua test harness chuyên biệt.
   - Bất biến danh bạ ("không được xóa liên hệ duy nhất") được kích hoạt sau khi liên hệ đầu tiên được tạo cho người dùng đó.
2. **Đồng nhất Quyền sở hữu Người dùng (`userId`) cho Danh bạ**:
   - Chốt đường dẫn danh bạ theo người dùng: `/api/v1/users/{userId}/contacts` cho GET/POST/PUT/DELETE/primary/toggle.
   - Bảng `emergency_contacts` gắn kết `user_id TEXT NOT NULL` và khóa chính kép `PRIMARY KEY (user_id, id)` kèm index.
   - Ranh giới v1: `userId` là định danh dữ liệu hợp lệ (path/header `X-User-Id`), không dựng auth server/OAuth2 phức tạp.
3. **Bền vững hóa Watchdog Cảnh báo (Persistence & Crash Recovery)**:
   - `safety_events` lưu trữ: `watchdog_deadline_ms INTEGER`, `watchdog_active INTEGER DEFAULT 0`, `dispatch_status TEXT`.
   - Phục hồi khi server khởi động lại (Startup Recovery Scan) và quét định kỳ: câu lệnh SQL quét các sự kiện `VERIFYING` có `watchdog_active = 1`. Nếu quá hạn (`nowMs >= watchdog_deadline_ms`), hệ thống tự động leo thang một lần duy nhất lên `AWAITING_HELP`, đặt `outcome = 'TIMEOUT_ESCALATED'`, `watchdog_active = 0`, ghi bản tin vào Outbox. Không dựa đơn thuần vào `setTimeout` bay hơi trong RAM.
4. **Phản ánh trung thực trạng thái gửi tin (`dispatchStatus`)**:
   - `/alerts/sos` trả về `dispatchStatus: "RECORDED"` và `eligibleContactsCount: n`, tuyệt đối không báo `SENT` khi chưa có nhà mạng thật (tuân thủ Gate 2 và D04). Cảnh báo được lưu an toàn vào bảng `alert_outbox` phía server.
   - Phản hồi của người thân (caregiver ACK) là độc lập với việc nhà mạng truyền tin và được ghi nhận riêng qua `/alerts/acknowledge`.
5. **Thu hẹp phạm vi Idempotency có thể chứng minh**:
   - Khóa tự nhiên: `(deviceId, sequenceNumber)` cho sensor, `eventId` cho sự kiện, lặp lại trạng thái an toàn cho `/alerts/cancel` / `/alerts/sos` / `/alerts/acknowledge` / `/alerts/resolve`.
   - POST liên hệ hỗ trợ client cung cấp sẵn `id` (UUID v4) để insert idempotent (`INSERT OR IGNORE` / conflict return). Bỏ tuyên bố chung về `Idempotency-Key` header chưa cài đặt.
6. **Rõ ràng trạng thái thiết bị**:
   - `deviceId` chưa từng có trong DB: trả về `404 Not Found` (`RESOURCE_NOT_FOUND`).
   - `deviceId` đã biết: tính toán động `isConnected = (nowMs - lastHeartbeatMs) <= HEARTBEAT_STALE_THRESHOLD_MS` (mặc định 120s). Nếu quá hạn, trả về `200 OK` với `isConnected: false`.
   - Các thao tác đòi hỏi thiết bị trực tiếp mới trả về `503 DEVICE_DISCONNECTED`.
7. **Chốt cứng Công nghệ Backend**:
   - **Node.js (v26) thuần dùng built-ins**: `node:http` (server), `node:sqlite` (database), `node:test` (test runner).
   - Zero dependencies ngoài (`package.json` có `dependencies: {}`).
8. **Chuẩn hóa 100% UUID v4 hợp lệ**:
   - Toàn bộ ví dụ định danh liên hệ, xóa liên hệ, và set primary trong contract sử dụng chuỗi UUID v4 chuẩn (đã kiểm chứng qua regex).

## Changes Made in Phase 2
- `docs/api-contract.md`: Cập nhật toàn diện giải quyết 9 yêu cầu review (xóa seed data, `/users/{userId}/contacts`, watchdog persistence & recovery, `dispatchStatus: "RECORDED"`, dynamic `isConnected`, zero-dependency Node.js built-ins DDL, chuẩn UUID v4).
- `docs/esp32-api.md`: Cập nhật ví dụ phản hồi SOS (`dispatchStatus: "RECORDED"`, `eligibleContactsCount: 2`) đồng bộ canonical contract.
- `docs/interface-contract.md`: Giữ nguyên scope note Section 8.8 dẫn chiếu sang `docs/api-contract.md`.
- `.ai/architecture.md`: Cập nhật quyết định D05, DDL SQLite bảng `alert_outbox`, watchdog persistence và user scoping.
- `.ai/task_on_progress.md`: Cập nhật trạng thái PHASE 2 COMPLETE (REVISED), chi tiết 9 điểm chỉnh sửa và context packet cho Phase 3.

## Phase 3 Assignment
- Owner: Codex.
- Writable scope: `backend/` only.
- Goal: implement the canonical API with strict TDD using Node.js built-ins and real SQLite/HTTP integration tests.
- Prohibited: production seed data, fake successful delivery, real SMS/calls, external dependencies, Android/ESP32/UI edits.

## Phase 3 Result
- Backend implemented under `backend/` with Node.js built-ins, layered HTTP API, SQLite migrations, validation, persistent watchdog recovery, transactional alert outbox and user-scoped contacts.
- Independent verification: `npm test` PASS — 32 tests, 32 passed, 0 failed, including real HTTP, file-backed restart, SIGTERM shutdown, watchdog race/recovery, outbox rollback, SOS/cancel/ACK/resolve and contacts CRUD.
- Runtime limitations remain intentional: no external SMS/call adapter and no credential authentication in v1; dispatch is truthfully reported as `RECORDED`.

## Phase 3 Review Correction — Nullable Gyroscope
- Corrected the single canonical `SensorReading`: metadata and accelerometer axes remain required/non-null; all three gyro axes remain required-present and are either three numbers or three nulls when unavailable. PHONE without gyroscope and ESP32 share this model; no Android changes in this correction.
- Backend validation now accepts the null group, rejects omitted/mixed/invalid axes, and preserves nulls through the existing repository and HTTP latest response. Canonical and backend initial SQLite DDL now permit NULL for the gyro columns; this is a fresh pre-release schema correction, with no runtime table rebuild migration or production seeds.
- TDD RED: `npm test` — 34 tests, 33 passed, 1 failed; PHONE null sample returned `400 VALIDATION_ERROR: Invalid gyroXDps`.
- TDD GREEN: `npm test` — 34 tests, 34 passed, 0 failed; null HTTP/SQLite round-trip and required metadata/accelerometer checks pass.
- JSON-block validation: `docs/api-contract.md` 32/32 valid; `docs/esp32-api.md` 11/11 valid (43 total, 0 invalid).

## Phase 4 Assignment
- Owner: Codex.
- Writable scope: `android/**` only.
- Goal: add one Android network configuration source and wire ApiService → Repository → lifecycle ViewModel/controller adapter → existing UI without visual redesign; preserve offline local safety behavior.

## Tests & Verifications Run
- **JSON Syntax Verification**: Toàn bộ 42 khối JSON ví dụ (`docs/api-contract.md`: 31 khối, `docs/esp32-api.md`: 11 khối) đạt 100% cú pháp JSON hợp lệ qua script Python stdlib.
- **UUID v4 Verification**: 100% ID liên hệ trong `docs/api-contract.md` khớp regex UUID v4 tiêu chuẩn.
- **Regression Tests**:
  - `./android/core/run-tests.sh` (PASS 12/12 tests).
  - `./esp/core/run-tests.sh` (PASS 11/11 tests).
- **Scope Verification**: Chỉ 5 file thuộc `FILES OWNED` được tạo/chỉnh sửa; không có bất kỳ file production/test/build nào bị thay đổi.

## Exact Next Context Packet for Codex (Phase 3 Backend Implementation)
1. **Target Directory**: `backend/` tại thư mục gốc repository.
2. **Runtime & Packages**: Node.js v26.8.2 (`package.json` với `"type": "commonjs"` hoặc `"module"`, `"dependencies": {}`, `"scripts": { "test": "node --test test/*.test.js", "start": "node src/app.js" }`).
3. **Canonical Contract**: Đọc và bám sát `docs/api-contract.md` (mục 3 Data Models, mục 4 Endpoints, mục 6 DDL & Config).
4. **Cấu trúc phân tầng**:
   - `backend/src/config.js`: `PORT` (3000), `HOST` ('0.0.0.0'), `DB_PATH` ('./data/fallsafe.db'), `COUNTDOWN_TIMEOUT_MS` (10000), `WATCHDOG_GRACE_MS` (5000), `HEARTBEAT_STALE_THRESHOLD_MS` (120000).
   - `backend/src/repositories/db.js`: Khởi tạo SQLite bằng `const { DatabaseSync } = require('node:sqlite');` thực thi migration DDL từ `docs/api-contract.md §6.3`.
   - `backend/src/services/alertStateMachine.js`: Chứa hàm `recoverWatchdogOnStartup()` quét `safety_events` đang `VERIFYING` có `watchdog_active = 1` để leo thang ngay nếu quá hạn, và hàm `tick()` định kỳ.
   - `backend/src/services/contactService.js`: Ràng buộc danh bạ theo `userId`, bất biến không xóa liên hệ cuối (khi `contacts.length <= 1` và đã có contact), duy trì đúng 1 `isPrimary`, validate regex số VN (`^(\+84[35789][0-9]{8}|0[35789][0-9]{8})$`).
   - `backend/src/services/deviceService.js`: Suy diễn động `isConnected = (nowMs - lastHeartbeatMs) <= HEARTBEAT_STALE_THRESHOLD_MS`.
   - `backend/src/controllers/` & `backend/src/routes/`: Trả đúng envelope duy nhất `{ success, data, timestamp }` hoặc `{ success, error: { code, message }, timestamp }`.
5. **TDD Test Suite (`backend/test/`)**:
   - Chạy bằng `node --test`.
   - Từng test file ứng với từng controller/service, sử dụng SQLite `:memory:` cho môi trường test sạch.
