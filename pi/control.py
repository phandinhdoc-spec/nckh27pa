"""One mode arbiter for GPIO gestures and Vietnamese spoken controls."""
from __future__ import annotations

import threading
import time
import unicodedata
from dataclasses import dataclass
from typing import Callable

CONFIRM_SECONDS = 15.0


@dataclass(frozen=True)
class Decision:
    action: str
    speech: str = ""
    cancel_current: bool = False


def _fold(text: str) -> str:
    text = text.replace("đ", "d").replace("Đ", "D")
    return "".join(char for char in unicodedata.normalize("NFD", text.casefold())
                   if unicodedata.category(char) != "Mn").strip(" .,!?:;")


class Controller:
    """Thread-safe mode decisions; device I/O stays behind the caller's seam."""

    def __init__(self, clock: Callable[[], float] = time.monotonic,
                 allow_experimental_plan3: bool = False) -> None:
        self._clock = clock
        self._lock = threading.RLock()
        self.mode = "plan1"
        self.plan1_active = True
        self.generation = 0
        self._navigation_deadline: float | None = None
        self.allow_experimental_plan3 = allow_experimental_plan3

    def gesture(self, count: int) -> Decision:
        with self._lock:
            if count == 1 and self._navigation_deadline is not None:
                return self.transcript("Có")
            if count == 3:
                return self._ask_navigation()
            if count == 2:
                self._navigation_deadline = None
                self._invalidate()
                self.mode = "plan1" if self.mode in {"plan2", "plan3"} else "plan2"
                self.plan1_active = True
                speech = ("Đã vào chế độ tìm đồ vật và đọc trang. Bạn cần tôi giúp gì?"
                          if self.mode == "plan2" else "Đã về chế độ hội thoại. Tôi có thể giúp gì cho bạn?")
                return Decision("mode_changed", speech, True)
            if count == 1:
                self._navigation_deadline = None
                self._invalidate()
                if self.mode == "plan3":
                    self.mode = "plan1"
                    self.plan1_active = True
                    return Decision("navigation_stopped", "Đã dừng chế độ thử nghiệm Plan 3.", True)
                if self.mode == "plan2":
                    return Decision("interrupted", "Đã ngắt câu trả lời. Tôi vẫn đang chờ yêu cầu.", True)
                self.plan1_active = not self.plan1_active
                if self.plan1_active:
                    return Decision("plan1_started", "Tôi có thể giúp gì cho bạn?", True)
                return Decision("plan1_stopped", "Đã dừng hội thoại.", True)
            return Decision("reserved")

    def transcript(self, text: str) -> Decision:
        plain = _fold(text)
        if not plain:
            return Decision("ignored")
        with self._lock:
            if self._navigation_deadline is not None:
                expired = self._clock() > self._navigation_deadline
                if expired:
                    self._navigation_deadline = None
                    return Decision("confirmation_expired", "Yêu cầu hướng dẫn đi đường đã hết thời gian xác nhận.")
                if plain in {"co", "dong y", "bat di", "muon", "vâng", "vang"}:
                    self._navigation_deadline = None
                    if self.allow_experimental_plan3:
                        self._invalidate()
                        self.mode = "plan3"
                        return Decision("navigation_started",
                                        "Đã bật Plan 3 thử nghiệm. Chưa có hướng dẫn đi đường.", True)
                    return Decision("navigation_unavailable",
                                    "Chế độ hướng dẫn đi đường chưa được kiểm chứng, hiện chưa thể bật. Xin đừng dựa vào thiết bị để đi đường.")
                if plain in {"khong", "thoi", "huy", "khong can"}:
                    self._navigation_deadline = None
                    return Decision("navigation_declined", "Đã hủy yêu cầu hướng dẫn đi đường.")
                return Decision("navigation_unclear", "Tôi chưa nghe rõ. Bạn có muốn bật hướng dẫn đi đường không? Hiện tính năng này chưa sẵn sàng.")
            if plain in {"dung", "dung lai", "ngung", "huy tac vu", "im lang"}:
                self._invalidate()
                if self.mode == "plan3":
                    self.mode = "plan1"
                    self.plan1_active = True
                    return Decision("navigation_stopped", "Đã dừng chế độ thử nghiệm Plan 3.", True)
                return Decision("interrupted", "Đã dừng tác vụ hiện tại.", True)
            if any(phrase in plain for phrase in ("bat che do di duong", "huong dan di duong", "dan duong cho toi")):
                return self._ask_navigation()
            if any(phrase in plain for phrase in ("bat che do tim do", "mo che do tim do", "bat che do camera", "bat che do doc trang")):
                if self.mode == "plan2":
                    return Decision("already_active", "Chế độ tìm đồ vật và đọc trang đang bật.")
                self._invalidate()
                self.mode = "plan2"
                self.plan1_active = True
                return Decision("mode_changed", "Đã vào chế độ tìm đồ vật và đọc trang.", True)
            if any(phrase in plain for phrase in ("tat che do tim do", "thoat che do tim do", "ve che do hoi thoai")):
                if self.mode == "plan1":
                    return Decision("already_active", "Chế độ hội thoại đang bật.")
                self._invalidate()
                self.mode = "plan1"
                self.plan1_active = True
                return Decision("mode_changed", "Đã về chế độ hội thoại.", True)
            if self.mode == "plan1" and not self.plan1_active:
                return Decision("ignored")
            if self.mode == "plan3":
                return Decision("navigation_running")
            return Decision("request")

    def confirmation_prompt_finished(self, generation: int) -> None:
        with self._lock:
            if self.generation == generation and self._navigation_deadline is not None:
                self._navigation_deadline = self._clock() + CONFIRM_SECONDS

    def _ask_navigation(self) -> Decision:
        self._invalidate()
        self._navigation_deadline = self._clock() + CONFIRM_SECONDS
        prompt = ("Bạn có muốn bật Plan 3 thử nghiệm không? Nói có hoặc bấm nút một lần để đồng ý. Chế độ này chưa đưa ra hướng dẫn đi đường."
                  if self.allow_experimental_plan3 else
                  "Bạn có muốn bật chế độ hướng dẫn đi đường không? Hiện tính năng này chưa sẵn sàng và không thể chỉ đường an toàn.")
        return Decision("navigation_confirmation", prompt, True)

    def _invalidate(self) -> None:
        self.generation += 1
