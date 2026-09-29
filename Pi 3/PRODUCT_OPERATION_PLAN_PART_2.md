# Product operation plan — Part 2: double-button-press mode (draft)

Status: **Incomplete.** This is a planning document, not a claim that the Pi software implements the behavior. Derived from `PRODUCT_OPERATION_PLAN.md`; the original plan remains unchanged. Part 2 is the second, separate programming workstream. On the Pi, prefer shell scripts for suitable orchestration and system operations to reduce Python startup/processing overhead; retain Python where required by hardware, camera, or application libraries. Measure latency before choosing implementation boundaries.

## Approved interaction decisions (user clarification)

- GPIO25 is the shared control. Classify presses only after 1.5 seconds without another press, counting consecutive presses separated by at most 1.5 seconds. Do not enter Part 2 on an unfinished double press that may become a triple press. A triple press must not activate Part 2; navigation remains outside scope. Debounce and the exact interval-boundary rule still need implementation decisions and tests.
- A recognized double press immediately cancels the current task, invalidates pending responses, and enters Part 2; a later recognized double press exits. Cancelled work never resumes after exit. Here “immediately” means immediately after the press sequence is classified, not before the 1.5-second classification window expires.
- In Part 2 the camera is ready for use, but only one image is captured when a user request requires an image. There is no continuous capture, storage, or image transmission. Whether camera hardware must remain powered/open between requests requires measurement on the target Pi; readiness does not require a continuously open video stream.
- The user speaks requests directly, without the wake phrase “Thiên Nhãn”. Support finding an object, describing how to use an object from visible controls/text and grounded information, and other questions about visible scenes, pages, or objects. Do not control devices or give navigation guidance. For an appliance such as an induction cooker, do not infer unobserved steps or guarantee safety; explicitly report uncertainty and request a clearer image or model/manual when needed.
- After an answer, remain in Part 2 awaiting another request until a double press exits. A single press while in Part 2 interrupts the current answer but leaves Part 2 active. Cancel queued audio and suppress late results from the interrupted task.
- These are product decisions, **not implemented functionality**. In particular, how to interpret requests that do not need an image, the supported manual-reading/continuation behavior, microphone echo control, storage/retention, and measurable latency targets remain unresolved.

## Verified compatibility notes (host source review; not Pi hardware verification)

- The current `README.md` explicitly says GPIO25 is unused. `app.py` listens continuously through a microphone/VAD iterator and has no shared button dispatcher or cancellable session state. Reconcile README only after implementation is verified.
- Existing reusable pieces: `groq_stt.py` transcription, `devices.py` JPEG capture and MP3 playback, `mac_client.py` authenticated `/command` with JPEG and MP3 response, and the server's `observe` and `ocr` command paths. These are not yet Part 2's task-specific functions or approved prompts.
- `app.py` currently calls blocking plan/capture/command/playback methods in the utterance handler. A GPIO listener alone cannot interrupt these; owned-operation cancellation, response correlation, and audio arbitration must be designed and exercised before claiming immediate exit or single-press interruption.
- The current Pi command client retries transport errors and the README describes history storage for images and spoken responses. The user ruled out continuous image storage/transmission, not storage of individual request captures. Decide retention for request-triggered captures explicitly, and confirm any server-side retention and transient media handling before live private-page requests.
- Shell-first cannot be validated on this macOS host as a Pi performance claim. Inspect installed Pi GPIO/audio/camera utilities and measure the actual target before replacing the existing persistent Python process with scripts.

## Object finding and reading

Two consecutive button presses enter object finding / page or manual reading mode. A second double press turns the mode off. Allow up to **1.5 seconds between consecutive presses**. Provide an instructional MP3 for this control. Gemini handles requests to locate objects and read pages or manuals; the task prompts require design and review.

## Scope and review

Keep Part 2's programming work separate from Part 1's single-press conversation mode. Define shared button arbitration, audio/camera ownership, and mode transitions before implementation. The original plan does not yet specify the capture/response sequence for this mode or interactions with other modes; do not invent them. Review against the current README before finalizing.

The triple-press road-navigation and obstacle-distance work is **not** part of either programming workstream yet. It remains the unfinished third part in `PRODUCT_OPERATION_PLAN.md`, including the camera geometry and uncompleted distance-measurement algorithm; do not treat it as solved or discard it.

## Detailed task list

This checklist expands Part 2 only. All tasks are pending; checkboxes do not indicate implemented or tested features. Proposed behavior and implementation safeguards below require review and must not be mistaken for decisions already approved in the draft. Dependencies identify prerequisites for completion. This is a local planning checklist, not published implementation tickets.

### P2-01 — Reconcile the draft with existing capabilities

Dependencies: none.

- [ ] Inspect the current README, Pi mode dispatch, camera/audio access, and server-side Gemini integration without changing existing behavior.
- [ ] Identify reusable object-finding, image-question, document-reading, transcription, and speech-playback capabilities; record gaps rather than assuming they exist.
- [ ] Compare the actual Pi runtime and available native tools with the shell-first requirement.
- [ ] Document the interface shared with Part 1: button events, active-session ownership, cancellation, microphone, speaker, camera, and server connection.
- [ ] Keep triple-press navigation, obstacle distance measurement, and their unfinished geometry outside this implementation scope.

