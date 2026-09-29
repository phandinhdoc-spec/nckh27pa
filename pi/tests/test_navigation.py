import json
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from navigation import FrameSender, NavigationError, SensorSample, run_frames


class Handler(BaseHTTPRequestHandler):
    calls = []
    wrong_id = False

    def do_POST(self):
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.calls.append((self.path, payload))
        result = {"sessionId": payload["sessionId"],
                  "frameId": payload["frameId"] + int(self.wrong_id),
                  "alert": None, "status": "unverified"}
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(result).encode("utf-8"))

    def log_message(self, format, *args):
        pass


class NavigationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def setUp(self):
        Handler.calls.clear()
        Handler.wrong_id = False
        self.sender = FrameSender(f"http://127.0.0.1:{self.server.server_port}", "test-token")

    def test_frame_contract_and_correlation(self):
        result = self.sender.send(b"\xff\xd8image\xff\xd9", SensorSample(1.2), 123.0)
        self.assertEqual(result["status"], "unverified")
        path, data = Handler.calls[0]
        self.assertEqual(path, "/plan3/analyze")
        self.assertEqual((data["frameId"], data["pitchDeg"], data["distanceM"]), (1, 45, 1.2))
        self.assertEqual(data["imageContentType"], "image/jpeg")

    def test_invalid_sensors_do_not_send(self):
        with self.assertRaises(NavigationError):
            self.sender.send(b"jpeg", SensorSample(float("nan")), 123.0)
        self.assertEqual(Handler.calls, [])

    def test_stale_response_rejected(self):
        Handler.wrong_id = True
        with self.assertRaises(NavigationError):
            self.sender.send(b"jpeg", SensorSample(1), 123.0)

    def test_one_frame_per_slot_no_backlog(self):
        ticks = [0.0]
        captures = []
        results = []

        class FakeSender:
            def send(self, image, sensor, captured_at):
                ticks[0] += 2.0  # server slower than 1-second camera cadence
                return {"frame": image}

        def capture():
            captures.append(ticks[0])
            return b"jpeg"

        run_frames(FakeSender(), lambda: SensorSample(1), lambda: len(captures) >= 3,
                   results.append, capture=capture, clock=lambda: ticks[0],
                   sleep=lambda duration: ticks.__setitem__(0, ticks[0] + duration))
        self.assertEqual(captures, [0.0, 2.0, 4.0])
        self.assertEqual(len(results), 3)


if __name__ == "__main__":
    unittest.main()
