import base64
import threading
import uuid

import pytest
from fastapi.testclient import TestClient

from thien_nhan.app import create_app
from test_api import CONFIG


HEADERS = {"Authorization": "Bearer test-token"}


def payload():
    return {"sessionId": str(uuid.uuid4()), "turnId": str(uuid.uuid4()), "text": "Đây là gì?"}


def test_plan1_text_only_mp3_correlation_and_duplicate(monkeypatch):
    calls = []

    class Gemini:
        async def generate(self, task, prompt, image=None):
            calls.append((task, prompt, image))
            return "Đây là câu trả lời."

    async def fake_tts(text, config):
        assert text == "Đây là câu trả lời."
        return b"mp3-data"

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_tts)
    body = payload()
    with TestClient(create_app(CONFIG, Gemini())) as client:
        assert client.post("/plan1/turn", json=body).status_code == 401
        result = client.post("/plan1/turn", json=body, headers=HEADERS)
        assert result.status_code == 200
        assert result.headers["content-type"] == "audio/mpeg"
        assert result.content == b"mp3-data"
        assert result.headers["x-thiennhan-session-id"] == body["sessionId"]
        assert result.headers["x-thiennhan-turn-id"] == body["turnId"]
        assert base64.b64decode(result.headers["x-thiennhan-text-base64"]).decode() == "Đây là câu trả lời."
        assert "answer;dur=" in result.headers["server-timing"]
        duplicate = client.post("/plan1/turn", json=body, headers=HEADERS)
        assert duplicate.status_code == 409
        assert duplicate.json()["error"]["code"] == "duplicate_turn"
        capture = client.post("/conversation/capture", json={"sessionId": body["sessionId"], "turnId": body["turnId"]}, headers=HEADERS)
        assert capture.status_code == 409
        assert capture.json()["error"]["code"] == "capture_not_authorized"
    assert calls == [("simpleConversation", "Trả lời ngắn gọn bằng tiếng Việt: Đây là gì?", None)]


@pytest.mark.parametrize("change", [
    {"sessionId": "bad"}, {"turnId": "bad"}, {"text": " "},
    {"image": "abc"}, {"mode": "observe"}, {"action": "CAPTUREX"},
])
def test_plan1_rejects_invalid_or_visual_payload(change):
    class Gemini:
        async def generate(self, *args, **kwargs):
            pytest.fail("Gemini must not be called")

    body = payload() | change
    with TestClient(create_app(CONFIG, Gemini())) as client:
        response = client.post("/plan1/turn", json=body, headers=HEADERS)
        assert response.status_code == 400


def test_plan1_cancel_before_request_prevents_gemini():
    class Gemini:
        async def generate(self, *args, **kwargs):
            pytest.fail("Gemini must not be called")

    body = payload()
    with TestClient(create_app(CONFIG, Gemini())) as client:
        cancel = client.post("/conversation/cancel", json={"sessionId": body["sessionId"], "turnId": body["turnId"]}, headers=HEADERS)
        assert cancel.json() == {"cancelled": True}
        result = client.post("/plan1/turn", json=body, headers=HEADERS)
        assert result.status_code == 409
        assert result.json()["error"]["code"] == "turn_cancelled"


def test_plan1_cancel_during_gemini_suppresses_tts(monkeypatch):
    entered, release = threading.Event(), threading.Event()

    class Gemini:
        async def generate(self, *args, **kwargs):
            entered.set()
            assert await __import__("asyncio").to_thread(release.wait, 5)
            return "Không được phát"

    async def fake_tts(*args):
        pytest.fail("TTS must not be called after cancellation")

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_tts)
    body = payload()
    with TestClient(create_app(CONFIG, Gemini())) as client:
        result = {}
        thread = threading.Thread(target=lambda: result.update(response=client.post("/plan1/turn", json=body, headers=HEADERS)))
        thread.start()
        try:
            assert entered.wait(5)
            cancel = client.post("/conversation/cancel", json={"sessionId": body["sessionId"], "turnId": body["turnId"]}, headers=HEADERS)
            assert cancel.json() == {"cancelled": True}
        finally:
            release.set()
            thread.join(5)
        assert not thread.is_alive()
        assert result["response"].status_code == 409
        assert result["response"].json()["error"]["code"] == "turn_cancelled"
