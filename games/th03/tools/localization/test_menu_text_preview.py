#!/usr/bin/env python3
"""End-to-end tests for production-rendered T3LNG001 contact sheets."""

from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


def menu_font() -> bytes:
    result = bytearray(3_168)
    result[:8] = b"T3PFNT1\0"
    struct.pack_into("<HHHHH", result, 8, 3_168, 32, 32, 128, 95)
    result[18:24] = bytes((0x20, 0x7E, 16, 16, 0x3F, 0))
    result[32:127] = b"\x06" * 95
    result[128:] = b"\xff" * (len(result) - 128)
    struct.pack_into("<H", result, 24, sum(result[32:]) & 0xFFFF)
    return bytes(result)


class MenuTextPreviewTests(unittest.TestCase):
    tool: Path
    source_root: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)
        cls.source_root = Path(OPTIONS.source_root)

    def invoke(
        self,
        font: Path,
        machine_font: Path,
        output: Path,
        locale: str,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                str(self.tool),
                "--font", str(machine_font),
                "--menu-font", str(font),
                "--language-pack", str(
                    self.source_root / f"localization/{locale}.t3lang"
                ),
                "--output", str(output),
            ],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_english_is_deterministic_and_japanese_is_complete(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / "MNUFONT.PF"
            font.write_bytes(menu_font())
            machine_font = root / "FONT.ROM"
            machine_font.write_bytes(b"\xff" * 288_768)
            first = root / "first"
            second = root / "second"
            japanese = root / "japanese"
            first_run = self.invoke(font, machine_font, first, "en-US")
            second_run = self.invoke(font, machine_font, second, "en-US")
            japanese_run = self.invoke(font, machine_font, japanese, "ja-JP")
            self.assertEqual(first_run.returncode, 0, first_run.stderr)
            self.assertEqual(second_run.returncode, 0, second_run.stderr)
            self.assertEqual(japanese_run.returncode, 0, japanese_run.stderr)
            names = [f"menu-{index:02}.ppm" for index in range(41)] + [
                "preview.json"
            ]
            self.assertEqual(
                [(first / name).read_bytes() for name in names],
                [(second / name).read_bytes() for name in names],
            )
            english_report = json.loads((first / "preview.json").read_text("utf-8"))
            japanese_report = json.loads(
                (japanese / "preview.json").read_text("utf-8")
            )
            for report, locale in (
                (english_report, "en-US"), (japanese_report, "ja-JP")
            ):
                self.assertEqual(report["format"], "T3MTP001")
                self.assertEqual(report["locale"], locale)
                self.assertEqual(len(report["messages"]), 410)
                self.assertTrue(all(
                    entry["width"] <= entry["maximum_width"]
                    for entry in report["messages"]
                ))
            for name in names[:-1]:
                payload = (first / name).read_bytes()
                self.assertTrue(payload.startswith(b"P6\n640 400\n255\n"))
                self.assertEqual(
                    len(payload), len(b"P6\n640 400\n255\n") + 640 * 400 * 3
                )

    def test_malformed_font_fails_without_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            font = root / "MNUFONT.PF"
            malformed = bytearray(menu_font())
            malformed[-1] ^= 1
            font.write_bytes(malformed)
            machine_font = root / "FONT.ROM"
            machine_font.write_bytes(b"\xff" * 288_768)
            output = root / "preview"
            failed = self.invoke(font, machine_font, output, "en-US")
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn("stage=menu-font-decode", failed.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--source-root", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
