from __future__ import annotations

import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from config import Config, ConfigError


def _write_env(directory: str, *extra: str) -> Path:
    path = Path(directory) / ".env"
    lines = [
        "MAC_SERVER_URL=https://mac.tailnet.ts.net",
        "SERVER_TOKEN=test-token",
        "GROQ_API_KEY=test-groq-key",
        "BLUETOOTH_DEVICE_MAC=04:60:61:EF:78:22",
        *extra,
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def _load(directory: str, *extra: str) -> Config:
    path = _write_env(directory, *extra)
    with patch.dict(os.environ, {}, clear=True):
        return Config.load(path)


class ConfigDefaultsTests(unittest.TestCase):
    def test_sensor_fields_default_when_absent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = _load(directory)
        self.assertEqual(config.gy25_device, "")
        self.assertEqual(config.gy25_baud, 115200)
        self.assertEqual(config.ms5611_bus, 1)
        self.assertEqual(config.ms5611_addr, 0x76)
        self.assertFalse(config.ms5611_enabled)

    def test_groq_fields_default_when_absent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = _load(directory)
        self.assertEqual(config.groq_stt_model, "whisper-large-v3-turbo")
        self.assertEqual(
            config.groq_stt_url,
            "https://api.groq.com/openai/v1/audio/transcriptions",
        )
        self.assertEqual(config.groq_language, "vi")
        self.assertEqual(config.groq_timeout_seconds, 30)


class ConfigOverrideTests(unittest.TestCase):
    def test_groq_key_from_separate_api_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = _write_env(directory)
            (Path(directory) / ".api").write_text("GROQ_API_KEY=from-api\n", encoding="utf-8")
            with patch.dict(os.environ, {}, clear=True):
                self.assertEqual(Config.load(path).groq_api_key, "from-api")

    def test_api_file_overrides_service_environment(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = _write_env(directory)
            (Path(directory) / ".api").write_text("GROQ_API_KEY=from-api\n", encoding="utf-8")
            with patch.dict(os.environ, {"GROQ_API_KEY": "from-environment"}, clear=True):
                self.assertEqual(Config.load(path).groq_api_key, "from-api")

    def test_sensor_fields_override_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = _load(
                directory,
                "GY25_DEVICE=/dev/ttyAMA0",
                "GY25_BAUD=9600",
                "MS5611_BUS=0",
                "MS5611_ADDR=0x77",
                "MS5611_ENABLED=true",
            )
        self.assertEqual(config.gy25_device, "/dev/ttyAMA0")
        self.assertEqual(config.gy25_baud, 9600)
        self.assertEqual(config.ms5611_bus, 0)
        self.assertEqual(config.ms5611_addr, 0x77)
        self.assertTrue(config.ms5611_enabled)

    def test_groq_fields_override_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = _load(
                directory,
                "GROQ_STT_MODEL=whisper-large-v3",
                "GROQ_STT_URL=https://groq.local/transcribe",
                "GROQ_LANGUAGE=en",
                "GROQ_TIMEOUT_SECONDS=15",
            )
        self.assertEqual(config.groq_stt_model, "whisper-large-v3")
        self.assertEqual(config.groq_stt_url, "https://groq.local/transcribe")
        self.assertEqual(config.groq_language, "en")
        self.assertEqual(config.groq_timeout_seconds, 15)

    def test_ms5611_addr_accepts_decimal(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            config = _load(directory, "MS5611_ADDR=118")
        self.assertEqual(config.ms5611_addr, 0x76)

    def test_boolean_accepts_common_true_false_values(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = _write_env(directory, "MS5611_ENABLED=on")
            with patch.dict(os.environ, {}, clear=True):
                self.assertTrue(Config.load(path).ms5611_enabled)
        with tempfile.TemporaryDirectory() as directory:
            path = _write_env(directory, "MS5611_ENABLED=no")
            with patch.dict(os.environ, {}, clear=True):
                self.assertFalse(Config.load(path).ms5611_enabled)


class ConfigInvalidTests(unittest.TestCase):
    def test_invalid_gy25_baud_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "GY25_BAUD=abc")

    def test_gy25_baud_below_minimum_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "GY25_BAUD=1200")

    def test_invalid_ms5611_addr_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "MS5611_ADDR=xyz")

    def test_ms5611_addr_out_of_range_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "MS5611_ADDR=0x100")

    def test_invalid_ms5611_enabled_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "MS5611_ENABLED=maybe")

    def test_missing_groq_api_key_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".env"
            path.write_text(
                "\n".join(
                    (
                        "MAC_SERVER_URL=https://mac.tailnet.ts.net",
                        "SERVER_TOKEN=test-token",
                        "BLUETOOTH_DEVICE_MAC=04:60:61:EF:78:22",
                    )
                ),
                encoding="utf-8",
            )
            with patch.dict(os.environ, {}, clear=True):
                with self.assertRaises(ConfigError):
                    Config.load(path)

    def test_groq_api_key_read_from_environment(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / ".env"
            path.write_text(
                "\n".join(
                    (
                        "MAC_SERVER_URL=https://mac.tailnet.ts.net",
                        "SERVER_TOKEN=test-token",
                        "BLUETOOTH_DEVICE_MAC=04:60:61:EF:78:22",
                    )
                ),
                encoding="utf-8",
            )
            with patch.dict(os.environ, {"GROQ_API_KEY": "from-env"}, clear=True):
                self.assertEqual(Config.load(path).groq_api_key, "from-env")

    def test_invalid_groq_timeout_raises(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ConfigError):
                _load(directory, "GROQ_TIMEOUT_SECONDS=abc")


if __name__ == "__main__":
    unittest.main()
