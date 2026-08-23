#!/usr/bin/env python3
"""Hermetic tests for source-free T3PTP001 author scaffolding."""

from __future__ import annotations

import binascii
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

LOCALIZATION_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(LOCALIZATION_ROOT))

import compile_picture_patch_pack as pack
import make_picture_patch_author as author


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def source_bmp() -> bytes:
    palette = [(index * 17, (15 - index) * 17, (index // 2) * 17)
               for index in range(16)]
    stride = pack.WIDTH // 2
    pixels = bytes(stride * pack.HEIGHT)
    offset = 14 + 40 + 64
    header = bytearray(offset)
    header[:2] = b"BM"
    struct.pack_into("<III", header, 2, offset + len(pixels), 0, offset)
    struct.pack_into(
        "<IiiHHIIiiII", header, 14, 40, pack.WIDTH, pack.HEIGHT, 1, 4,
        0, len(pixels), 0, 0, 16, 0,
    )
    for index, (red, green, blue) in enumerate(palette):
        header[54 + index * 4:58 + index * 4] = bytes((blue, green, red, 0))
    return bytes(header) + pixels


def edited_overlay() -> bytes:
    palette = bytearray()
    for index in range(16):
        palette.extend((index * 17, (15 - index) * 17, (index // 2) * 17))
    palette.extend((0, 0, 0))
    rows = []
    for y in range(pack.HEIGHT):
        row = bytearray([16] * pack.WIDTH)
        if y == 1:
            row[1:4] = bytes((2, 3, 4))
        if y == 2:
            row[5:7] = bytes((15, 1))
        rows.append(b"\0" + bytes(row))
    ihdr = struct.pack(
        ">IIBBBBB", pack.WIDTH, pack.HEIGHT, 8, 3, 0, 0, 0
    )
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"PLTE", bytes(palette)) +
            chunk(b"tRNS", bytes([255] * 16 + [0])) +
            chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) +
            chunk(b"IEND", b""))


class PicturePatchAuthorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.sources = self.root / "source previews"
        self.sources.mkdir()
        (self.sources / "dm3b.BMP").write_bytes(source_bmp())
        self.project = self.root / "author project"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def initialize(self) -> Path:
        return author.initialize_project(
            "en-US", self.sources, self.project, ["dm3b.pi"]
        )

    def test_init_contains_only_transparent_overlay_pixels(self) -> None:
        manifest = self.initialize()
        document = json.loads(manifest.read_text(encoding="utf-8"))
        self.assertEqual(document["pictures"][0]["member"], "DM3B.PI")
        self.assertIsNone(document["pictures"][0]["bounds"])
        overlay_path = self.project / document["pictures"][0]["overlay_png"]
        pixels, colors, alphas = pack.load_overlay_png(overlay_path)
        self.assertEqual(set(pixels), {16})
        self.assertEqual(len(colors), 17)
        self.assertEqual(alphas, bytes([255] * 16 + [0]))
        with self.assertRaisesRegex(pack.PackError, "changes no pixels"):
            pack.load_author(manifest)

    def test_finalize_freezes_exact_compile_ready_bounds(self) -> None:
        manifest = self.initialize()
        document = json.loads(manifest.read_text(encoding="utf-8"))
        overlay_path = self.project / document["pictures"][0]["overlay_png"]
        overlay_path.write_bytes(edited_overlay())
        finalized = self.project / "picture-patch-en-US.final.json"
        self.assertEqual(author.finalize_author(manifest, finalized), 1)
        locale, pictures = pack.load_author(finalized)
        self.assertEqual(locale, "en-US")
        self.assertEqual(pictures[0].bounds, (1, 1, 7, 3))
        first = pack.compile_pack(locale, pictures)
        second_manifest = self.project / "second.final.json"
        author.finalize_author(manifest, second_manifest)
        second = pack.compile_pack(*pack.load_author(second_manifest))
        self.assertEqual(first, second)

    def test_init_refuses_to_overwrite_existing_project(self) -> None:
        manifest = self.initialize()
        before = manifest.read_bytes()
        with self.assertRaisesRegex(author.ScaffoldError, "refusing to overwrite"):
            self.initialize()
        self.assertEqual(manifest.read_bytes(), before)

    def test_finalize_rejects_a_pristine_overlay(self) -> None:
        manifest = self.initialize()
        with self.assertRaisesRegex(pack.PackError, "changes no pixels"):
            author.finalize_author(
                manifest, self.project / "picture-patch-en-US.final.json"
            )


if __name__ == "__main__":
    unittest.main()
