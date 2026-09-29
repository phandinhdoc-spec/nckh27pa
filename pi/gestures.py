"""Shared button gesture and Plan 1 state machine; no GPIO ownership here."""
from __future__ import annotations

import threading
import time
from collections.abc import Callable
from typing import Protocol

WINDOW_SECONDS = 1.5
DEBOUNCE_SECONDS = 0.06


class TimerLike(Protocol):
    daemon: bool

    def start(self) -> None: ...
    def cancel(self) -> None: ...


class GestureClassifier:
    """Emit one count after a quiet window; timestamps use a monotonic clock."""

    def __init__(self, emit: Callable[[int], None], *, clock: Callable[[], float] = time.monotonic,
                 timer_factory: Callable[..., TimerLike] = threading.Timer) -> None:
        self._emit = emit
        self._clock = clock
        self._timer_factory = timer_factory
        self._lock = threading.Lock()
        self._last: float | None = None
        self._count = 0
        self._generation = 0
        self._timer: TimerLike | None = None
        self._closed = False

    def press(self, when: float | None = None) -> None:
        now = self._clock() if when is None else when
        previous = 0
        with self._lock:
            if self._closed or (self._last is not None and now - self._last < DEBOUNCE_SECONDS):
                return
            if self._last is not None and now - self._last > WINDOW_SECONDS:
                previous, self._count = self._count, 0
            self._count += 1
            self._last = now
            self._generation += 1
            if self._timer is not None:
                self._timer.cancel()
            generation = self._generation
            self._timer = self._timer_factory(WINDOW_SECONDS + 0.01, self._flush, args=(generation,))
            self._timer.daemon = True
            self._timer.start()
        if previous:
            self._emit(previous)

    def _flush(self, generation: int) -> None:
        with self._lock:
            if self._closed or generation != self._generation:
                return
            count, self._count = self._count, 0
            self._last = None
            self._timer = None
        if count:
            self._emit(count)

    def close(self) -> None:
        with self._lock:
            self._closed = True
            self._generation += 1
            if self._timer is not None:
                self._timer.cancel()
            self._timer = None
            self._count = 0


class ModeState:
    """One press toggles Plan 1; multi-press gestures do nothing until implemented."""

    def __init__(self) -> None:
        self.plan1_active = True
        self.generation = 0

    def gesture(self, count: int) -> str:
        if count == 1:
            self.plan1_active = not self.plan1_active
            self.generation += 1
            return "plan1_started" if self.plan1_active else "plan1_stopped"
        if count == 2:
            return "reserved_plan2"
        return "reserved_plan3" if count >= 3 else "invalid"
