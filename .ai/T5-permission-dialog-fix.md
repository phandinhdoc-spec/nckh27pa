# T5 — Permission request must never be a silent no-op (runtime dialog verification)

STATUS: DONE (fixed by AGY Gemini, accepted by Hermes on the emulator)
OWNER: AGY Gemini (`gemini-3.8-flash-medium`)
FILES: `HomeScreen.kt`, `PermissionCenterSection.kt`, `permissions/CapabilityAccess.kt` (copy strings only),
`PermissionCenterUiTest.kt`
GOAL: guarantee that every permission-request tap either starts a real Android system dialog or produces an
explicit, actionable message. No tap may silently do nothing.

## Evidence that drove the ticket (Hermes, emulator, APK v0.2/versionCode 2)
- The three Android system dialogs DO appear from the app:
  `START ... REQUEST_PERMISSIONS ... GrantPermissionsActivity` in logcat, with
  "Allow FallSafe THỬ NGHIỆM to access this device’s location?",
  "Allow FallSafe THỬ NGHIỆM to make and manage phone calls?",
  "Allow FallSafe THỬ NGHIỆM to send and view SMS messages?".
- Real gap: the Compose code discarded the `PermissionRequestResult`. When the result is
  `SETTINGS_REQUIRED` (we already asked and Android will not show the dialog again) a tap produced
  NOTHING: no system dialog, no Settings screen, no message. `EXPLANATION_REQUIRED` was equally silent.

## Fix accepted
- `NEEDS_SETTINGS` copy is now exactly: "Quyền này đang bị tắt. Hãy bật trong Cài đặt."
- Both request entry points (permission center rows, first-run explanation dialog) branch on
  `resolveRequestOutcome(result)` (`PermissionFollowUp.NONE | SHOW_SETTINGS_DIALOG | REQUEST_WITH_EXPLANATION`):
  `SETTINGS_REQUIRED` opens a dialog with that exact sentence and a >=56dp button
  "MỞ CÀI ĐẶT ỨNG DỤNG" (App Settings via `Settings.ACTION_APPLICATION_DETAILS_SETTINGS`);
  `EXPLANATION_REQUIRED` re-requests with the explanation acknowledged.
- Unit tests added for the mapping and for the exact sentence in all three capabilities.

## Verification (Hermes)
- `python3 android/scripts/permission-dialog-acceptance.py` on a clean install (uninstall + install) of
  APK `versionCode=3`, `versionName=0.3-permission`. Evidence in `docs/evidence/permission-dialog/`
  (`acceptance-results.json`, UI dumps for each system dialog, `c-message.xml` with the exact sentence).
- Unit tests + `assembleDebug` in the host terminal: PASS.
