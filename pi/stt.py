from __future__ import annotations

import json
import socket
import time
import uuid
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


DEFAULT_URL = "https://api.groq.com/openai/v1/audio/transcriptions"


class GroqError(RuntimeError):
    pass


def transcribe(
    mp3: bytes,
    api_key: str,
    model: str,
    timeout: int,
    *,
    url: str = DEFAULT_URL,
    language: str = "vi",
    retries: int = 1,
) -> str:
    """Gửi đoạn MP3 lên Groq Whisper và trả transcript dạng text."""
    if not mp3:
        raise GroqError("Dữ liệu MP3 đang trống.")
    if not api_key:
        raise GroqError("Thiếu GROQ_API_KEY.")

    fields = {"model": model, "response_format": "json"}
    if language:
        fields["language"] = language
    body, content_type = _multipart(fields, "file", "audio.mp3", "audio/mpeg", mp3)
    request = Request(
        url,
        data=body,
        method="POST",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": content_type,
        },
    )

    for attempt in range(retries + 1):
        try:
            with urlopen(request, timeout=timeout) as response:
                raw = response.read()
            try:
                payload = json.loads(raw.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as error:
                raise GroqError("Groq trả JSON không hợp lệ.") from error
            break
        except HTTPError as error:
            detail = error.read(64 * 1024).decode("utf-8", errors="replace").strip()
            raise GroqError(
                f"Groq trả HTTP {error.code}: {detail or error.reason}"
            ) from error
        except (URLError, TimeoutError, socket.timeout, OSError) as error:
            if attempt >= retries:
                raise GroqError(f"Không thể kết nối Groq: {error}") from error
            time.sleep(min(2**attempt, 3))
    else:
        raise AssertionError("Vòng retry Groq kết thúc ngoài dự kiến.")

    text = payload.get("text") if isinstance(payload, dict) else None
    if not isinstance(text, str):
        raise GroqError("Groq trả JSON thiếu trường text.")
    return text.strip()


def _multipart(
    fields: dict[str, str],
    name: str,
    filename: str,
    content_type: str,
    data: bytes,
) -> tuple[bytes, str]:
    boundary = f"----thiennhan{uuid.uuid4().hex}"
    body = bytearray()
    for key, value in fields.items():
        body.extend(f"--{boundary}\r\n".encode("utf-8"))
        body.extend(f'Content-Disposition: form-data; name="{key}"\r\n\r\n'.encode("utf-8"))
        body.extend(str(value).encode("utf-8"))
        body.extend(b"\r\n")
    body.extend(f"--{boundary}\r\n".encode("utf-8"))
    body.extend(
        f'Content-Disposition: form-data; name="{name}"; filename="{filename}"\r\n'.encode(
            "utf-8"
        )
    )
    body.extend(f"Content-Type: {content_type}\r\n\r\n".encode("utf-8"))
    body.extend(data)
    body.extend(b"\r\n")
    body.extend(f"--{boundary}--\r\n".encode("utf-8"))
    return bytes(body), f"multipart/form-data; boundary={boundary}"
