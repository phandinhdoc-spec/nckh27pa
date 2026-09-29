# Product operation plan — Part 1: single-button-press conversation mode (draft)

Status: **Incomplete.** This is a planning document, not a claim that the Pi software implements the behavior. Derived from `PRODUCT_OPERATION_PLAN.md`; the original plan remains unchanged. Part 1 is the first, separate programming workstream. On the Pi, prefer shell scripts for suitable orchestration and system operations to reduce Python startup/processing overhead; retain Python where required by hardware, camera, or application libraries. Measure latency before choosing implementation boundaries.

## First startup and hardware status

On first boot, check whether the GY-53 sensor, GY-25 sensor, Bluetooth headphones, speaker, MAX amplifier, and microphone are usable, and report their status. The test for each component, failure reporting without working audio, and whether failures block startup remain undecided. After startup, say **“How may I help you?”** and begin listening.

## Voice conversation

Voice control has the highest priority. Record the user's speech with the microphone and send the audio to Groq for transcription by **Whisper Large v3 Turbo**. The server passes the transcript to Gemini to decide whether a camera image is strictly necessary.

- If an image is necessary, Gemini returns `CAPTUREX`; the server sends it immediately to the Pi. The Pi activates the camera, captures a photo, and sends it to the server. The server forwards the image to Gemini, converts Gemini's answer to MP3, and returns the MP3 for playback through the Pi speaker.
- Otherwise, Gemini returns `NOCAPTUREX`, which stays on the server. The server asks Gemini to answer from the transcript, converts the answer to MP3, and returns it for playback through the Pi speaker.

After playback, listen for another utterance. If the user does not respond, play **“Do you need any assistance from me?”** The silence timeout, reminder count, and subsequent behavior remain unspecified.

## Single-button-press behavior

Provide an instructional MP3 for the controls. One press during a conversation stops it; one press after stopping starts a new conversation. Allow up to **1.5 seconds between consecutive presses** so a single press can be distinguished from a double or triple press. How to handle a pending single press during this interval and the interaction with other modes remain undecided.

## Scope and review

Keep Part 1's programming work separate from Part 2's double-press mode. Define interfaces between them before implementation; do not assume the existing software already matches this plan. Review the current README, camera decision behavior, timing, failure paths, and button arbitration before finalizing.

## Detailed task list

This checklist expands Part 1 only. All tasks are pending; checkboxes do not indicate implemented or tested features. Items described as proposed are recommendations for review, not approved requirements. Dependencies identify prerequisites for completion. This is a local planning checklist, not published implementation tickets.

### P1-01 — Reconcile the draft with the existing system

Dependencies: none.

- [ ] Inspect the current Pi entry points, startup configuration, button handling, audio tools, camera access, and Pi/server protocol without changing existing behavior.
- [ ] Compare the current README and implementation against each Part 1 requirement; record what can be reused, what conflicts, and what is absent.
- [ ] Establish the target Pi model, operating system, available shell, GPIO access method, and installed native audio/camera/network tools.
- [ ] Record actual commands and interfaces only after verification; do not assume a package or executable exists on the Pi.
- [ ] Define Part 1's interface to the shared button dispatcher and resource ownership. Reserve double-press and triple-press events without implementing their modes here.

Acceptance: a reviewed compatibility/gap list and verified runtime prerequisites exist; unrelated modes and user changes are preserved.

### P1-02 — Resolve behavioral decisions and protocol contracts

Dependencies: P1-01.

- [ ] Clarify whether “first boot” means initial provisioning only or every device startup, and how a repeat hardware check is requested.
- [ ] Decide hardware failure severity and whether conversation may proceed when navigation-only sensors are unavailable.
- [ ] Decide single-press arbitration: defer action until the multi-press window closes, or use an explicitly defined immediate-stop policy while avoiding unintended restart/mode activation.
- [ ] Define debounce, the exact 1.5-second boundary rule, and handling of extra presses. Keep debounce separate from the inter-press window.
- [ ] Define silence timeout, maximum utterance length, end-of-speech detection, reminder count, reminder interval, and behavior after continued silence.
- [ ] Define whether speech may interrupt playback and how microphone capture avoids recording speaker output. This is unresolved in the draft.
- [ ] Confirm whether the Pi sends audio directly to Groq or through the server, where credentials reside, and the audio/transcript handoff.
- [ ] Specify message framing, request/session identifiers, media formats, timeouts, errors, cancellation, and authentication for Pi/server exchanges.
- [ ] Preserve `CAPTUREX` and `NOCAPTUREX` exactly. Specify that only `CAPTUREX` reaches the Pi; invalid model output must never trigger capture.
- [ ] Resolve whether voice replies must use the speaker even when Bluetooth headphones are connected; the draft requires checking headphones but specifies speaker playback.
- [ ] Agree on measurable responsiveness targets on the target Pi rather than inventing latency guarantees.

