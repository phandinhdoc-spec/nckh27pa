import pytest

from thien_nhan.planning import (
    ROUTE_GUIDANCE_UNAVAILABLE, camera_prompt, normalize, parse_camera_plan,
    route_guidance_intent, rule_plan, smart_plan, strict_conversation_plan, visual_prompt,
)


@pytest.mark.parametrize("speech", [
    "Đây là cái gì?", "Tôi đang cầm cái gì?",
    "Có chướng ngại vật nào trước tôi không?", "Bên trái có ai không?",
    "Đường đi có an toàn không?",
])
def test_visual_intent(speech):
    assert rule_plan(speech).needs_image


def test_camera_decision():
    speech = "Đây là vật gì?"
    result = parse_camera_plan('{"camera":"yes","mode":"observe","text":"Đây là vật gì?"}', speech)
    assert result.mode == "observe"
    assert result.needs_image
    assert parse_camera_plan('{"camera":"no","mode":"observe","text":"x"}', speech) is None
    assert speech in camera_prompt(speech)


@pytest.mark.parametrize("speech", [
    "Hãy hướng dẫn tôi đi đường", "co y huong dan toi di duong",
    "Thông báo giờ tôi đi đường", "Thiên Nhãn, hướng dẫn tôi đi đường",
])
def test_route_guidance_stays_local(speech):
    assert route_guidance_intent(speech)
    assert rule_plan(speech).mode == "local"
    assert not rule_plan(speech).needs_image


def test_unrelated_time_and_safety_text():
    assert not route_guidance_intent("Bây giờ là mấy giờ?")
    assert "chưa được hiệu chuẩn" in ROUTE_GUIDANCE_UNAVAILABLE
    assert not any(c.isdigit() for c in ROUTE_GUIDANCE_UNAVAILABLE)


def test_vietnamese_normalization_and_visual_prompt():
    assert normalize("Đường") == "duong"
    assert "vật trong tay" in visual_prompt("Đây là vật gì?")


@pytest.mark.asyncio
async def test_camera_safety_fallback():
    class FakeGemini:
        async def generate(self, task, prompt):
            return '{"camera":"no","mode":"chat","text":"Đây là vật gì?"}'

    assert (await smart_plan("Đây là vật gì?", FakeGemini())).mode == "observe"


@pytest.mark.asyncio
async def test_strict_conversation_plan_rejects_conflicting_no_capture():
    class UnderconfidentGemini:
        async def generate(self, task, prompt):
            return '{"decision":"NOCAPTUREX","mode":"chat","text":"Đây là vật gì?"}'

    with pytest.raises(ValueError):
        await strict_conversation_plan("Đây là vật gì?", UnderconfidentGemini())
