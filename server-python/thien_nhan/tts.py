import asyncio
import base64
import os
import subprocess
import tempfile

import httpx
from fastapi.responses import Response

from .config import Config
from .errors import APIError


MAX_TTS_CHARS = 2000


async def synthesize(text: str, config: Config) -> bytes:
    source = await _gemini_audio(text, config)
    try:
        return await asyncio.to_thread(_normalize_mp3, source, config)
    except FileNotFoundError as error:
        raise APIError(503, "tts_encoder_unavailable", "Bộ mã hóa âm thanh chưa sẵn sàng.") from error
    except subprocess.TimeoutExpired as error:
        raise APIError(504, "tts_timeout", "Tạo giọng nói phản hồi quá chậm.") from error
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        raise APIError(500, "tts_failed", "Không thể tạo file âm thanh.") from error


async def _gemini_audio(text: str, config: Config) -> bytes:
    body = {
        "contents": [{"role": "user", "parts": [{"text": text}]}],
        "generationConfig": {"responseModalities": ["AUDIO"],
                             "speechConfig": {"voiceConfig": {"voice": "Kore"}}},
    }
    url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.8-flash-tts:generateContent"
    try:
        async with httpx.AsyncClient(timeout=config.timeout) as client:
            response = await client.post(url, headers={"x-goog-api-key": config.gemini_key}, json=body)
        response.raise_for_status()
        encoded = response.json()["candidates"][0]["content"]["parts"][0]["inlineData"]["data"]
        audio = base64.b64decode(encoded, validate=True)
        if not audio.startswith(b"RIFF"):
            raise ValueError("Gemini không trả WAV.")
        return audio
    except (httpx.HTTPError, KeyError, IndexError, ValueError) as error:
        raise APIError(502, "gemini_tts_unavailable", "Gemini không tạo được giọng nói.") from error


def _normalize_mp3(source: bytes, config: Config) -> bytes:
    with tempfile.TemporaryDirectory(prefix="thiennhan-tts-") as directory:
        target = os.path.join(directory, "speech.mp3")
        subprocess.run(
            [config.ffmpeg_cmd, "-hide_banner", "-loglevel", "error", "-y",
             "-i", "pipe:0", "-ac", "1", "-ar", "22050", "-b:a", "64k", target],
            input=source, capture_output=True, check=True, timeout=config.timeout,
        )
        with open(target, "rb") as file:
            result = file.read()
        if not result:
            raise ValueError("empty output")
        return result


def mp3_response(audio: bytes, text: str | None = None) -> Response:
    headers = {}
    if text is not None:
        headers["X-ThienNhan-Text-Base64"] = base64.b64encode(text.encode("utf-8")).decode("ascii")
    return Response(audio, media_type="audio/mpeg", headers=headers)
