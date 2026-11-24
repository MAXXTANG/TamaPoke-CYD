#!/usr/bin/env python3
"""Stage exact, verified CYD release files for GitHub Pages. Never reads devices."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
VERSION = "0.4.0-cyd"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    catalog = {"name": "TamaPoke CYD", "version": VERSION, "boards": []}
    for variant, title, tested in [("cyd-st7789", "雙 USB／ST7789", True), ("cyd-ili9341", "單 USB／ILI9341", False)]:
        source = ROOT / "firmware/cyd" / variant
        manifest = json.loads((source / "manifest.json").read_text())
        assert manifest["version"] == VERSION and manifest["variant"] == variant
        dest = WEB / "firmware" / variant
        dest.mkdir(parents=True, exist_ok=True)
        for file, info in manifest["files"].items():
            assert (source / file).stat().st_size == info["bytes"] and sha(source / file) == info["sha256"]
            shutil.copyfile(source / file, dest / file)
        for file in ["SHA256SUMS", "manifest.json"]:
            shutil.copyfile(source / file, dest / file)
        def entry(name):
            return {"path": f"./firmware/{variant}/{name}", **manifest["files"][name]}
        catalog["boards"].append({"id": variant, "label": title, "hardware_tested": tested,
            "factory": entry("tamapoke-cyd-factory.bin"), "app": entry("tamapoke-cyd-app.bin"), "partitions": entry("partitions.bin")})
    (WEB / "catalog.json").write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n")
    screens = WEB / "assets/screens"
    screens.mkdir(parents=True, exist_ok=True)
    provenance = json.loads((ROOT / "docs/cyd/screens/provenance.json").read_text())
    images = []
    for name in provenance["screens"]:
        file = name + ".png"
        source = ROOT / "docs/cyd/screens" / file
        shutil.copyfile(source, screens / file)
        images.append({"file": file, "sha256": sha(source)})
    (screens / "provenance.json").write_text(json.dumps({**provenance, "images": images}, indent=2) + "\n")
    shutil.copyfile(ROOT / "docs/images/cover.png", WEB / "assets/cover.png")
    (WEB / ".nojekyll").write_text("")
    print(f"Prepared {VERSION}, two board packages, cover and {len(images)} exact firmware screenshots")


if __name__ == "__main__":
    main()
