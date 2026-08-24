#!/usr/bin/env python3
"""Compile fixed-cell Character Select locale assets into T3CSP001."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass
from pathlib import Path


MAGIC = b"T3CSP001"
AUTHOR_FORMAT = "T3CSP-AUTHOR-2"
HEADER_SIZE = 192
NAME_FONT_MAGIC = b"T3CSF001"
STAT_FONT_MAGIC = b"T3UFN001"
FONT_HEADER_SIZE = 96
FONT_ENTRY_SIZE = 40
NAME_WIDTH = 128
NAME_HEIGHT = 16
NAME_CEL_WIDTH = 64
NAME_COUNT = 9
STAT_STORAGE_WIDTH = 64
STAT_SAFE_WIDTH = 56
STAT_HEIGHT = 48
STAT_ROW_HEIGHT = 16
SOURCE_PROFILE_ENGLISH = 1
BACKGROUND_INDEX = 14
FOREGROUND_INDEX = 7
NAME_INDEX = 15
TYPOGRAPHY_CONTRACT_VERSION = 2
NAME_FONT_ADVANCE = 10
NAME_STYLE = "courier-prime-display-serif-16"
STAT_STYLE = "unifont-thin-8x16"


class CharacterSelectAssetError(ValueError):
    pass


@dataclass(frozen=True)
class Glyph:
    width: int
    advance: int
    rows: tuple[int, ...]


@dataclass(frozen=True)
class Author:
    locale: str
    source_profile: int
    source_name_sha256: bytes
    source_stats_sha256: bytes
    source_palette_sha256: bytes
    names: tuple[str, ...]
    stats: tuple[str, ...]


def _require_hash(value: object, field: str) -> bytes:
    if not isinstance(value, str) or len(value) != 64:
        raise CharacterSelectAssetError(f"{field} must be a SHA-256 hex digest")
    try:
        result = bytes.fromhex(value)
    except ValueError as error:
        raise CharacterSelectAssetError(
            f"{field} must be a SHA-256 hex digest"
        ) from error
    if len(result) != 32:
        raise CharacterSelectAssetError(f"{field} must be a SHA-256 hex digest")
    return result


def _require_strings(value: object, count: int, field: str) -> tuple[str, ...]:
    if (
        not isinstance(value, list)
        or len(value) != count
        or any(not isinstance(item, str) or not item for item in value)
    ):
        raise CharacterSelectAssetError(
            f"{field} must contain exactly {count} nonempty strings"
        )
    return tuple(value)


def load_author(path: Path) -> Author:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CharacterSelectAssetError(f"invalid author file: {error}") from error
    if not isinstance(document, dict) or document.get("format") != AUTHOR_FORMAT:
        raise CharacterSelectAssetError(f"author format must be {AUTHOR_FORMAT}")
    expected = {
        "format", "locale", "source_profile", "source_assets", "typography",
        "character_names", "stat_labels", "non_shipping_notice",
    }
    if set(document) != expected:
        raise CharacterSelectAssetError("author fields do not match the schema")
    locale = document.get("locale")
    if (
        not isinstance(locale, str)
        or not 1 <= len(locale) <= 15
        or any(not (character.isascii() and (
            character.isalnum() or character == "-"
        )) for character in locale)
    ):
        raise CharacterSelectAssetError("locale must be a canonical ASCII tag")
    if document.get("source_profile") != "english":
        raise CharacterSelectAssetError("only the authenticated English profile is supported")
    notice = document.get("non_shipping_notice")
    if not isinstance(notice, str) or not notice:
        raise CharacterSelectAssetError("the test locale requires a non-shipping notice")
    source = document.get("source_assets")
    if not isinstance(source, dict) or set(source) != {
        "CHNAME.BFT", "SLWIN.CDG", "TLSL.RGB"
    }:
        raise CharacterSelectAssetError("source_assets must bind all selection sheets")
    typography = document.get("typography")
    if not isinstance(typography, dict) or typography != {
        "character_names": NAME_STYLE,
        "stat_labels": STAT_STYLE,
    }:
        raise CharacterSelectAssetError("typography must bind both source presentation roles")
    return Author(
        locale=locale,
        source_profile=SOURCE_PROFILE_ENGLISH,
        source_name_sha256=_require_hash(source["CHNAME.BFT"], "CHNAME.BFT"),
        source_stats_sha256=_require_hash(source["SLWIN.CDG"], "SLWIN.CDG"),
        source_palette_sha256=_require_hash(source["TLSL.RGB"], "TLSL.RGB"),
        names=_require_strings(document.get("character_names"), NAME_COUNT, "character_names"),
        stats=_require_strings(document.get("stat_labels"), 3, "stat_labels"),
    )


def decode_font(package: bytes, expected_magic: bytes) -> dict[int, Glyph]:
    if len(package) < FONT_HEADER_SIZE or package[:8] != expected_magic:
        raise CharacterSelectAssetError(
            f"font role is not {expected_magic.decode('ascii')}"
        )
    version, header_size, total, count, entry_size = struct.unpack_from(
        "<HHIII", package, 8
    )
    if (
        version != 1
        or header_size != FONT_HEADER_SIZE
        or total != len(package)
        or entry_size != FONT_ENTRY_SIZE
        or FONT_HEADER_SIZE + count * FONT_ENTRY_SIZE != len(package)
        or any(package[88:96])
        or hashlib.sha256(package[FONT_HEADER_SIZE:]).digest() != package[56:88]
    ):
        raise CharacterSelectAssetError("Unicode font header or digest is invalid")
    result: dict[int, Glyph] = {}
    previous = -1
    for index in range(count):
        offset = FONT_HEADER_SIZE + index * FONT_ENTRY_SIZE
        scalar, width, advance, reserved = struct.unpack_from("<IBBH", package, offset)
        rows = struct.unpack_from(">16H", package, offset + 8)
        if expected_magic == NAME_FONT_MAGIC:
            role_shape = width == 16 and advance == NAME_FONT_ADVANCE
        else:
            role_shape = width in (8, 16) and advance == width
        if (
            not role_shape
            or reserved != 0
            or scalar <= previous
            or (width == 8 and any(row & 0x00FF for row in rows))
        ):
            raise CharacterSelectAssetError("Unicode font glyph table is invalid")
        result[scalar] = Glyph(width, advance, rows)
        previous = scalar
    return result


def _raw_text_pixels(text: str, font: dict[int, Glyph]) -> set[tuple[int, int]]:
    cursor = 0
    pixels: set[tuple[int, int]] = set()
    for character in text:
        scalar = ord(character)
        glyph = font.get(scalar)
        if glyph is None:
            raise CharacterSelectAssetError(f"font is missing U+{scalar:04X}")
        for y, row in enumerate(glyph.rows):
            for x in range(glyph.width):
                if row & (0x8000 >> x):
                    pixels.add((cursor + x, y))
        cursor += glyph.advance
    if not pixels:
        raise CharacterSelectAssetError("text renders no pixels")
    return pixels


def render_fixed(text: str, font: dict[int, Glyph], width: int) -> list[list[int]]:
    raw = _raw_text_pixels(text, font)
    left = min(x for x, _ in raw)
    right = max(x for x, _ in raw)
    top = min(y for _, y in raw)
    bottom = max(y for _, y in raw)
    normalized = {(x - left, y - top) for x, y in raw}
    occupied_width = right - left + 1
    occupied_height = bottom - top + 1
    if occupied_width > width or occupied_height > NAME_HEIGHT:
        raise CharacterSelectAssetError(
            f"{text!r} occupies {occupied_width}x{occupied_height}, "
            f"exceeding {width}x{NAME_HEIGHT}"
        )
    origin_x = (width - occupied_width) // 2
    origin_y = (NAME_HEIGHT - occupied_height) // 2
    result = [[0 for _ in range(width)] for _ in range(NAME_HEIGHT)]
    for x, y in normalized:
        result[origin_y + y][origin_x + x] = 1
    return result


def compile_bfnt(author: Author, name_font: dict[int, Glyph]) -> bytes:
    header = bytearray(32)
    header[:5] = b"BFNT\x1A"
    header[5] = 3
    header[6] = 0x16
    struct.pack_into("<HHHH", header, 8, NAME_CEL_WIDTH, NAME_HEIGHT, 0, 17)
    header[16:24] = b"T3CSP001"
    struct.pack_into("<HH", header, 28, 4, 4)
    body = bytearray(b"\x10\x00\x00\x00")
    for name in author.names:
        rendered = render_fixed(name, name_font, NAME_WIDTH)
        for cel in range(2):
            for row in rendered:
                colors = [NAME_INDEX if value else 0 for value in row[
                    cel * NAME_CEL_WIDTH:(cel + 1) * NAME_CEL_WIDTH
                ]]
                body.extend(
                    (colors[index] << 4) | colors[index + 1]
                    for index in range(0, NAME_CEL_WIDTH, 2)
                )
    return bytes(header + body)


def _pack_plane(pixels: list[list[int]], bit: int) -> bytes:
    result = bytearray()
    for row in reversed(pixels):
        for left in range(0, STAT_STORAGE_WIDTH, 8):
            value = 0
            for x in range(8):
                if row[left + x] & bit:
                    value |= 0x80 >> x
            result.append(value)
    return bytes(result)


def compile_cdg(author: Author, stat_font: dict[int, Glyph]) -> bytes:
    pixels = [
        [0 for _ in range(STAT_STORAGE_WIDTH)] for _ in range(STAT_HEIGHT)
    ]
    alpha = [
        [1 if x < STAT_SAFE_WIDTH else 0 for x in range(STAT_STORAGE_WIDTH)]
        for _ in range(STAT_HEIGHT)
    ]
    for row_index, label in enumerate(author.stats):
        rendered = render_fixed(label, stat_font, STAT_SAFE_WIDTH)
        for y, row in enumerate(rendered):
            for x, value in enumerate(row):
                pixels[row_index * STAT_ROW_HEIGHT + y][x] = (
                    FOREGROUND_INDEX if value else BACKGROUND_INDEX
                )
    plane_size = STAT_STORAGE_WIDTH // 8 * STAT_HEIGHT
    header = struct.pack(
        "<HHHhHBBHH", plane_size, STAT_STORAGE_WIDTH, STAT_HEIGHT,
        80 * (STAT_HEIGHT - 1), STAT_STORAGE_WIDTH // 32, 0, 0, 0, 0,
    )
    return header + _pack_plane(alpha, 1) + b"".join(
        _pack_plane(pixels, 1 << plane) for plane in range(4)
    )


def compile_pack(
    author: Author,
    name_font_package: bytes,
    stat_font_package: bytes,
) -> bytes:
    name_font = decode_font(name_font_package, NAME_FONT_MAGIC)
    stat_font = decode_font(stat_font_package, STAT_FONT_MAGIC)
    bfnt = compile_bfnt(author, name_font)
    cdg = compile_cdg(author, stat_font)
    header = bytearray(HEADER_SIZE)
    header[:8] = MAGIC
    names_offset = HEADER_SIZE
    stats_offset = names_offset + len(bfnt)
    total = stats_offset + len(cdg)
    struct.pack_into(
        "<IIIIIIII", header, 8,
        HEADER_SIZE, total, names_offset, len(bfnt), stats_offset, len(cdg),
        author.source_profile, TYPOGRAPHY_CONTRACT_VERSION,
    )
    header[40:72] = hashlib.sha256(bfnt + cdg).digest()
    header[72:72 + len(author.locale)] = author.locale.encode("ascii")
    header[88:120] = author.source_name_sha256
    header[120:152] = author.source_stats_sha256
    header[152:184] = author.source_palette_sha256
    return bytes(header) + bfnt + cdg


def _write_bmp(
    path: Path,
    author: Author,
    name_font: dict[int, Glyph],
    stat_font: dict[int, Glyph],
) -> None:
    width, height = 384, 224
    image = [[0 for _ in range(width)] for _ in range(height)]
    for index, name in enumerate(author.names):
        rendered = render_fixed(name, name_font, NAME_WIDTH)
        top = 16 + index * 20
        for y, row in enumerate(rendered):
            for x, value in enumerate(row):
                if value:
                    image[top + y][16 + x] = NAME_INDEX
    for index, label in enumerate(author.stats):
        rendered = render_fixed(label, stat_font, STAT_SAFE_WIDTH)
        top = 24 + index * 40
        for y in range(NAME_HEIGHT):
            for x in range(STAT_SAFE_WIDTH):
                image[top + y][208 + x] = (
                    FOREGROUND_INDEX if rendered[y][x] else BACKGROUND_INDEX
                )
    palette = {
        0: (0, 0, 0), 7: (255, 255, 255),
        14: (238, 170, 68), 15: (255, 255, 255),
    }
    stride = (width * 3 + 3) & ~3
    payload = bytearray()
    for row in reversed(image):
        for index in row:
            red, green, blue = palette.get(index, (0, 0, 0))
            payload.extend((blue, green, red))
        payload.extend(b"\x00" * (stride - width * 3))
    header = bytearray(54)
    header[:2] = b"BM"
    struct.pack_into("<I", header, 2, len(header) + len(payload))
    struct.pack_into("<I", header, 10, len(header))
    struct.pack_into("<IIIHHIIIIII", header, 14, 40, width, height, 1, 24,
                     0, len(payload), 0, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + payload)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("author", type=Path)
    parser.add_argument("name_font", type=Path)
    parser.add_argument("stat_font", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    arguments = parser.parse_args(argv)
    try:
        author = load_author(arguments.author)
        name_font_package = arguments.name_font.read_bytes()
        stat_font_package = arguments.stat_font.read_bytes()
        result = compile_pack(author, name_font_package, stat_font_package)
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_bytes(result)
        if arguments.preview:
            _write_bmp(
                arguments.preview,
                author,
                decode_font(name_font_package, NAME_FONT_MAGIC),
                decode_font(stat_font_package, STAT_FONT_MAGIC),
            )
    except (OSError, CharacterSelectAssetError) as error:
        parser.error(str(error))
    print(f"locale={author.locale}")
    print(f"bytes={len(result)}")
    print(f"sha256={hashlib.sha256(result).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
