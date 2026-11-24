#!/usr/bin/env python3
"""Package already-built CYD images. Does not connect to or flash a device."""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--esptool", type=Path, required=True, help="PlatformIO tool-esptoolpy/esptool.py")
args = parser.parse_args()
version = re.search(r'Version\s*=\s*"([0-9.]+-cyd)"', (ROOT / "cyd/include/version.h").read_text()).group(1)
sources = sorted([*ROOT.glob("cyd/include/*.h"), *ROOT.glob("cyd/src/*.cpp"),
                  ROOT / "cyd/platformio.ini", ROOT / "cyd/partitions.csv",
                  ROOT / "TamaPoke/pet.cpp", ROOT / "TamaPoke/pet.h", ROOT / "TamaPoke/dex.h",
                  ROOT / "TamaPoke/i18n.cpp", ROOT / "TamaPoke/i18n.h",
                  ROOT / "TamaPoke/audio.h",
                  ROOT / "TamaPoke/species.h",
                  ROOT / "TamaPoke/embedded_mon.cpp", ROOT / "TamaPoke/embedded_mon.h"])
source_hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
for variant in ("cyd-st7789", "cyd-ili9341"):
    build = ROOT / "cyd/.pio/build" / variant
    dest = ROOT / "firmware/cyd" / variant
    dest.mkdir(parents=True, exist_ok=True)
    parts = [(0x1000, "bootloader.bin"), (0x8000, "partitions.bin"), (0x10000, "tamapoke-cyd-app.bin")]
    for original, name in (("bootloader.bin", "bootloader.bin"), ("partitions.bin", "partitions.bin"),
                           ("firmware.bin", "tamapoke-cyd-app.bin")):
        shutil.copyfile(build / original, dest / name)
    app = (dest / "tamapoke-cyd-app.bin").read_bytes()
    if not app or app[0] != 0xE9 or len(app) > 0x300000:
        raise SystemExit("Invalid or oversized ESP app")
    factory = dest / "tamapoke-cyd-factory.bin"
    cmd = [sys.executable, str(args.esptool), "--chip", "esp32", "merge_bin", "--flash_mode", "dio",
           "--flash_freq", "40m", "--flash_size", "4MB", "--fill-flash-size", "4MB", "--target-offset", "0", "-o", str(factory)]
    for offset, name in parts:
        cmd += [hex(offset), str(dest / name)]
    subprocess.run(cmd, check=True)
    merged = factory.read_bytes()
    for offset, name in parts:
        data = (dest / name).read_bytes()
        if merged[offset:offset+len(data)] != data:
            raise SystemExit(f"Factory image differs from {name}")
    assert merged[0x9000:0xE000] == b"\xff" * 0x5000
    assert len(merged) == 0x400000 and merged[0x3F0000:] == b"\xff" * 0x10000
    names = [name for _, name in parts] + [factory.name]
    files = {name: {"bytes": (dest/name).stat().st_size,
                    "sha256": hashlib.sha256((dest/name).read_bytes()).hexdigest()} for name in names}
    manifest = {
        "version": version, "variant": variant, "chip": "ESP32", "flash_bytes": 4*1024*1024,
        "psram_required": False, "factory_offset": 0, "app_offset": 0x10000,
        "partition_app_bytes": 0x300000, "render_band_bytes": 15360,
        "dependencies": {"platformio_core": "6.2.0", "espressif32": "6.5.0", "arduino_esp32": "2.0.14", "tft_espi": "2.5.43"},
        "first_install_clears_nvs": True,
        "hardware_validation": "See docs/cyd/VALIDATION.md; results apply only to the recorded variant and image SHA-256",
        "source_base_commit": "021e84688e47eae0d806cf0e38403b21772b2718",
        "source_sha256": source_hashes, "files": files,
    }
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (dest / "SHA256SUMS").write_text("".join(f"{files[name]['sha256']}  {name}\n" for name in names))
    print(f"Packaged and byte-verified {variant}: {len(app)} byte app")
