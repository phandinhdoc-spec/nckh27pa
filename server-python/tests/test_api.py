import base64
import uuid

from fastapi.testclient import TestClient

from thien_nhan.app import create_app
from thien_nhan.config import Config
from thien_nhan.errors import APIError
from thien_nhan.gemini import Gemini
from unittest.mock import AsyncMock
import pytest


CONFIG = Config(
    host="127.0.0.1", port=8765, token="test-token", gemini_key="test-key",
    fast_model="fast", vision_model="vision", reasoning_model="reasoning",
    fallback_model="fallback", timeout=5, tts_voice="vi-VN-HoaiMyNeural",
    tesseract_cmd=None, ffmpeg_cmd="ffmpeg",
)


class FakeGemini:
    async def generate(self, task, prompt, image=None):
        if task == "quickClassification":
            return '{"decision":"CAPTUREX","mode":"observe","text":"Đây là cái gì?"}'
        return "Câu trả lời."


@pytest.mark.asyncio
async def test_gemini_does_not_repeat_ambiguous_failed_request():
    gemini = Gemini(CONFIG)
    gemini._request = AsyncMock(side_effect=APIError(504, "gemini_timeout", "timeout"))
    with pytest.raises(APIError):
        await gemini.generate("simpleConversation", "Hello")
    assert gemini._request.await_count == 1


@pytest.mark.asyncio
async def test_gemini_uses_fallback_only_for_missing_model():
    gemini = Gemini(CONFIG)
    gemini._request = AsyncMock(side_effect=[
        APIError(502, "gemini_model_unavailable", "missing"), "answer"
    ])
    assert await gemini.generate("simpleConversation", "Hello") == "answer"
    assert gemini._request.await_count == 2


def test_health_and_authentication():
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        assert client.get("/health").json()["status"] == "ok"
        response = client.post("/plan", json={"text": "Đây là cái gì?"})
        assert response.status_code == 401
        assert response.json()["error"]["code"] == "unauthorized"


def test_plan_and_image_requirement():
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        headers = {"Authorization": "Bearer test-token"}
        plan = client.post("/plan", json={"text": "Đây là cái gì?"}, headers=headers)
        assert plan.status_code == 200
        assert plan.json() == {"command": "Đây là cái gì?", "mode": "observe", "needsImage": True}
        command = client.post("/command", json={"text": "Đây là cái gì?", "mode": "observe"}, headers=headers)
        assert command.status_code == 400
        assert command.json()["error"]["code"] == "image_required"


def test_gemini_response_shape():
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        result = client.post("/gemini", json={"prompt": "Xin chào", "task": "simpleConversation"},
                             headers={"Authorization": "Bearer test-token"})
        assert result.status_code == 200
        assert result.json()["text"] == "Câu trả lời."
        assert result.json()["processingTime"] >= 0


def test_command_mp3_and_text_header(monkeypatch):
    async def fake_synthesize(text, config):
        return b"fake-mp3"

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_synthesize)
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        response = client.post(
            "/command", json={"text": "Xin chào", "mode": "chat"},
            headers={"Authorization": "Bearer test-token"},
        )
        assert response.status_code == 200
        assert response.headers["content-type"] == "audio/mpeg"
        assert response.content == b"fake-mp3"
        assert base64.b64decode(response.headers["x-thiennhan-text-base64"]).decode() == "Câu trả lời."


def test_conversation_returns_only_capturex_for_image_turn():
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    headers = {"Authorization": "Bearer test-token"}
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        response = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đây là cái gì?"},
            headers=headers,
        )
        assert response.status_code == 200
        assert response.json() == {
            "action": "CAPTUREX",
            "sessionId": session_id,
            "turnId": turn_id,
            "command": "Đây là cái gì?",
            "mode": "observe",
        }
        assert "NOCAPTUREX" not in response.text


def test_conversation_without_image_returns_audio_directly(monkeypatch):
    async def fake_synthesize(text, config):
        return b"answer-mp3"

    class NoImageGemini(FakeGemini):
        async def generate(self, task, prompt, image=None):
            if task == "quickClassification":
                return '{"decision":"NOCAPTUREX","mode":"chat","text":"Xin chào"}'
            return "Xin chào bạn."

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_synthesize)
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    with TestClient(create_app(CONFIG, NoImageGemini())) as client:
        response = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Xin chào"},
            headers={"Authorization": "Bearer test-token"},
        )
        assert response.status_code == 200
        assert response.headers["content-type"] == "audio/mpeg"
        assert response.content == b"answer-mp3"
        assert response.headers["x-thiennhan-session-id"] == session_id
        assert response.headers["x-thiennhan-turn-id"] == turn_id
        assert "decision;dur=" in response.headers["server-timing"]
        assert "answer;dur=" in response.headers["server-timing"]
        assert "tts;dur=" in response.headers["server-timing"]


