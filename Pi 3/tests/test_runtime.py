from __future__ import annotations

import os
import struct
import threading
import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from types import SimpleNamespace
from unittest.mock import patch

import app
import devices
from devices import VoiceStream, mp3_from_pcm
from mac_client import MacClient, MacClientError


def _chunk(value: int, seconds: float = 0.1, sample_rate: int = 16000) -> bytes:
    return struct.pack("<h", value) * int(sample_rate * seconds)


def _config(**overrides: object) -> SimpleNamespace:
    values = dict(
        server_url="https://mac.example",
        server_token="token",
        onboard_playback_device="hw:MAX,0",
        bluetooth_device_mac="04:60:61:EF:78:22",
        mic_device="default",
        mic_sample_rate=16000,
        silence_seconds=1,
        silence_rms=500,
        max_record_seconds=5,
        groq_api_key="groq-key",
        groq_stt_model="whisper-large-v3-turbo",
        groq_stt_url="https://api.groq.com/openai/v1/audio/transcriptions",
        groq_language="vi",
        groq_timeout_seconds=30,
        command_timeout_seconds=60,
        http_retries=0,
        cpu_idle_percent=10,
        ram_available_percent=10,
        gy25_device="",
        gy25_baud=115200,
        ms5611_bus=1,
        ms5611_addr=0x76,
        ms5611_enabled=False,
    )
    values.update(overrides)
    return SimpleNamespace(**values)


class _FakeProcess:
    def __init__(self, stdout: object) -> None:
        self.stdout = stdout
        self.returncode = 0
        self.terminated = False

    def poll(self) -> None:
        return None

    def terminate(self) -> None:
        self.terminated = True

    def kill(self) -> None:
        pass

    def communicate(self, timeout: int | None = None) -> tuple[bytes, bytes]:
        return (b"", b"")


class VoiceStreamTests(unittest.TestCase):
    def test_stream_keeps_one_arecord_open_and_cuts_voiced_utterance(self) -> None:
        pcm = _chunk(0) * 5 + _chunk(3000) * 3 + _chunk(0) * 15
        read_fd, write_fd = os.pipe()

        def feed() -> None:
            with os.fdopen(write_fd, "wb") as sink:
                sink.write(pcm)

        threading.Thread(target=feed, daemon=True).start()
        stdout = os.fdopen(read_fd, "rb")
        process = _FakeProcess(stdout)
        stream = VoiceStream("default", 16000, silence_seconds=1, max_seconds=5, silence_rms=500)

        with (
            patch.object(devices, "_command", side_effect=lambda name: f"/usr/bin/{name}"),
            patch.object(devices.subprocess, "Popen", return_value=process) as popen,
        ):
            generator = stream.segments()
            try:
                utterance = next(generator)
            finally:
                generator.close()
                stream.close()

        stdout.close()
        self.assertEqual(popen.call_count, 1)
        command = popen.call_args.args[0]
        self.assertEqual(command[0], "/usr/bin/arecord")
        self.assertIn("-t", command)
        self.assertEqual(command[command.index("-D") + 1], "default")
        # 3 chunk trigger (pre-roll) + 10 chunk trailing silence (1 giây).
        self.assertGreaterEqual(len(utterance), 13 * 3200)
        self.assertGreater(devices._pcm_rms(utterance), 500)
        self.assertTrue(process.terminated)

    def test_mp3_encoding_uses_lame_on_raw_pcm(self) -> None:
        completed = SimpleNamespace(returncode=0, stdout=b"ID3mp3", stderr=b"")
        with (
            patch.object(devices, "_command", return_value="/usr/bin/lame"),
            patch.object(devices.subprocess, "run", return_value=completed) as run,
        ):
            mp3 = mp3_from_pcm(b"\x00\x00" * 1600, 16000)

        self.assertEqual(mp3, b"ID3mp3")
        command = run.call_args.args[0]
        self.assertEqual(command[0], "/usr/bin/lame")
        self.assertIn("-r", command)
        self.assertEqual(command[command.index("-s") + 1], "16000")