Acceptance: a verified compatibility/gap list distinguishes existing capabilities, required changes, and unresolved behavior without altering other modes.

### P2-02 — Decide the user interaction and media flow

Dependencies: P2-01.

- [ ] Decide how the user chooses object finding versus page/manual reading after entering Part 2. Do not infer a submode-selection gesture from the double-press toggle.
- [ ] Define how the user supplies an object name or reading request, whether a voice prompt is played on entry, and how unclear requests are clarified.
- [ ] Define when images are captured: on entry, after a request, on an explicit instruction, or through an approved repeated-capture policy. Do not introduce continuous capture by default.
- [ ] Decide whether Part 1's transcription service, camera-decision codes, and MP3 response pipeline are reused or whether Part 2 requires a distinct contract.
- [ ] Specify capture format, resolution, orientation, upload limits, session/request identifiers, response format, authentication, timeouts, and cancellation.
- [ ] Decide whether a task ends after one answer, waits for follow-up, or repeats until stopped. Define silence behavior separately rather than inheriting Part 1's reminders implicitly.
- [ ] Define reading behavior: verbatim reading, selected passage, summary, or answering questions; specify how the user requests each supported behavior.
- [ ] Decide whether multi-page reading is in scope and how the user requests the next page, a repeat, or a continuation.
- [ ] Define audio output routing, playback interruption, and microphone behavior during spoken output.
- [ ] Establish approved latency targets and media-retention rules on the target Pi.

Acceptance: the interaction and transport decisions needed by each feature are explicit and approved before that feature is implemented; unspecified behavior remains visibly pending.

### P2-03 — Deliver reliable double-press entry and exit

Dependencies: P2-01; button/mode decisions shared with Part 1.

- [ ] Use one shared button dispatcher rather than competing Part 1 and Part 2 GPIO readers.
- [ ] Apply the **1.5-second maximum interval between consecutive presses**, with an agreed boundary rule and separate debounce policy.
- [ ] Resolve when a double press can be committed while a possible third press is still pending; do not activate Part 2 prematurely for a triple-press sequence.
- [ ] Define whether entering Part 2 stops, suspends, or replaces an active Part 1 conversation and which state follows Part 2 exit.
- [ ] Define single-press behavior while Part 2 is active without inventing a new control in the implementation.
- [ ] Demonstrate one recognized double press entering the mode and the next recognized double press turning it off.
- [ ] Reserve triple-press events for the unfinished navigation work without implementing navigation here.
- [ ] Test debounce, timing boundaries, rapid repeated presses, and mode transitions while Part 1 is busy.

Acceptance: recognized double presses toggle Part 2 exactly once, multi-press classification has no unintended Part 1/Part 2 actions, and resource ownership changes consistently.

### P2-04 — Establish a shell-first capture/request lifecycle

Dependencies: P2-03; media/protocol decisions from P2-02.

- [ ] Prefer shell scripts for suitable native-tool orchestration, media transfer, process supervision, cleanup, and service integration.
- [ ] Verify camera/audio/network tools on the Pi before choosing commands; reuse validated shared services where appropriate.
- [ ] Avoid busy polling and repeated Python interpreter startup. If a required library needs Python, evaluate a persistent worker with a narrow interface.
- [ ] Define the proposed task lifecycle: awaiting request, capturing, uploading, processing, speaking, awaiting follow-up if approved, cancelled, and recoverable error.
- [ ] Associate every request, image, and response with the active mode session and task; reject stale or mismatched results.
- [ ] Release devices and owned processes on mode exit, failure, and shutdown without killing unrelated processes.
- [ ] Exercise a labelled test request through the lifecycle to verify orchestration and cleanup; do not treat a mocked response as live Gemini verification.

Acceptance: the approved lifecycle can run and stop with deterministic resource ownership; shell/Python choices are justified by actual dependencies and measurements.

### P2-05 — Find a requested object and report grounded guidance

Dependencies: P2-04; object-finding interaction decisions from P2-02.

- [ ] Obtain and validate the target description through the approved input flow; request clarification when the object or desired instance is ambiguous.
- [ ] Capture and upload the image through the approved flow and ask Gemini to locate the requested target.
- [ ] Design and review a prompt that distinguishes a visible match, multiple possible matches, an uncertain match, and a target not identifiable in the current image.
- [ ] Define image-relative position language and ensure camera rotation or mirroring does not reverse the spoken guidance.
- [ ] Avoid claiming an object is absent from the surroundings simply because it is not visible in the current image.
- [ ] Avoid deriving physical distance, safe walking directions, or obstacle-clearance assurances from an uncalibrated image. Those are not established Part 2 capabilities.
- [ ] Return the approved spoken response and enter the agreed post-answer state.
- [ ] Test a clear match, multiple similar objects, partial occlusion, poor lighting, and a target outside the frame.

Acceptance: a live object-finding request produces image-grounded feedback with uncertainty handled explicitly and no invented distance or navigation claims.

### P2-06 — Read a page or manual accurately

