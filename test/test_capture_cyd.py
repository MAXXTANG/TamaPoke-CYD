import io
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import capture_cyd as capture


class FramePort:
    def __init__(self, raw, header=capture.HEADER, done=b"DONE\n"):
        self.data = io.BytesIO(raw)
        self.lines = [b"STATUS version=0.2.0-cyd\n", header + b"\n", done]
        self.sent = []

    def write(self, command):
        self.sent.append(command)

    def readline(self):
        return self.lines.pop(0) if self.lines else b""

    def read(self, size):
        # Fragmented serial packets, including arbitrary newline-valued pixels.
        return self.data.read(min(size, 113))


class CaptureTests(unittest.TestCase):
    def test_fragmented_binary_frame_is_read_exactly_once(self):
        raw = bytes(range(256)) * (capture.FRAME_BYTES // 256)
        port = FramePort(raw)
        self.assertEqual(capture.capture(port), raw)
        self.assertEqual(port.sent, [b"CAPTURE\n"])

    def test_rejects_unknown_format_and_missing_completion(self):
        with self.assertRaisesRegex(RuntimeError, "unsupported"):
            capture.capture(FramePort(b"", b"FRAME 640 480 RGB888 921600"))
        with self.assertRaisesRegex(RuntimeError, "completion"):
            capture.capture(FramePort(b"\0" * capture.FRAME_BYTES, done=b"ERROR\n"))

    def test_rgb565_channels_and_length(self):
        raw = b"\x00\xf8\xe0\x07\x1f\x00" * (capture.FRAME_BYTES // 6)
        self.assertEqual(capture.rgb565_to_rgb(raw)[:9], bytes([255, 0, 0, 0, 255, 0, 0, 0, 255]))
        with self.assertRaises(ValueError):
            capture.rgb565_to_rgb(b"\0")


if __name__ == "__main__":
    unittest.main()
