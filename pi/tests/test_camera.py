import os
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from PIL import Image

from camera import CameraError, capture_jpeg


class CameraTests(unittest.TestCase):
    def test_single_capture_and_cleanup(self):
        paths = []

        def fake_run(command, **kwargs):
            output = Path(command[command.index("--output") + 1])
            paths.append(output)
            Image.new("RGB", (640, 640), "white").save(output, "JPEG")
            return SimpleNamespace(returncode=0)

        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as directory:
            with patch("camera.subprocess.run", side_effect=fake_run) as run:
                image = capture_jpeg(ram_dir=Path(directory))
        self.assertTrue(image.startswith(b"\xff\xd8"))
        self.assertEqual(run.call_count, 1)
        self.assertFalse(paths[0].exists())
        self.assertEqual(run.call_args.args[0][run.call_args.args[0].index("--width") + 1], "640")

    def test_wrong_resolution_rejected(self):
        def fake_run(command, **kwargs):
            Image.new("RGB", (1280, 720)).save(command[-1], "JPEG")
            return SimpleNamespace(returncode=0)

        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as directory:
            with patch("camera.subprocess.run", side_effect=fake_run):
                with self.assertRaises(CameraError):
                    capture_jpeg(ram_dir=Path(directory))


if __name__ == "__main__":
    unittest.main()