class WakePhraseTests(unittest.TestCase):
    def test_wake_and_command_in_same_segment(self) -> None:
        self.assertEqual(app._split_wake_phrase("Thiên Nhãn mấy giờ rồi"), (True, "mấy giờ rồi"))

    def test_wake_alone_awaits_next_utterance(self) -> None:
        self.assertEqual(app._split_wake_phrase("thiên nhãn"), (True, ""))

    def test_wake_matching_ignores_case_and_diacritics(self) -> None:
        decomposed = "Thie\u0302n Nha\u0303n ba giờ"  # NFD: Thiên Nhãn ba giờ
        self.assertEqual(app._split_wake_phrase(decomposed), (True, "ba giờ"))
        self.assertEqual(app._split_wake_phrase("THIÊN NHÃN ơi"), (True, "ơi"))

    def test_command_without_wake_is_not_a_command(self) -> None:
        self.assertEqual(app._split_wake_phrase("mấy giờ rồi"), (False, ""))

    def test_connect_request_detection(self) -> None:
        self.assertTrue(app._matches("kết nối tai nghe đi", app._CONNECT_PHRASES))
        self.assertTrue(app._matches("bật Bluetooth", app._CONNECT_PHRASES))
        self.assertFalse(app._matches("mấy giờ rồi", app._CONNECT_PHRASES))


class _ScriptedStream:
    def __init__(self, segments: list[bytes]) -> None:
        self._segments = segments

    def segments(self):
        for segment in self._segments:
            yield segment
        raise KeyboardInterrupt

    def close(self) -> None:
        pass


class MainLoopTests(unittest.TestCase):
    def _run(
        self,
        transcripts: list[str],
        segments: list[bytes],
        connect_ok: bool = True,
    ) -> SimpleNamespace:
        config = _config()
        sent: list[tuple] = []
        announcements: list[tuple[str, str]] = []

        def fake_command(*args: object) -> tuple[bytes, str]:
            sent.append(args)
            return (b"mp3", "Câu trả lời")

        def fake_plan(text: str) -> dict[str, object]:
            return {
                "command": text,
                "mode": "observe" if "cầm gì" in text else "auto",
                "needsImage": "cầm gì" in text,
            }

        def fake_runtime(config, client, stream, conversation):
            def run():
                try:
                    for pcm in stream.segments():
                        conversation(pcm, lambda: True, None)
                except KeyboardInterrupt:
                    pass
            return SimpleNamespace(run=run)

        with (
            patch.object(app.Config, "load", return_value=config),
            patch.object(app, "_startup"),
            patch.object(app, "History"),
            patch.object(
                app, "MacClient", return_value=SimpleNamespace(plan=fake_plan, command=fake_command)
            ),
            patch.object(app, "VoiceStream", return_value=_ScriptedStream(segments)),
            patch.object(app, "Runtime", side_effect=fake_runtime),
            patch.object(app, "mp3_from_pcm", return_value=b"mp3-segment"),
            patch.object(app, "transcribe", side_effect=transcripts) as transcribe,
            patch.object(app, "capture_jpeg", return_value=b"jpeg") as camera,
            patch.object(app, "play_mp3"),
            patch.object(
                app,
                "_announce",
                side_effect=lambda filename, device: announcements.append((filename, device)),
            ),
            patch.object(app, "read_sensors", return_value={"imu": None, "pressure": None}),
            patch.object(app, "try_connect_headset_fast", return_value=connect_ok) as connect,
            redirect_stdout(StringIO()),
            redirect_stderr(StringIO()),
        ):
            self.assertEqual(app.main(), 0)

        result = SimpleNamespace(
            sent=sent,
            announcements=announcements,
            camera=camera,
            connect=connect,
            transcribe=transcribe,
        )
        for call_args in result.transcribe.call_args_list:
            # Groq nhan MP3 (khong phai WAV); audio khong bao gio di sang Mac.
            self.assertIsInstance(call_args.args[0], bytes)
        return result

    def test_wake_then_next_utterance_and_visual_camera_path(self) -> None:
        transcripts = [
            "Thiên Nhãn mấy giờ rồi",
            "trời hôm nay thế nào",
            "thiên nhãn",
            "tôi đang cầm gì",
        ]
        result = self._run(transcripts, [b"seg1", b"seg2", b"seg3", b"seg4"])

        self.assertEqual(len(result.sent), 2)
        self.assertEqual(result.sent[0][0], "mấy giờ rồi")
        self.assertIsNone(result.sent[0][1])
        self.assertEqual(result.sent[0][2], "auto")
        self.assertEqual(result.sent[0][3], {"imu": None, "pressure": None})
        self.assertEqual(result.sent[1][0], "tôi đang cầm gì")
        self.assertEqual(result.sent[1][1], b"jpeg")
        self.assertEqual(result.sent[1][2], "observe")
        result.camera.assert_called_once_with()
        result.connect.assert_not_called()
        self.assertTrue(result.announcements)
        self.assertTrue(all(device == "hw:MAX,0" for _, device in result.announcements))

    def test_spoken_connect_request_is_bounded_and_reported_via_max(self) -> None:
        result = self._run(["Thiên Nhãn kết nối tai nghe"], [b"seg1"], connect_ok=True)

        self.assertEqual(result.sent, [])
        result.connect.assert_called_once_with(
            "04:60:61:EF:78:22", attempts=2, attempt_seconds=3
        )
        self.assertEqual(result.announcements, [("bluetooth-ready.mp3", "hw:MAX,0")])


