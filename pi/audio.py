"""USB microphone VAD, MP3 encoding, and MAX98357A playback."""
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
from typing import Callable, Iterator

class DeviceError(RuntimeError):
    pass

_DEFAULT_MIC_SAMPLE_RATE = 16000
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
        on_health: Callable[[bool], None] | None = None,
    ) -> None:
        self.device = device
        self.sample_rate = sample_rate
        self.silence_seconds = silence_seconds
        self.max_seconds = max_seconds
        self.silence_rms = silence_rms
        self.on_health = on_health
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
                raise DeviceError("Mic USB ngừng cấp dữ liệu.")
            except (DeviceError, OSError) as error:
                print(f"[MIC] {error}", file=sys.stderr, flush=True)
                if self.on_health is not None:
                    self.on_health(False)
            self.close()
            time.sleep(1)

    def _read_segments(self, chunk_bytes: int) -> Iterator[bytes]:
        assert self._process is not None and self._process.stdout is not None
        descriptor = self._process.stdout.fileno()
        noise_rms = self._measure_noise(descriptor, chunk_bytes)
        if self.on_health is not None:
            self.on_health(True)
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
        last_data = time.monotonic()
        while len(data) < chunk_bytes:
            timeout = 0.2
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return bytes(data)
                timeout = min(timeout, remaining)
            readable, _, _ = select.select([descriptor], [], [], timeout)
            if not readable:
                if time.monotonic() - last_data >= 3:
                    raise DeviceError("Mic USB không truyền dữ liệu trong 3 giây.")
                continue
            block = os.read(descriptor, chunk_bytes - len(data))
            if not block:
                return bytes(data)
            data.extend(block)
            last_data = time.monotonic()
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

def _command(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise DeviceError(f"Không tìm thấy lệnh hệ thống: {name}")
    return path

def _message(command: str, stderr: bytes) -> str:
    detail = stderr.decode("utf-8", errors="replace").strip()
    return detail or f"{command} thực thi thất bại."
