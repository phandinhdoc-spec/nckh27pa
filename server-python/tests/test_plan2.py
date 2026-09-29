import asyncio
import base64
from io import BytesIO
import uuid

import httpx
import pytest
from PIL import Image

from test_api import CONFIG
from thien_nhan.app import create_app

HEADERS = {"Authorization": "Bearer test-token"}


def ids():
    return {"sessionId": str(uuid.uuid4()), "turnId": str(uuid.uuid4())}


def jpeg(size=(640, 640)):
    stream = BytesIO()
    Image.new("RGB", size, "white").save(stream, format="JPEG")
    return base64.b64encode(stream.getvalue()).decode()


class Fake:
    def __init__(self, decision):
        self.decision = decision
        self.calls = []
        self.wait = None

    async def generate(self, task, prompt, image=None):
        self.calls.append((task, prompt, image))
        if task == "quickClassification":
            return self.decision
        if self.wait:
            await self.wait.wait()
        return "Đây là câu trả lời."


@pytest.fixture
def tts(monkeypatch):
    async def synthesize(text, config):
        return b"mp3-test"
    monkeypatch.setattr("thien_nhan.app.synthesize", synthesize)


@pytest.mark.asyncio
@pytest.mark.parametrize("mode,text", [("observe", "Tìm cái cốc trước mặt"), ("ocr", "Đọc trang giấy này")])
async def test_capture_flow(tts, mode, text):
    fake = Fake('{"decision":"CAPTUREX","mode":"' + mode + '","text":"request"}')
    transport = httpx.ASGITransport(app=create_app(CONFIG, fake))
    key = ids()
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        early = await client.post("/plan2/capture", json={**key, "image": jpeg(), "imageContentType": "image/jpeg"}, headers=HEADERS)
        assert early.status_code == 409
        response = await client.post("/plan2/turn", json={**key, "text": text}, headers=HEADERS)
        assert response.json() == {"action": "CAPTUREX", **key, "command": text, "mode": mode}
        assert (await client.post("/plan2/turn", json={**key, "text": text}, headers=HEADERS)).status_code == 409
        result = await client.post("/plan2/capture", json={**key, "image": jpeg(), "imageContentType": "image/jpeg"}, headers=HEADERS)
        assert result.status_code == 200 and result.content == b"mp3-test"
        assert result.headers["content-type"] == "audio/mpeg"
        assert result.headers["x-thiennhan-turn-id"] == key["turnId"]
        assert fake.calls[-1][2][1] == "image/jpeg"
        assert "Yêu cầu người dùng" in fake.calls[-1][1]
        assert (await client.post("/plan2/capture", json={**key, "image": jpeg(), "imageContentType": "image/jpeg"}, headers=HEADERS)).status_code == 409


@pytest.mark.asyncio
async def test_no_capture_internal_answer_and_invalid_decision_fallback(tts):
    fake = Fake('{"decision":"NOCAPTUREX","mode":"chat","text":"hi"}')
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(CONFIG, fake)), base_url="http://test") as client:
        result = await client.post("/plan2/turn", json={**ids(), "text": "Vì sao trời xanh?"}, headers=HEADERS)
        assert result.status_code == 200 and result.content == b"mp3-test"
        assert [task for task, _, _ in fake.calls] == ["quickClassification", "simpleConversation"]
        assert "NOCAPTUREX" not in result.text
    fake = Fake("not-json")
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(CONFIG, fake)), base_url="http://test") as client:
        clear = await client.post("/plan2/turn", json={**ids(), "text": "Đọc chữ trên trang giấy này"}, headers=HEADERS)
        assert clear.json()["action"] == "CAPTUREX"
        ambiguous = await client.post("/plan2/turn", json={**ids(), "text": "Giúp tôi với"}, headers=HEADERS)
        assert ambiguous.status_code == 200 and ambiguous.content == b"mp3-test"
        assert len(fake.calls) == 2  # clarification synthesized locally, not guessed by Gemini


@pytest.mark.asyncio
async def test_image_validation_cancel_and_stale(tts):
    fake = Fake('{"decision":"CAPTUREX","mode":"observe","text":"look"}')
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(CONFIG, fake)), base_url="http://test") as client:
        key = ids()
        assert (await client.post("/plan2/turn", json={**key, "text": "Đây là cái gì?"}, headers=HEADERS)).status_code == 200
        for image, mime, code in [("!", "image/jpeg", 400), (jpeg((320, 320)), "image/jpeg", 400), (base64.b64encode(b"not a jpeg").decode(), "image/jpeg", 400), (jpeg(), "image/png", 415)]:
            result = await client.post("/plan2/capture", json={**key, "image": image, "imageContentType": mime}, headers=HEADERS)
            assert result.status_code == code
        assert (await client.post("/plan2/cancel", json=key, headers=HEADERS)).json() == {"cancelled": True}
        assert (await client.post("/plan2/capture", json={**key, "image": jpeg(), "imageContentType": "image/jpeg"}, headers=HEADERS)).status_code == 409
        second = ids()
        await client.post("/plan2/cancel", json=second, headers=HEADERS)
        assert (await client.post("/plan2/turn", json={**second, "text": "Nhìn đi"}, headers=HEADERS)).status_code == 409
        assert len(fake.calls) == 1


@pytest.mark.asyncio
async def test_cancel_during_answer_and_concurrent_capture(tts):
    fake = Fake('{"decision":"CAPTUREX","mode":"observe","text":"look"}')
    fake.wait = asyncio.Event()
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(CONFIG, fake)), base_url="http://test") as client:
        key = ids()
        await client.post("/plan2/turn", json={**key, "text": "Nhìn vật trước mặt"}, headers=HEADERS)
        payload = {**key, "image": jpeg(), "imageContentType": "image/jpeg"}
        first = asyncio.create_task(client.post("/plan2/capture", json=payload, headers=HEADERS))
        for _ in range(100):
            if len(fake.calls) == 2:
                break
            await asyncio.sleep(0.01)
        assert len(fake.calls) == 2
        assert (await client.post("/plan2/capture", json=payload, headers=HEADERS)).status_code == 409
        await client.post("/plan2/cancel", json=key, headers=HEADERS)
        fake.wait.set()
        result = await first
        assert result.status_code == 409 and result.json()["error"]["code"] == "turn_cancelled"


@pytest.mark.asyncio
async def test_auth_and_no_client_camera_override(tts):
    fake = Fake('{"decision":"NOCAPTUREX","mode":"chat","text":"x"}')
    async with httpx.AsyncClient(transport=httpx.ASGITransport(app=create_app(CONFIG, fake)), base_url="http://test") as client:
        key = ids()
        assert (await client.post("/plan2/turn", json={**key, "text": "Hi"})).status_code == 401
        assert (await client.post("/plan2/turn", json={**key, "text": "Hi", "mode": "observe"}, headers=HEADERS)).status_code == 400
        assert (await client.post("/plan2/capture", json={**key, "image": jpeg(), "imageContentType": "image/jpeg"}, headers=HEADERS)).status_code == 409
        assert fake.calls == []
