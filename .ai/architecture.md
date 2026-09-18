# Architecture & System Map (NCKH27PA)

> Verified baseline facts established during Phase 1 Discovery. No unverified assumptions or speculative components.

## 1. Purpose
Hệ thống hỗ trợ phát hiện té ngã, choáng váng và kích hoạt cảnh báo khẩn cấp dành cho người cao tuổi, vận hành theo 3 chế độ (`PHONE_ONLY`, `HYBRID`, `ESP32_ONLY_INPUT`). Mục tiêu là phát hiện nguy cơ, mở cửa sổ đếm ngược xác minh an toàn tại chỗ và gửi thông báo/cuộc gọi trợ giúp khi hết hạn mà không có phản hồi an toàn từ người dùng.

## 2. Tech Stack
- **Android Application**:
  - Language: Kotlin (`jvmTarget = "17"`), Gradle 8.13, Android SDK compileSdk/targetSdk 36, minSdk 26.
  - UI: Jetpack Compose (BOM 2025.08.01, Material3), `ComponentActivity`.
  - Architecture: Lightweight Single-Activity + Composable Tabs (`DemoScreen`), process-scoped controller (`DemoController`), pure domain core (`AlertCore`), repository pattern (`ContactRepository`), Gson serialization (`gson:2.11.0`).
  - Native Sensors: `SensorManager` (Accelerometer, Linear Acceleration, Gyroscope, Rotation Vector, Barometer/Pressure).
  - Background Service: `MonitoringService` (`FOREGROUND_SERVICE_HEALTH|LOCATION`, `WAKE_LOCK` scoped to active verification countdown). Location FGS type is enabled only after foreground location permission; unrestricted background location is not claimed.
- **Embedded / ESP32**:
  - Target Framework: Arduino ecosystem (quyết định D01, `~/.arduinoIDE/arduino-cli.yaml`, toolchain `arduino-cli` v1.5.1, core `esp32:esp32` 3.3.11).
  - Host Core Logic: C++17 pure finite state machine (`LocalAlertStateMachine` in `esp/core/local_alert.{h,cpp}`).
  - Verification: Sanitizers (ASan, UBSan) on host C++ unit tests.
- **Protocol Lab**:
  - Host Framing Prototype (IF-003): Kotlin (`Framing.kt`) and C++17 (`framing.hpp`) bounded fragment reassembler (1024-byte max, ATT_MTU chunking).
- **Backend / Remote Server**:
  - **TRẠNG THÁI HIỆN TẠI: Android/backend emergency dispatch implementation and contract are present; real external dispatch remains disabled until provider configuration.**
  - Architecture: Lightweight Local Monolith (Node.js v26 runtime, zero external dependencies, built-in `node:http`, `node:sqlite`, `node:test`), phân tầng Route → Controller → Service → Repository.
  - Storage: Cơ sở dữ liệu SQLite cục bộ (chạy in-memory khi test, file `data/fallsafe.db` khi chạy thực tế; khởi động hoàn toàn trống, không seed dữ liệu giả vào production). Bền vững hóa watchdog qua `watchdog_deadline_ms`, `watchdog_active` và cơ chế startup scan recovery.
  - REST Contract: Chuẩn hóa duy nhất tại `docs/api-contract.md` (`/api/v1`), cấu trúc envelope duy nhất (`{"success": true, "data": ..., "timestamp": ...}`), ánh xạ trực tiếp `AlertCore.State` và `MainScreenStatus`, danh bạ khẩn cấp theo người dùng (`/api/v1/users/{userId}/contacts`), `dispatchStatus: "RECORDED"`.
  - ESP32 REST Guide: Chuẩn hóa tại `docs/esp32-api.md`.
  - Implementation: Android location/SMS coordinator, durable sync outbox, backend voice dispatch adapter, authenticated provider callbacks and transport-status reporting are covered by automated tests. Provider network calls remain opt-in configuration.

