#!/usr/bin/env python3
"""End-to-end tests for complete locale bundle publication and checking."""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class LocaleBundleTests(unittest.TestCase):
    tool: Path
    preview_tool: Path
    menu_preview_tool: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)
        cls.preview_tool = Path(OPTIONS.preview_tool)
        cls.menu_preview_tool = Path(OPTIONS.menu_preview_tool)

    @staticmethod
    def menu_font() -> bytes:
        result = bytearray(3_168)
        result[:8] = b"T3PFNT1\0"
        struct.pack_into("<HHHHH", result, 8, 3_168, 32, 32, 128, 95)
        result[18:24] = bytes((0x20, 0x7E, 16, 16, 0x3F, 0))
        result[32:127] = b"\x06" * 95
        result[128:] = b"\xff" * (len(result) - 128)
        struct.pack_into("<H", result, 24, sum(result[32:]) & 0xFFFF)
        return bytes(result)

    def invoke(
        self, command: str, font: Path, output: Path
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable, str(self.tool), command,
                "--font", str(font),
                "--preview-tool", str(self.preview_tool),
                "--menu-preview-tool", str(self.menu_preview_tool),
                "--menu-font", str(font.with_name("MNUFONT.PF")),
                "--output", str(output),
            ],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_build_is_deterministic_and_check_is_read_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / "FONT.ROM"
            font.write_bytes(b"\xff" * 288_768)
            (root / "MNUFONT.PF").write_bytes(self.menu_font())
            first = root / "first"
            second = root / "second"
            built = self.invoke("build", font, first)
            rebuilt = self.invoke("build", font, second)
            self.assertEqual(built.returncode, 0, built.stderr)
            self.assertEqual(rebuilt.returncode, 0, rebuilt.stderr)
            self.assertEqual(
                (first / "manifest.json").read_bytes(),
                (second / "manifest.json").read_bytes(),
            )
            manifest = json.loads((first / "manifest.json").read_text("utf-8"))
            self.assertEqual(manifest["format"], "T3LCB001")
            self.assertEqual(
                [entry["locale"] for entry in manifest["locales"]],
                ["en-US", "ja-JP"],
            )
            for locale in manifest["locales"]:
                self.assertEqual(locale["counts"]["game_text_messages"], 738)
                self.assertEqual(locale["counts"]["preview_pages"], 40)
                self.assertEqual(locale["counts"]["menu_preview_pages"], 41)
            checked = self.invoke("check", font, first)
            self.assertEqual(checked.returncode, 0, checked.stderr)

            pack = first / "en-US/game-text-en-US.t3gtl"
            pack.write_bytes(pack.read_bytes() + b"corrupt")
            before = pack.read_bytes()
            rejected = self.invoke("check", font, first)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("changed:en-US/game-text-en-US.t3gtl", rejected.stdout)
            self.assertEqual(pack.read_bytes(), before)

    def test_failure_preserves_existing_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bad_font = root / "FONT.ROM"
            bad_font.write_bytes(b"short")
            (root / "MNUFONT.PF").write_bytes(self.menu_font())
            output = root / "candidate"
            output.mkdir()
            marker = output / "keep.txt"
            marker.write_text("original", encoding="utf-8")
            failed = self.invoke("build", bad_font, output)
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(marker.read_text(encoding="utf-8"), "original")
            self.assertEqual([path.name for path in output.iterdir()], ["keep.txt"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--preview-tool", required=True)
    parser.add_argument("--menu-preview-tool", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
