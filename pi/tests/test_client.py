import base64
import json
import threading
import unittest
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from client import Plan1Client, Plan1Error


class Handler(BaseHTTPRequestHandler):
    calls = []

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.calls.append((self.path, body, self.headers.get("Authorization")))
        if self.path.endswith("/cancel"):
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"cancelled":true}')
            return
        self.send_response(200)
        self.send_header("Content-Type", "audio/mpeg")
        self.send_header("X-ThienNhan-Session-Id", body["sessionId"])
        self.send_header("X-ThienNhan-Turn-Id", body["turnId"])
        self.send_header("X-ThienNhan-Text-Base64", base64.b64encode("Xin chào".encode()).decode())
        self.end_headers()
        self.wfile.write(b"ID3test")

    def log_message(self, format, *args):
        pass


class ClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.client = Plan1Client(f"http://127.0.0.1:{cls.server.server_port}", "test-token")

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def setUp(self):
        Handler.calls.clear()
        self.session, self.turn = str(uuid.uuid4()), str(uuid.uuid4())

    def test_text_turn_and_cancel(self):
        audio, answer = self.client.turn("Chào bạn", self.session, self.turn)
        self.assertEqual((audio, answer), (b"ID3test", "Xin chào"))
        self.client.cancel(self.session, self.turn)
        self.assertEqual([call[0] for call in Handler.calls], ["/plan1/turn", "/conversation/cancel"])
        self.assertTrue(all(call[2] == "Bearer test-token" for call in Handler.calls))
        self.assertEqual(Handler.calls[0][1], {"text": "Chào bạn", "sessionId": self.session, "turnId": self.turn})

    def test_empty_request_never_sent(self):
        with self.assertRaises(Plan1Error):
            self.client.turn(" ", self.session, self.turn)
        self.assertEqual(Handler.calls, [])

    def test_invalid_uuid_never_sent(self):
        with self.assertRaises(ValueError):
            self.client.turn("Xin chào", "not-a-uuid", self.turn)
        self.assertEqual(Handler.calls, [])


if __name__ == "__main__":
    unittest.main()
