from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


DEFAULT_GROQ_STT_URL = "https://api.groq.com/openai/v1/audio/transcriptions"


class ConfigError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class Config:
    server_url: str
    server_token: str
    onboard_playback_device: str
    bluetooth_device_mac: str
    mic_device: str
    mic_sample_rate: int
    silence_seconds: int
    silence_rms: int
    max_record_seconds: int
    groq_api_key: str
    groq_stt_model: str
    groq_stt_url: str
    groq_language: str
    groq_timeout_seconds: int
    command_timeout_seconds: int
    http_retries: int
    cpu_idle_percent: int
    ram_available_percent: int
    gy25_device: str
    gy25_baud: int
    ms5611_bus: int
    ms5611_addr: int
    ms5611_enabled: bool

    @property
    def bluetooth_playback_device(self) -> str:
        return f"bluealsa:DEV={self.bluetooth_device_mac},PROFILE=a2dp"

    @classmethod
    def load(cls, path: Path | None = None) -> "Config":
        env_path = path or Path(__file__).with_name(".env")
        values = _read_env(env_path)
        # A populated .api is authoritative even when systemd exports .env.
        # A blank template must not shadow an existing .env value.
        api_key = _read_env(env_path.with_name(".api")).get("GROQ_API_KEY", "").strip()
        values.update(os.environ)
        if api_key:
            values["GROQ_API_KEY"] = api_key

        server_url = _required(values, "MAC_SERVER_URL").rstrip("/")
        if not server_url.startswith(("http://", "https://")):
            raise ConfigError("MAC_SERVER_URL phải bắt đầu bằng http:// hoặc https://")

        return cls(
            server_url=server_url,
            server_token=_required(values, "SERVER_TOKEN"),
            onboard_playback_device=values.get("ONBOARD_PLAYBACK_DEVICE", "default").strip()
            or "default",
            bluetooth_device_mac=_required(values, "BLUETOOTH_DEVICE_MAC"),
            mic_device=values.get("MIC_DEVICE", "default").strip() or "default",
            mic_sample_rate=_integer(values, "MIC_SAMPLE_RATE", 16000, minimum=1),
            silence_seconds=_integer(values, "SILENCE_SECONDS", 2, minimum=1),
            silence_rms=_integer(values, "SILENCE_RMS", 500, minimum=1),
            max_record_seconds=_integer(values, "MAX_RECORD_SECONDS", 30, minimum=3),
            groq_api_key=_required(values, "GROQ_API_KEY"),
            groq_stt_model=values.get("GROQ_STT_MODEL", "whisper-large-v3-turbo").strip()
            or "whisper-large-v3-turbo",
            groq_stt_url=values.get("GROQ_STT_URL", DEFAULT_GROQ_STT_URL).strip()
            or DEFAULT_GROQ_STT_URL,
            groq_language=values.get("GROQ_LANGUAGE", "vi").strip(),
            groq_timeout_seconds=_integer(values, "GROQ_TIMEOUT_SECONDS", 30, minimum=1),
            command_timeout_seconds=_integer(
                values, "COMMAND_TIMEOUT_SECONDS", 60, minimum=1
            ),
            http_retries=_integer(values, "HTTP_RETRIES", 2, minimum=0),
            cpu_idle_percent=_integer(values, "CPU_IDLE_PERCENT", 80, minimum=0, maximum=100),
            ram_available_percent=_integer(
                values,
                "RAM_AVAILABLE_PERCENT",
                15,
                minimum=1,
                maximum=100,
            ),
            gy25_device=values.get("GY25_DEVICE", "").strip(),
            gy25_baud=_integer(values, "GY25_BAUD", 115200, minimum=9600),
            ms5611_bus=_integer(values, "MS5611_BUS", 1, minimum=0),
            ms5611_addr=_hex(values, "MS5611_ADDR", 0x76),
            ms5611_enabled=_bool(values, "MS5611_ENABLED", False),
        )


def _read_env(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}

    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:].lstrip()
        key, separator, value = line.partition("=")
        if not separator:
            raise ConfigError(f"Dòng .env không hợp lệ: {raw_line}")
        key, value = key.strip(), value.strip()
        if not key:
            raise ConfigError(f"Dòng .env không hợp lệ: {raw_line}")
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value
    return values


def _required(values: dict[str, str], key: str) -> str:
    value = values.get(key, "").strip()
    if not value:
        raise ConfigError(f"Thiếu cấu hình bắt buộc: {key}")
    return value


def _integer(
    values: dict[str, str],
    key: str,
    default: int,
    *,
    minimum: int,
    maximum: int | None = None,
) -> int:
    raw = values.get(key, str(default)).strip()
    try:
        value = int(raw)
    except ValueError as error:
        raise ConfigError(f"{key} không phải số nguyên: {raw}") from error
    if value < minimum or maximum is not None and value > maximum:
        limit = f"{minimum}..{maximum}" if maximum is not None else f">= {minimum}"
        raise ConfigError(f"{key} phải nằm trong khoảng {limit}: {raw}")
    return value


def _hex(values: dict[str, str], key: str, default: int) -> int:
    raw = values.get(key, "").strip()
    if not raw:
        return default
    try:
        value = int(raw, 0)  # accepts 0x76 and 118
    except ValueError as error:
        raise ConfigError(f"{key} không phải số hex: {raw}") from error
    if not 0 <= value <= 0xFF:
        raise ConfigError(f"{key} phải nằm trong 0x00..0xFF: {raw}")
    return value


def _bool(values: dict[str, str], key: str, default: bool) -> bool:
    raw = values.get(key, "").strip().lower()
    if not raw:
        return default
    if raw in ("1", "true", "yes", "on"):
        return True
    if raw in ("0", "false", "no", "off"):
        return False
    raise ConfigError(f"{key} không phải giá trị đúng/sai: {raw}")
