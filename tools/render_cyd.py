#!/usr/bin/env python3
"""Render the firmware's CYD UI on the host; uses only Python's standard library."""
import hashlib
import json
import struct
import subprocess
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/cyd-preview"
OUT = ROOT / "docs/cyd/screens"
BUILD.mkdir(parents=True, exist_ok=True)
OUT.mkdir(parents=True, exist_ok=True)
subprocess.run([
    "c++", "-std=c++17", "-O2", "-I", str(ROOT / "TamaPoke/test/shim"),
    str(ROOT / "cyd/test/render.cpp"), str(ROOT / "TamaPoke/test/shim/shim.cpp"),
    str(ROOT / "TamaPoke/pet.cpp"), str(ROOT / "TamaPoke/i18n.cpp"),
    str(ROOT / "TamaPoke/embedded_mon.cpp"), "-o", str(BUILD / "render"),
], check=True)
subprocess.run([str(BUILD / "render"), str(BUILD)], check=True)

def png(path, width, height, rgb):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + rgb[y*width*3:(y+1)*width*3] for y in range(height))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))

names = ["home", "food", "games", "catch", "training", "result", "info", "settings", "dex", "confirm",
         "sleep", "shiny", "raichu", "egg", "status", "rhythm-ready", "rhythm-wait", "rhythm-cue", "rhythm-perfect", "rhythm-result",
         "catch-falling", "catch-collected", "catch-missed", "catch-result", "training-charge", "training-punch", "training-break", "training-result"]
images = []
for name in names:
    magic, dimensions, depth, rgb = (BUILD / (name + ".ppm")).read_bytes().split(b"\n", 3)
    assert (magic, dimensions, depth) == (b"P6", b"320 240", b"255") and len(rgb) == 320*240*3
    png(OUT / (name + ".png"), 320, 240, rgb)
    images.append(rgb)
rows = (len(names)+2)//3
width, height = 320*3+16*4, 240*rows+16*(rows+1)
sheet = bytearray(bytes((30, 36, 48)) * width * height)
for i, rgb in enumerate(images):
    left, top = 16+(i % 3)*336, 16+(i // 3)*256
    for y in range(240):
        start = ((top+y)*width+left)*3
        sheet[start:start+320*3] = rgb[y*320*3:(y+1)*320*3]
png(OUT / "overview.png", width, height, sheet)
sources = ["cyd/include/ui.h", "cyd/include/pixel_art.h", "cyd/include/model.h", "cyd/include/rhythm.h", "cyd/include/games.h", "cyd/include/sprite.h", "cyd/include/version.h", "cyd/test/host_canvas.h",
           "cyd/test/render.cpp", "TamaPoke/embedded_mon.cpp", "TamaPoke/species.h"]
(OUT / "provenance.json").write_text(json.dumps({
    "kind": "host-rendered firmware UI examples, not physical LCD photos",
    "sample_save": True, "resolution": [320, 240], "screens": names,
    "command": "python3 tools/render_cyd.py",
    "source_sha256": {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in sources},
}, indent=2) + "\n")
print(f"Rendered {len(names)} CYD screens and overview: {OUT}")
