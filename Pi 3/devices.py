from __future__ import annotations

import os
import select
import shutil
import subprocess
import sys
import tempfile
import time
from array import array
from collections import deque
from math import isqrt
from pathlib import Path
from typing import Callable, Iterator


class DeviceError(RuntimeError):
    pass


_CAMERA_RAM_DIR = Path("/dev/shm")
_CAMERA_WIDTH = 1280
_CAMERA_HEIGHT = 720
_CAMERA_TIMEOUT_MS = 500
_DEFAULT_MIC_SAMPLE_RATE = 16000


def connected_wifi() -> str:
    network_root = Path("/sys/class/net")
    try:
        interfaces = sorted(path.parent.name for path in network_root.glob("*/wireless"))
    except OSError as error:
        raise DeviceError(f"Không đọc được trạng thái Wi-Fi: {error}") from error
    if not interfaces:
        raise DeviceError("Không tìm thấy giao diện Wi-Fi.")

    for interface in interfaces:
        base = network_root / interface
        try:
            carrier = (base / "carrier").read_text(encoding="ascii").strip()
            state = (base / "operstate").read_text(encoding="ascii").strip()
        except OSError:
            continue
        if carrier == "1" and state == "up":
            ssid = _wifi_ssid(interface)
            return f"{interface} ({ssid})" if ssid else interface
    raise DeviceError("Wi-Fi chưa kết nối.")


_MIC_CHUNK_SECONDS = 0.1
_MIC_CALIBRATION_CHUNKS = 5
_MIC_TRIGGER_CHUNKS = 3
_MIC_PRE_ROLL_CHUNKS = 3


