#!/usr/bin/env python3
"""Capture the CYD renderer over USB; does not photograph the physical LCD."""
import argparse
import hashlib
import struct
import time
import zlib
from pathlib import Path

from cyd_console import synchronize

HEADER = b"FRAME 320 240 RGB565LE 153600"
FRAME_BYTES = 320 * 240 * 2


def capture(port, timeout=40):
    # Never retry CAPTURE: retransmission could be mistaken for binary pixels.
    port.write(b"CAPTURE\n")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().strip()
        if line == HEADER:
            break
        if line.startswith((b"ERROR", b"FRAME")):
            raise RuntimeError("Capture rejected or unsupported frame format")
    else:
        raise RuntimeError("Timed out waiting for frame header")
    raw = bytearray()
    while len(raw) < FRAME_BYTES and time.monotonic() < deadline:
        raw.extend(port.read(FRAME_BYTES - len(raw)))
    if len(raw) != FRAME_BYTES:
        raise RuntimeError(f"Incomplete frame: {len(raw)} / {FRAME_BYTES} bytes")
    if port.readline().strip() != b"DONE":
        raise RuntimeError("Missing frame completion marker")
    return bytes(raw)


def rgb565_to_rgb(raw):
    if len(raw) != FRAME_BYTES:
        raise ValueError("Expected one 320x240 RGB565 frame")
    rgb = bytearray()
    for (color,) in struct.iter_unpack("<H", raw):
        r, g, b = (color >> 11) & 31, (color >> 5) & 63, color & 31
        rgb.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    return rgb


def save_png(path, raw):
    rgb = rgb565_to_rgb(raw)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + rgb[y*960:(y+1)*960] for y in range(240))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 240, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--set-time", action="store_true")
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        parser.exit(2, "pyserial is required\n")
    try:
        with serial.Serial(port=None, baudrate=115200, timeout=0.25) as port:
            port.dtr = port.rts = False
            port.port = args.port
            port.open()
            synchronize(port, args.set_time)
            raw = capture(port)
            save_png(args.output, raw)
            print(f"Saved {args.output}; RGB565 SHA256 {hashlib.sha256(raw).hexdigest()}")
    except (RuntimeError, serial.SerialException, OSError) as exc:
        parser.exit(1, str(exc) + "\n")


if __name__ == "__main__":
    main()
