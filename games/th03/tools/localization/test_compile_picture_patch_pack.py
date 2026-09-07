#!/usr/bin/env python3
"""Hermetic tests for the transparent-overlay T3PTP001 compiler."""

from __future__ import annotations

import binascii
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools/localization/compile_picture_patch_pack.py"
SPEC = importlib.util.spec_from_file_location("compile_picture_patch_pack", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
PACK = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PACK
SPEC.loader.exec_module(PACK)


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def source_bmp() -> bytes:
    palette = [(index * 17, (15 - index) * 17, (index // 2) * 17)
               for index in range(16)]
    stride = PACK.WIDTH // 2
    pixels = bytes(stride * PACK.HEIGHT)
    offset = 14 + 40 + 64
    header = bytearray(offset)
    header[:2] = b"BM"
    struct.pack_into("<III", header, 2, offset + len(pixels), 0, offset)
    struct.pack_into(
        "<IiiHHIIiiII", header, 14, 40, PACK.WIDTH, PACK.HEIGHT, 1, 4,
        0, len(pixels), 0, 0, 16, 0,
    )
    for index, (red, green, blue) in enumerate(palette):
        header[54 + index * 4:58 + index * 4] = bytes((blue, green, red, 0))
    return bytes(header) + pixels


def overlay_png(noop: bool = False) -> bytes:
    palette = bytearray()
    for index in range(16):
        palette.extend((index * 17, (15 - index) * 17, (index // 2) * 17))
    palette.extend((1, 2, 3))
    alpha = bytes([255] * 16 + [0])
    rows = []
    for y in range(PACK.HEIGHT):
        row = bytearray([16] * PACK.WIDTH)
        if y == 1:
            row[1:4] = bytes((0 if noop else 2, 3, 4))
        if y == 2:
            row[5:7] = bytes((15, 1))
        rows.append(b"\0" + bytes(row))
    ihdr = struct.pack(">IIBBBBB", PACK.WIDTH, PACK.HEIGHT, 8, 3, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"PLTE", bytes(palette)) + chunk(b"tRNS", alpha) +
            chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b""))


class PicturePatchPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / "source.bmp").write_bytes(source_bmp())
        (self.root / "overlay.png").write_bytes(overlay_png())
        self.author = self.root / "author.json"
        self.author.write_text(json.dumps({
            "format": PACK.AUTHOR_FORMAT,
            "locale": "en-US",
            "pictures": [{
                "member": "dm3b.pi",
                "source_bmp": "source.bmp",
                "overlay_png": "overlay.png",
                "bounds": [1, 1, 7, 3],
            }],
        }), encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_deterministic_sparse_pack(self) -> None:
        locale, pictures = PACK.load_author(self.author)
        first = PACK.compile_pack(locale, pictures)
        second = PACK.compile_pack(*PACK.load_author(self.author))
        self.assertEqual(first, second)
        self.assertEqual(first[:8], PACK.MAGIC)
        self.assertEqual(struct.unpack_from("<I", first, 12)[0], 1)
        self.assertEqual(struct.unpack_from("<I", first, 16)[0], 2)
        self.assertEqual(first[68:73], b"en-US")
        entry = PACK.HEADER_SIZE
        self.assertEqual(first[entry:entry + 7], b"DM3B.PI")
        self.assertEqual(struct.unpack_from("<HHHHI", first, entry + 56),
                         (1, 1, 7, 3, 5))
        payload_offset = struct.unpack_from("<I", first, 32)[0]
        self.assertEqual(first[payload_offset:], b"\x23\x40\xF1")

    def test_exact_bounds_required(self) -> None:
        document = json.loads(self.author.read_text(encoding="utf-8"))
        document["pictures"][0]["bounds"] = [1, 1, 8, 3]
        self.author.write_text(json.dumps(document), encoding="utf-8")
        with self.assertRaisesRegex(PACK.PackError, "declared bounds"):
            PACK.load_author(self.author)

    def test_opaque_noop_pixel_rejected(self) -> None:
        (self.root / "overlay.png").write_bytes(overlay_png(noop=True))
        with self.assertRaisesRegex(PACK.PackError, "does not change"):
            PACK.load_author(self.author)

    def test_empty_complete_locale(self) -> None:
        self.author.write_text(json.dumps({
            "format": PACK.AUTHOR_FORMAT,
            "locale": "ja-JP",
            "pictures": [],
        }), encoding="utf-8")
        locale, pictures = PACK.load_author(self.author)
        result = PACK.compile_pack(locale, pictures)
        self.assertEqual(len(result), PACK.HEADER_SIZE)
        self.assertEqual(result[68:73], b"ja-JP")

    def test_png_crc_and_alpha_fail_closed(self) -> None:
        damaged = bytearray((self.root / "overlay.png").read_bytes())
        damaged[-5] ^= 1
        (self.root / "overlay.png").write_bytes(damaged)
        with self.assertRaisesRegex(PACK.PackError, "checksum"):
            PACK.load_author(self.author)


if __name__ == "__main__":
    unittest.main()