class VoiceStream:
    """Luồng mic USB luôn mở + VAD cục bộ, cắt ra từng đoạn có giọng nói.

    Không mở/đóng arecord mỗi lượt: chỉ spawn một lần, đọc liên tục và phát ra
    PCM mono S16_LE cho mỗi utterance để tầng trên mã hoá MP3 và gọi Groq.
    """

    def __init__(
        self,
        device: str,
        sample_rate: int = _DEFAULT_MIC_SAMPLE_RATE,
        silence_seconds: int = 2,
        max_seconds: int = 30,
        silence_rms: int = 500,
    ) -> None:
        self.device = device
        self.sample_rate = sample_rate
        self.silence_seconds = silence_seconds
        self.max_seconds = max_seconds
        self.silence_rms = silence_rms
        self._process: subprocess.Popen[bytes] | None = None
        self._stderr = None

    def start(self) -> None:
        if self._process is not None:
            return
        command = [
            _command("arecord"),
            "-q",
            "-D",
            self.device,
            "-f",
            "S16_LE",
            "-r",
            str(self.sample_rate),
            "-c",
            "1",
            "-t",
            "raw",
            "-",
        ]
        self._stderr = tempfile.TemporaryFile()
        try:
            self._process = subprocess.Popen(
                command, stdout=subprocess.PIPE, stderr=self._stderr
            )
        except OSError as error:
            self._stderr.close()
            self._stderr = None
            raise DeviceError(f"Không thể chạy arecord: {error}") from error
        print(
            f"[MIC] Đã mở luồng mic {self.device} @ {self.sample_rate} Hz (luôn mở).",
            flush=True,
        )

    def close(self) -> None:
        process, self._process = self._process, None
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.communicate(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
        if self._stderr is not None:
            self._stderr.close()
            self._stderr = None

    def segments(self) -> Iterator[bytes]:
        """Sinh PCM cho từng đoạn có giọng nói; tự mở lại mic nếu arecord chết."""
        chunk_bytes = max(2, int(self.sample_rate * 2 * _MIC_CHUNK_SECONDS))
        while True:
            try:
                self.start()
                yield from self._read_segments(chunk_bytes)
            except DeviceError as error:
                print(f"[MIC] {error}", file=sys.stderr, flush=True)
            self.close()
            time.sleep(1)

    def _read_segments(self, chunk_bytes: int) -> Iterator[bytes]:
        assert self._process is not None and self._process.stdout is not None
        descriptor = self._process.stdout.fileno()
        noise_rms = self._measure_noise(descriptor, chunk_bytes)
        threshold = max(self.silence_rms, noise_rms * 2)
        print(f"[MIC] RMS nền {noise_rms}, ngưỡng giọng nói {threshold}.", flush=True)
        pre_roll: deque[bytes] = deque(maxlen=_MIC_PRE_ROLL_CHUNKS)
        while True:
            utterance = self._capture_utterance(descriptor, chunk_bytes, threshold, pre_roll)
            if utterance is None:
                return
            yield utterance

    def _measure_noise(self, descriptor: int, chunk_bytes: int) -> int:
        levels: list[int] = []
        for _ in range(_MIC_CALIBRATION_CHUNKS):
            chunk = self._read_chunk(descriptor, chunk_bytes, None)
            if not chunk:
                raise DeviceError("Mic USB ngừng cấp dữ liệu khi đo nền.")
            levels.append(_pcm_rms(chunk))
        return min(levels) if levels else 0

    def _capture_utterance(
        self,
        descriptor: int,
        chunk_bytes: int,
        threshold: int,
        pre_roll: deque[bytes],
    ) -> bytes | None:
        chunks_per_second = int(1 / _MIC_CHUNK_SECONDS)
        voice_chunks = 0
        while voice_chunks < _MIC_TRIGGER_CHUNKS:
            chunk = self._read_chunk(descriptor, chunk_bytes, None)
            if not chunk:
                return None
            pre_roll.append(chunk)
            voice_chunks = voice_chunks + 1 if _pcm_rms(chunk) >= threshold else 0

        pcm = bytearray(b"".join(pre_roll))
        quiet_chunks = 0
        deadline = time.monotonic() + self.max_seconds
        while quiet_chunks < self.silence_seconds * chunks_per_second:
            chunk = self._read_chunk(descriptor, chunk_bytes, deadline)
            if not chunk:
                break
            pcm.extend(chunk)
            quiet_chunks = 0 if _pcm_rms(chunk) >= threshold else quiet_chunks + 1
        return bytes(pcm)

    def _read_chunk(
        self, descriptor: int, chunk_bytes: int, deadline: float | None
    ) -> bytes:
        data = bytearray()
        while len(data) < chunk_bytes:
            timeout = 0.2
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return bytes(data)
                timeout = min(timeout, remaining)
            readable, _, _ = select.select([descriptor], [], [], timeout)
            if not readable:
                continue
            block = os.read(descriptor, chunk_bytes - len(data))
            if not block:
                return bytes(data)
            data.extend(block)
        return bytes(data)


def _pcm_rms(data: bytes) -> int:
    samples = array("h")
    samples.frombytes(data[: len(data) // 2 * 2])
    if sys.byteorder != "little":
        samples.byteswap()
    return isqrt(sum(sample * sample for sample in samples) // len(samples)) if samples else 0


def mp3_from_pcm(
    pcm: bytes,
    sample_rate: int = _DEFAULT_MIC_SAMPLE_RATE,
    timeout: int = 30,
) -> bytes:
    if not pcm:
        raise DeviceError("Dữ liệu PCM đang trống.")
    command = [
        _command("lame"),
        "--silent",
        "-r",
        "-s",
        str(sample_rate),
        "-m",
        "m",
        "--preset",
        "voice",
        "-",
        "-",
    ]
    try:
        result = subprocess.run(
            command,
            input=pcm,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise DeviceError("Mã hoá MP3 quá thời gian cho phép.") from error
    except OSError as error:
        raise DeviceError(f"Không thể chạy lame: {error}") from error
    if result.returncode != 0:
        raise DeviceError(_message(command[0], result.stderr))
    if not result.stdout:
        raise DeviceError("lame trả file MP3 rỗng.")
    return result.stdout


def capture_jpeg() -> bytes:
    camera = shutil.which("rpicam-jpeg")
    if camera is None:
        raise DeviceError("Không tìm thấy lệnh rpicam-jpeg.")
    try:
        handle = tempfile.NamedTemporaryFile(
            prefix="thiennhan-camera-",
            suffix=".jpg",
            dir=_CAMERA_RAM_DIR,
            delete=False,
        )
        path = Path(handle.name)
        handle.close()
    except OSError as error:
        raise DeviceError(f"Không tạo được vùng RAM cho camera: {error}") from error

    try:
        _run(
            [
                camera,
                "--nopreview",
                "--timeout",
                str(_CAMERA_TIMEOUT_MS),
                "--width",
                str(_CAMERA_WIDTH),
                "--height",
                str(_CAMERA_HEIGHT),
                "--output",
                str(path),
            ],
            timeout=10,
            empty_message=None,
        )
        data = path.read_bytes()
    except OSError as error:
        raise DeviceError(f"Không đọc được ảnh camera trong RAM: {error}") from error
    finally:
        path.unlink(missing_ok=True)
    if not data:
        raise DeviceError("Camera tạo file JPEG rỗng.")
    if not data.startswith(b"\xff\xd8"):
        raise DeviceError("Camera trả dữ liệu không phải JPEG.")
    return data


def headset_connected(mac: str) -> bool:
    """Trả True nếu tai nghe đã kết nối (không chạy lệnh khác)."""
    try:
        bluetoothctl = _command("bluetoothctl")
        info = _bluetooth_info(bluetoothctl, mac)
        return "Connected: yes" in info
    except DeviceError:
        return False


def try_connect_headset_fast(mac: str, attempts: int = 2, attempt_seconds: int = 3) -> bool:
    """Thử kết nối tai nghe NHANH (tối đa attempts x attempt_seconds giây).

    Không scan/pair lâu; nếu chưa trust thì trust và connect ngắn gọn.
    Trả True nếu kết nối thành công trong thời gian giới hạn, False nếu không.
    """
    try:
        bluetoothctl = _command("bluetoothctl")
    except DeviceError:
        return False

    try:
        _bluetoothctl(bluetoothctl, 3, "power", "on", allow_failure=True)
        _bluetoothctl(bluetoothctl, 3, "trust", mac, allow_failure=True)
    except DeviceError:
        pass

    for _ in range(max(1, attempts)):
        try:
            _bluetoothctl(bluetoothctl, attempt_seconds, "connect", mac, allow_failure=True)
        except DeviceError:
            continue
        if "Connected: yes" in _bluetooth_info(bluetoothctl, mac):
            return True
    return False


def play_mp3(data: bytes, device: str, timeout: int = 120,
             cancelled: Callable[[], bool] | None = None) -> None:
    if not data:
        raise DeviceError("Dữ liệu MP3 đang trống.")
    command = [_command("mpg123"), "-q", "-a", device, "-"]
    if cancelled is not None:
        if cancelled():
            return
        # A seekable temporary input avoids blocking on a full stdin pipe.
        with tempfile.TemporaryFile() as source, tempfile.TemporaryFile() as errors:
            source.write(data)
            source.seek(0)
            try:
                process = subprocess.Popen(command, stdin=source,
                                           stdout=subprocess.DEVNULL, stderr=errors)
            except OSError as error:
                raise DeviceError(f"Không thể chạy mpg123: {error}") from error
            deadline = time.monotonic() + timeout
            try:
                while process.poll() is None:
                    if cancelled() or time.monotonic() >= deadline:
                        process.terminate()
                        try:
                            process.wait(timeout=1)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
                        if not cancelled():
                            raise DeviceError("Phát MP3 quá thời gian cho phép.")
                        return
                    try:
                        process.wait(timeout=0.1)
                    except subprocess.TimeoutExpired:
                        pass
                if process.returncode != 0 and not cancelled():
                    errors.seek(0)
                    raise DeviceError(_message(command[0], errors.read()))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        return
    try:
        result = subprocess.run(
            command,
            input=data,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise DeviceError("Phát MP3 quá thời gian cho phép.") from error
    except OSError as error:
        raise DeviceError(f"Không thể chạy mpg123: {error}") from error
    if result.returncode != 0:
        raise DeviceError(_message(command[0], result.stderr))


def _run(command: list[str], *, timeout: int, empty_message: str | None) -> bytes:
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise DeviceError(f"{command[0]} quá thời gian cho phép.") from error
    except OSError as error:
        raise DeviceError(f"Không thể chạy {command[0]}: {error}") from error
    if result.returncode != 0:
        raise DeviceError(_message(command[0], result.stderr))
    if empty_message is not None and not result.stdout:
        raise DeviceError(empty_message)
    return result.stdout


def _command(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise DeviceError(f"Không tìm thấy lệnh hệ thống: {name}")
    return path


def _wifi_ssid(interface: str) -> str:
    iwgetid = shutil.which("iwgetid")
    if iwgetid is None:
        return ""
    try:
        result = subprocess.run(
            [iwgetid, interface, "--raw"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.decode("utf-8", errors="replace").strip() if result.returncode == 0 else ""


def _bluetoothctl(
    executable: str,
    timeout: int,
    *arguments: str,
    allow_failure: bool = False,
    agent: bool = False,
) -> str:
    command = [executable, "--timeout", str(timeout)]
    if agent:
        command += ["--agent", "NoInputNoOutput"]
    command += arguments
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout + 2,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise DeviceError("bluetoothctl quá thời gian cho phép.") from error
    except OSError as error:
        raise DeviceError(f"Không thể chạy bluetoothctl: {error}") from error
    output = (result.stdout + result.stderr).decode("utf-8", errors="replace").strip()
    if result.returncode != 0 and not allow_failure:
        raise DeviceError(output or "bluetoothctl thực thi thất bại.")
    return output


def _bluetooth_info(executable: str, mac: str) -> str:
    return _bluetoothctl(executable, 5, "info", mac, allow_failure=True)


def _message(command: str, stderr: bytes) -> str:
    detail = stderr.decode("utf-8", errors="replace").strip()
    return detail or f"{command} thực thi thất bại."
