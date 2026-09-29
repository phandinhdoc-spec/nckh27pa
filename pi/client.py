"""Plan 1 text-only HTTP contract for the future Windows Python server."""
from __future__ import annotations

import base64
import json
import uuid
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

MAX_MP3_BYTES = 10 * 1024 * 1024


class Plan1Error(RuntimeError):
    pass


class Plan1Client:
    def __init__(self, url: str, token: str, timeout: int = 60) -> None:
        if not url.startswith(("http://", "https://")) or not token:
            raise ValueError("Cần URL server và token hợp lệ.")
        self.url = url.rstrip("/")
        self.token = token
        self.timeout = timeout

    def turn(self, text: str, session_id: str, turn_id: str) -> tuple[bytes, str]:
        text = text.strip()
        if not text or len(text) > 2000:
            raise Plan1Error("Yêu cầu phải có từ 1 đến 2000 ký tự.")
        for value in (session_id, turn_id):
            uuid.UUID(value)
        body = {"sessionId": session_id, "turnId": turn_id, "text": text}
        request = self._request("/plan1/turn", body)
        try:
            with urlopen(request, timeout=self.timeout) as response:
                if response.headers.get_content_type() != "audio/mpeg":
                    raise Plan1Error("Server không trả MP3.")
                if (response.headers.get("X-ThienNhan-Session-Id") != session_id
                        or response.headers.get("X-ThienNhan-Turn-Id") != turn_id):
                    raise Plan1Error("Phản hồi không khớp phiên hoặc lượt.")
                audio = response.read(MAX_MP3_BYTES + 1)
                if not audio or len(audio) > MAX_MP3_BYTES:
                    raise Plan1Error("MP3 trống hoặc vượt giới hạn.")
                encoded = response.headers.get("X-ThienNhan-Text-Base64", "")
                answer = base64.b64decode(encoded, validate=True).decode("utf-8") if encoded else ""
                return audio, answer
        except (HTTPError, URLError, TimeoutError) as error:
            raise Plan1Error(f"Lỗi kết nối server Plan 1: {error}") from error
        except (ValueError, UnicodeDecodeError) as error:
            raise Plan1Error("Header văn bản trả lời không hợp lệ.") from error

    def cancel(self, session_id: str, turn_id: str) -> None:
        request = self._request("/conversation/cancel", {"sessionId": session_id, "turnId": turn_id})
        try:
            with urlopen(request, timeout=min(self.timeout, 5)) as response:
                if response.headers.get_content_type() != "application/json":
                    raise Plan1Error("Server trả sai định dạng cho lệnh hủy.")
                response.read(1024)
        except (HTTPError, URLError, TimeoutError) as error:
            raise Plan1Error(f"Không gửi được lệnh hủy: {error}") from error

    def _request(self, path: str, payload: dict[str, str]) -> Request:
        return Request(
            self.url + path,
            data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
            headers={"Authorization": "Bearer " + self.token, "Content-Type": "application/json"},
            method="POST",
        )
