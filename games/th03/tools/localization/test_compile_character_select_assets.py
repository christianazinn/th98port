from __future__ import annotations

import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

import compile_character_select_assets as CSP


def font_package(scalars: set[int]) -> bytes:
    body = bytearray()
    for scalar in sorted(scalars):
        rows = tuple(0x7E00 if 2 <= y < 13 else 0 for y in range(16))
        body.extend(struct.pack("<IBBH", scalar, 8, 8, 0))
        body.extend(struct.pack(">16H", *rows))
    header = bytearray(96)
    header[:8] = CSP.STAT_FONT_MAGIC
    struct.pack_into("<HHIII", header, 8, 1, 96, 96 + len(body), len(scalars), 40)
    header[56:88] = hashlib.sha256(body).digest()
    return bytes(header + body)


def author_document() -> dict[str, object]:
    return {
        "format": "T3CSP-AUTHOR-3",
        "locale": "es-TEST",
        "source_profile": "english",
        "source_assets": {
            "CHNAME.BFT": "11" * 32,
            "SLWIN.CDG": "22" * 32,
            "TLSL.RGB": "33" * 32,
        },
        "typography": {
            "character_names": "authenticated-source",
            "stat_labels": "unifont-thin-8x16",
        },
        "stat_labels": ["RÁPIDO", "PODER", "CARGA"],
        "non_shipping_notice": "Technical fixture only.",
    }


class CharacterSelectAssetCompilerTests(unittest.TestCase):
    def setUp(self) -> None:
        stats = author_document()["stat_labels"]
        self.stat_font = font_package(
            {ord(character) for text in stats for character in text}
        )

    def load_author(self, document: dict[str, object]) -> CSP.Author:
        temporary = tempfile.TemporaryDirectory(prefix="th03-csp-author-")
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "author.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return CSP.load_author(path)

    def test_deterministic_stat_asset_and_source_identity(self) -> None:
        author = self.load_author(author_document())
        first = CSP.compile_pack(author, self.stat_font)
        second = CSP.compile_pack(author, self.stat_font)
        self.assertEqual(first, second)
        self.assertEqual(first[:8], b"T3CSP001")
        self.assertEqual(int.from_bytes(first[8:12], "little"), 192)
        self.assertEqual(int.from_bytes(first[12:16], "little"), len(first))
        self.assertEqual(first[72:79], b"es-TEST")
        self.assertEqual(first[88:120], b"\x11" * 32)
        self.assertEqual(int.from_bytes(first[36:40], "little"), 3)
        self.assertEqual(int.from_bytes(first[16:20], "little"), 0)
        self.assertEqual(int.from_bytes(first[20:24], "little"), 0)
        stats_at = int.from_bytes(first[24:28], "little")
        self.assertEqual(stats_at, 192)
        self.assertEqual(first[stats_at + 2:stats_at + 6], b"@\x000\x00")
        self.assertEqual(first[40:72], hashlib.sha256(first[stats_at:]).digest())

    def test_fixed_geometry_protects_the_stat_star_field(self) -> None:
        author = self.load_author(author_document())
        cdg = CSP.compile_cdg(author, CSP.decode_font(self.stat_font))
        plane_size = 64 // 8 * 48
        alpha = cdg[16:16 + plane_size]
        for stored_y in range(48):
            row = alpha[stored_y * 8:(stored_y + 1) * 8]
            self.assertEqual(row[:7], b"\xff" * 7)
            self.assertEqual(row[7], 0)

    def test_accent_is_rendered_without_expanding_thin_strokes(self) -> None:
        author = self.load_author(author_document())
        font = CSP.decode_font(self.stat_font)
        rendered = CSP.render_fixed("RÁPIDO", font, 56)
        self.assertTrue(any(any(row) for row in rendered))
        self.assertEqual(
            sum(sum(row) for row in rendered),
            len(CSP._raw_text_pixels("RÁPIDO", font)),
        )
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "exceeding"):
            CSP.render_fixed("CARGACARGACARGA", font, 56)
        self.assertEqual(author.stats[2], "CARGA")

    def test_missing_glyph_and_schema_drift_fail_closed(self) -> None:
        author = self.load_author(author_document())
        available = {
            ord(character)
            for text in author.stats
            for character in text
            if character != "Á"
        }
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, r"missing U\+00C1"):
            CSP.compile_pack(author, font_package(available))
        document = author_document()
        document["character_names"] = ["REIMU"] * 9
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "fields"):
            self.load_author(document)

    def test_contact_sheet_has_stable_stat_only_geometry(self) -> None:
        author = self.load_author(author_document())
        temporary = tempfile.TemporaryDirectory(prefix="th03-csp-preview-")
        self.addCleanup(temporary.cleanup)
        output = Path(temporary.name) / "preview.bmp"
        CSP._write_bmp(output, author, CSP.decode_font(self.stat_font))
        data = output.read_bytes()
        self.assertEqual(data[:2], b"BM")
        self.assertEqual(struct.unpack_from("<II", data, 18), (128, 80))
        self.assertEqual(int.from_bytes(data[2:6], "little"), len(data))

    def test_tracked_package_exactly_matches_its_author(self) -> None:
        root = Path(__file__).resolve().parents[2]
        author = CSP.load_author(root / "localization" / "character-select-es-TEST.json")
        package = CSP.compile_pack(
            author,
            (root / "assets" / "new-classic" / "menu-backgrounds" /
             "T3UFN001.BIN").read_bytes(),
        )
        tracked = (
            root / "assets" / "new-classic" / "menu-backgrounds" /
            "T3CSP001-ES-TEST.BIN"
        ).read_bytes()
        self.assertEqual(package, tracked)
        self.assertEqual(
            hashlib.sha256(tracked).hexdigest(),
            "5d4112b8a861aee04c1530ccb307a0e803d40bbf9d41feee77f4057f10494dff",
        )

    def test_only_the_stat_font_role_is_accepted(self) -> None:
        author = self.load_author(author_document())
        wrong = bytearray(self.stat_font)
        wrong[:8] = b"T3BAD001"
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "T3UFN001"):
            CSP.compile_pack(author, bytes(wrong))


if __name__ == "__main__":
    unittest.main()
