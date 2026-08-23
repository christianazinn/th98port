from __future__ import annotations

import hashlib
import tempfile
import unittest
from pathlib import Path

from compile_unicode_font import FontError, compile_font, parse_hex


def fixture() -> bytes:
    records = []
    for scalar in (0x00A1, 0x00BF, 0x00E9, 0x3042, 0xFFFD):
        width = 16 if scalar == 0x3042 else 8
        records.append(f"{scalar:04X}:" + ("81" * (width * 2)) + "\n")
    return "".join(records).encode("ascii")


class UnicodeFontCompilerTests(unittest.TestCase):
    def test_compiles_deterministically_with_source_and_body_identity(self) -> None:
        first = compile_font(fixture())
        second = compile_font(fixture())
        self.assertEqual(first, second)
        self.assertEqual(first[:8], b"T3UFN001")
        self.assertEqual(int.from_bytes(first[12:16], "little"), len(first))
        self.assertEqual(first[24:56], hashlib.sha256(fixture()).digest())
        self.assertEqual(first[56:88], hashlib.sha256(first[96:]).digest())

    def test_normalizes_narrow_and_wide_rows(self) -> None:
        compiled = compile_font(fixture())
        narrow = compiled[96:136]
        wide = compiled[96 + (3 * 40):96 + (4 * 40)]
        self.assertEqual(narrow[4], 8)
        self.assertEqual(narrow[8:12], bytes((0x81, 0, 0x81, 0)))
        self.assertEqual(wide[4], 16)
        self.assertEqual(wide[8:12], bytes((0x81, 0x81, 0x81, 0x81)))

    def test_rejects_order_duplicates_and_missing_fallback(self) -> None:
        with self.assertRaises(FontError):
            parse_hex(b"00A1:" + b"00" * 16 + b"\n00A1:" + b"00" * 16)
        with self.assertRaisesRegex(FontError, "missing required"):
            parse_hex(b"0041:" + b"00" * 16)

    def test_omits_permanently_reserved_unicode_noncharacters(self) -> None:
        source = fixture().replace(
            b"FFFD:", b"FDD0:" + (b"00" * 16) + b"\nFFFD:",
        )
        glyphs = parse_hex(source)
        self.assertEqual(len(glyphs), 5)
        self.assertNotIn(0xFDD0, [scalar for scalar, _, _ in glyphs])


if __name__ == "__main__":
    unittest.main()
