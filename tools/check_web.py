#!/usr/bin/env python3
"""Verify exact public website assets and release data, without device access."""
import hashlib
import json
import re
import struct
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.paths, self.ids = [], []

    def handle_starttag(self, _tag, attrs):
        attrs = dict(attrs)
        if "id" in attrs:
            self.ids.append(attrs["id"])
        for key in ["src", "href"]:
            if attrs.get(key):
                self.paths.append(attrs[key])


def main():
    page = Page()
    page.feed((WEB / "index.html").read_text())
    assert len(page.ids) == len(set(page.ids)), "Duplicate HTML IDs"
    for path in page.paths:
        parsed = urlparse(path)
        if parsed.scheme or parsed.netloc:
            continue
        if parsed.path:
            asset = (WEB / unquote(parsed.path)).resolve()
            assert asset.is_relative_to(WEB) and asset.is_file(), path
        if parsed.fragment:
            assert parsed.fragment in page.ids, path
    catalog = json.loads((WEB / "catalog.json").read_text())
    assert catalog["version"] == "0.4.0-cyd"
    assert {b["id"] for b in catalog["boards"]} == {"cyd-st7789", "cyd-ili9341"}
    for board in catalog["boards"]:
        source = ROOT / "firmware/cyd" / board["id"]
        manifest = json.loads((source / "manifest.json").read_text())
        assert manifest["version"] == catalog["version"]
        assert board["hardware_tested"] is (board["id"] == "cyd-st7789")
        for mode in ["factory", "app", "partitions"]:
            file = board[mode]
            path = (WEB / file["path"]).resolve()
            assert path.is_relative_to(WEB / "firmware"), file
            assert path.read_bytes() == (source / path.name).read_bytes(), file
            assert path.stat().st_size == file["bytes"] and digest(path) == file["sha256"], file
        data = (WEB / board["factory"]["path"]).read_bytes()
        assert len(data) == 0x400000 and data[0x9000:0xe000] == b"\xff" * 0x5000
        app = (WEB / board["app"]["path"]).read_bytes()
        assert app[0] == 0xe9 and struct.unpack_from("<H", app, 12)[0] == 0
    provenance = json.loads((WEB / "assets/screens/provenance.json").read_text())
    assert len(provenance["images"]) == 28
    for entry in provenance["images"]:
        path = WEB / "assets/screens" / entry["file"]
        assert digest(path) == entry["sha256"]
        assert path.read_bytes() == (ROOT / "docs/cyd/screens" / entry["file"]).read_bytes()
        assert struct.unpack_from(">II", path.read_bytes(), 16) == (320, 240)
    for path in [*WEB.glob("*.mjs")]:
        for relative in re.findall(r'from\s+["\x27](\./[^"\x27]+)["\x27]', path.read_text()):
            assert (path.parent / relative).is_file(), relative
    for name in ["esptool-js-0.6.0", "crypto-js-4.2.0"]:
        folder = WEB / "vendor" / name
        source = json.loads((folder / "SOURCE.json").read_text())
        for file, expected in source["files"].items():
            assert digest(folder / file) == expected, file
        assert (folder / "LICENSE").stat().st_size > 500
    assert (WEB / "assets/cover.png").read_bytes() == (ROOT / "docs/images/cover.png").read_bytes()
    assert "ESP32-S3" not in (WEB / "catalog.json").read_text()
    assert not list(WEB.rglob("*original-flash*"))
    assert (WEB / ".nojekyll").is_file()
    print("Verified local page links, two exact CYD firmware packages, 28 screenshots, cover and vendor hashes")


if __name__ == "__main__":
    main()
