#!/usr/bin/env python3
"""Compile a pinned GNU Unifont .hex source into deterministic T3UFN001."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import struct
from pathlib import Path


MAGIC = b"T3UFN001"
VERSION = 1
HEADER_SIZE = 96
ENTRY_SIZE = 40


class FontError(ValueError):
    pass


def noncharacter(scalar: int) -> bool:
    return (0xFDD0 <= scalar <= 0xFDEF or
            (scalar <= 0x10FFFF and (scalar & 0xFFFF) >= 0xFFFE))


def source_bytes(path: Path) -> bytes:
    raw = path.read_bytes()
    if path.suffix.lower() == ".gz":
        try:
            return gzip.decompress(raw)
        except gzip.BadGzipFile as error:
            raise FontError("source is not a valid gzip stream") from error
    return raw


def parse_hex(raw: bytes) -> list[tuple[int, int, bytes]]:
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise FontError("Unifont .hex source must be ASCII") from error
    glyphs: list[tuple[int, int, bytes]] = []
    previous = -1
    for line_number, line in enumerate(text.splitlines(), start=1):
        if not line:
            continue
        try:
            scalar_text, bitmap_text = line.split(":", 1)
            scalar = int(scalar_text, 16)
            bitmap = bytes.fromhex(bitmap_text)
        except (ValueError, TypeError) as error:
            raise FontError(f"line {line_number}: malformed Unifont record") from error
        if scalar <= previous or scalar > 0x10FFFF or 0xD800 <= scalar <= 0xDFFF:
            raise FontError(f"line {line_number}: scalar order or range is invalid")
        # Unicode permanently reserves these scalar values for internal use.
        # The runtime rejects them in UTF-8 input, so they cannot be glyphs.
        if noncharacter(scalar):
            previous = scalar
            continue
        if len(bitmap) not in (16, 32):
            raise FontError(f"line {line_number}: glyph must be 8x16 or 16x16")
        width = 8 if len(bitmap) == 16 else 16
        rows = bytearray(32)
        if width == 8:
            for row, value in enumerate(bitmap):
                rows[row * 2] = value
        else:
            rows[:] = bitmap
        glyphs.append((scalar, width, bytes(rows)))
        previous = scalar
    if not glyphs:
        raise FontError("Unifont source contains no glyphs")
    required = {0x00A1, 0x00BF, 0x00E9, 0x3042, 0xFFFD}
    present = {scalar for scalar, _, _ in glyphs}
    missing = sorted(required - present)
    if missing:
        raise FontError(f"source is missing required U+{missing[0]:04X}")
    return glyphs


def compile_font(raw: bytes) -> bytes:
    glyphs = parse_hex(raw)
    body = bytearray()
    for scalar, width, rows in glyphs:
        body.extend(struct.pack("<IBBH", scalar, width, width, 0))
        body.extend(rows)
    header = bytearray(HEADER_SIZE)
    header[0:8] = MAGIC
    struct.pack_into(
        "<HHIII", header, 8, VERSION, HEADER_SIZE,
        HEADER_SIZE + len(body), len(glyphs), ENTRY_SIZE,
    )
    header[24:56] = hashlib.sha256(raw).digest()
    header[56:88] = hashlib.sha256(body).digest()
    return bytes(header) + bytes(body)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--source-sha256")
    arguments = parser.parse_args(argv)
    try:
        raw = source_bytes(arguments.source)
        digest = hashlib.sha256(raw).hexdigest()
        if arguments.source_sha256 and digest.lower() != arguments.source_sha256.lower():
            raise FontError("decompressed source SHA-256 does not match the pin")
        compiled = compile_font(raw)
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_bytes(compiled)
    except (OSError, FontError) as error:
        parser.error(str(error))
    print(f"source-sha256={digest}")
    print(f"glyphs={(len(compiled) - HEADER_SIZE) // ENTRY_SIZE}")
    print(f"bytes={len(compiled)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