def test_conversation_capture_requires_a_pending_authorized_turn(monkeypatch):
    async def fake_synthesize(text, config):
        return b"visual-mp3"

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_synthesize)
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    headers = {"Authorization": "Bearer test-token"}
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        payload = {
            "sessionId": session_id,
            "turnId": turn_id,
            "image": base64.b64encode(b"fake-jpeg").decode(),
            "imageContentType": "image/jpeg",
        }
        denied = client.post("/conversation/capture", json=payload, headers=headers)
        assert denied.status_code == 409
        assert denied.json()["error"]["code"] == "capture_not_authorized"

        planned = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đây là cái gì?"},
            headers=headers,
        )
        assert planned.json()["action"] == "CAPTUREX"
        captured = client.post("/conversation/capture", json=payload, headers=headers)
        assert captured.status_code == 200
        assert captured.content == b"visual-mp3"
        duplicate = client.post("/conversation/capture", json=payload, headers=headers)
        assert duplicate.status_code == 409


def test_conversation_ocr_forwards_image_to_gemini(monkeypatch):
    class OcrGemini(FakeGemini):
        received_image = None

        async def generate(self, task, prompt, image=None):
            if task == "quickClassification":
                return '{"decision":"CAPTUREX","mode":"ocr","text":"Đọc chữ này"}'
            self.received_image = image
            return "Nội dung được đọc."

    async def fake_synthesize(text, config):
        return b"spoken-ocr"

    monkeypatch.setattr("thien_nhan.app.synthesize", fake_synthesize)
    gemini = OcrGemini()
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    headers = {"Authorization": "Bearer test-token"}
    with TestClient(create_app(CONFIG, gemini)) as client:
        decision = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đọc chữ này"},
            headers=headers,
        )
        assert decision.json()["action"] == "CAPTUREX"
        response = client.post(
            "/conversation/capture",
            json={
                "sessionId": session_id, "turnId": turn_id,
                "image": base64.b64encode(b"jpeg-data").decode(),
                "imageContentType": "image/jpeg",
            },
            headers=headers,
        )
        assert response.status_code == 200
        assert response.content == b"spoken-ocr"
        assert gemini.received_image == (b"jpeg-data", "image/jpeg")


def test_conversation_cancel_invalidates_pending_capture():
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    headers = {"Authorization": "Bearer test-token"}
    with TestClient(create_app(CONFIG, FakeGemini())) as client:
        client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đây là cái gì?"},
            headers=headers,
        )
        cancelled = client.post(
            "/conversation/cancel",
            json={"sessionId": session_id, "turnId": turn_id},
            headers=headers,
        )
        assert cancelled.json() == {"cancelled": True}
        capture = client.post(
            "/conversation/capture",
            json={
                "sessionId": session_id,
                "turnId": turn_id,
                "image": base64.b64encode(b"fake-jpeg").decode(),
                "imageContentType": "image/jpeg",
            },
            headers=headers,
        )
        assert capture.status_code == 409


def test_cancel_before_turn_request_prevents_remote_work():
    class CountingGemini(FakeGemini):
        calls = 0

        async def generate(self, task, prompt, image=None):
            self.calls += 1
            return await super().generate(task, prompt, image)

    gemini = CountingGemini()
    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    headers = {"Authorization": "Bearer test-token"}
    with TestClient(create_app(CONFIG, gemini)) as client:
        cancelled = client.post(
            "/conversation/cancel",
            json={"sessionId": session_id, "turnId": turn_id},
            headers=headers,
        )
        assert cancelled.json() == {"cancelled": True}
        late = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đây là cái gì?"},
            headers=headers,
        )
        assert late.status_code == 409
        assert late.json()["error"]["code"] == "turn_cancelled"
        assert gemini.calls == 0


def test_invalid_camera_decision_never_authorizes_capture():
    class InvalidDecisionGemini(FakeGemini):
        async def generate(self, task, prompt, image=None):
            if task == "quickClassification":
                return "not-json"
            return "answer"

    session_id, turn_id = str(uuid.uuid4()), str(uuid.uuid4())
    with TestClient(create_app(CONFIG, InvalidDecisionGemini())) as client:
        response = client.post(
            "/conversation",
            json={"sessionId": session_id, "turnId": turn_id, "text": "Đây là cái gì?"},
            headers={"Authorization": "Bearer test-token"},
        )
        assert response.status_code == 502
        assert response.json()["error"]["code"] == "camera_decision_failed"
