from dataclasses import dataclass
import os
from pathlib import Path


def _read_dotenv(path: Path) -> dict[str, str]:
    if not path.is_file():
        return {}
    result = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:]
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        result[key.strip()] = value
    return result


@dataclass(frozen=True)
class Config:
    host: str
    port: int
    token: str
    gemini_key: str
    fast_model: str
    vision_model: str
    reasoning_model: str
    fallback_model: str
    timeout: int
    tts_voice: str
    tesseract_cmd: str | None
    ffmpeg_cmd: str

    @classmethod
    def load(cls, path: Path | None = None) -> "Config":
        root = Path(__file__).resolve().parents[1]
        values = _read_dotenv(path or root / ".env")
        # API credentials live in a separate, git-ignored file. Environment
        # variables still take precedence for deployments and tests.
        values.update({key: value for key, value in _read_dotenv(root / ".api").items() if value})
        values.update(os.environ)

        def get(key: str, default: str = "") -> str:
            return values.get(key, default).strip()

        def required(key: str) -> str:
            value = get(key)
            if not value:
                raise ValueError(f"Thiếu biến cấu hình bắt buộc: {key}")
            return value

        port_text = get("SERVER_PORT", "8765")
        timeout_text = get("REQUEST_TIMEOUT_SECONDS", "30")
        try:
            port, timeout = int(port_text), int(timeout_text)
        except ValueError as error:
            raise ValueError("SERVER_PORT hoặc REQUEST_TIMEOUT_SECONDS không hợp lệ") from error
        if not 1 <= port <= 65535 or timeout <= 0:
            raise ValueError("SERVER_PORT hoặc REQUEST_TIMEOUT_SECONDS không hợp lệ")
        return cls(
            get("SERVER_HOST", "0.0.0.0"), port, required("SERVER_TOKEN"),
            required("GEMINI_API_KEY"), required("GEMINI_MODEL_FAST"),
            required("GEMINI_MODEL_VISION"), required("GEMINI_MODEL_REASONING"),
            required("GEMINI_MODEL_FALLBACK"), timeout,
            get("TTS_VOICE", "vi-VN-HoaiMyNeural"),
            get("TESSERACT_CMD") or None, get("FFMPEG_CMD", "ffmpeg"),
        )
