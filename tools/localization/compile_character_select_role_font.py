#!/usr/bin/env python3
"""Compile a pinned display face into a Character Select bitmap-font role."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path


MAGIC = b"T3CSF001"
VERSION = 1
HEADER_SIZE = 96
ENTRY_SIZE = 40
STORAGE_WIDTH = 16
STORAGE_HEIGHT = 16
PINNED_PILLOW_VERSION = "10.2.0"
PINNED_FREETYPE_VERSION = "2.13.2"


class RoleFontError(ValueError):
    pass


def parse_characters(raw: str) -> tuple[int, ...]:
    characters: set[int] = set()
    for line in raw.splitlines():
        content = line.split("#", 1)[0]
        characters.update(ord(character) for character in content if not character.isspace())
    if not characters:
        raise RoleFontError("character inventory contains no glyphs")
    return tuple(sorted(characters))


def compile_font(
    font_path: Path,
    characters: tuple[int, ...],
    pixel_size: int,
    expected_sha256: str | None = None,
) -> bytes:
    try:
        from PIL import Image, ImageDraw, ImageFont, __version__, features
    except ImportError as error:
        raise RoleFontError(
            "Pillow is required (install with: python -m pip install Pillow)"
        ) from error
    freetype_version = features.version_module("freetype2")
    if (
        __version__ != PINNED_PILLOW_VERSION
        or freetype_version != PINNED_FREETYPE_VERSION
    ):
        raise RoleFontError(
            "role-font regeneration requires Pillow "
            f"{PINNED_PILLOW_VERSION} with FreeType {PINNED_FREETYPE_VERSION}"
        )

    font_bytes = font_path.read_bytes()
    source_digest = hashlib.sha256(font_bytes).hexdigest()
    if expected_sha256 and source_digest.lower() != expected_sha256.lower():
        raise RoleFontError("font source SHA-256 does not match the pin")
    if not 1 <= pixel_size <= STORAGE_HEIGHT:
        raise RoleFontError("pixel size must fit the 16-pixel source cell")
    try:
        font = ImageFont.truetype(
            str(font_path),
            pixel_size,
            layout_engine=ImageFont.Layout.BASIC,
        )
    except OSError as error:
        raise RoleFontError(f"font source could not be loaded: {error}") from error
    advance = round(font.getlength("M"))
    if not 1 <= advance <= STORAGE_WIDTH:
        raise RoleFontError("font advance does not fit the 16-pixel role storage")
    ascent, _ = font.getmetrics()
    baseline = min(ascent, STORAGE_HEIGHT - 1)

    body = bytearray()
    for scalar in characters:
        character = chr(scalar)
        if round(font.getlength(character)) != advance:
            raise RoleFontError(
                f"U+{scalar:04X} violates the fixed {advance}-pixel advance"
            )
        bounds = font.getbbox(character, anchor="ls")
        x_offset = max(0, -bounds[0])
        if (
            x_offset + bounds[2] > STORAGE_WIDTH
            or baseline + bounds[1] < 0
            or baseline + bounds[3] > STORAGE_HEIGHT
        ):
            raise RoleFontError(
                f"U+{scalar:04X} escapes its {advance}x{STORAGE_HEIGHT} cell"
            )
        image = Image.new("1", (STORAGE_WIDTH, STORAGE_HEIGHT), 0)
        draw = ImageDraw.Draw(image)
        draw.fontmode = "1"
        draw.text((x_offset, baseline), character, font=font, fill=1, anchor="ls")
        bounds = image.getbbox()
        if bounds is None:
            raise RoleFontError(f"U+{scalar:04X} renders no pixels")
        if bounds[2] > advance or bounds[3] > STORAGE_HEIGHT:
            raise RoleFontError(
                f"U+{scalar:04X} escapes its {advance}x{STORAGE_HEIGHT} cell"
            )
        rows = bytearray()
        for y in range(STORAGE_HEIGHT):
            value = 0
            for x in range(STORAGE_WIDTH):
                if image.getpixel((x, y)):
                    value |= 0x8000 >> x
            rows.extend(struct.pack(">H", value))
        body.extend(struct.pack("<IBBH", scalar, STORAGE_WIDTH, advance, 0))
        body.extend(rows)

    header = bytearray(HEADER_SIZE)
    header[:8] = MAGIC
    struct.pack_into(
        "<HHIII", header, 8, VERSION, HEADER_SIZE,
        HEADER_SIZE + len(body), len(characters), ENTRY_SIZE,
    )
    header[24:56] = bytes.fromhex(source_digest)
    header[56:88] = hashlib.sha256(body).digest()
    return bytes(header + body)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("characters", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--pixel-size", type=int, default=16)
    parser.add_argument("--source-sha256")
    arguments = parser.parse_args(argv)
    try:
        characters = parse_characters(arguments.characters.read_text(encoding="utf-8"))
        result = compile_font(
            arguments.font,
            characters,
            arguments.pixel_size,
            arguments.source_sha256,
        )
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_bytes(result)
    except (OSError, UnicodeError, RoleFontError) as error:
        parser.error(str(error))
    print(f"glyphs={len(characters)}")
    print(f"bytes={len(result)}")
    print(f"sha256={hashlib.sha256(result).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
