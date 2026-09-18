# T3 (AGY Gemini) — Permission Center UI + first-run permission explanation (Compose only)

STATUS: DONE (accepted by Hermes; one review defect returned and fixed by the same worker)
OWNER: AGY Gemini (model `gemini-3.8-flash-medium`)
GOAL: Elderly-friendly UI that surfaces the SOS permissions the logic already implements. NO Android
permission logic, NO manifest edits, NO new permissions, NO refactor of existing screens.

## WRITABLE SCOPE (exclusive — write nothing else)
- NEW `android/app/src/main/java/vn/nckh27pa/fallsafe/PermissionCenterSection.kt`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/HomeScreen.kt`
- `android/app/src/main/java/vn/nckh27pa/fallsafe/MainActivity.kt` — ONLY inside `private fun SettingsScreen(...)`
  (insert one call to your new composable). Do NOT touch the `MainActivity` Activity class, `DemoScreen`,
  `EventsScreen` or `CenterActionButton`'s SOS firing logic.
- NEW `android/app/src/test/java/vn/nckh27pa/fallsafe/PermissionCenterUiTest.kt` (pure helper tests only)

FORBIDDEN: permissions/, location/, emergency/, api/, FallDetection*, ContactsScreen.kt, AndroidManifest.xml,
build files, docs, backend, ESP. No commits. No dependency additions. No new permissions.

## INTERFACE FROZEN BY T2 (Codex) — use exactly these, do not invent fields
On `DemoController` (read the file after T2 lands and use the real signatures):
- `callingCapability`, `messagingCapability`, `locationCapability`: `CapabilityDisplay` with
  `state: CapabilityDisplayState` (`GRANTED` | `CAN_REQUEST` | `NEEDS_SETTINGS`), `reason`, `remediation`,
  plus the location precision/approximate note added by T2.
- `requestCallingPermission(explanationAcknowledged: Boolean)`,
  `requestMessagingPermission(...)`, `requestLocationPermission(...)` → `PermissionRequestResult`
- `openCallingPermissionSettings()`, `openMessagingPermissionSettings()`, `openLocationPermissionSettings()`
- `nextPermissionSetupStep(): Capability?`, `permissionSetupSeen`, `markPermissionSetupSeen()`,
  the first-run explanation string constant, and the latest per-step `SosDispatchReport`.
- `refreshCapabilityTruth()` already runs from `MainActivity.onResume()`, so reading these properties inside
  a composable recomposes on return from Android Settings.

## REQUIRED UI

### A. "QUYỀN ỨNG DỤNG" section inside `SettingsScreen` (via your new composable)
Three rows, in this order and with these labels/icons:
- `📍 Vị trí`
- `📞 Điện thoại`
- `💬 SMS`
Each row must show the REAL state as plain Vietnamese, never a permission constant:
- granted → `Đã cấp`
- never asked / can ask again → `Chưa cấp`
- permanently denied → `Bị từ chối — cần mở Cài đặt ứng dụng`
- approximate-only location → an explicit working state, e.g. `Đã cấp (vị trí gần đúng)` plus one short
  explanatory line; approximate is NOT an error and must not be shown as refused.
Each row: one large button, min height 56dp, min 18sp bold label:
- state can be requested → `CẤP QUYỀN` → calls the matching `request...Permission(explanationAcknowledged=true)`
- permanently denied → `MỞ CÀI ĐẶT ỨNG DỤNG` → calls the matching `open...PermissionSettings()`
- granted → no action button (show a checkmark/`Đã cấp` only)
Where the state's `reason`/`remediation` copy exists, show it as short body text (>=16sp) under the label.
Accessibility: give every card/button a `contentDescription`/`talkBackLabel` in Vietnamese in the existing
style of this codebase.

### B. First-run explanation (HomeScreen)
- Show ONE dialog the first time the app is usable in `MainScreenStatus.SAFE` while
  `permissionSetupSeen == false` and at least one capability is not granted.
- Title: `Cho phép ứng dụng hoạt động khi khẩn cấp`
- Body (exact text): `Để gửi cảnh báo khi phát hiện té ngã, ứng dụng cần quyền định vị, gửi tin nhắn và gọi người thân.`
- Buttons: `TIẾP TỤC CẤP QUYỀN` (min 56dp) → `markPermissionSetupSeen()` then request ONLY
  `nextPermissionSetupStep()` (one capability, one system dialog); `ĐỂ SAU` → `markPermissionSetupSeen()` only.
- CRITICAL SAFETY RULE: this dialog must NEVER block, delay or gate the SOS action. The SOS hold-to-fire
  path keeps working exactly as it does today, with or without permissions and with or without this dialog.

### C. Truthful per-step SOS feedback (HomeScreen)
After `controller.sos()` results exist for an event, show the per-step outcome in the SOS status area /
a dismissible dialog, using the frozen `SosDispatchReport` labels: which step succeeded, which failed and why,
in plain Vietnamese and no invented success (e.g. never claim a call happened when the call step says
`PERMISSION_MISSING`). Reuse the existing status card/dialog styling; do not redesign the home layout.

## CONSTRAINTS
- Elderly-friendly: body text >= 16sp, headings >= 20sp bold, buttons >= 56dp height, short sentences,
  no English, no technical terms, no raw permission names anywhere on screen.
- Preserve the current UI structure, colors, cards, dark/light theme handling, TalkBack labels and the
  5 main screen states. Do not change layout of existing sections. Do not change existing behavior of
  contacts, location, share, call, device dialogs.
- Pure helpers (state → label/button copy mapping) must be `internal` top-level functions so they can be
  unit tested without a device, mirroring `resolveLocationCardStatus` / `resolveSmsDispatchStatusText`.

## TESTS + VERIFICATION (run them; paste real output)
Add pure unit tests for: granted/never-asked/permanently-denied/approximate labels, button choice per state,
and that no user-visible string contains an Android permission constant (`ACCESS_`, `SEND_SMS`, `CALL_PHONE`).
```
cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon
cd /home/pdd/nckh27pa/android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew assembleDebug --no-daemon
cd /home/pdd/nckh27pa && git diff --check && git status --short
```
No device/emulator screenshot claim is acceptable as proof; report layout as unit-verified only.

## REPORT BACK (exact format)
STATUS / OWNER / FILES / GOAL / RESULT / NEXT_ACTION with: exact changed files, test output, which
requirement each screen element satisfies, and any limitation.


---

## RESULT (Hermes acceptance, 2026-09-18)
- Emulator-verified on a real APK (1080x1920, API 36): `QUYỀN ỨNG DỤNG` shows 📍 Vị trí / 📞 Điện thoại /
  💬 SMS with live states (`Đã cấp`, `Đã cấp (vị trí gần đúng)`, `Chưa cấp`, `Bị từ chối — cần mở Cài đặt
  ứng dụng`), no raw permission names on screen, buttons `CẤP QUYỀN` / `MỞ CÀI ĐẶT ỨNG DỤNG` behave per state.
- Review defect (1) returned to the same worker: the first-run explanation was suppressed in PHONE_ONLY
  mode because it required `MainScreenStatus.SAFE` while a phone-only install is `DEVICE_DISCONNECTED`.
  Corrected for SAFE + DEVICE_DISCONNECTED with regression tests; re-verified on the emulator
  (`first-run` check PASS).
- Reviewer test-method note (not a defect): the dev denied twice → `NEEDS_SETTINGS`; `adb shell pm revoke`
  alone does not reproduce "Don't ask again" on API 36, so device checks use two UI denials.
