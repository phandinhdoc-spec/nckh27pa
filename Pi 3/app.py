from __future__ import annotations

import sys
import time
import unicodedata
from pathlib import Path

from config import Config, ConfigError
from devices import (
    DeviceError,
    VoiceStream,
    capture_jpeg,
    connected_wifi,
    mp3_from_pcm,
    play_mp3,
    try_connect_headset_fast,
)
from groq_stt import GroqError, transcribe
from history import History
from mac_client import MacClient, MacClientError
from sensors import read_sensors
from part2_runtime import Runtime


_STARTUP_RETRY_SECONDS = 5
_SYSTEM_SOUNDS = Path(__file__).with_name("sounds")
_WAKE_WORDS = ("thien", "nhan")
_HEADSET_CONNECT_ATTEMPTS = 2
_HEADSET_ATTEMPT_SECONDS = 3
_HELP_PHRASES = ("giup toi", "thien nhan giup toi")
_CONNECT_PHRASES = (
    "ket noi tai nghe",
    "ket noi bluetooth",
    "bat tai nghe",
    "bat bluetooth",
    "mo tai nghe",
)
_VISUAL_PHRASES = (
    " nhin ",
    " xem ",
    " cam ",
    " truoc mat",
    " cai nay",
    " day la gi",
    " man hinh",
    " anh nay",
    " hinh nay",
)


def main() -> int:
    try:
        config = Config.load()
    except ConfigError as error:
        print(f"[KHỞI ĐỘNG] Lỗi cấu hình: {error}", file=sys.stderr, flush=True)
        return 1

    try:
        _startup(config)
    except KeyboardInterrupt:
        print("\nĐã dừng trong lúc khởi động.", flush=True)
        return 130

    client = MacClient(
        config.server_url,
        config.server_token,
        config.command_timeout_seconds,
        config.http_retries,
    )
    history = History(
        Path(__file__).with_name("history"),
        cpu_idle_percent=config.cpu_idle_percent,
        ram_available_percent=config.ram_available_percent,
    )
    stream = VoiceStream(
        config.mic_device,
        config.mic_sample_rate,
        config.silence_seconds,
        config.max_record_seconds,
        config.silence_rms,
    )

    print(
        "[MIC] Nghe liên tục. Nói 'Thiên Nhãn' kèm câu lệnh, hoặc nói 'Thiên Nhãn' "
        "rồi nói câu lệnh ở lượt kế tiếp.",
        flush=True,
    )
    awaiting_command = False
    def conversation(pcm, valid, speaker):
        nonlocal awaiting_command
        next_state = _handle_utterance(pcm, config, client, history,
                                       awaiting_command, valid=valid, speaker=speaker)
        if valid():
            awaiting_command = next_state
    try:
        Runtime(config, client, stream, conversation).run()
    except ImportError as error:
        print(f"[GPIO25] Cần gpiozero: {error}", file=sys.stderr, flush=True)
        return 1
    except KeyboardInterrupt:
        print("Đã dừng.")
    finally:
        stream.close()
        history.close()
    return 0


def _handle_utterance(
    pcm: bytes,
    config: Config,
    client: MacClient,
    history: History,
    awaiting_command: bool,
    valid=lambda: True,
    speaker=None,
) -> bool:
    """Xử lý một đoạn giọng nói; trả về trạng thái 'đang chờ câu lệnh' mới."""
    mp3 = mp3_from_pcm(pcm, config.mic_sample_rate)
    del pcm
    text = transcribe(
        mp3,
        config.groq_api_key,
        config.groq_stt_model,
        config.groq_timeout_seconds,
        url=config.groq_stt_url,
        language=config.groq_language,
    )
    del mp3
    print(f"[GROQ] {text}", flush=True)
    if not valid():
        return False
    if not text:
        return awaiting_command

    heard_wake, command = _split_wake_phrase(text)
    if heard_wake:
        if not command:
            print("[WAKE] Đã nghe 'Thiên Nhãn'; chờ câu lệnh ở lượt nói kế tiếp.", flush=True)
            _announce("recognizing.mp3", config.onboard_playback_device)
            return True
        awaiting_command = False
    elif awaiting_command:
        command = text
        awaiting_command = False
    else:
        print("[WAKE] Bỏ qua vì thiếu từ khóa 'Thiên Nhãn'.", flush=True)
        return False

    if _matches(command, _CONNECT_PHRASES):
        _connect_headset(config)
        return awaiting_command
    if _matches(command, _HELP_PHRASES):
        _announce("usage-guide.mp3", config.onboard_playback_device)
        return False

    _send_command(command, config, client, history, valid=valid, speaker=speaker)
    return False


