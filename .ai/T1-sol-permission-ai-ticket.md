# T1 — Android permission orchestration and optional AI contract

STATUS: IN_PROGRESS
OWNER: Codex Sol
FILES: Android non-Compose permission/location/emergency code, `MainActivity.kt`, manifest only if needed, Android unit tests; `backend/src/**`, `backend/test/**`, `docs/api-contract.md` only for a new `/api/v1` AI endpoint. Do not modify `HomeScreen.kt`, `SettingsScreen` Compose layout, contacts UI, profile/calibration, ESP files, or unrelated existing edits.
GOAL: Complete the existing local SOS implementation without rewrite or parallel services. Preserve working 10-second countdown, contacts, SIM SMS/call and backend voice behavior.

Frozen contract:
1. `CALL_PHONE`, `SEND_SMS`, and foreground fine/coarse location are independent runtime capabilities. Do not request `ACCESS_BACKGROUND_LOCATION`. Keep `READ_PHONE_STATE` only if necessary for the existing multi-SIM gateway.
2. Delete the unconditional startup permission batch. Permission requests happen only after an explicit user action; request one named capability at a time, after a non-technical explanation supplied by UI. Refresh truth from Android on resume.
3. Expose a stable, UI-ready non-Compose interface on `DemoController` (or a narrowly named model/controller) for: capability display state (`GRANTED`, `CAN_REQUEST`, `NEEDS_SETTINGS`), reason/remediation, `request...` intent/callback plumbing, and a non-side-effect `checkSosReadiness` result. Do not expose Android permission constant names to UI text. A permanent denial must not loop request; opening App Settings happens only after another explicit UI action.
4. Location must use a bounded current-fix timeout and then valid last-known fallback if available; null, disabled provider, timeout, permission absence and invalid coordinates leave SOS dispatch intact. Emergency text must include `CẢNH BÁO SOS: Tôi có thể đã bị ngã.`, event time, coordinates and clickable maps URL when available, otherwise `Chưa xác định được vị trí.`
5. AI is an independent persisted consent/toggle, not a runtime permission. Create an Android abstraction exposing enabled/disabled and an optional text-only request. No SOS path calls it. Add a backend `/api/v1` contract only if an implementation needs it: provider abstraction, disabled/unavailable behavior, provider API key only from server environment. Never send contacts, SMS/call history or precise location by default. No secret in APK.
6. Preserve local SOS if offline/backend/AI fails. Preserve existing contact priority rules. No production dispatches or test real SMS/calls.

TDD required: add a focused failing test before each new behavior and capture/retain the RED evidence in your report, then implement and run green. Cover all granted; each missing permission; denied vs permanent denial; GPS timeout/disabled and missing fix; no contact/multiple contacts; SMS/call failure; countdown cancellation/background behavior regressions; AI disabled, consent enabled, unavailable/offline/backend unavailable, and explicit proof AI failure does not block SOS.

Verification required: relevant unit tests, `cd backend && npm test`, `cd android && JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64 ./gradlew testDebugUnitTest --no-daemon`, and `./gradlew assembleDebug --no-daemon`. Do not commit. Finish with STATUS/OWNER/FILES/GOAL/RESULT/NEXT_ACTION plus exact changed files, tests and any limitations.

## Review correction 1 — REQUIRED

RESULT: FAIL pending correction. The Android abstraction currently binds only `UnavailableTextAiProvider`; there is no `/api/v1` endpoint, so enabling AI can never use the project backend. Implement the smallest configured-provider path, while retaining disabled/default-safe behavior:

- Add `POST /api/v1/ai/text` with a bounded text-only request/response in `docs/api-contract.md`, route/controller/service/provider interface, and tests. It must return a factual `NOT_CONFIGURED`/service-unavailable response when no provider configuration exists; it must not accept or log contacts, SMS/call history, SOS records or location fields.
- Implement a server-side OpenAI-compatible provider only when configured by environment (`AI_PROVIDER`, base URL/model/key), with all secrets environment-only. No key, provider base URL, or secret may enter Android BuildConfig/APK. Keep a disabled provider and tests with a fake provider; do not call a live provider.
- Wire the Android `TextAiProvider` to the existing API client for this endpoint. Preserve text-only input, timeout/error mapping, consent enforcement and SOS independence. Do not modify Compose/UI files.
- Add tests proving configured fake-provider success, disabled configuration, backend failure/offline mapping, request field minimization, and no access to GPT from SOS paths. Run the required full test/build commands again.