## 3. Repository Structure
```
/home/pdd/nckh27pa/
├── .ai/                       # AI context & progress tracking
│   ├── architecture.md        # System architecture and facts (this file)
│   └── task_on_progress.md    # Active task tracking
├── AGENTS.md                  # Project orchestration & quota policy
├── android/                   # Android Compose application & shared core
│   ├── app/                   # Android application module (:app)
│   │   ├── build.gradle.kts   # Gradle build config
│   │   └── src/
│   │       ├── main/          # MainActivity, HomeScreen, ContactsScreen, DemoApplication, MonitoringService, etc.
│   │       └── test/          # JUnit unit tests (DemoTests, HomeScreenLogicTest, OwnershipTests, DecoderTests)
│   ├── core/                  # Pure Kotlin multiplatform core logic
│   │   ├── src/Core.kt        # AlertCore state machine (MonotonicClock, AlertSink, State, Status)
│   │   ├── src/Demo.kt        # Demo entrypoint
│   │   └── tests/CoreTests.kt # AlertCore pure tests
│   └── evidence/              # Verification logs & screenshots
├── docs/                      # Requirements, decisions, and protocol specs
│   ├── decisions.md           # Architectural decisions (D01–D04, C01–C07)
│   ├── interface-contract.md  # Android-ESP32 API v1 packet contracts
│   ├── interface-framing-draft.md # Binary framing draft (PROPOSAL)
│   ├── next-gate.md           # Project gates & pending decisions
│   └── fixtures/protocol-v1.json # Shared test vectors
├── esp/                       # ESP32 firmware workspace
│   ├── core/                  # C++17 local alert state machine & host tests
│   └── esp32-plan.md          # ESP32 master plan
└── protocol-lab/              # Cross-language BLE framing test lab
```

## 4. Entry Points
- **Android App**: `vn.nckh27pa.fallsafe.MainActivity` (UI lifecycle), `vn.nckh27pa.fallsafe.DemoApplication` (Application context & controller holder), `vn.nckh27pa.fallsafe.MonitoringService` (Health foreground service).
- **Android Core Unit Engine**: `core.AlertCore` in `android/core/src/Core.kt`.
- **ESP32 State Engine**: `fallsafe::LocalAlertStateMachine` in `esp/core/local_alert.h`.
- **Protocol Lab Harness**: `protocol-lab/kotlin/Main.kt` and `protocol-lab/cpp/main.cpp`.

## 5. Core Modules & Components
1. **AlertCore (`android/core/src/Core.kt`)**:
   - Thread-confined, zero-background-thread state engine.
   - States: `MONITORING` → `SUSPECTED` → `VERIFYING` → `ALERTING` → `AWAITING_HELP`.
   - Actions: `suspected()`, `evidenceConfirmed()`, `riskCleared()`, `tick()`, `safe()`, `needHelp()`, `sos()`, `complete()`.
   - Timeout: 10,000 ms countdown (`MonotonicClock`). Upon timeout, automatically triggers `Response.NO_RESPONSE` alert.
2. **DemoController (`android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt`)**:
   - Manages state observation for Compose UI, sensor pipeline binding, contacts CRUD, and demo replays.
   - Holds `MainScreenStatus`: `SAFE`, `WARNING_COUNTDOWN`, `SOS_SENT`, `HELP_ACKNOWLEDGED`, `DEVICE_DISCONNECTED`.
3. **Emergency Contacts System (`ContactData.kt` & `ContactsScreen.kt`)**:
   - Entity: `EmergencyContact(id, name, relationship, phone, receiveSos, isPrimary)`.
   - Persistence: `SharedPrefsContactRepository` (JSON via Gson under `fallsafe_emergency_contacts`).
   - Validation: `ContactValidator` enforces Vietnamese phone format (`^(\+84[35789][0-9]{8}|0[35789][0-9]{8})$`), phone normalization, masking.
   - Business Rules: Always maintains at least 1 contact (cannot delete last contact), enforces exactly one primary contact.
4. **Sensor Pipeline (`PhoneSensorCollector.kt`, `DemoLogic.kt`)**:
   - `PhoneNormalizer`: bounds pressure history to 5s/64 samples, calculates relative altitude (`altitudeDeltaM`).
   - `DemoDetector`: receives the current `FallDetectionConfig` from the active local profile; the initial experimental values preserve `magnitude >= 25 m/s²` followed by 1s quiet period (`9.81 ± 1 m/s²`). No detector threshold remains as a magic number.
   - `FallDetectionObservation` exposes only the current algorithm's real evidence: acceleration magnitude, detector phase, impact threshold state, stillness progress/sample count, and active profile identity/config.
   - `SensorOwnership`: arbitrates sensor listener between foreground Activity and background `MonitoringService`.
