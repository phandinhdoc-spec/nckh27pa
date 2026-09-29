from dataclasses import dataclass
import json
import re
import unicodedata


ROUTE_GUIDANCE_UNAVAILABLE = (
    "Chức năng hướng dẫn đường chưa được hiệu chuẩn thực nghiệm; hiện chưa thể đưa ra "
    "khoảng cách hay chỉ dẫn an toàn. Xin đừng dựa vào thiết bị để đi đường."
)
MODES = {"local", "auto", "chat", "ocr", "observe"}


@dataclass(frozen=True)
class Plan:
    command: str
    mode: str

    @property
    def needs_image(self) -> bool:
        return self.mode in {"ocr", "observe"}

    def as_response(self) -> dict:
        return {"command": self.command, "mode": self.mode, "needsImage": self.needs_image}


def normalize(text: str) -> str:
    text = text.replace("đ", "d").replace("Đ", "d")
    return "".join(c for c in unicodedata.normalize("NFD", text.lower())
                   if unicodedata.category(c) != "Mn")


def strip_wake_word(text: str) -> str:
    text = text.strip()
    if text.lower().startswith("thiên nhãn"):
        return text[len("thiên nhãn"):].strip(" \t\r\n,.!?;:")
    return text


def route_guidance_intent(text: str) -> bool:
    plain = normalize(text)
    return ("huong dan" in plain and "di duong" in plain) or (
        "thong bao" in plain and "gio" in plain and "di duong" in plain
    )


def rule_plan(speech: str) -> Plan:
    command = strip_wake_word(speech)
    if route_guidance_intent(command):
        return Plan(command, "local")
    plain = normalize(command)
    words = set(re.findall(r"[^\W\d_]+", plain, re.UNICODE))
    if not plain or any(x in plain for x in ("kiem tra he thong", "trang thai he thong", "may chu")):
        return Plan(command, "local")
    if words & {"doc", "chu", "ocr"} or any(x in plain for x in ("van ban", "bien bao", "nhan san pham")):
        return Plan(command, "ocr")
    visual = (
        "phia truoc", "truoc mat", "day la gi", "day la cai gi", "vat gi", "mau gi", "ai vay",
        "dang cam", "ben trai", "ben phai", "gan toi", "bat camera", "mo camera",
        "chup anh", "nhin", "quan sat", "mo ta", "xung quanh", "chuong ngai",
        "loi di", "duong di", "bac thang", "cau thang",
    )
    if "nhin" in words or any(x in plain for x in visual):
        return Plan(command, "observe")
    return Plan(command, "chat")


def camera_prompt(speech: str) -> str:
    return f"""Bạn là bộ định tuyến camera cho thiết bị hỗ trợ người khiếm thị.
Chỉ phân loại yêu cầu, không trả lời yêu cầu của người dùng.
Camera=yes khi cần hình ảnh hiện tại để trả lời, ví dụ: nhận diện vật/người/màu sắc,
mô tả cảnh vật, đọc chữ/biển báo/nhãn, tìm chướng ngại hoặc hỗ trợ di chuyển.
Camera=no với trò chuyện, kiến thức chung, thời gian, tính toán hoặc yêu cầu không cần nhìn.
Chỉ trả đúng một JSON một dòng, không Markdown, không giải thích:
{{"camera":"yes|no","mode":"ocr|observe|chat|local","text":"nguyên văn yêu cầu"}}
Dùng mode=ocr khi cần đọc chữ; mode=observe cho các nhu cầu hình ảnh khác;
mode=chat hoặc local khi camera=no. camera=yes chỉ đi cùng ocr hoặc observe.
Yêu cầu người dùng: {speech}"""


def parse_camera_plan(response: str, speech: str) -> Plan | None:
    start, end = response.find("{"), response.rfind("}")
    if start < 0 or end < start:
        return None
    try:
        decision = json.loads(response[start:end + 1])
        mode, camera = decision["mode"], decision["camera"].lower()
        if not isinstance(decision["text"], str) or not decision["text"].strip():
            return None
        if mode not in MODES - {"auto"} or camera not in {"yes", "no"}:
            return None
        if (camera == "yes") != (mode in {"ocr", "observe"}):
            return None
        return Plan(strip_wake_word(speech), mode)
    except (ValueError, KeyError, TypeError, AttributeError):
        return None


async def smart_plan(speech: str, gemini) -> Plan:
    fallback = rule_plan(speech)
    if route_guidance_intent(speech):
        return fallback
    try:
        result = parse_camera_plan(await gemini.generate("quickClassification", camera_prompt(speech)), speech)
        if result:
            return fallback if fallback.needs_image and not result.needs_image else result
    except Exception:
        pass
    return fallback