def _send_command(
    text: str,
    config: Config,
    client: MacClient,
    history: History,
    valid=lambda: True,
    speaker=None,
) -> None:
    plan = client.plan(text)
    if not valid():
        return
    command_text = str(plan["command"])
    mode = str(plan["mode"])
    needs_image = bool(plan["needsImage"])
    print(f"[PHÂN LOẠI] mode={mode}; camera={'bật' if needs_image else 'không cần'}.", flush=True)

    image = None
    if needs_image:
        if not valid():
            return
        _announce("taking-photo.mp3", config.onboard_playback_device)
        print("[CAMERA] Đang chụp ảnh theo nội dung câu lệnh...", flush=True)
        image = capture_jpeg()
        print(f"[CAMERA] Đã chụp {len(image)} byte JPEG.", flush=True)

    transmitted_at = time.time()
    _announce("analyzing.mp3", config.onboard_playback_device)
    sensor_payload = _read_sensor_payload(config)
    print("[LỆNH] Đang gửi transcript (không gửi audio) đến Mac...", flush=True)
    mp3, answer_text = client.command(command_text, image, mode, sensor_payload)
    if not valid():
        return
    print(f"[GEMINI] {answer_text or '(Mac không gửi text)'}", flush=True)
    if image is not None:
        history.add_image(image, transmitted_at)
        del image

    device = config.onboard_playback_device
    print(f"[PHÁT] Đang phát phản hồi ({len(mp3)} byte) qua {device}.", flush=True)
    if speaker is None:
        play_mp3(mp3, device)
    else:
        with speaker:
            if not valid():
                return
            play_mp3(mp3, device, cancelled=lambda: not valid())
    if not valid():
        return
    history.add_sound(mp3, time.time())
    del mp3


def _connect_headset(config: Config) -> None:
    """Chỉ kết nối tai nghe khi người dùng yêu cầu rõ ràng; báo kết quả qua MAX."""
    print("[BLUETOOTH] Có yêu cầu bằng giọng nói; thử kết nối giới hạn.", flush=True)
    if try_connect_headset_fast(
        config.bluetooth_device_mac,
        attempts=_HEADSET_CONNECT_ATTEMPTS,
        attempt_seconds=_HEADSET_ATTEMPT_SECONDS,
    ):
        print(f"[BLUETOOTH] Đã kết nối {config.bluetooth_device_mac}.", flush=True)
        _announce("bluetooth-ready.mp3", config.onboard_playback_device)
    else:
        print(
            f"[BLUETOOTH] Không kết nối được {config.bluetooth_device_mac}.",
            file=sys.stderr,
            flush=True,
        )
        _announce("operation-error.mp3", config.onboard_playback_device)


def _read_sensor_payload(config: Config) -> dict[str, object]:
    """Đọc cảm biến lightweight (GY25 + GY63) mỗi lần gửi lệnh.

    Không bao giờ raise: sensor lỗi/thiếu -> None trong payload để giữ vòng
    chính và request sống (đúng kiến trúc: Pi không chạy AI, chỉ lấy dữ liệu).
    """
    return dict(
        read_sensors(
            config.gy25_device,
            config.gy25_baud,
            config.ms5611_bus,
            config.ms5611_addr,
            config.ms5611_enabled,
        )
    )


def _startup(config: Config) -> None:
    print("[KHỞI ĐỘNG] Bắt đầu kiểm tra hệ thống.", flush=True)
    device = config.onboard_playback_device

    while True:
        print("[1/2] Đang kiểm tra Wi-Fi...", flush=True)
        try:
            wifi = connected_wifi()
            print(f"[1/2] Wi-Fi đã kết nối: {wifi}", flush=True)
            _announce("wifi-connected.mp3", device)
            break
        except DeviceError as error:
            _startup_retry("Wi-Fi", error)

    while True:
        print("[2/2] Đang test camera...", flush=True)
        try:
            image = capture_jpeg()
            print(f"[2/2] Camera hoạt động: {len(image)} byte JPEG", flush=True)
            del image
            _announce("camera-ready.mp3", device)
            break
        except DeviceError as error:
            _startup_retry("Camera", error)

    # Bluetooth KHÔNG được chạm tới khi khởi động hoặc khi rảnh: chỉ kết nối khi
    # người dùng yêu cầu bằng giọng nói. Mọi âm thanh luôn phát qua loa MAX98357A.
    print("[KHỞI ĐỘNG] Hệ thống đã sẵn sàng.", flush=True)
    _announce("system-ready.mp3", device)
    _announce("usage-guide.mp3", device)


def _announce(filename: str, device: str) -> None:
    try:
        data = (_SYSTEM_SOUNDS / filename).read_bytes()
        play_mp3(data, device, timeout=20)
    except (OSError, DeviceError) as error:
        print(f"[ÂM THANH] Không phát được {filename}: {error}", file=sys.stderr, flush=True)


def _startup_retry(component: str, error: DeviceError) -> None:
    print(
        f"[{component}] Chưa sẵn sàng: {error}. Thử lại sau {_STARTUP_RETRY_SECONDS} giây.",
        file=sys.stderr,
        flush=True,
    )
    time.sleep(_STARTUP_RETRY_SECONDS)


def _split_wake_phrase(text: str) -> tuple[bool, str]:
    """Tách 'Thiên Nhãn <câu lệnh>'; khớp không phân biệt hoa/thường và dấu."""
    tokens = text.split()
    folded = [_fold(token).strip(" .,!?:;") for token in tokens]
    for index in range(len(folded) - 1):
        if folded[index] == _WAKE_WORDS[0] and folded[index + 1] == _WAKE_WORDS[1]:
            return True, " ".join(tokens[index + 2 :]).strip(" .,!?:;")
    return False, ""


def _matches(text: str, phrases: tuple[str, ...]) -> bool:
    haystack = f" {_fold(text)} "
    return any(phrase in haystack for phrase in phrases)


def _fold(text: str) -> str:
    return "".join(
        character
        for character in unicodedata.normalize("NFD", text.casefold())
        if unicodedata.category(character) != "Mn"
    )


if __name__ == "__main__":
    raise SystemExit(main())
