from __future__ import annotations

import threading
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from button_runtime import ButtonClassifier, Sessions
import part2_runtime


class GestureTests(unittest.TestCase):
    def test_debounce_boundary_triple_and_toggle(self):
        events = []
        button = ButtonClassifier(events.append)
        try:
            button.press(0)
            button.press(0.03)  # bounce
            button.press(1.5)  # inclusive boundary
            button.press(3.0)
            button._flush(button._generation)
            self.assertEqual(events, [3])
            sessions = Sessions()
            self.assertEqual(sessions.gesture(events[0]), "reserved")
            self.assertEqual(sessions.mode, "conversation")
            self.assertEqual(sessions.gesture(2), "visual")
            self.assertEqual(sessions.gesture(2), "conversation")
        finally:
            button.close()

    def test_single_interrupt_invalidates_only_visual_task(self):
        sessions = Sessions()
        sessions.gesture(2)
        token = sessions.begin()
        self.assertIsNotNone(token)
        self.assertEqual(sessions.gesture(1), "interrupted")
        self.assertFalse(sessions.valid(token))
        self.assertEqual(sessions.mode, "visual")
        self.assertIsNotNone(sessions.begin())
        sessions.gesture(2)
        self.assertEqual(sessions.mode, "conversation")


class VisualTests(unittest.TestCase):
    def test_direct_speech_one_capture_no_plan_and_no_history(self):
        sessions = Sessions()
        sessions.gesture(2)
        token = sessions.begin()
        config = SimpleNamespace(mic_sample_rate=16000, groq_api_key="key",
            groq_stt_model="model", groq_timeout_seconds=10,
            groq_stt_url="https://example.org", groq_language="vi",
            onboard_playback_device="speaker")
        calls = []
        client = SimpleNamespace(command=lambda *args: (calls.append(args) or b"audio", "answer"))
        with (patch.object(part2_runtime, "mp3_from_pcm", return_value=b"input"),
              patch.object(part2_runtime, "transcribe", return_value="Tìm chiếc cốc"),
              patch.object(part2_runtime, "play_mp3") as play):
            part2_runtime.visual_request(b"pcm", config, client, sessions, token,
                threading.Lock(), capture=lambda: b"\xff\xd8image")
        self.assertEqual(calls, [("Tìm chiếc cốc", b"\xff\xd8image", "observe")])
        play.assert_called_once()

    def test_late_response_after_exit_never_plays(self):
        sessions = Sessions()
        sessions.gesture(2)
        token = sessions.begin()
        config = SimpleNamespace(mic_sample_rate=16000, groq_api_key="key",
            groq_stt_model="model", groq_timeout_seconds=10,
            groq_stt_url="https://example.org", groq_language="vi",
            onboard_playback_device="speaker")
        def command(*args):
            sessions.gesture(2)
            return b"late", "late"
        with (patch.object(part2_runtime, "mp3_from_pcm", return_value=b"input"),
              patch.object(part2_runtime, "transcribe", return_value="Đọc trang"),
              patch.object(part2_runtime, "play_mp3") as play):
            part2_runtime.visual_request(b"pcm", config, SimpleNamespace(command=command),
                sessions, token, threading.Lock(), capture=lambda: b"image")
        play.assert_not_called()


if __name__ == "__main__":
    unittest.main()
