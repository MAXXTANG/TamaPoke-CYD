#!/usr/bin/env python3
"""Read CYD status or explicitly set its clock. Never flashes or erases."""
import argparse
import time

def request(port, command, accepts, timeout=10):
    deadline = time.monotonic() + timeout
    next_send = 0
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_send:
            text = command() if callable(command) else command
            port.write((text + "\n").encode("ascii"))
            next_send = now + 1
        line = port.readline().decode("utf-8", errors="replace").strip()
        if not line:
            continue
        print(line, flush=True)
        if line.startswith("ERROR"):
            raise RuntimeError("Device rejected the command")
        if accepts(line):
            return line
    raise RuntimeError("Timed out waiting for CYD firmware")


def synchronize(port, set_time=False):
    # Opening CH340 can reboot ESP32 and drop an early command. Wait for a
    # known firmware status, retrying only the idempotent STATUS/TIME commands.
    status = request(port, "STATUS", lambda line: line.startswith("STATUS "))
    fields = dict(part.split("=", 1) for part in status.split() if "=" in part)
    if fields.get("variant") not in ("cyd-st7789", "cyd-ili9341") or not fields.get("version", "").endswith("-cyd"):
        raise RuntimeError("Not recognized as TamaPoke CYD firmware; no clock write sent")
    if set_time:
        request(port, lambda: f"TIME {int(time.time())}", lambda line: line == "DONE")
        status = request(port, "STATUS", lambda line: line.startswith("STATUS "))
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--set-time", action="store_true", help="Set UTC time from this computer, then show status")
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        parser.exit(2, "pyserial is required: python3 -m pip install pyserial\n")
    try:
        with serial.Serial(port=None, baudrate=115200, timeout=0.25) as port:
            port.dtr = False
            port.rts = False
            port.port = args.port
            port.open()
            synchronize(port, args.set_time)
    except (RuntimeError, serial.SerialException) as exc:
        parser.exit(1, str(exc) + "\n")


if __name__ == "__main__":
    main()
