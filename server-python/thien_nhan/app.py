import asyncio
import base64
import binascii
import hmac
import json
import math
import time
import uuid
from time import perf_counter

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from .config import Config
from .errors import APIError, handle_api_error
from .gemini import Gemini, TASKS
from .planning import (
    MODES, ROUTE_GUIDANCE_UNAVAILABLE, needs_text_interpretation,
    route_guidance_intent, rule_plan, smart_plan, visual_prompt,
    strict_conversation_plan,
    plan2_decision,
)
from .plan3_geometry import FIXED_PITCH_DEG, GeometryInputError, compute_geometry
from .plan3_vision import PROMPT as CENTER_PROMPT, center_obstacles
from .tts import MAX_TTS_CHARS, mp3_response, synthesize
from .vision import IMAGE_TYPES, MAX_IMAGE_SIZE, decode_image, normalized_type, recognize


def create_app(config: Config | None = None, gemini: Gemini | None = None) -> FastAPI:
    config = config or Config.load()
    gemini = gemini or Gemini(config)
    app = FastAPI(title="Thiên Nhãn Server", docs_url=None, redoc_url=None)
    app.add_exception_handler(APIError, handle_api_error)
    conversation_turns: dict[tuple[str, str], dict] = {}
    plan2_turns: dict[tuple[str, str], dict] = {}

    def authenticate(request: Request) -> None:
        authorization = request.headers.get("authorization", "")
        if not authorization.startswith("Bearer ") or not hmac.compare_digest(
            authorization[7:], config.token
        ):
            raise APIError(401, "unauthorized", "Bearer Token không hợp lệ.")

    async def limited_body(request: Request, limit: int, code: str = "request_too_large",
                           message: str = "Dữ liệu yêu cầu vượt quá giới hạn cho phép.") -> bytes:
        data = bytearray()
        async for chunk in request.stream():
            data.extend(chunk)
            if len(data) > limit:
                raise APIError(413, code, message)
        return bytes(data)

    async def json_body(request: Request, limit: int, label: str) -> dict:
        if normalized_type(request.headers.get("content-type")) != "application/json":
            raise APIError(415, "unsupported_content_type", "Content-Type phải là application/json.")
        data = await limited_body(request, limit)
        try:
            body = json.loads(data)
            if not isinstance(body, dict):
                raise ValueError("object required")
            return body
        except (ValueError, UnicodeDecodeError) as error:
            raise APIError(400, "invalid_json", f"JSON của {label} không hợp lệ.") from error

    def text_field(body: dict, key: str, label: str, limit: int = 2000) -> str:
        value = body.get(key)
        if not isinstance(value, str):
            raise APIError(400, "invalid_json", f"JSON của {label} không hợp lệ.")
        text = value.strip()
        if not text:
            code = "prompt_empty" if key == "prompt" else "text_empty"
            raise APIError(400, code, "Nội dung đang trống.")
        if len(text) > limit:
            code = "prompt_too_long" if key == "prompt" else "text_too_long"
            raise APIError(413, code, "Nội dung vượt quá 2.000 ký tự.")
        return text

    def command_image(body: dict) -> tuple[bytes, str] | None:
        encoded, content_type = body.get("image"), body.get("imageContentType")
        if encoded is None and content_type is None:
            return None
        if encoded is None or content_type is None:
            raise APIError(400, "incomplete_image", "Ảnh và imageContentType phải được gửi cùng nhau.")
        if not isinstance(encoded, str) or not isinstance(content_type, str):
            raise APIError(400, "invalid_json", "JSON của lệnh không hợp lệ.")
        mime = normalized_type(content_type)
        if mime not in IMAGE_TYPES:
            raise APIError(415, "unsupported_image_type", "Định dạng ảnh không được hỗ trợ.")
        try:
            data = base64.b64decode(encoded, validate=True)
        except (ValueError, binascii.Error) as error:
            raise APIError(400, "invalid_json", "JSON của lệnh không hợp lệ.") from error
        if not data:
            raise APIError(400, "image_empty", "Dữ liệu ảnh đang trống.")
        if len(data) > MAX_IMAGE_SIZE:
            raise APIError(413, "image_too_large", "Ảnh vượt quá 10 MB.")
        return data, mime

    def conversation_id(body: dict, key: str) -> str:
        value = body.get(key)
        if not isinstance(value, str):
            raise APIError(400, "invalid_json", f"Thiếu {key} hợp lệ.")
        try:
            return str(uuid.UUID(value))
        except (ValueError, AttributeError) as error:
            raise APIError(400, "invalid_json", f"{key} không hợp lệ.") from error

    def prune_conversation_turns() -> None:
        cutoff = time.monotonic() - max(config.timeout * 4, 300)
        expired = [key for key, turn in conversation_turns.items() if turn["created"] < cutoff]
        for key in expired:
            conversation_turns.pop(key, None)
        while len(conversation_turns) > 500:
            oldest = min(conversation_turns, key=lambda key: conversation_turns[key]["created"])
            conversation_turns.pop(oldest, None)

    async def answer_text(
        command_text: str, mode: str, image: tuple[bytes, str] | None,
        *, conversation: bool = False,
    ) -> str:
        if route_guidance_intent(command_text):
            return ROUTE_GUIDANCE_UNAVAILABLE
        if mode == "local":
            return "Máy chủ Thiên Nhãn đang hoạt động."
        if mode == "chat":
            return await gemini.generate(
                "simpleConversation", f"Trả lời ngắn gọn bằng tiếng Việt: {command_text}"
            )
        if mode == "ocr":
            if image is None:
                raise APIError(400, "image_required", "Lệnh này cần ảnh từ camera.")
            if conversation:
                return await gemini.generate(
                    "visualQuestionAnswering",
                    visual_prompt(command_text) + "\nRead any relevant visible text directly from the image.",
                    image,
                )
            result = await asyncio.to_thread(recognize, image[0], image[1], config)
            if not result["text"]:
                return "Tôi không đọc được chữ rõ ràng trong ảnh."
            if needs_text_interpretation(command_text):
                return await gemini.generate(
                    "textInterpretation",
                    f"Hãy diễn giải ngắn gọn bằng tiếng Việt nội dung OCR sau theo yêu cầu: "
                    f"{command_text}\n\n{result['text'][:8000]}",
                )
            return result["text"]
        if mode == "observe":
            if image is None:
                raise APIError(400, "image_required", "Lệnh này cần ảnh từ camera.")
            return await gemini.generate(
                "visualQuestionAnswering", visual_prompt(command_text), image
            )
        raise APIError(400, "invalid_json", "Chế độ hội thoại không hợp lệ.")

    async def audio_for_turn(answer: str, session_id: str, turn_id: str):
        spoken = answer[:MAX_TTS_CHARS]
        started = perf_counter()
        response = mp3_response(await synthesize(spoken, config), spoken)
        response.headers["Server-Timing"] = f"tts;dur={(perf_counter() - started) * 1000:.1f}"
        response.headers["X-ThienNhan-Session-Id"] = session_id
        response.headers["X-ThienNhan-Turn-Id"] = turn_id
        return response

    def plan2_prune() -> None:
        cutoff = time.monotonic() - 24 * 60 * 60
        for key, turn in list(plan2_turns.items()):
            if turn["created"] < cutoff:
                plan2_turns.pop(key, None)

    def plan2_active(turn: dict) -> None:
        if turn["cancelled"]:
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")

    plan3_streams = {}

    async def analyze_plan3(body, image):
        session_id, frame_id, captured_at = body["sessionId"], body["frameId"], body["capturedAt"]
        detections, alert = [], None
        status = "vision_unknown"
        try:
            raw = await asyncio.wait_for(
                gemini.generate("navigationAnalysis", CENTER_PROMPT, image), timeout=12)
            detections = center_obstacles(raw)
            status = "center_obstacle" if detections else "no_center_obstacle_detected"
            alert = "CENTER_OBSTACLE" if detections else None
        except (APIError, ValueError, TimeoutError) as error:
            print(f"[PLAN3 VISION] {type(error).__name__}: {getattr(error, 'code', str(error))}", flush=True)
        return {"sessionId": session_id, "frameId": frame_id, "capturedAt": captured_at,
                "imageWidth": 640, "imageHeight": 640,
                "sensor": {"pitchDeg": FIXED_PITCH_DEG,
                           "distanceM": body["distanceM"]},
                "detections": detections, "geometry": [], "valid": False,
                "status": status, "alert": alert, "sosEnabled": False,
                "blockers": ["geometry_not_calibrated"]}

    async def drain_plan3(state):
        while state["pending"] is not None:
            body, image = state["pending"]
            state["pending"] = None
            state["result"] = await analyze_plan3(body, image)

    @app.post("/plan3/analyze")
    async def plan3_analyze(request: Request) -> dict:
        authenticate(request)
        body = await json_body(request, 15 * 1024 * 1024, "Plan3")
        required = {"sessionId", "frameId", "capturedAt", "image", "imageContentType",
                    "pitchDeg", "distanceM"}
        if set(body) != required:
            raise APIError(400, "invalid_json", "Plan3 yêu cầu đúng các trường định danh, ảnh và cảm biến.")
        session_id = conversation_id(body, "sessionId")
        frame_id, captured_at = body["frameId"], body["capturedAt"]
        if isinstance(frame_id, bool) or not isinstance(frame_id, int) or frame_id < 0:
            raise APIError(400, "invalid_json", "frameId phải là số nguyên không âm.")
        if (isinstance(captured_at, bool) or not isinstance(captured_at, (int, float))
                or not math.isfinite(captured_at) or captured_at < 0):
            raise APIError(400, "invalid_json", "capturedAt phải là Unix seconds hữu hạn.")
        image = command_image(body)
        if image is None or image[1] not in {"image/jpeg", "image/jpg"}:
            raise APIError(415, "unsupported_image_type", "Plan3 yêu cầu JPEG.")
        if not image[0].startswith(b"\xff\xd8"):
            raise APIError(400, "invalid_image", "Ảnh không phải JPEG.")
        decoded = decode_image(*image)
        if decoded.size != (640, 640):
            raise APIError(400, "invalid_image_dimensions", "Plan3 yêu cầu ảnh 640x640.")
        if body["pitchDeg"] != FIXED_PITCH_DEG:
            raise APIError(400, "invalid_sensor_values", "Góc pitch phải cố định ở 45 độ.")
        try:
            # Validate distance without inventing an obstacle foot point.
            compute_geometry(320, 320, body["distanceM"])
        except GeometryInputError as error:
            raise APIError(400, "invalid_sensor_values", str(error)) from error
        if request.query_params.get("stream") == "1":
            now = time.monotonic()
            for key, old in list(plan3_streams.items()):
                if now - old["seen"] > 30 and old["task"].done():
                    del plan3_streams[key]
            state = plan3_streams.get(session_id)
            if state is None:
                if len(plan3_streams) >= 8:
                    raise APIError(429, "streams_busy", "Server đang xử lý đủ luồng camera.")
                state = {"pending": None, "result": None, "task": None, "seen": now, "last": -1}
                plan3_streams[session_id] = state
            if frame_id <= state["last"]:
                raise APIError(409, "stale_frame", "Ảnh đã cũ.")
            state.update(pending=(body, image), seen=now, last=frame_id)
            if state["task"] is None or state["task"].done():
                state["task"] = asyncio.create_task(drain_plan3(state))
            return {"sessionId": session_id, "frameId": frame_id, "status": "accepted",
                    "alert": None, "analysis": state["result"]}
        return await analyze_plan3(body, image)

    @app.post("/plan2/turn")
    async def plan2_turn(request: Request):
        authenticate(request)
        body = await json_body(request, 16 * 1024, "Plan2")
        if set(body) != {"sessionId", "turnId", "text"}:
            raise APIError(400, "invalid_json", "Plan2 chỉ nhận sessionId, turnId và text.")
        session_id, turn_id = conversation_id(body, "sessionId"), conversation_id(body, "turnId")
        text = text_field(body, "text", "Plan2")
        plan2_prune()
        key = session_id, turn_id
        if key in plan2_turns:
            code = "turn_cancelled" if plan2_turns[key]["cancelled"] else "duplicate_turn"
            raise APIError(409, code, "Lượt hội thoại này đã được nhận hoặc đã bị dừng.")
        turn = {"created": time.monotonic(), "state": "planning", "cancelled": False}
        plan2_turns[key] = turn
        started = perf_counter()
        try:
            plan = await plan2_decision(text, gemini)
            plan2_active(turn)
            turn["decision_ms"] = (perf_counter() - started) * 1000
            if plan is None:
                turn["state"] = "answering"
                answer = "Bạn muốn tôi xem điều gì qua camera? Hãy nói rõ vật, trang giấy hoặc cảnh cần xem."
            elif plan.needs_image:
                turn.update(state="waiting_capture", command=plan.command, mode=plan.mode)
                response = JSONResponse({"action": "CAPTUREX", "sessionId": session_id,
                                         "turnId": turn_id, "command": plan.command, "mode": plan.mode})
                response.headers["Server-Timing"] = f"decision;dur={turn['decision_ms']:.1f}"
                return response
            else:
                turn["state"] = "answering"
                answer = await answer_text(plan.command, plan.mode, None)
            plan2_active(turn)
            response = await audio_for_turn(answer, session_id, turn_id)
            plan2_active(turn)
            turn["state"] = "complete"
            return response
        except Exception:
            turn["state"] = "cancelled" if turn["cancelled"] else "failed"
            raise

    @app.post("/plan2/capture")
    async def plan2_capture(request: Request):
        authenticate(request)
        body = await json_body(request, 15 * 1024 * 1024, "ảnh Plan2")
        if set(body) != {"sessionId", "turnId", "image", "imageContentType"}:
            raise APIError(400, "invalid_json", "Ảnh Plan2 chỉ nhận sessionId, turnId, image và imageContentType.")
        session_id, turn_id = conversation_id(body, "sessionId"), conversation_id(body, "turnId")
        plan2_prune()
        turn = plan2_turns.get((session_id, turn_id))
        if turn is None or turn["state"] != "waiting_capture" or turn["cancelled"]:
            raise APIError(409, "capture_not_authorized", "Lượt này chưa được phép chụp ảnh.")
        image = command_image(body)
        if image is None or image[1] not in {"image/jpeg", "image/jpg"}:
            raise APIError(415, "unsupported_image_type", "Plan2 yêu cầu ảnh JPEG.")
        decoded = decode_image(*image)
        if decoded.size != (640, 640):
            raise APIError(400, "invalid_image_dimensions", "Plan2 yêu cầu ảnh 640x640.")
        turn["state"] = "answering"  # Claim before awaiting Gemini; rejects concurrent duplicates.
        try:
            answer = await answer_text(turn["command"], turn["mode"], image, conversation=True)
            plan2_active(turn)
            response = await audio_for_turn(answer, session_id, turn_id)
            plan2_active(turn)
            turn["state"] = "complete"
            return response
        except Exception:
            turn["state"] = "cancelled" if turn["cancelled"] else "failed"
            raise

    @app.post("/plan2/cancel")
    async def plan2_cancel(request: Request) -> dict:
        authenticate(request)
        body = await json_body(request, 16 * 1024, "dừng Plan2")
        if set(body) != {"sessionId", "turnId"}:
            raise APIError(400, "invalid_json", "Thiếu định danh lượt hội thoại.")
        key = conversation_id(body, "sessionId"), conversation_id(body, "turnId")
        plan2_prune()
        turn = plan2_turns.get(key)
        if turn is None:
            plan2_turns[key] = {"created": time.monotonic(), "state": "cancelled", "cancelled": True}
            return {"cancelled": True}
        if turn["state"] == "complete":
            return {"cancelled": False}
        turn.update(state="cancelled", cancelled=True)
        return {"cancelled": True}

    @app.get("/health")
    async def health() -> dict:
        return {"success": True, "service": "ThienNhanServer", "status": "ok", "version": "0.1.0"}

    @app.post("/stt")
    async def speech_to_text(request: Request) -> dict:
        authenticate(request)
        if normalized_type(request.headers.get("content-type")) != "audio/mpeg":
            raise APIError(415, "unsupported_audio_type", "STT yêu cầu âm thanh MP3.")
        audio = await limited_body(request, 10 * 1024 * 1024)
        if not audio:
            raise APIError(400, "audio_empty", "Âm thanh đang trống.")
        transcript = await gemini.generate(
            "simpleConversation",
            "Chép lại chính xác lời nói tiếng Việt trong âm thanh. Chỉ trả văn bản được nói, không giải thích.",
            (audio, "audio/mpeg"),
        )
        return {"text": transcript.strip()}

    @app.post("/plan1/turn")
    async def plan1_turn(request: Request):
        authenticate(request)
        body = await json_body(request, 16 * 1024, "Plan1")
        # This route has no image, mode, or camera decision input by design.
        if set(body) != {"sessionId", "turnId", "text"}:
            raise APIError(400, "invalid_json", "Plan1 chỉ nhận sessionId, turnId và text.")
        session_id = conversation_id(body, "sessionId")
        turn_id = conversation_id(body, "turnId")
        text = text_field(body, "text", "Plan1")
        prune_conversation_turns()
        key = (session_id, turn_id)
        if key in conversation_turns:
            code = "turn_cancelled" if conversation_turns[key]["cancelled"] else "duplicate_turn"
            raise APIError(409, code, "Lượt hội thoại này đã được nhận hoặc đã bị dừng.")
        turn = {"created": time.monotonic(), "state": "answering", "cancelled": False}
        conversation_turns[key] = turn
        answer_started = perf_counter()
        try:
            answer = await gemini.generate(
                "simpleConversation", f"Trả lời ngắn gọn bằng tiếng Việt: {text}"
            )
            answer_ms = (perf_counter() - answer_started) * 1000
            if turn["cancelled"]:
                raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
            response = await audio_for_turn(answer, session_id, turn_id)
            response.headers["Server-Timing"] = (
                f"answer;dur={answer_ms:.1f}, " + response.headers["Server-Timing"]
            )
            if turn["cancelled"]:
                raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
            turn["state"] = "complete"
            return response
        except Exception:
            turn["state"] = "cancelled" if turn["cancelled"] else "failed"
            raise

    @app.post("/conversation")
    async def conversation(request: Request):
        authenticate(request)
        body = await json_body(request, 64 * 1024, "hội thoại")
        session_id = conversation_id(body, "sessionId")
        turn_id = conversation_id(body, "turnId")
        text = text_field(body, "text", "hội thoại")
        prune_conversation_turns()
        key = (session_id, turn_id)
        if key in conversation_turns:
            code = "turn_cancelled" if conversation_turns[key]["cancelled"] else "duplicate_turn"
            raise APIError(409, code, "Lượt hội thoại này đã được nhận hoặc đã bị dừng.")
        turn = {
            "created": time.monotonic(),
            "text": text,
            "mode": None,
            "command": None,
            "state": "planning",
            "cancelled": False,
        }
        conversation_turns[key] = turn

        decision_started = perf_counter()
        try:
            plan = await strict_conversation_plan(text, gemini)
        except Exception as error:
            turn["state"] = "failed"
            raise APIError(
                502,
                "camera_decision_failed",
                "Gemini camera decision failed; capture was not authorized.",
            ) from error
        turn["decision_ms"] = (perf_counter() - decision_started) * 1000
        if turn["cancelled"]:
            turn["state"] = "cancelled"
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
        turn["command"] = plan.command
        turn["mode"] = plan.mode
        if plan.needs_image:
            turn["state"] = "waiting_capture"
            response = JSONResponse(
                {
                    "action": "CAPTUREX",
                    "sessionId": session_id,
                    "turnId": turn_id,
                    "command": plan.command,
                    "mode": plan.mode,
                }
            )
            response.headers["Server-Timing"] = f"decision;dur={turn['decision_ms']:.1f}"
            return response

        turn["state"] = "answering"
        answer_started = perf_counter()
        answer = await answer_text(plan.command, plan.mode, None)
        answer_ms = (perf_counter() - answer_started) * 1000
        if turn["cancelled"]:
            turn["state"] = "cancelled"
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
        response = await audio_for_turn(answer, session_id, turn_id)
        response.headers["Server-Timing"] = (
            f"decision;dur={turn['decision_ms']:.1f}, answer;dur={answer_ms:.1f}, "
            + response.headers["Server-Timing"]
        )
        if turn["cancelled"]:
            turn["state"] = "cancelled"
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
        turn["state"] = "complete"
        return response

    @app.post("/conversation/capture")
    async def conversation_capture(request: Request):
        authenticate(request)
        body = await json_body(request, 15 * 1024 * 1024, "ảnh hội thoại")
        session_id = conversation_id(body, "sessionId")
        turn_id = conversation_id(body, "turnId")
        key = (session_id, turn_id)
        turn = conversation_turns.get(key)
        if turn is None or turn["state"] != "waiting_capture" or turn["cancelled"]:
            raise APIError(409, "capture_not_authorized", "Lượt này chưa được phép chụp ảnh.")
        image = command_image(body)
        if image is None:
            raise APIError(400, "incomplete_image", "Ảnh hội thoại đang bị thiếu.")
        turn["state"] = "answering"
        answer_started = perf_counter()
        answer = await answer_text(turn["command"], turn["mode"], image, conversation=True)
        answer_ms = (perf_counter() - answer_started) * 1000
        if turn["cancelled"]:
            turn["state"] = "cancelled"
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
        response = await audio_for_turn(answer, session_id, turn_id)
        response.headers["Server-Timing"] = (
            f"decision;dur={turn['decision_ms']:.1f}, answer;dur={answer_ms:.1f}, "
            + response.headers["Server-Timing"]
        )
        if turn["cancelled"]:
            turn["state"] = "cancelled"
            raise APIError(409, "turn_cancelled", "Lượt hội thoại đã bị dừng.")
        turn["state"] = "complete"
        return response

    @app.post("/conversation/cancel")
    async def conversation_cancel(request: Request) -> dict:
        authenticate(request)
        body = await json_body(request, 16 * 1024, "dừng hội thoại")
        session_id = conversation_id(body, "sessionId")
        turn_id = conversation_id(body, "turnId")
        key = (session_id, turn_id)
        prune_conversation_turns()
        turn = conversation_turns.get(key)
        if turn is None:
            # A stop can overtake the original request on separate connections.
            conversation_turns[key] = {
                "created": time.monotonic(), "state": "cancelled", "cancelled": True,
            }
            return {"cancelled": True}
        if turn["state"] == "complete":
            return {"cancelled": False}
        turn["cancelled"] = True
        turn["state"] = "cancelled"
        return {"cancelled": True}

    @app.post("/plan")
    async def plan(request: Request) -> dict:
        authenticate(request)
        body = await json_body(request, 15 * 1024 * 1024, "lệnh")
        text = text_field(body, "text", "lệnh")
        return (await smart_plan(text, gemini)).as_response()

    @app.post("/gemini")
    async def gemini_route(request: Request) -> dict:
        authenticate(request)
        body = await json_body(request, 16 * 1024, "AI")
        prompt = text_field(body, "prompt", "AI")
        task = body.get("task")
        if not isinstance(task, str) or task not in TASKS:
            raise APIError(400, "invalid_json", "JSON của AI không hợp lệ.")
        start = perf_counter()
        result = await gemini.generate(task, prompt)
        return {"text": result, "processingTime": perf_counter() - start}

    @app.post("/vision/ocr")
    async def vision_ocr(request: Request) -> dict:
        authenticate(request)
        mime = normalized_type(request.headers.get("content-type"))
        if mime not in IMAGE_TYPES:
            raise APIError(415, "unsupported_image_type", "Định dạng ảnh không được hỗ trợ.")
        data = await limited_body(request, MAX_IMAGE_SIZE, "image_too_large", "Ảnh vượt quá 10 MB.")
        return await asyncio.to_thread(recognize, data, mime, config)

    @app.post("/tts")
    async def tts_route(request: Request):
        authenticate(request)
        body = await json_body(request, 16 * 1024, "TTS")
        text = text_field(body, "text", "TTS", MAX_TTS_CHARS)
        return mp3_response(await synthesize(text, config))

    @app.post("/command")
    async def command(request: Request):
        authenticate(request)
        body = await json_body(request, 15 * 1024 * 1024, "lệnh")
        text = text_field(body, "text", "lệnh")
        mode = body.get("mode")
        if mode is not None and (not isinstance(mode, str) or mode not in MODES):
            raise APIError(400, "invalid_json", "JSON của lệnh không hợp lệ.")
        if route_guidance_intent(text):
            answer = ROUTE_GUIDANCE_UNAVAILABLE
        else:
            image = command_image(body)
            automatic = rule_plan(text)
            command_text = automatic.command
            mode = mode if mode and mode != "auto" else automatic.mode
            if mode in {"ocr", "observe"} and image is None:
                raise APIError(400, "image_required", "Lệnh này cần ảnh từ camera.")
            if mode == "local":
                answer = "Máy chủ Thiên Nhãn đang hoạt động." if command_text else "Tôi chưa nghe rõ. Bạn hãy nói lại."
            elif mode == "chat":
                answer = await gemini.generate("simpleConversation", f"Trả lời ngắn gọn bằng tiếng Việt: {command_text}")
            elif mode == "ocr":
                result = await asyncio.to_thread(recognize, image[0], image[1], config)
                if not result["text"]:
                    answer = "Tôi không đọc được chữ rõ ràng trong ảnh."
                elif needs_text_interpretation(command_text):
                    answer = await gemini.generate(
                        "textInterpretation",
                        f"Hãy diễn giải ngắn gọn bằng tiếng Việt nội dung OCR sau theo yêu cầu: {command_text}\n\n{result['text'][:8000]}",
                    )
                else:
                    answer = result["text"]
            else:
                answer = await gemini.generate("visualQuestionAnswering", visual_prompt(command_text), image)
        spoken = answer[:MAX_TTS_CHARS]
        return mp3_response(await synthesize(spoken, config), spoken)

    return app