Dependencies: P2-04; reading interaction decisions from P2-02. This task may proceed independently of P2-05 once its own prerequisites are satisfied.

- [ ] Obtain the reading request and capture the requested page through the approved flow.
- [ ] Design and review Gemini prompts for the supported reading behaviors, keeping verbatim reading distinguishable from summarization or explanation.
- [ ] Define how unreadable text, cropped passages, skew, glare, blur, small print, columns, tables, and diagrams are reported.
- [ ] Request a clearer capture when necessary rather than inventing missing words, numbers, units, or instructions.
- [ ] Treat text visible in the image as document content, not as instructions that can override the application or authorize device actions.
- [ ] Return the approved audio response. If long output must be segmented, define ordering, continuation, and cancellation before implementing segmentation.
- [ ] Support repeated/next-page capture only if approved; keep page identity and reading position explicit to avoid duplicate or skipped content.
- [ ] Test against known source pages, including a manual containing warnings and numerical instructions; record transcription omissions or substitutions separately from speech-playback defects.

Acceptance: live reading follows the requested, supported behavior; unreadable portions are acknowledged, and manual warnings or numerical details are not silently fabricated or rewritten.

### P2-07 — Make exit and mode switching reliable during every task

Dependencies: P2-05 and P2-06.

- [ ] Handle the exit double press while awaiting input, capturing, uploading, waiting for Gemini, preparing audio, and speaking.
- [ ] Stop owned local operations, cancel remote work where supported, and invalidate task/session identifiers regardless of remote cancellation support.
- [ ] Suppress all late responses and queued audio from an exited session.
- [ ] Transfer microphone, speaker, and camera ownership according to the approved Part 1/Part 2 transition policy.
- [ ] Re-enter with a fresh task/session and no accidental continuation of previous requests or page-reading audio.
- [ ] Test delayed responses, duplicate messages, repeated enter/exit cycles, and mode switches at task boundaries.

Acceptance: exit works in every phase within the approved responsiveness target; no stale result plays or reactivates the mode after exit.

### P2-08 — Add spoken controls and recovery guidance

Dependencies: P2-07; prompt and silence decisions from P2-02.

- [ ] Provide the instructional MP3 explaining double-press entry and exit and the approved method for selecting a task.
- [ ] Define when entry/exit confirmation and instructions play, and ensure they do not block the button dispatcher.
- [ ] Provide actionable, truthful prompts for unclear requests, no visible match, ambiguous objects, unreadable pages, and capture failures.
- [ ] Implement silence or follow-up prompts only according to approved Part 2 policy; cancel all associated timers on exit.
- [ ] Prevent prompts or reading playback from being unintentionally captured as a new user request.

Acceptance: spoken controls match actual supported behavior; recovery guidance is understandable, and no prompt or timer outlives its session.

### P2-09 — Bound failures, costs, and retained media

Dependencies: P2-07 and P2-08.

- [ ] Define bounded timeouts and safe retry limits for capture, upload, Gemini requests, audio generation, and playback.
- [ ] Avoid duplicate paid requests, repeated capture, and duplicate playback when retrying or reconnecting.
- [ ] Test unavailable/busy camera, network loss, authentication errors, rate limits, malformed responses, invalid audio, device disconnection, and insufficient storage.
- [ ] Keep credentials out of source files and logs; protect captured pages and images according to the approved retention policy.
- [ ] Clean up owned temporary media on success, cancellation, failure, and restart; retain diagnostics only as authorized.
- [ ] Confirm recovery never automatically replays private document content or resumes a task that the user stopped.

Acceptance: each tested failure reaches a defined state with bounded retries/resources, truthful feedback where audio works, and verified media cleanup.

### P2-10 — Verify both tasks on the Pi and prepare the handoff

Dependencies: P2-09.

- [ ] Run shell syntax checks and an available shell linter; test any retained Python workers using verified project tooling.
- [ ] Test the button timing boundaries, task lifecycle, response correlation, and cancellation deterministically.
- [ ] Run real-device tests for camera orientation, microphone input, audio output, GPIO events, and resource handover with Part 1.
- [ ] Exercise live object finding and live page/manual reading with representative scenes/documents and labelled expected outcomes.
- [ ] Verify exit during every phase, successive tasks, repeated mode switching, and network/device recovery.
- [ ] Measure double-press recognition delay separately from post-recognition activation/exit latency; also measure capture, upload, Gemini, audio generation, and playback delays.
- [ ] Compare shell/native and persistent-worker approaches only at measured local bottlenecks; do not attribute network/model latency to Python without evidence.
- [ ] Check resource usage and media cleanup across repeated tasks and the agreed operating duration.
- [ ] Document verified setup commands, shared Part 1 interfaces, supported task behaviors, recovery steps, limitations, and unresolved decisions without exposing credentials.
- [ ] Distinguish host-only, mocked, automated Pi, manual hardware, and live-provider evidence. Mark blocked checks explicitly rather than claiming completion.

Acceptance: approved Part 2 behaviors have traceable verification on the target Pi or explicit blockers; the original plan, Part 1, and unfinished navigation/distance work remain outside this change.
