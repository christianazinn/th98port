#!/usr/bin/env python3
"""Hermetic tests for New Classic custom-menu background compilation."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

LOCALIZATION_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(LOCALIZATION_ROOT))

import compile_menu_background as compiler

try:
    from PIL import Image
except ImportError:
    Image = None


class MenuBackgroundCompilerTests(unittest.TestCase):
    def test_palette_rounding_and_reserved_colors(self) -> None:
        palette = compiler.complete_palette([
            (1, 9, 16), (250, 129, 64), (1, 9, 16),
        ])
        self.assertEqual(palette[0], (0, 17, 17))
        self.assertEqual(palette[1], (255, 136, 68))
        for index, color in compiler.FIXED_COLORS.items():
            self.assertEqual(palette[index], color)
        self.assertEqual(palette[10], palette[0])

    def test_mapping_uses_no_reserved_indices(self) -> None:
        palette = compiler.complete_palette([
            (0, 0, 0), (255, 255, 255), (255, 0, 0), (0, 0, 255),
        ])
        pixels = [
            ((x * 37) % 256, (y * 53) % 256, ((x + y) * 29) % 256)
            for y in range(6) for x in range(9)
        ]
        for dither in ("none", "floyd-steinberg"):
            first = compiler.map_pixels(pixels, palette, dither, 9, 6)
            second = compiler.map_pixels(pixels, palette, dither, 9, 6)
            self.assertEqual(first, second)
            self.assertTrue(set(first) <= set(compiler.AVAILABLE_INDICES))
            self.assertFalse(set(first) & set(compiler.RESERVED_INDICES))

    def test_split_bmp_round_trip_preserves_orientation(self) -> None:
        palette = compiler.complete_palette([
            (0, 0, 0), (255, 0, 0), (0, 255, 0), (0, 0, 255),
        ])
        allowed = compiler.AVAILABLE_INDICES
        indices = bytes(
            allowed[((y // 25) + (x // 80)) % len(allowed)]
            for y in range(compiler.HEIGHT)
            for x in range(compiler.WIDTH)
        )
        top = compiler.pack_bmp_half(indices, palette, 0)
        bottom = compiler.pack_bmp_half(
            indices, palette, compiler.HALF_HEIGHT
        )
        self.assertEqual(len(top), compiler.BMP_HALF_BYTES)
        self.assertEqual(len(bottom), compiler.BMP_HALF_BYTES)
        decoded_top, top_palette = compiler.unpack_bmp_half(top)
        decoded_bottom, bottom_palette = compiler.unpack_bmp_half(bottom)
        self.assertEqual(decoded_top, indices[:compiler.WIDTH * 200])
        self.assertEqual(decoded_bottom, indices[compiler.WIDTH * 200:])
        self.assertEqual(top_palette, palette)
        self.assertEqual(bottom_palette, palette)

    def test_names_and_colors_are_strict(self) -> None:
        self.assertEqual(compiler.valid_name("m008"), "M008")
        for invalid in ("", "M1234567", "M-01", "../../x"):
            with self.assertRaises(compiler.BackgroundError):
                compiler.valid_name(invalid)
        self.assertEqual(compiler.parse_hex_color("#12aBcD"), (18, 171, 205))
        with self.assertRaises(compiler.BackgroundError):
            compiler.parse_hex_color("black")

    def test_repo_owned_custom_backgrounds_reserve_ui_indices(self) -> None:
        root = LOCALIZATION_ROOT.parents[1] / "assets" / "new-classic" / (
            "menu-backgrounds"
        )
        for number in range(1, 8):
            name = f"M{number:03d}"
            top, top_palette = compiler.unpack_bmp_half(
                (root / f"{name}A.BMP").read_bytes()
            )
            bottom, bottom_palette = compiler.unpack_bmp_half(
                (root / f"{name}B.BMP").read_bytes()
            )
            self.assertEqual(top_palette, bottom_palette, name)
            self.assertFalse(
                (set(top) | set(bottom)) & set(compiler.RESERVED_INDICES),
                name,
            )

    @unittest.skipIf(Image is None, "Pillow is not installed")
    def test_contain_and_stretch_fit_contracts(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "square.png"
            Image.new("RGBA", (20, 20), (255, 0, 0, 128)).save(source)
            contained, size, _ = compiler.load_and_fit(
                source, "contain", (0, 34, 68)
            )
            stretched, _, _ = compiler.load_and_fit(
                source, "stretch", (0, 34, 68)
            )
            self.assertEqual(size, (20, 20))
            self.assertEqual(contained[0], (0, 34, 68))
            self.assertNotEqual(contained[(200 * 640) + 320], contained[0])
            self.assertNotEqual(stretched[0], (0, 34, 68))

    @unittest.skipIf(Image is None, "Pillow is not installed")
    def test_end_to_end_is_deterministic_and_non_destructive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source art.png"
            image = Image.new("RGBA", (960, 600))
            image.putdata([
                (
                    (x * 255) // 959,
                    (y * 255) // 599,
                    ((x + y) * 255) // (959 + 599),
                    96 + ((x + y) % 160),
                )
                for y in range(600) for x in range(960)
            ])
            image.save(source)
            first_root = root / "first"
            second_root = root / "second"
            first_manifest = compiler.compile_background(
                source, first_root, "M008", "cover", "floyd-steinberg",
                (17, 34, 51),
            )
            second_manifest = compiler.compile_background(
                source, second_root, "M008", "cover", "floyd-steinberg",
                (17, 34, 51),
            )
            self.assertEqual(
                first_manifest.read_bytes(), second_manifest.read_bytes()
            )
            for filename in ("M008A.BMP", "M008B.BMP", "M008-preview.png"):
                self.assertEqual(
                    (first_root / filename).read_bytes(),
                    (second_root / filename).read_bytes(),
                )
            document = json.loads(first_manifest.read_text(encoding="utf-8"))
            self.assertEqual(document["format"], compiler.FORMAT)
            self.assertEqual(document["reserved_indices"],
                             list(compiler.RESERVED_INDICES))
            self.assertTrue(all(
                document["index_counts"][index] == 0
                for index in compiler.RESERVED_INDICES
            ))
            self.assertEqual(
                (first_root / "M008A.BMP").stat().st_size,
                compiler.BMP_HALF_BYTES,
            )
            with self.assertRaisesRegex(
                compiler.BackgroundError, "refusing to overwrite"
            ):
                compiler.compile_background(source, first_root, "M008")


if __name__ == "__main__":
    unittest.main()
