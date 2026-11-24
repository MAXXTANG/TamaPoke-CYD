import importlib.util
import io
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("cyd_console", Path(__file__).parents[1] / "tools/cyd_console.py")
console = importlib.util.module_from_spec(spec)
spec.loader.exec_module(console)

STATUS = "STATUS version=0.1.0-cyd variant=cyd-st7789 species=25 time_valid=0"


class RebootingPort:
    def __init__(self, status=STATUS, ready_at=1.5):
        self.now = 0
        self.ready_at = ready_at
        self.status = status
        self.sent = []
        self.lines = []

    def write(self, data):
        command = data.decode().strip()
        self.sent.append((self.now, command))
        if self.now < self.ready_at:
            return  # Reproduces the real CH340 reset dropping an early command.
        if command == "STATUS":
            self.lines.append(self.status)
        elif command.startswith("TIME "):
            self.status = self.status.replace("time_valid=0", "time_valid=1")
            self.lines += [command + " valid=1", "DONE"]

    def readline(self):
        self.now += 0.25
        return ((self.lines.pop(0) + "\n").encode() if self.lines else b"")


class ConsoleTests(unittest.TestCase):
    def run_sync(self, port, set_time):
        with patch.object(console.time, "monotonic", side_effect=lambda: port.now), redirect_stdout(io.StringIO()):
            return console.synchronize(port, set_time)

    def test_recovers_from_dropped_startup_commands_before_setting_time(self):
        port = RebootingPort()
        result = self.run_sync(port, True)
        self.assertIn("time_valid=1", result)
        writes = [(t, c) for t, c in port.sent if c.startswith("TIME ")]
        self.assertEqual(len(writes), 1)
        self.assertGreaterEqual(writes[0][0], port.ready_at)

    def test_unknown_firmware_receives_no_clock_write(self):
        port = RebootingPort("STATUS version=0.3.3 variant=tra-board", 0)
        with self.assertRaisesRegex(RuntimeError, "no clock write"):
            self.run_sync(port, True)
        self.assertFalse(any(c.startswith("TIME ") for _, c in port.sent))

    def test_permanently_unresponsive_device_has_bounded_timeout(self):
        port = RebootingPort(ready_at=100)
        with self.assertRaisesRegex(RuntimeError, "Timed out"):
            self.run_sync(port, True)
        self.assertLessEqual(port.now, 10.5)
        self.assertFalse(any(c.startswith("TIME ") for _, c in port.sent))


if __name__ == "__main__":
    unittest.main()