5. **Fall Detection Profiles (`FallDetectionProfiles.kt`, `FallDetectionCalibrationScreen.kt`)**:
   - `FallDetectionConfig` contains exactly the seven parameters used by `DemoDetector`, with unit-bearing names and bounded validation.
   - `FallDetectionProfile` has a stable ID, immutable system name `Bảng N`, monotonic `profileNumber`, timestamps, one active flag, and config. `lastAssignedProfileNumber` prevents number reuse after deletion.
   - One Gson snapshot is committed synchronously to SharedPreferences `fallsafe_detection_profiles_v1_<userId>` under `profiles_snapshot_json`; corrupt, empty, invalid, or multi-active data recovers to one active `Bảng 1` using initial experimental values.
   - Save updates one existing profile; Save As creates a new inactive profile; activation is explicit; the active or sole remaining profile cannot be deleted. Detection remains local and offline.
   - Calibration UI is reachable only from `Cài đặt → Phát hiện té ngã → Hiệu chỉnh thử nghiệm`; selected/draft and active profile state are distinct, and realtime display is throttled to approximately 4 Hz.
6. **Android SOS permission flow (`permissions/`, `location/LocationResolution.kt`, `PermissionCenterSection.kt`)**:
   - Three independent runtime capabilities: `CALLING` (`CALL_PHONE`), `MESSAGING` (`SEND_SMS`; `READ_PHONE_STATE` only adds a multi-SIM degradation note), `LOCATION` (foreground fine/coarse). `ACCESS_BACKGROUND_LOCATION` is never requested.
   - Display states `GRANTED | CAN_REQUEST | NEEDS_SETTINGS` derive from the attempt store plus `shouldShowRequestPermissionRationale`; a permanent denial never re-loops the system dialog and opens App Settings only from a separate explicit action. Location precision (`PRECISE|APPROXIMATE|NONE`) is surfaced, and approximate-only is a working state.
   - No permission is requested at startup. The first-run explanation appears once in non-emergency states (SAFE and DEVICE_DISCONNECTED, so it also works in PHONE_ONLY mode) and never gates the SOS hold-to-fire action; a modal system dialog appears only after an explicit tap, one capability at a time (Vị trí → SMS → Điện thoại).
   - `EmergencyCoordinator` produces a persisted per-step `SosDispatchReport` (`LOCATION`, `SMS`, `VOICE_CALL`, `MAP_LINK` each with its own status) driven by an injected `CapabilitySnapshot`, so SOS degradation is unit-testable without Android. Map opening resolves Google Maps → generic `geo:` → https maps URL in a browser. Full contract, matrix and manual test steps: `docs/permission-flow.md` (decision D07).
7. **Packet Decoder (`Esp32PacketDecoder.kt`)**:
   - Strict JSON streaming parser for `Esp32SensorPacket` (max 4096 bytes, strict numeric ranges, reject duplicate keys/nested objects).
8. **ESP32 Local Alert (`esp/core/local_alert.{h,cpp}`)**:
   - C++17 state machine tracking alert deadline, buzzer output, sensor health, and BLE connection status.

## 6. Data Flow & Call Chains
### Call Chain 1: Android UI → State Machine → Alert Sink
- User touches "Báo động ngay (SOS)" or holds button for 3s in `HomeScreen.kt` / `DemoScreen`.
- Calls `controller.help()` / `controller.sos()` in `DemoApplication.kt`.
- Invocates `session.needHelp()` → `core.needHelp()` → `core.dispatch(Response.NEED_HELP)` in `Core.kt`.
- `AlertCore` records event, transitions state to `ALERTING`, status to `SENDING`, invokes `sink.send(Alert)`.
- Upon sink success: state transitions to `AWAITING_HELP`, status to `SENT`. UI displays `MainScreenStatus.SOS_SENT`.
- When caregiver confirms: calls `controller.acknowledgeHelp()`, transitioning UI to `MainScreenStatus.HELP_ACKNOWLEDGED`.
- Call `controller.complete()` resets core to `MONITORING` (`Status.ACKNOWLEDGED`).