Acceptance: unresolved choices are explicitly recorded and relevant choices are approved before their dependent behavior is implemented.

### P1-03 — Establish a shell-first runtime and local conversation lifecycle

Dependencies: P1-01; lifecycle and arbitration decisions from P1-02.

- [ ] Use shell scripts where suitable for service startup, native-tool orchestration, file handling, health probes, and shutdown.
- [ ] Compare available native tools and library requirements before assigning recording, playback, camera capture, networking, and GPIO operations to shell or Python.
- [ ] Avoid repeatedly starting Python for individual events. If a library requires it, evaluate a persistent worker with an explicit interface.
- [ ] Avoid busy polling and excessive subprocess creation; shell scripting alone does not guarantee lower latency.
- [ ] Define a proposed lifecycle covering startup, listening, recording, transcribing, deciding, capture when required, answer generation, playback, stopped, and recoverable error states.
- [ ] Track subprocess ownership and ensure shutdown stops only this application's processes, releases devices, and removes its temporary media.
- [ ] Prevent duplicate runtime instances from competing for microphone, speaker, camera, or GPIO access.
- [ ] Demonstrate a single press stopping a local conversation and a later single press starting a fresh one using the agreed arbitration policy.

Acceptance: the local lifecycle can start, stop, and restart without orphan processes or resource conflicts; interpreter choices are justified by dependencies and measurements.

### P1-04 — Deliver startup checks and the greeting

Dependencies: P1-03; startup/failure policy from P1-02.

- [ ] Define a bounded, non-destructive check for each of GY-53, GY-25, Bluetooth headphones, speaker, MAX amplifier, and microphone.
- [ ] Distinguish device presence, successful communication, usable data, and verified physical operation. An audio command succeeding is not proof that a speaker or amplifier is audible.
- [ ] Label hardware that cannot be automatically verified as unknown or requiring manual confirmation instead of reporting a false pass.
- [ ] Report each component's result using available output and the approved failure policy; define a fallback when audio is unavailable.
- [ ] Play “How may I help you?” after startup completes, then enter listening mode.
- [ ] Test healthy startup, a missing component, a stalled probe, and unavailable audio output.

Acceptance: startup cannot hang indefinitely on a probe; reported results match observed evidence, and the greeting precedes listening when startup is allowed to proceed.

### P1-05 — Complete a conversation without camera capture

Dependencies: P1-04; transport, audio, and credential decisions from P1-02.

- [ ] Record one utterance using the agreed audio format and speech/silence boundaries.
- [ ] Send audio through the agreed route to Groq using **Whisper Large v3 Turbo** and deliver its transcript to the server.
- [ ] Handle empty audio, empty transcripts, recording errors, and transcription failures without submitting a meaningless answer request.
- [ ] Have the server ask Gemini whether an image is strictly necessary and validate its decision against the fixed code contract.
- [ ] On `NOCAPTUREX`, keep the code internal to the server and request an answer based on the transcript.
- [ ] Convert the answer to MP3 on the server, return it to the matching Pi turn, and play it through the approved output.
- [ ] Return to listening only after playback completes and the session remains active.
- [ ] Keep credentials out of source code, command-line arguments where exposure is possible, and logs.

Acceptance: a live, non-image question produces a spoken answer and resumes listening; camera access does not occur and `NOCAPTUREX` is not sent to the Pi. Separately label any mocked tests.

### P1-06 — Complete a conversation requiring an image

Dependencies: P1-05.

