"""Shared GPIO25 gesture classification and cancellable session ownership."""
from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Callable

WINDOW = 1.5
DEBOUNCE = 0.06


class ButtonClassifier:
    """Release a gesture only after a full quiet window; equality joins a sequence."""

    def __init__(self, emit: Callable[[int], None], clock: Callable[[], float] = time.monotonic):
        self.emit = emit
        self.clock = clock
        self._lock = threading.Lock()
        self._timer: threading.Timer | None = None
        self._last: float | None = None
        self._count = 0
        self._generation = 0

    def press(self, when: float | None = None) -> None:
        now = self.clock() if when is None else when
        pending = None
        with self._lock:
            if self._last is not None and now - self._last < DEBOUNCE:
                return
            if self._last is not None and now - self._last > WINDOW:
                pending = self._count
                self._count = 0
            self._count += 1
            self._last = now
            self._generation += 1
            generation = self._generation
            if self._timer:
                self._timer.cancel()
            # Tiny scheduling margin lets an edge timestamped exactly at WINDOW
            # join before the timer callback competes for the lock.
            self._timer = threading.Timer(WINDOW + 0.01, self._flush, args=(generation,))
            self._timer.daemon = True
            self._timer.start()
        if pending:
            self.emit(pending)

    def _flush(self, generation: int) -> None:
        with self._lock:
            if generation != self._generation:
                return
            count, self._count = self._count, 0
            self._last = None
            self._timer = None
        if count:
            self.emit(count)

    def close(self) -> None:
        with self._lock:
            self._generation += 1
            if self._timer:
                self._timer.cancel()
            self._timer = None
            self._count = 0


class GPIOButton:
    """One GPIO25 input, BCM numbering, active-low pull-up."""

    def __init__(self, callback: Callable[[], None]):
        from gpiozero import Button
        self._button = Button(25, pull_up=True, bounce_time=None)
        self._button.when_pressed = callback

    def close(self) -> None:
        self._button.when_pressed = None
        self._button.close()


@dataclass
class TaskToken:
    session: int
    task: int
    cancelled: threading.Event = field(default_factory=threading.Event)


class Sessions:
    """Atomic admission and invalidation; never resume a cancelled task."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self.mode = "conversation"
        self._session = 0
        self._task = 0
        self._current: TaskToken | None = None

    def gesture(self, count: int) -> str:
        with self._lock:
            if count == 2:
                self._invalidate()
                self._session += 1
                self.mode = "visual" if self.mode == "conversation" else "conversation"
                return self.mode
            if count == 1 and self.mode == "visual":
                self._invalidate()
                return "interrupted"
            return "reserved" if count >= 3 else "single"

    def _invalidate(self) -> None:
        if self._current:
            self._current.cancelled.set()
            self._current = None

    def begin(self) -> TaskToken | None:
        with self._lock:
            if self._current is not None:
                return None
            self._task += 1
            token = TaskToken(self._session, self._task)
            self._current = token
            return token

    def valid(self, token: TaskToken) -> bool:
        with self._lock:
            return self._current is token and not token.cancelled.is_set() and token.session == self._session

    def finish(self, token: TaskToken) -> None:
        with self._lock:
            if self._current is token:
                self._current = None

    def stop(self) -> None:
        with self._lock:
            self._invalidate()
            self._session += 1