class StartupTests(unittest.TestCase):
    def test_startup_uses_max_only_and_never_touches_bluetooth(self) -> None:
        config = _config()
        events: list[tuple[str, str]] = []

        with (
            patch.object(app, "connected_wifi", return_value="wlan0"),
            patch.object(app, "capture_jpeg", return_value=b"\xff\xd8jpeg"),
            patch.object(
                app,
                "_announce",
                side_effect=lambda filename, device: events.append((filename, device)),
            ),
            patch.object(app, "try_connect_headset_fast") as connect,
        ):
            app._startup(config)

        connect.assert_not_called()
        self.assertTrue(events)
        self.assertTrue(all(device == "hw:MAX,0" for _, device in events))
        self.assertEqual(events[-1], ("usage-guide.mp3", "hw:MAX,0"))


class SensorPayloadTests(unittest.TestCase):
    def test_read_sensor_payload_passes_config_fields(self) -> None:
        config = _config(gy25_device="/dev/serial0", ms5611_enabled=True)
        payload = {"imu": {"yaw": 1.0}, "pressure": {"pressure_pa": 100009.0}}
        with patch.object(app, "read_sensors", return_value=payload) as read:
            result = app._read_sensor_payload(config)

        read.assert_called_once_with("/dev/serial0", 115200, 1, 0x76, True)
        self.assertEqual(result, payload)


class MacClientPlanTests(unittest.TestCase):
    def test_plan_parses_server_camera_decision(self) -> None:
        client = MacClient("https://mac.example", "token", 60, 0)
        response = b'{"command":"nhin phia truoc","mode":"observe","needsImage":true}'
        with patch.object(client, "_post", return_value=(response, "application/json", None)) as post:
            result = client.plan("nhìn phía trước")

        self.assertEqual(result["command"], "nhin phia truoc")
        self.assertEqual(result["mode"], "observe")
        self.assertTrue(result["needsImage"])
        post.assert_called_once()
        self.assertEqual(post.call_args.args[0], "/plan")

    def test_plan_rejects_invalid_server_response(self) -> None:
        client = MacClient("https://mac.example", "token", 60, 0)
        with (
            patch.object(client, "_post", return_value=(b"{}", "application/json", None)),
            self.assertRaises(MacClientError),
        ):
            client.plan("nhìn phía trước")


if __name__ == "__main__":
    unittest.main()
