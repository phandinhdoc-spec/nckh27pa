"""Live GPIO25 validation for the new mode arbiter; stdin simulates STT only."""
from __future__ import annotations

import signal
import sys
import threading

from control import Controller
from gestures import GestureClassifier


def main() -> None:
    from gpiozero import Button

    stop = threading.Event()
    state = Controller()

    def gesture(count: int) -> None:
        action = state.gesture(count)
        print(f"GPIO25 presses={count} action={action.action} mode={state.mode} prompt={action.speech}", flush=True)

    classifier = GestureClassifier(gesture)
    button = Button(25, pull_up=True, bounce_time=None)
    button.when_pressed = classifier.press
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    print("GPIO25 ready: press 1, 2, or 3 times; type transcript + Enter to simulate STT", flush=True)
    try:
        while not stop.is_set():
            line = sys.stdin.readline()
            if not line:
                stop.wait(0.2)
                continue
            decision = state.transcript(line)
            print(f"VOICE action={decision.action} mode={state.mode} prompt={decision.speech}", flush=True)
    finally:
        button.when_pressed = None
        button.close()
        classifier.close()


if __name__ == "__main__":
    main()
