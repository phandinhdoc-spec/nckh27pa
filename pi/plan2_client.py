"""Plan 2 turn/capture protocol with the Windows-compatible Python server."""
from __future__ import annotations

import base64
import json
import uuid
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from client import MAX_MP3_BYTES


class Plan2Error(RuntimeError):
    pass


class Plan2Client:
    def __init__(self, url: str, token: str, timeout: int = 60) -> None:
        if not url.startswith(("http://", "https://")) or not token:
            raise ValueError("Cần URL server và token hợp lệ.")
        self.url, self.token, self.timeout = url.rstrip("/"), token, timeout

    def turn(self, text: str, session_id: str, turn_id: str) -> bytes | dict:
        text = text.strip()
        if not text or len(text) > 2000:
            raise Plan2Error("Yêu cầu phải có từ 1 đến 2000 ký tự.")
        return self._post("/plan2/turn", {"sessionId": session_id, "turnId": turn_id, "text": text})

    def capture(self, image: bytes, session_id: str, turn_id: str) -> bytes:
        result = self._post("/plan2/capture", {
            "sessionId": session_id, "turnId": turn_id,
            "image": base64.b64encode(image).decode("ascii"), "imageContentType": "image/jpeg",
        })
        if not isinstance(result, bytes):
            raise Plan2Error("Server không trả âm thanh sau khi chụp ảnh.")
        return result

    def cancel(self, session_id: str, turn_id: str) -> None:
        self._post("/plan2/cancel", {"sessionId": session_id, "turnId": turn_id})

    def _post(self, path: str, body: dict) -> bytes | dict:
        for key in ("sessionId", "turnId"):
            uuid.UUID(body[key])
        request = Request(self.url + path, method="POST",
                          data=json.dumps(body, ensure_ascii=False).encode("utf-8"),
                          headers={"Authorization": "Bearer " + self.token,
                                   "Content-Type": "application/json"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                kind = response.headers.get_content_type()
                raw = response.read(MAX_MP3_BYTES + 1)
                if len(raw) > MAX_MP3_BYTES:
                    raise Plan2Error("Phản hồi server quá lớn.")
                if kind == "audio/mpeg" and raw:
                    if (response.headers.get("X-ThienNhan-Session-Id") != body["sessionId"]
                            or response.headers.get("X-ThienNhan-Turn-Id") != body["turnId"]):
                        raise Plan2Error("Phản hồi không khớp phiên hoặc lượt.")
                    return raw
                if kind == "application/json":
                    result = json.loads(raw)
                    if path == "/plan2/cancel" and isinstance(result, dict) and isinstance(result.get("cancelled"), bool):
                        return result
                    if (not isinstance(result, dict) or result.get("action") != "CAPTUREX"
                            or result.get("sessionId") != body["sessionId"]
                            or result.get("turnId") != body["turnId"]):
                        raise Plan2Error("Lệnh chụp ảnh không hợp lệ.")
                    return result
                raise Plan2Error("Server trả sai định dạng Plan 2.")
        except (HTTPError, URLError, TimeoutError, json.JSONDecodeError) as error:
            raise Plan2Error(f"Lỗi kết nối server Plan 2: {error}") from error