- [ ] On a validated `CAPTUREX`, have the server immediately request capture from the Pi for the active turn.
- [ ] Activate the camera only for an authorized, active request; capture and upload the image using an agreed format and resolution. Do not assume the navigation resolution applies to this mode.
- [ ] Correlate the image with the original question and forward both required context and image to Gemini.
- [ ] Convert the resulting answer to MP3, return it to the Pi, play it, and resume listening.
- [ ] Handle unavailable camera, failed capture, upload failure, and unusable images without presenting an answer as visually verified.
- [ ] Reject stale, duplicate, malformed, or cancelled capture requests according to the agreed protocol.

Acceptance: a live image-dependent question causes capture only after `CAPTUREX`, returns a spoken answer, and resumes listening; failure cases are explicit rather than silent.

### P1-07 — Make stopping reliable throughout a remote turn

Dependencies: P1-05 and P1-06.

- [ ] Apply single-press stopping during recording, transcription, decision-making, capture/upload, answer generation, and playback.
- [ ] Stop owned local media processes, suppress queued playback, and cancel remote work when supported.
- [ ] Invalidate the stopped session so late transcripts, capture requests, images, and MP3 responses cannot restart it or play unexpectedly.
- [ ] On the next single press, create a fresh conversation with clean turn ownership and released resources.
- [ ] Test presses near state transitions, delayed responses, and repeated stop/start cycles.
- [ ] Verify double/triple-press sequences do not accidentally trigger Part 1 actions under the agreed dispatcher policy.

Acceptance: stopping is effective in every conversation phase within the approved target; no late response revives a stopped conversation.

### P1-08 — Add silence reminders and spoken controls

Dependencies: P1-07; silence/reminder policy from P1-02.

- [ ] Start the silence timer only when waiting for a user utterance, not during network processing or playback.
- [ ] Play “Do you need any assistance from me?” after the approved interval, honoring reminder limits and subsequent behavior.
- [ ] Reset or cancel timers on speech, stop, restart, and mode transitions.
- [ ] Provide an instructional MP3 explaining single-press stop/restart and the shared control scheme without claiming unfinished modes are usable.
- [ ] Decide when instructions play and verify they do not block button handling or contaminate microphone input.

Acceptance: reminders occur only in valid listening states and cease after stopping; instructions match the agreed controls.

### P1-09 — Recover from failures without unsafe repetition

Dependencies: P1-07 and P1-08.

- [ ] Apply bounded timeouts to external commands, network requests, recording, and playback as appropriate.
- [ ] Define limited retries for safe operations, avoiding duplicate capture, duplicate paid requests, or repeated playback.
- [ ] Test network loss, authentication failure, rate limiting, malformed decisions, server errors, invalid MP3, audio-device loss, and insufficient storage.
- [ ] Provide a truthful local failure prompt where audio works, with diagnostic logging that does not retain secrets or unnecessary user media.
- [ ] Define media retention/cleanup and verify cleanup after success, cancellation, failure, and service restart.
- [ ] Verify reconnection or service recovery does not replay old responses or automatically resume a stopped conversation.

Acceptance: each tested failure reaches a defined recoverable or stopped state, with bounded resource usage and no misleading success report.

### P1-10 — Verify on the Pi and prepare the implementation handoff

Dependencies: P1-09.

- [ ] Run shell syntax checks and an available shell linter; test any retained Python components with the project's verified test tooling.
- [ ] Test protocol validation, state transitions, cancellation, and press timing deterministically, including interval boundary cases.
- [ ] Run real-device checks for sensors, microphone, speaker/amplifier, Bluetooth status, camera, and button events. Keep manual observations distinct from automated results.
- [ ] Exercise both live conversation branches, consecutive turns, silence handling, startup failures, and stop/restart during each phase.
- [ ] Measure button-to-stop, end-of-speech-to-transcript, decision time, capture/upload time, answer/TTS time, and time-to-playback. Separate network/provider delay from local process overhead.
- [ ] Compare shell/native and persistent-worker alternatives for actual bottlenecks; reject changes that reduce reliability without demonstrated latency benefit.
- [ ] Check resource use and cleanup across repeated turns and the agreed operating duration.
- [ ] Document installation, service startup/shutdown, configuration, verified commands, recovery, and known limitations without including credentials.
- [ ] Record test evidence and unverified items; do not mark Pi hardware acceptance complete from host-only or mocked tests.

Acceptance: the agreed Part 1 requirements have traceable test results on the target Pi, or explicit blockers; Part 2 and navigation remain separate and the original plan is unchanged.
