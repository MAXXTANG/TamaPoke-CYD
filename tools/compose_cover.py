#!/usr/bin/env python3
"""Compose the approved cover and existing native sprite (requires Pillow)."""
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageOps

ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "docs/images"
SCALE, X, Y = 7, 1036, 686


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    base = Image.open(IMAGES / "cover-landscape.png").convert("RGB")
    source = Image.open(IMAGES / "pikachu-idle.png").convert("RGBA")
    assert source.size == (19, 25)
    sprite = source.resize((source.width * SCALE, source.height * SCALE), Image.Resampling.NEAREST)
    assert X + sprite.width <= base.width and Y + sprite.height <= base.height
    result = base.convert("RGBA")
    result.alpha_composite(sprite, (X, Y))
    result = result.convert("RGB")
    difference = ImageChops.difference(base, result)
    mask = Image.new("L", base.size, 0)
    mask.paste(sprite.getchannel("A"), (X, Y))
    # All pixels outside the existing opaque sprite must stay exactly equal.
    assert ImageChops.multiply(difference, ImageOps.invert(mask).convert("RGB")).getbbox() is None
    assert difference.getbbox() is not None
    output = IMAGES / "cover.png"
    result.save(output)
    assert Image.open(output).size == base.size
    record = {
        "method": "User-authorized direct composition of the existing native sprite; no generated character",
        "base": "cover-landscape.png", "base_sha256": sha(IMAGES / "cover-landscape.png"),
        "sprite": "pikachu-idle.png", "sprite_sha256": sha(IMAGES / "pikachu-idle.png"),
        "sprite_source": "assets/p025.bin, first idle frame cropped to opaque bounds",
        "sprite_native_pixels": [19, 25], "scale": SCALE, "resampling": "nearest-neighbor",
        "position": [X, Y], "sprite_bounds": [X, Y, X + sprite.width, Y + sprite.height],
        "cover_pixels": list(base.size), "outside_sprite_pixels_unchanged": True,
        "output_sha256": sha(output),
        "base_file_preserved": True,
        "provenance": "Derived PNG is a composition; original generated cover and its original metadata are retained separately.",
    }
    (IMAGES / "cover-composition.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"Composed {output}: {base.size}, native sprite enlarged {SCALE}x; all other pixels unchanged")


if __name__ == "__main__":
    main()
