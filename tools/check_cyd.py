#!/usr/bin/env python3
"""Check CYD package bytes, current source hashes, and host preview provenance."""
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def sha(data):
    return hashlib.sha256(data).hexdigest()

for variant in ("cyd-st7789", "cyd-ili9341"):
    folder = ROOT / "firmware/cyd" / variant
    manifest = json.loads((folder / "manifest.json").read_text())
    assert manifest["variant"] == variant and manifest["chip"] == "ESP32"
    assert manifest["flash_bytes"] == 4194304 and manifest["psram_required"] is False
    assert manifest["render_band_bytes"] == 15360
    for name, info in manifest["files"].items():
        data = (folder / name).read_bytes()
        assert len(data) == info["bytes"] and sha(data) == info["sha256"], name
    for path, expected in manifest["source_sha256"].items():
        assert sha((ROOT / path).read_bytes()) == expected, path
    app = (folder / "tamapoke-cyd-app.bin").read_bytes()
    boot = (folder / "bootloader.bin").read_bytes()
    assert len(app) <= manifest["partition_app_bytes"]
    # ESP image extended header: chip_id=0 means classic ESP32, not S3.
    for data in (app, boot):
        assert data[0] == 0xE9 and struct.unpack_from("<H", data, 12)[0] == 0
    partitions = (folder / "partitions.bin").read_bytes()
    entries = {}
    for pos in range(0, len(partitions), 32):
        magic, kind, sub, offset, size, label, flags = struct.unpack_from("<HBBII16sI", partitions, pos)
        if magic != 0x50AA:
            break
        assert offset + size <= 0x400000
        entries[label.split(b"\0")[0].decode()] = (kind, sub, offset, size)
    assert entries["nvs"] == (1, 2, 0x9000, 0x5000)
    assert entries["factory"] == (0, 0, 0x10000, 0x300000)
    assert entries["coredump"] == (1, 3, 0x3F0000, 0x10000)
    merged = (folder / "tamapoke-cyd-factory.bin").read_bytes()
    assert len(merged) == 0x400000
    assert merged[0x3F0000:] == b"\xff" * 0x10000
    for offset, name in ((0x1000, "bootloader.bin"), (0x8000, "partitions.bin"), (0x10000, "tamapoke-cyd-app.bin")):
        data = (folder / name).read_bytes()
        assert merged[offset:offset+len(data)] == data
    assert merged[0x9000:0xE000] == b"\xff" * 0x5000
    sums = (folder / "SHA256SUMS").read_text().splitlines()
    assert set(sums) == {f"{info['sha256']}  {name}" for name, info in manifest["files"].items()}

screens = ROOT / "docs/cyd/screens"
provenance = json.loads((screens / "provenance.json").read_text())
for path, expected in provenance["source_sha256"].items():
    assert sha((ROOT / path).read_bytes()) == expected, path
for name in provenance["screens"]:
    data = (screens / (name + ".png")).read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack_from(">II", data, 16) == (320, 240)
print(f"Verified both ESP32 CYD packages, partition layout and {len(provenance['screens'])} host-rendered screens")
