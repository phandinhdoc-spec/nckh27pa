"""Part 2 visual requests. Host-tested orchestration, not hardware validation."""
from __future__ import annotations

import queue
import sys
import threading
from typing import Callable

from button_runtime import ButtonClassifier, GPIOButton, Sessions, TaskToken
from devices import DeviceError, VoiceStream, capture_jpeg, mp3_from_pcm, play_mp3
from groq_stt import transcribe
from mac_client import MacClient


def visual_request(pcm: bytes, config, client: MacClient, sessions: Sessions,
                   token: TaskToken, speaker: threading.Lock,
                   capture: Callable[[], bytes] = capture_jpeg) -> None:
    """One utterance -> one image -> one response; never persist media locally."""
    if not sessions.valid(token):
        return
    mp3 = mp3_from_pcm(pcm, config.mic_sample_rate)
    if not sessions.valid(token):
        return
    text = transcribe(mp3, config.groq_api_key, config.groq_stt_model,
                      config.groq_timeout_seconds, url=config.groq_stt_url,
                      language=config.groq_language).strip()
    if not text or not sessions.valid(token):
        return
    # The Part 2 request is direct speech, never routed through /plan or wake.
    # Capture exactly once only after a nonempty request.
    image = capture()
    if not sessions.valid(token):
        return
    audio, _ = client.command(text, image, "observe")
    if not sessions.valid(token):
        return
    with speaker:
        if sessions.valid(token):
            play_mp3(audio, config.onboard_playback_device,
                     cancelled=lambda: not sessions.valid(token))


class Runtime:
    """One button owner and one mic reader, with worker isolation for blocking I/O."""

    def __init__(self, config, client: MacClient, stream: VoiceStream,
                 conversation: Callable[[bytes, Callable[[], bool], threading.Lock], None],
                 button_factory=GPIOButton):
        self.config, self.client, self.stream = config, client, stream
        self.conversation = conversation
        self.sessions = Sessions()
        self.speaker = threading.Lock()
        self.incoming: queue.Queue[bytes] = queue.Queue(maxsize=1)
        self.stop_event = threading.Event()
        self.classifier = ButtonClassifier(self._gesture)
        self.button = button_factory(self.classifier.press)
        self.reader: threading.Thread | None = None

    def _gesture(self, count: int) -> None:
        state = self.sessions.gesture(count)
        print(f"[GPIO25] {count}: {state}", flush=True)
        try:
            while True:
                self.incoming.get_nowait()
        except queue.Empty:
            pass

    def _listen(self) -> None:
        try:
            for pcm in self.stream.segments():
                if self.stop_event.is_set():
                    break
                # Do not enqueue an utterance completed during playback.
                # Acoustic echo before/after the lock still needs Pi testing.
                if self.speaker.locked():
                    continue
                try:
                    self.incoming.put_nowait(pcm)
                except queue.Full:
                    pass
        except (DeviceError, OSError) as error:
            print(f"[MIC] {error}", file=sys.stderr, flush=True)

    def run(self) -> None:
        self.reader = threading.Thread(target=self._listen, daemon=True)
        self.reader.start()
        try:
            while not self.stop_event.is_set():
                try:
                    pcm = self.incoming.get(timeout=0.2)
                except queue.Empty:
                    if not self.reader.is_alive():
                        break
                    continue
                token = self.sessions.begin()
                if token is None:
                    continue
                mode = self.sessions.mode
                threading.Thread(target=self._work, args=(pcm, token, mode), daemon=True).start()
        finally:
            self.close()

    def _work(self, pcm: bytes, token: TaskToken, mode: str) -> None:
        try:
            if mode == "visual":
                visual_request(pcm, self.config, self.client, self.sessions, token, self.speaker)
            else:
                self.conversation(pcm, lambda: self.sessions.valid(token), self.speaker)
        except Exception as error:
            if self.sessions.valid(token):
                print(f"[TASK] {error}", file=sys.stderr, flush=True)
        finally:
            self.sessions.finish(token)

    def close(self) -> None:
        self.stop_event.set()
        self.sessions.stop()
        self.classifier.close()
        self.button.close()
        self.stream.close()
