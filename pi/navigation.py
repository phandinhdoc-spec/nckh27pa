"""Plan 3 frame transport: one frame at a time, no CAPTUREX or backlog.

Experimental until server vision and geometry are validated outdoors. Alerts are
fail-closed by default; this module does not assert the road is safe.
"""
from __future__ import annotations

import base64
import json
import math
import time
import uuid
from dataclasses import dataclass
from typing import Callable, Protocol
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from camera import capture_jpeg

FIXED_ALPHA_DEG = 45.0
FIXED_PITCH_DEG = 90.0 - FIXED_ALPHA_DEG


@dataclass(frozen=True)
class SensorSample:
    distance_m: float


class NavigationError(RuntimeError):
    pass


class NavigationSender(Protocol):
    def send(self, image: bytes, sensors: SensorSample, captured_at: float) -> dict: ...


class FrameSender:
    def __init__(self, url: str, token: str, *, timeout: float = 2.0,
                 allow_alerts: bool = False) -> None:
        if not url.startswith(("http://", "https://")) or not token:
            raise ValueError("Cần URL server và token.")
        self.url = url.rstrip("/") + "/plan3/analyze"
        self.token = token
        self.timeout = timeout
        self.allow_alerts = allow_alerts
        self.session_id = str(uuid.uuid4())
        self.frame_id = 0

    def send(self, image: bytes, sensors: SensorSample, captured_at: float) -> dict:
        if not math.isfinite(sensors.distance_m) or sensors.distance_m <= 0:
            raise NavigationError("Dữ liệu GY53 không hợp lệ.")
        self.frame_id += 1
        frame_id = self.frame_id
        payload = {"sessionId": self.session_id, "frameId": frame_id,
                   "capturedAt": captured_at, "pitchDeg": FIXED_PITCH_DEG,
                   "distanceM": sensors.distance_m, "image": base64.b64encode(image).decode("ascii"),
                   "imageContentType": "image/jpeg"}
        request = Request(self.url, method="POST", data=json.dumps(payload).encode("utf-8"),
                          headers={"Authorization": "Bearer " + self.token,
                                   "Content-Type": "application/json"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                if response.headers.get_content_type() != "application/json":
                    raise NavigationError("Server trả sai định dạng Plan 3.")
                raw = response.read(64 * 1024 + 1)
                if len(raw) > 64 * 1024:
                    raise NavigationError("Phản hồi Plan 3 quá lớn.")
                result = json.loads(raw)
        except (HTTPError, URLError, TimeoutError, json.JSONDecodeError) as error:
            raise NavigationError("Không thể kiểm tra vật cản qua server.") from error
        if not isinstance(result, dict) or result.get("sessionId") != self.session_id or result.get("frameId") != frame_id:
            raise NavigationError("Phản hồi Plan 3 không khớp phiên hoặc ảnh.")
        if result.get("alert") == "SOS" and not self.allow_alerts:
            raise NavigationError("Cảnh báo SOS chưa được hiệu chuẩn để sử dụng.")
        return result


class StreamFrameSender(FrameSender):
    """Upload immediately; consume each completed server analysis only once."""
    def __init__(self, url, token):
        super().__init__(url, token, timeout=2)
        self.url += "?stream=1"
        self.last_result_id = 0

    def send(self, image, sensors, captured_at):
        ack = super().send(image, sensors, captured_at)
        result = ack.get("analysis")
        if (isinstance(result, dict) and result.get("sessionId") == self.session_id
                and isinstance(result.get("frameId"), int)
                and self.last_result_id < result["frameId"] <= self.frame_id):
            self.last_result_id = result["frameId"]
            if 0 <= time.time() - result.get("capturedAt", 0) <= 15:
                return result
        return {"status": "streaming", "alert": None}


def run_frames(sender: NavigationSender, sensors: Callable[[], SensorSample],
               stop: Callable[[], bool], report: Callable[[dict | NavigationError], None],
               capture: Callable[[], bytes] = capture_jpeg,
               clock: Callable[[], float] = time.monotonic,
               sleep: Callable[[float], None] = time.sleep) -> None:
    """Capture/upload every 500 ms; skip missed slots instead of building a queue."""
    next_frame = clock()
    while not stop():
        now = clock()
        if now < next_frame:
            sleep(min(next_frame - now, 0.1))
            continue
        next_frame = max(next_frame + 0.5, now)
        try:
            sample = sensors()
            image = capture()
            report(sender.send(image, sample, time.time()))
        except NavigationError as error:
            report(error)
        except Exception as error:
            report(NavigationError(f"Camera hoặc cảm biến lỗi: {type(error).__name__}"))
