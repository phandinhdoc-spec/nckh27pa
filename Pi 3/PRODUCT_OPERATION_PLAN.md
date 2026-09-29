# Product operation plan (draft)

Status: **Incomplete.** This records the proposed behavior through the first description of the road navigation algorithm. It is a planning document, not a claim that the software already implements this behavior. Review, counterarguments, and final decisions remain pending.

## 1. First startup and hardware status

On the machine's first boot, check each of these components and tell the user whether it is usable and functioning:

- GY-53 sensor
- GY-25 sensor
- Bluetooth headphones
- Speaker
- MAX amplifier
- Microphone

The precise test for each component, how to report a failure when audio output is unavailable, and whether a failed check blocks startup are still to be decided.

After startup completes, say **“How may I help you?”** and begin listening.

## 2. Voice conversation: one turn

Voice control has the highest priority. The microphone records the user's utterance. The audio is sent to Groq and transcribed with **Whisper Large v3 Turbo**. The server receives the resulting text and uses Gemini to decide whether answering the request strictly requires a camera image.

### Camera required

1. Gemini returns the fixed decision code `CAPTUREX`.
2. The server immediately sends `CAPTUREX` to the Pi.
3. On receiving the code, the Pi activates the camera, captures a photo, and sends it to the server.
4. The server forwards the image to Gemini and receives an answer.
5. The server converts the answer to an MP3 file and sends it to the Pi.
6. The Pi plays the MP3 through the speaker.

### Camera not required

1. Gemini returns the fixed decision code `NOCAPTUREX`.
2. The server keeps this code internal; it does not send it to the Pi.
3. The server asks Gemini to answer the user's question using the transcript.
4. The server converts the answer to an MP3 file and sends it to the Pi.
5. The Pi plays the MP3 through the speaker.

Playback of the answer ends one dialogue turn. The microphone then listens for another utterance, repeating this cycle. If the user does not respond, play the MP3 prompt **“Do you need any assistance from me?”** The silence interval, number of reminders, and behavior after further silence are not yet specified.

## 3. Button instructions and modes

Provide an instructional MP3 explaining these controls:

| Button action | Proposed behavior |
| --- | --- |
| One press during a conversation | Stop the conversation. |
| One press after stopping | Start a new conversation. |
| Two consecutive presses | Enter object finding / page or manual reading mode. A second double press turns this mode off. |
| Three consecutive presses | Enter road navigation and obstacle avoidance guidance mode. A second triple press turns this mode off. |

Allow up to **1.5 seconds between consecutive presses**. The handling of a pending single press while waiting to see whether it becomes a double or triple press, and interactions between modes, still need definition.

Gemini handles requests to locate objects and read pages or manuals. The prompts for these tasks require careful design and review.

## 4. Road navigation guidance (unfinished)

Use a dedicated navigation algorithm file. Gemini may contribute to this mode, but navigation must not rely solely on Gemini.

The proposed image processing inputs and outputs so far are:

- Capture camera images at **640 × 640 pixels**.
- Detect objects in each image and draw bounding boxes.
- Determine whether the base of each detected object rests on the road surface.
- Determine the pixel coordinates of each object in the image.

### Camera geometry in `so-do-khoang-cach-camera.svg`

- O is the camera position. OG is the camera's principal axis; G lies on the road surface and corresponds to the point described as `(320, 320)` in the captured image.
- H is the wearer's foot, the perpendicular projection of O onto the road surface. The angle HOG is the camera tilt angle, measured by the GY-25.
- N is the bottom point of the image, centered horizontally, described as `(640, 320)`. M is the top point, described as `(0, 320)`. N and M are points in the actual image, not merely diagram markers.
- The angles NOG and GOM each represent half the camera's field of view, subject to the stated technical limitations.
- T is an object, or the base of an obstacle on the road, for which the user needs an alert.
- The distance OG is measured with the GY-53.

Coordinate convention needs confirmation before implementing calculations: the given coordinates for N and M treat the first component as the vertical position, unlike the usual `(x, y)` image convention. Preserve the supplied values until the axis order is confirmed.

The user has not yet described how these detections become a route or spoken guidance, how road surface is established, how distance and motion are estimated, or how uncertainty and immediate hazards are handled. Continue this section when the remaining algorithm is provided.

## 5. Review pending

Once the full process has been described, review the architecture, timing, failure paths, button behavior, camera decision, navigation safety, and Gemini prompts; raise counterarguments; then agree on a final plan. The current repository README describes some different behavior, so implementation work will also need an explicit reconciliation with the existing system.
