"""One authorized Part 2 JPEG capture; no continuous stream or Pi history."""
from __future__ import annotations

import subprocess
import threading
import time
import tempfile
from pathlib import Path

from PIL import Image, UnidentifiedImageError

IMAGE_SIZE = 640
MAX_IMAGE_BYTES = 10 * 1024 * 1024


class CameraError(RuntimeError):
    pass


class MJPEGCamera:
    """Keep camera open at 2 fps; retain only the latest complete JPEG in RAM."""

    def __enter__(self):
        self.condition = threading.Condition()
        self.latest = None
        self.sequence = self.consumed = 0
        self.error = None
        self.process = subprocess.Popen(
            ["rpicam-vid", "--nopreview", "--timeout", "0", "--width", "640",
             "--height", "640", "--framerate", "2", "--codec", "mjpeg",
             "--quality", "80", "--flush", "--output", "-"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()
        return self

    def _read(self):
        pending = bytearray()
        try:
            while True:
                block = self.process.stdout.read1(65536)
                if not block:
                    raise CameraError("Luồng camera đã dừng.")
                pending.extend(block)
                if len(pending) > MAX_IMAGE_BYTES:
                    raise CameraError("Khung hình camera quá lớn.")
                while True:
                    start = pending.find(b"\xff\xd8")
                    end = pending.find(b"\xff\xd9", max(start, 0))
                    if start < 0 or end < 0:
                        break
                    frame = bytes(pending[start:end + 2])
                    del pending[:end + 2]
                    with self.condition:
                        self.latest = frame
                        self.sequence += 1
                        self.condition.notify_all()
        except Exception as error:
            with self.condition:
                self.error = error
                self.condition.notify_all()

    def capture(self):
        with self.condition:
            if not self.condition.wait_for(
                lambda: self.sequence > self.consumed or self.error is not None, timeout=5
            ):
                raise CameraError("Camera không gửi khung hình mới.")
            if self.error is not None:
                raise CameraError(str(self.error))
            self.consumed = self.sequence
            return self.latest

    def __exit__(self, *_):
        self.process.terminate()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.reader.join(timeout=2)
        self.process.stdout.close()


def capture_jpeg(*, camera: str = "rpicam-jpeg", timeout: int = 12,
                 ram_dir: Path = Path("/dev/shm")) -> bytes:
    """Capture once into tmpfs; verify exact dimensions; unlink on every path."""
    try:
        with tempfile.TemporaryDirectory(prefix="thiennhan-p2-", dir=ram_dir) as directory:
            path = Path(directory) / "capture.jpg"
            command = [camera, "--nopreview", "--timeout", "500", "--width", str(IMAGE_SIZE),
                       "--height", str(IMAGE_SIZE), "--output", str(path)]
            completed = subprocess.run(command, stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE, timeout=timeout, check=False)
            if completed.returncode:
                raise CameraError("Camera không chụp được ảnh. Hãy thử lại hoặc kiểm tra camera.")
            if path.stat().st_size > MAX_IMAGE_BYTES:
                raise CameraError("Ảnh camera vượt giới hạn 10 MiB.")
            data = path.read_bytes()
            if not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
                raise CameraError("Ảnh camera không phải JPEG hợp lệ.")
            try:
                from io import BytesIO
                with Image.open(BytesIO(data)) as image:
                    image.verify()
                with Image.open(BytesIO(data)) as image:
                    if image.size != (IMAGE_SIZE, IMAGE_SIZE):
                        raise CameraError("Camera trả ảnh sai kích thước 640 × 640.")
            except (UnidentifiedImageError, OSError) as error:
                raise CameraError("Ảnh JPEG bị lỗi.") from error
            return data
    except (OSError, subprocess.TimeoutExpired) as error:
        raise CameraError("Camera quá thời gian hoặc không thể khởi động.") from error
