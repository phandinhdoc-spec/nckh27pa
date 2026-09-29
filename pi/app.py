"""Pi Zero 2 W runtime for the three plans.

Plan 3 only sends experimental frames; the server intentionally returns no
navigation advice until vision and geometry have been validated.
"""
from __future__ import annotations

import json
import os
import queue
import signal
import sys
import threading
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from urllib.request import Request, urlopen

from audio import VoiceStream, mp3_from_pcm, play_mp3
from camera import capture_jpeg, MJPEGCamera
from client import Plan1Client
from control import Controller, Decision
from gestures import GestureClassifier
from navigation import FrameSender, StreamFrameSender, NavigationError, run_frames
from navigation_sensors import GY53FixedAngle
from plan2_client import Plan2Client
from stt import transcribe

ROOT = Path(__file__).resolve().parent


def _read_settings(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    result = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip().strip('"\'')
    return result


@dataclass(frozen=True)
class Config:
    server_url: str
    token: str
    groq_key: str
    stt_backend: str
    mic_device: str
    speaker_device: str

    @classmethod
    def load(cls) -> "Config":
        values = _read_settings(ROOT / ".env")
        values.update({k: v for k, v in _read_settings(ROOT / ".api").items() if v})
        values.update(os.environ)
        url, token = values.get("SERVER_URL", ""), values.get("SERVER_TOKEN", "")
        if not url.startswith(("http://", "https://")) or not token:
            raise ValueError("Cần SERVER_URL và SERVER_TOKEN trong .env của Pi.")
        backend = values.get("STT_BACKEND", "server").lower()
        if backend not in {"server", "groq"}:
            raise ValueError("STT_BACKEND phải là server hoặc groq.")
        return cls(url.rstrip("/"), token, values.get("GROQ_API_KEY", ""), backend,
                   values.get("MIC_DEVICE", "plughw:CARD=Device,DEV=0"),
                   values.get("SPEAKER_DEVICE", "plughw:CARD=MAX98357A,DEV=0"))


class Runtime:
    def __init__(self, config: Config) -> None:
        self.config = config
        self.controller = Controller(allow_experimental_plan3=True)
        self.plan1 = Plan1Client(config.server_url, config.token)
        self.plan2 = Plan2Client(config.server_url, config.token)
        self.session_id = str(uuid.uuid4())
        self.events: queue.Queue[tuple[str, object]] = queue.Queue(maxsize=3)
        self.stop = threading.Event()
        self.speaking = threading.Event()
        self.nav_stop = threading.Event()
        self.nav_thread: threading.Thread | None = None
        self.nav_lock = threading.Lock()
        self.mic_connected = False
        self.last_mic_warning = float("-inf")
        self.voice = VoiceStream(config.mic_device, on_health=self._mic_health)
        self.button = None
        self.gestures = GestureClassifier(self._button_gesture)

    def _enqueue(self, kind: str, value: object) -> None:
        try:
            self.events.put_nowait((kind, value))
        except queue.Full:
            if kind == "decision":
                try:
                    self.events.get_nowait()
                    self.events.put_nowait((kind, value))
                except queue.Empty:
                    pass

    def _button_gesture(self, count: int) -> None:
        decision = self.controller.gesture(count)
        print(f"[BUTTON] count={count} mode={self.controller.mode} action={decision.action}", flush=True)
        self._sync_navigation()
        self._enqueue("decision", decision)

    def _sync_navigation(self) -> None:
        with self.nav_lock:
            if self.controller.mode == "plan3":
                if self.nav_thread is None or not self.nav_thread.is_alive():
                    self.nav_stop = threading.Event()
                    self.nav_thread = threading.Thread(target=self._navigation_loop, daemon=True)
                    self.nav_thread.start()
            else:
                self.nav_stop.set()

    def _navigation_loop(self) -> None:
        try:
            sensor = GY53FixedAngle()
            sender = StreamFrameSender(self.config.server_url, self.config.token)
            previous = None
            last_alert = float("-inf")

            def report(result: dict | NavigationError) -> None:
                nonlocal previous, last_alert
                status = str(result) if isinstance(result, Exception) else result.get("status")
                if status != previous:
                    print(f"[PLAN3] {status}", flush=True)
                    previous = status
                if (isinstance(result, dict) and result.get("alert") == "CENTER_OBSTACLE"
                        and not self.nav_stop.is_set() and self.controller.mode == "plan3"
                        and time.monotonic() - last_alert >= 5):
                    try:
                        self.events.put_nowait(("obstacle", (self.controller.generation, time.monotonic())))
                        last_alert = time.monotonic()
                    except queue.Full:
                        pass

            try:
                with MJPEGCamera() as camera:
                    run_frames(sender, sensor.sample, self.nav_stop.is_set, report,
                               capture=camera.capture)
            finally:
                sensor.close()
        except Exception as error:
            print(f"[PLAN3] Không chạy được: {error}", file=sys.stderr, flush=True)

    def _mic_health(self, connected: bool) -> None:
        was_connected = self.mic_connected
        self.mic_connected = connected
        if connected:
            if not was_connected:
                print("[MIC] Đã nhận dữ liệu micro.", flush=True)
            return
        if self.stop.is_set():
            return
        now = time.monotonic()
        if now - self.last_mic_warning >= 60:
            try:
                self.events.put_nowait(("mic_error", None))
            except queue.Full:
                return
            self.last_mic_warning = now

    def _read_mic(self) -> None:
        for pcm in self.voice.segments():
            if self.stop.is_set():
                break
            if not self.speaking.is_set():
                self._enqueue("pcm", pcm)

    def _transcribe(self, mp3: bytes) -> str:
        if self.config.stt_backend == "groq":
            return transcribe(mp3, self.config.groq_key, "whisper-large-v3-turbo", 30)
        request = Request(self.config.server_url + "/stt", method="POST", data=mp3,
                          headers={"Authorization": "Bearer " + self.config.token,
                                   "Content-Type": "audio/mpeg"})
        with urlopen(request, timeout=45) as response:
            if response.headers.get_content_type() != "application/json":
                raise RuntimeError("Server STT trả sai định dạng.")
            result = json.loads(response.read(8192))
            if not isinstance(result, dict) or not isinstance(result.get("text"), str):
                raise RuntimeError("Server STT không trả văn bản.")
            return result["text"].strip()

    def _tts(self, speech: str) -> bytes:
        request = Request(self.config.server_url + "/tts", method="POST",
                          data=json.dumps({"text": speech}, ensure_ascii=False).encode("utf-8"),
                          headers={"Authorization": "Bearer " + self.config.token,
                                   "Content-Type": "application/json"})
        with urlopen(request, timeout=30) as response:
            if response.headers.get_content_type() != "audio/mpeg":
                raise RuntimeError("Server không trả MP3.")
            audio = response.read(10 * 1024 * 1024 + 1)
            if not audio or len(audio) > 10 * 1024 * 1024:
                raise RuntimeError("MP3 thông báo trống hoặc quá lớn.")
            return audio

    def _play(self, data: bytes, generation: int) -> None:
        if self.controller.generation != generation:
            return
        self.speaking.set()
        try:
            play_mp3(data, self.config.speaker_device,
                     cancelled=lambda: self.stop.is_set() or self.controller.generation != generation)
        finally:
            self.speaking.clear()

    def _announce(self, decision: Decision) -> None:
        if not decision.speech:
            return
        generation = self.controller.generation
        print(f"[MODE] {decision.speech}", flush=True)
        try:
            self._play(self._tts(decision.speech), generation)
        except Exception as error:
            print(f"[AUDIO] {error}", file=sys.stderr, flush=True)
        finally:
            if decision.action == "navigation_confirmation":
                self.controller.confirmation_prompt_finished(generation)

    def _handle_speech(self, pcm: bytes) -> None:
        generation = self.controller.generation
        mp3 = mp3_from_pcm(pcm)
        text = self._transcribe(mp3)
        if self.controller.generation != generation:
            return
        decision = self.controller.transcript(text)
        self._sync_navigation()
        if decision.action != "request":
            self._announce(decision)
            return
        generation = self.controller.generation
        turn_id = str(uuid.uuid4())
        if self.controller.mode == "plan1":
            audio, _ = self.plan1.turn(text, self.session_id, turn_id)
            if self.controller.generation != generation:
                self.plan1.cancel(self.session_id, turn_id)
                return
        elif self.controller.mode == "plan2":
            result = self.plan2.turn(text, self.session_id, turn_id)
            if self.controller.generation != generation:
                self.plan2.cancel(self.session_id, turn_id)
                return
            if isinstance(result, dict):
                image = capture_jpeg()
                if self.controller.generation != generation:
                    self.plan2.cancel(self.session_id, turn_id)
                    return
                audio = self.plan2.capture(image, self.session_id, turn_id)
            else:
                audio = result
        else:
            return
        self._play(audio, generation)

    def run(self) -> None:
        from gpiozero import Button

        signal.signal(signal.SIGTERM, lambda *_: self.stop.set())
        self.button = Button(25, pull_up=True, bounce_time=0.06)
        self.button.when_pressed = lambda: self.gestures.press()
        try:
            print("[BOOT] Đang phát lời chào trên loa MAX98357A.", flush=True)
            self._play((ROOT / "assets" / "startup.mp3").read_bytes(), self.controller.generation)
            print("[BOOT] Đã hoàn tất phát lời chào.", flush=True)
        except Exception as error:
            print(f"[AUDIO] Lời chào khởi động: {error}", file=sys.stderr, flush=True)
        reader = threading.Thread(target=self._read_mic, daemon=True)
        reader.start()
        print("[READY] Plan 1/2; nhấn đôi đổi chế độ, nhấn ba lần rồi xác nhận để thử Plan 3.", flush=True)
        try:
            while not self.stop.is_set():
                try:
                    kind, value = self.events.get(timeout=0.2)
                except queue.Empty:
                    continue
                try:
                    if kind == "decision":
                        self._announce(value)
                    elif kind == "obstacle":
                        generation, detected_at = value
                        if (self.controller.mode == "plan3" and self.controller.generation == generation
                                and time.monotonic() - detected_at < 3):
                            print("[PLAN3 ALERT] Có vật cản phía trước, tại tâm ảnh.", flush=True)
                            self._play((ROOT / "assets" / "center-obstacle.mp3").read_bytes(), generation)
                            print("[PLAN3 ALERT] Đã hoàn tất phát cảnh báo.", flush=True)
                    elif kind == "mic_error":
                        if not self.mic_connected:
                            print("[MIC WARNING] Bạn hãy chỉnh lại micro, micro đang không kết nối được", flush=True)
                            self._play((ROOT / "assets" / "mic-disconnected.mp3").read_bytes(),
                                       self.controller.generation)
                            print("[MIC WARNING] Đã hoàn tất phát thông báo.", flush=True)
                    else:
                        self._handle_speech(value)
                except Exception as error:
                    print(f"[TASK] {type(error).__name__}: {error}", file=sys.stderr, flush=True)
        finally:
            self.nav_stop.set()
            if self.nav_thread is not None:
                self.nav_thread.join(timeout=3)
            self.gestures.close()
            if self.button is not None:
                self.button.close()
            self.voice.close()


def main() -> int:
    try:
        config = Config.load()
    except ValueError as error:
        print(f"[CONFIG] {error}", file=sys.stderr, flush=True)
        return 1
    if config.stt_backend == "groq" and not config.groq_key:
        print(f"[CONFIG] Thiếu GROQ_API_KEY trong {ROOT / '.api'}", file=sys.stderr, flush=True)
        return 1
    Runtime(config).run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