### Call Chain 2: Sensor Impact → Countdown → Cancel / Expiry
- Real sensor reading or `DemoReplay` feeds `PhoneSensorPacket` into `DemoSession.accept(packet)`.
- `DemoDetector.accept(packet)` detects fall impact + quiet period → calls `core.suspected()` + `core.evidenceConfirmed()`.
- State transitions to `VERIFYING` (`Status.COUNTDOWN`), starting 10s timer.
- Branch A (User cancels): User holds "Tôi vẫn ổn" for 2s (`SosHold.ready() == true`) → calls `controller.safe()` → `core.safe()` → transitions back to `MONITORING`, status `NOT_REQUIRED`.
- Branch B (Expiry): Periodic `poll` / `tick` runnable checks `clock.nowMs() - started >= 10000ms`. When exceeded, `core.tick()` calls `dispatch(Response.NO_RESPONSE)` → dispatches emergency alert to sink.

### Call Chain 3: Contacts CRUD
- UI `ContactsScreen.kt` collects inputs → invokes `controller.addContact(...)` / `updateContact(...)` / `deleteContact(...)`.
- `ContactValidator` normalizes and validates VN phone number format.
- Business constraints applied (at least 1 primary, no deleting last contact).
- Saved synchronously to SharedPreferences via `SharedPrefsContactRepository.saveContacts(list)`.
- State `contacts` updated in `DemoController`, triggering immediate Compose recomposition.

## 7. Storage & APIs
- **Android Local Storage**: SharedPreferences for contacts plus `fallsafe_detection_profiles_v1_<userId>` (`profiles_snapshot_json`) for one atomic profile snapshot, active profile, and monotonic numbering. In-memory ring buffer for event history (`ArrayDeque<RecordedEvent>`, max 32 entries).
- **Backend Monolith Storage (SQLite)**: Bảng `devices` (suy diễn `isConnected` động từ `last_heartbeat_ms`), `sensor_readings` (chống trùng `deviceId, sequenceNumber`), `safety_events` (bền vững `watchdog_deadline_ms`, `watchdog_active`, `dispatch_status`), `emergency_contacts` (định danh kép `user_id, id`), `alert_outbox` (ghi nhận cảnh báo an toàn máy chủ). DDL tại `docs/api-contract.md §6.3`. Hỗ trợ chạy in-memory cho test và persistent file `data/fallsafe.db`. Không seed dữ liệu giả vào production.
- **Canonical REST API v1 Contract** (`docs/api-contract.md`):
  - Base path `/api/v1`, format JSON `application/json; charset=utf-8`.
  - SensorReading correction (Phase 3 review): metadata và ba trục gia tốc vẫn bắt buộc, không null. `gyroXDps/gyroYDps/gyroZDps` bắt buộc xuất hiện nhưng nullable theo nhóm: ba số hoặc ba null khi cảm biến không khả dụng (bao gồm PHONE). SQLite gyro columns cho phép NULL; sửa initial DDL pre-release, không thêm table-rebuild migration và không seed runtime.
  - Chuẩn hóa envelope duy nhất (`success: true/false`, `data`/`error`, `timestamp`).
  - Endpoints: System/device status, sensor ingestion/latest, event ingestion/read, alerts lifecycle (sos, cancel, acknowledge, resolve), user contacts CRUD (`/api/v1/users/{userId}/contacts`).
- **ESP32 Integration**:
  - Wi-Fi HTTP REST: Theo `docs/esp32-api.md`.
  - BLE GATT: Theo `docs/interface-contract.md`.

## 8. Build & Verification Commands
- **Environment Requirement**: JDK 21 (`export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64`). Host system default is Java 25.0.3, which is incompatible với Gradle 8.13.
- **Android Unit Tests**:
  ```bash
  JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon
  ```
- **Android Build APK**:
  ```bash
  JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew assembleDebug --no-daemon
  ```
- **Android Core Unit Tests (without Gradle)**:
  ```bash
  ./android/core/run-tests.sh
  ```
- **Android Emulator Permission/SOS Flow Check** (emulator only, refuses physical devices):
  ```bash
  adb uninstall vn.nckh27pa.fallsafe && adb install android/app/build/outputs/apk/debug/app-debug.apk
  python3 android/scripts/permission-dialog-acceptance.py   # real system dialogs on a clean install
  python3 android/scripts/permission-flow-check.py          # permission center, SOS degradation, maps
  ```
  Evidence: `docs/evidence/permission-dialog/` and `docs/evidence/permission-flow/`.
- **ESP32 Core Host Tests**:
  ```bash
  ./esp/core/run-tests.sh
  ./esp/core/run-tests.sh --sanitize
  ```
- **Protocol Lab Cross-language Tests**:
  ```bash
  ./protocol-lab/run.sh
  ```

