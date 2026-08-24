from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

import compile_character_select_role_font as ROLE


try:
    import PIL
    from PIL import features
except ImportError:
    PIL_AVAILABLE = False
else:
    PIL_AVAILABLE = (
        PIL.__version__ == ROLE.PINNED_PILLOW_VERSION
        and features.version_module("freetype2") == ROLE.PINNED_FREETYPE_VERSION
    )


class CharacterSelectRoleFontCompilerTests(unittest.TestCase):
    def test_character_inventory_is_sorted_unique_and_ignores_comments(self) -> None:
        self.assertEqual(
            ROLE.parse_characters("# heading\nCABA\n"),
            (ord("A"), ord("B"), ord("C")),
        )
        with self.assertRaisesRegex(ROLE.RoleFontError, "no glyphs"):
            ROLE.parse_characters("# only a comment\n")

    @unittest.skipUnless(PIL_AVAILABLE, "pinned Pillow/FreeType is not installed")
    def test_tracked_role_is_exactly_reproducible(self) -> None:
        root = Path(__file__).resolve().parents[2]
        source = root / "third_party" / "courier-prime" / "CourierPrime-Regular.ttf"
        characters = ROLE.parse_characters(
            (root / "localization" / "character-select-name-glyphs.txt").read_text(
                encoding="utf-8"
            )
        )
        compiled = ROLE.compile_font(
            source,
            characters,
            16,
            "72f793376f8e2841656bf21d77a5de010f2929bd6956a22ee848ad0c7eb978af",
        )
        tracked = (
            root / "tools" / "localization" / "T3CSF001-NAMES.BIN"
        ).read_bytes()
        self.assertEqual(compiled, tracked)
        self.assertEqual(
            hashlib.sha256(tracked).hexdigest(),
            "8e74d2d3d232a21c8e2b17cdbb9ec8d37f97a5ea443c3a02717a0e14bd12fb66",
        )
        with self.assertRaisesRegex(ROLE.RoleFontError, "SHA-256"):
            ROLE.compile_font(source, characters, 16, "00" * 32)


if __name__ == "__main__":
    unittest.main()
