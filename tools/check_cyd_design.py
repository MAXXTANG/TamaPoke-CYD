#!/usr/bin/env python3
"""Compare a real firmware render to the independently approved canvas pixels."""
import hashlib
import json
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    folder = ROOT / "docs/cyd/design"
    meta = json.loads((folder / "reference.json").read_text())
    reference = zlib.decompress((folder / "approved-home.rgb565.zlib").read_bytes())
    assert len(reference) == 320 * 240 * 2
    assert hashlib.sha256(reference).hexdigest() == meta["reference_sha256"]
    assert hashlib.sha256((folder / "approved-layout.html").read_bytes()).hexdigest() == meta["approved_layout_sha256"]
    magic, size, depth, rgb = (ROOT / "build/cyd-preview/approved-home.ppm").read_bytes().split(b"\n", 3)
    assert (magic, size, depth, len(rgb)) == (b"P6", b"320 240", b"255", 320 * 240 * 3)
    actual = b"".join(struct.pack("<H", ((rgb[i] >> 3) << 11) | ((rgb[i+1] >> 2) << 5) | (rgb[i+2] >> 3))
                      for i in range(0, len(rgb), 3))
    different = sum(reference[i:i+2] != actual[i:i+2] for i in range(0, len(actual), 2))
    if different:
        raise SystemExit(f"Approved home layout differs at {different} / 76800 pixels")
    print("Approved home layout: all 76800 RGB565 pixels match")


if __name__ == "__main__":
    main()