## 9. Important File Map
- `android/core/src/Core.kt`: Pure state machine for detection, verification, and alerting.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/DemoApplication.kt`: Application lifecycle & `DemoController`.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt`: Compose UI hosting and background service orchestration.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionProfiles.kt`: Validated config/profile model, repository contract, Gson snapshot persistence, numbering and active-profile invariants.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/FallDetectionCalibrationScreen.kt`: Research-only profile editor and throttled detector observation panel.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/permissions/CapabilityAccess.kt`: capability states, request/settings policy, first-run setup ordering, SOS readiness.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/PermissionCenterSection.kt`: Compose "QUYỀN ỨNG DỤNG" section plus pure label helpers for tests.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/HomeScreen.kt`: High-contrast elderly-friendly UI, center action button, 5 states, first-run permission explanation, per-step SOS report dialog.
- `android/scripts/permission-flow-check.py`: emulator-only end-to-end permission/SOS degradation harness.
- `docs/permission-flow.md`: permission contract, fail-safe matrix, automated and manual test procedures.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/ContactData.kt`: EmergencyContact models, validator, and repository.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/ContactsScreen.kt`: Full CRUD UI for emergency contacts.
- `android/app/src/main/java/vn/nckh27pa/fallsafe/protocol/Esp32PacketDecoder.kt`: JSON protocol parser for ESP32 packets.
- `docs/api-contract.md`: Canonical REST API v1 contract cho Android, ESP32, và Backend.
- `docs/esp32-api.md`: Hướng dẫn tích hợp REST API cho firmware ESP32.
- `docs/interface-contract.md`: Formal schema và packet contract giữa Android và ESP32 (BLE GATT).
- `docs/decisions.md`: Project constraints, decisions (D01–D04), and protocol gaps (C01–C07).
- `docs/next-gate.md`: Pending gates requiring project owner approval.

## 10. Constraints, Decisions & Gaps
- **Decisions**:
  - D01: Arduino ecosystem for ESP32 firmware (`arduino-cli`), not ESP-IDF.
  - D02: `PHONE_ONLY` is a required independent operational mode.
  - D03: 10s countdown automatically fires SOS on timeout; GPS failure must not block emergency alerts. Android SIM SMS may be attempted only after runtime permission; external voice dispatch is disabled unless backend provider configuration is explicitly supplied.
  - D04: No cloud services, external backend, SMS/cellular charges, or real emergency dispatches without explicit owner authorization.
  - D05 (Phase 2): Backend Monolith dùng Node.js v26 built-in modules (`node:http`, `node:sqlite`, `node:test`) hoàn toàn zero external dependencies; khởi động sạch không seed data production; bền vững watchdog recovery qua database; contacts đóng gói theo `userId` (`/api/v1/users/{userId}/contacts`).
  - D06: Android fall detection profiles are local-first SharedPreferences snapshots. Exactly one profile is active; editing or Save As never activates implicitly; local detection does not depend on `/api/v1` or connectivity. Defaults are initial experimental values, not validated medical thresholds.
- **Identified Gaps (Frontend vs Backend/ESP32)**:
  1. **External Voice Provider**: Twilio-compatible adapter and authenticated callbacks are implemented and mock-tested, but there is no account, outbound number, public HTTPS callback or permitted test number. No real call or recorded Vietnamese speech has been verified.
  2. **Android Field Verification**: Permission center, request/denied/permanent-denial, App Settings round trip, grant-on-resume, approximate-only location, location-service-off degradation, Google Maps opens/absent fallback and crash-free SOS degradation are verified on the project emulator (see `docs/evidence/permission-flow/`). Still unverified on a physical device: OEM permission dialogs, multi-SIM selection, carrier SMS delivery reports, real SIM calls, outdoor GPS fix time, lock-screen behaviour and device-specific FGS/background limits.
  3. **ESP32 BLE Transport**: Giao thức framing phân mảnh nhị phân (IF-003) mới chỉ ở mức lab prototype trên host, chưa được phê duyệt tích hợp vào BLE GATT stack thật trên ESP32 hay Android.
  4. **Event & Status Decoders**: Android mới chỉ triển khai `Esp32PacketDecoder.decodeSensor`; chưa có parser cho `Esp32DeviceStatus`, `Esp32EventPacket`, hay `Esp32CommandAck`.
