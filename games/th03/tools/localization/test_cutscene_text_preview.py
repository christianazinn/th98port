#!/usr/bin/env python3
"""End-to-end tests for production-rendered T3CTP001 contact sheets."""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class CutsceneTextPreviewTests(unittest.TestCase):
    tool: Path
    source_root: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)
        cls.source_root = Path(OPTIONS.source_root)

    def invoke(
        self, font: Path, pack: Path, output: Path
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                str(self.tool),
                "--font", str(font),
                "--cutscene-text-pack", str(pack),
                "--output", str(output),
            ],
            text=True,
            capture_output=True,
            check=False,
        )

    @staticmethod
    def assert_bmp(test: unittest.TestCase, path: Path) -> None:
        payload = path.read_bytes()
        test.assertEqual(len(payload), 128_118)
        test.assertEqual(payload[:2], b"BM")
        test.assertEqual(struct.unpack_from("<I", payload, 2)[0], len(payload))
        test.assertEqual(struct.unpack_from("<II", payload, 18), (640, 400))
        test.assertEqual(struct.unpack_from("<H", payload, 28)[0], 4)

    def test_english_is_deterministic_and_japanese_is_complete(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / "FONT.ROM"
            font.write_bytes(b"\xff" * 288_768)
            first = root / "first"
            second = root / "second"
            japanese = root / "japanese"
            english_pack = self.source_root / "localization/cutscene-text-en-US.t3ctp"
            japanese_pack = self.source_root / "localization/cutscene-text-ja-JP.t3ctp"
            first_run = self.invoke(font, english_pack, first)
            second_run = self.invoke(font, english_pack, second)
            japanese_run = self.invoke(font, japanese_pack, japanese)
            self.assertEqual(first_run.returncode, 0, first_run.stderr)
            self.assertEqual(second_run.returncode, 0, second_run.stderr)
            self.assertEqual(japanese_run.returncode, 0, japanese_run.stderr)

            first_files = {
                path.name: path.read_bytes()
                for path in first.iterdir() if path.is_file()
            }
            second_files = {
                path.name: path.read_bytes()
                for path in second.iterdir() if path.is_file()
            }
            self.assertEqual(first_files, second_files)

            for output, locale, messages in (
                (first, "en-US", 1_732),
                (japanese, "ja-JP", 1_533),
            ):
                report = json.loads((output / "preview.json").read_text("utf-8"))
                entries = report["messages"]
                self.assertEqual(report["format"], "T3STP001")
                self.assertEqual(report["locale"], locale)
                self.assertEqual(len(entries), messages)
                pages = {entry["page"] for entry in entries}
                self.assertEqual(
                    {path.stem for path in output.glob("*.bmp")}, pages
                )
                self.assertTrue(all(
                    entry["width"] == entry["glyphs"] * 16
                    and entry["width"] <= 624
                    for entry in entries
                ))
                for page in pages:
                    self.assert_bmp(self, output / f"{page}.bmp")

    def test_malformed_pack_fails_without_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / "FONT.ROM"
            font.write_bytes(b"\xff" * 288_768)
            pack = root / "malformed.t3ctp"
            pack.write_bytes(b"not a cutscene text pack")
            output = root / "preview"
            failed = self.invoke(font, pack, output)
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn("stage=cutscene-text-pack", failed.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--source-root", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
