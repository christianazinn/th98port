from __future__ import annotations

import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

import compile_character_select_assets as CSP


def font_package(
    scalars: set[int],
    magic: bytes,
    width: int,
    advance: int,
) -> bytes:
    body = bytearray()
    for scalar in sorted(scalars):
        rows = tuple(0x7E00 if 2 <= y < 13 else 0 for y in range(16))
        body.extend(struct.pack("<IBBH", scalar, width, advance, 0))
        body.extend(struct.pack(">16H", *rows))
    header = bytearray(96)
    header[:8] = magic
    struct.pack_into("<HHIII", header, 8, 1, 96, 96 + len(body), len(scalars), 40)
    header[56:88] = hashlib.sha256(body).digest()
    return bytes(header + body)


def author_document() -> dict[str, object]:
    return {
        "format": "T3CSP-AUTHOR-2",
        "locale": "es-TEST",
        "source_profile": "english",
        "source_assets": {
            "CHNAME.BFT": "11" * 32,
            "SLWIN.CDG": "22" * 32,
            "TLSL.RGB": "33" * 32,
        },
        "typography": {
            "character_names": "courier-prime-display-serif-16",
            "stat_labels": "unifont-thin-8x16",
        },
        "character_names": [
            "REIMU", "MIMA", "MARISA", "ELLEN", "KOTOHIME",
            "KANA", "RIKAKO", "CHIYURI", "YUMEMI",
        ],
        "stat_labels": ["RÁPIDO", "PODER", "CARGA"],
        "non_shipping_notice": "Technical fixture only.",
    }


class CharacterSelectAssetCompilerTests(unittest.TestCase):
    def setUp(self) -> None:
        names = author_document()["character_names"]
        stats = author_document()["stat_labels"]
        self.name_font = font_package(
            {ord(character) for text in names for character in text},
            CSP.NAME_FONT_MAGIC,
            16,
            CSP.NAME_FONT_ADVANCE,
        )
        self.stat_font = font_package(
            {ord(character) for text in stats for character in text},
            CSP.STAT_FONT_MAGIC,
            8,
            8,
        )

    def load_author(self, document: dict[str, object]) -> CSP.Author:
        temporary = tempfile.TemporaryDirectory(prefix="th03-csp-author-")
        self.addCleanup(temporary.cleanup)
        path = Path(temporary.name) / "author.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return CSP.load_author(path)

    def test_deterministic_native_assets_and_identity(self) -> None:
        author = self.load_author(author_document())
        first = CSP.compile_pack(author, self.name_font, self.stat_font)
        second = CSP.compile_pack(author, self.name_font, self.stat_font)
        self.assertEqual(first, second)
        self.assertEqual(first[:8], b"T3CSP001")
        self.assertEqual(int.from_bytes(first[8:12], "little"), 192)
        self.assertEqual(int.from_bytes(first[12:16], "little"), len(first))
        self.assertEqual(first[72:79], b"es-TEST")
        self.assertEqual(first[88:120], b"\x11" * 32)
        self.assertEqual(int.from_bytes(first[36:40], "little"), 2)
        names_at = int.from_bytes(first[16:20], "little")
        names_size = int.from_bytes(first[20:24], "little")
        stats_at = int.from_bytes(first[24:28], "little")
        self.assertEqual(first[names_at:names_at + 5], b"BFNT\x1a")
        self.assertEqual(first[stats_at + 2:stats_at + 6], b"@\x000\x00")
        self.assertEqual(
            first[40:72], hashlib.sha256(first[names_at:]).digest()
        )
        self.assertEqual(stats_at, names_at + names_size)

    def test_fixed_geometry_protects_the_stat_star_field(self) -> None:
        author = self.load_author(author_document())
        font = CSP.decode_font(self.stat_font, CSP.STAT_FONT_MAGIC)
        cdg = CSP.compile_cdg(author, font)
        plane_size = 64 // 8 * 48
        alpha = cdg[16:16 + plane_size]
        for stored_y in range(48):
            row = alpha[stored_y * 8:(stored_y + 1) * 8]
            self.assertEqual(row[:7], b"\xff" * 7)
            self.assertEqual(row[7], 0)

    def test_accent_is_rendered_and_overflow_fails_closed(self) -> None:
        author = self.load_author(author_document())
        font = CSP.decode_font(self.stat_font, CSP.STAT_FONT_MAGIC)
        rendered = CSP.render_fixed("RÁPIDO", font, 56)
        self.assertTrue(any(any(row) for row in rendered))
        self.assertEqual(
            sum(sum(row) for row in rendered),
            len(CSP._raw_text_pixels("RÁPIDO", font)),
            "the thin stat-label role must not expand its one-pixel strokes",
        )
        name_font = CSP.decode_font(self.name_font, CSP.NAME_FONT_MAGIC)
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "exceeding"):
            CSP.render_fixed("KOTOHIMEKOTOHIMEKOTOHIME", name_font, 56)

    def test_missing_glyph_and_schema_drift_fail_closed(self) -> None:
        author = self.load_author(author_document())
        available = {
            ord(character)
            for text in author.names + author.stats
            for character in text
            if character != "Á"
        }
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, r"missing U\+00C1"):
            CSP.compile_pack(
                author,
                self.name_font,
                font_package(available, CSP.STAT_FONT_MAGIC, 8, 8),
            )
        document = author_document()
        document["unexpected"] = True
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "fields"):
            self.load_author(document)

    def test_contact_sheet_has_stable_bmp_geometry(self) -> None:
        author = self.load_author(author_document())
        temporary = tempfile.TemporaryDirectory(prefix="th03-csp-preview-")
        self.addCleanup(temporary.cleanup)
        output = Path(temporary.name) / "preview.bmp"
        CSP._write_bmp(
            output,
            author,
            CSP.decode_font(self.name_font, CSP.NAME_FONT_MAGIC),
            CSP.decode_font(self.stat_font, CSP.STAT_FONT_MAGIC),
        )
        data = output.read_bytes()
        self.assertEqual(data[:2], b"BM")
        self.assertEqual(struct.unpack_from("<II", data, 18), (384, 224))
        self.assertEqual(int.from_bytes(data[2:6], "little"), len(data))

    def test_tracked_package_exactly_matches_its_author(self) -> None:
        root = Path(__file__).resolve().parents[2]
        author = CSP.load_author(root / "localization" / "character-select-es-TEST.json")
        package = CSP.compile_pack(
            author,
            (root / "tools" / "localization" / "T3CSF001-NAMES.BIN").read_bytes(),
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
            "73a136783ea3d745ae72559c29e7562fac6c03041616b24d50b3a81bbf747d13",
        )

    def test_font_roles_cannot_be_swapped(self) -> None:
        author = self.load_author(author_document())
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "T3CSF001"):
            CSP.compile_pack(author, self.stat_font, self.stat_font)
        with self.assertRaisesRegex(CSP.CharacterSelectAssetError, "T3UFN001"):
            CSP.compile_pack(author, self.name_font, self.name_font)


if __name__ == "__main__":
    unittest.main()