async def strict_conversation_plan(speech: str, gemini) -> Plan:
    fallback = rule_plan(speech)
    if route_guidance_intent(speech):
        return fallback
    prompt = f"""Decide whether answering the user's request strictly requires a new camera image.
Return exactly one JSON object with these fields and no Markdown:
{{"decision":"CAPTUREX|NOCAPTUREX","mode":"ocr|observe|chat|local","text":"original request"}}
Use CAPTUREX only with ocr or observe. Use NOCAPTUREX only with chat or local.
Use ocr for reading visible text and observe for other visual questions.
Request: {speech}"""
    response = await gemini.generate("quickClassification", prompt)
    try:
        decision = json.loads(response)
        if not isinstance(decision, dict) or set(decision) != {"decision", "mode", "text"}:
            raise ValueError("invalid fields")
        code, mode = decision["decision"], decision["mode"]
        if not isinstance(decision["text"], str) or not decision["text"].strip():
            raise ValueError("empty text")
        if code == "CAPTUREX" and mode in {"ocr", "observe"}:
            return Plan(strip_wake_word(speech), mode)
        if code == "NOCAPTUREX" and mode in {"chat", "local"} and not fallback.needs_image:
            return Plan(strip_wake_word(speech), mode)
    except (TypeError, ValueError, KeyError):
        pass
    # A conflicting or malformed decision must neither capture nor answer a
    # visual question from text alone.
    if fallback.needs_image:
        raise ValueError("Gemini camera decision conflicts with a visual request.")
    else:
        raise ValueError("Gemini camera decision is invalid; capture is not authorized.")


async def plan2_decision(speech: str, gemini) -> Plan | None:
    """Return a camera plan, text plan, or None to ask for clarification.

    Only a clear visual request may authorize capture when Gemini's response is
    malformed. Never silently turn an ambiguous request into a camera request.
    """
    command = strip_wake_word(speech)
    if route_guidance_intent(command):
        return Plan(command, "local")
    prompt = (
        "Decide if a NEW camera image is necessary to answer this request. "
        "Return exactly one JSON object, no Markdown or other text: "
        '{"decision":"CAPTUREX|NOCAPTUREX","mode":"ocr|observe|chat","text":"original request"}. '
        "CAPTUREX only with ocr (read one visible page/text) or observe (find an object, "
        "describe the scene, or a how-to grounded in what is visible). "
        "NOCAPTUREX only with chat (general Q&A or nonvisual how-to). "
        "If the request is unclear, choose NOCAPTUREX/chat and ask for clarification; "
        "never invent visual details or navigation/device-control instructions. "
        f"Request: {speech}"
    )
    try:
        parsed = json.loads(await gemini.generate("quickClassification", prompt))
        if not isinstance(parsed, dict) or set(parsed) != {"decision", "mode", "text"}:
            raise ValueError("invalid fields")
        if not isinstance(parsed["text"], str) or not parsed["text"].strip():
            raise ValueError("missing original request")
        pair = parsed["decision"], parsed["mode"]
        if pair in {("CAPTUREX", "ocr"), ("CAPTUREX", "observe")}:
            return Plan(command, pair[1])
        if pair == ("NOCAPTUREX", "chat"):
            # A clear visual request cannot be answered from text alone.
            if clear_visual_plan(speech) is not None:
                return clear_visual_plan(speech)
            return Plan(command, "chat")
    except (ValueError, TypeError, KeyError):
        pass
    return clear_visual_plan(speech)


def clear_visual_plan(speech: str) -> Plan | None:
    """Conservative fallback; generic words such as 'read' alone do not authorize a photo."""
    command = strip_wake_word(speech)
    plain = normalize(command)
    if any(term in plain for term in (
        "doc trang", "doc chu tren", "doc noi dung tren", "doc bien bao", "doc nhan",
        "van ban nay", "trang giay nay", "chu trong anh",
    )):
        return Plan(command, "ocr")
    if any(term in plain for term in (
        "truoc mat", "phia truoc", "trong anh", "qua camera", "day la cai gi",
        "day la gi", "tim cai", "tim chiec", "tim vat", "vat trong tay",
        "nhin vat", "nhin xem", "mo ta canh", "xung quanh toi",
    )):
        return Plan(command, "observe")
    return None


def visual_prompt(command: str) -> str:
    return (
        "Trả lời ngắn gọn, rõ ràng bằng tiếng Việt cho người khiếm thị.\n"
        "Nếu ảnh có bàn tay đang cầm hoặc đưa một vật về phía camera, hãy coi vật trong tay\n"
        "là đối tượng chính: ưu tiên nhận diện và mô tả vật đó, bỏ qua các vật ở bên cạnh và hậu cảnh.\n"
        "Chỉ mô tả toàn cảnh khi không có vật được cầm hoặc yêu cầu người dùng hỏi rõ về xung quanh.\n"
        "Nếu vật bị che, mờ hoặc không đủ chắc chắn, hãy nói mức độ không chắc chắn thay vì đoán.\n\n"
        f"Yêu cầu người dùng: {command}"
    )


def needs_text_interpretation(command: str) -> bool:
    return any(x in normalize(command) for x in ("giai thich", "tom tat", "noi dung", "y nghia"))
