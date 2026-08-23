#!/usr/bin/env python3
"""End-to-end tests for deterministic production-font game-text previews."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class PreviewTests(unittest.TestCase):
    tool: Path
    source_root: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)
        cls.source_root = Path(OPTIONS.source_root)

    def run_preview(self, root: Path, *extra: str) -> subprocess.CompletedProcess[str]:
        root.mkdir(parents=True, exist_ok=True)
        font = root / "FONT.ROM"
        if not font.exists():
            font.write_bytes(b"\xff" * 288_768)
        output = root / "preview"
        return subprocess.run(
            [str(self.tool), "--font", str(font), "--output", str(output), *extra],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_builtin_and_custom_contact_sheets_are_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = self.run_preview(root / "first")
            second = self.run_preview(root / "second")
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertEqual(second.returncode, 0, second.stderr)
            names = sorted(
                path.name for path in (root / "first/preview").glob("*.ppm")
            ) + ["preview.json"]
            self.assertEqual(len(names), 44)
            first_files = [(root / "first/preview" / name).read_bytes() for name in names]
            second_files = [(root / "second/preview" / name).read_bytes() for name in names]
            self.assertEqual(first_files, second_files)
            report = json.loads(first_files[-1])
            self.assertEqual(report["format"], "T3GTP001")
            self.assertEqual(report["locale"], "en-US")
            self.assertEqual(len(report["messages"]), 761)
            self.assertEqual(
                {entry["id"] for entry in report["messages"]},
                {
                    line.split('"')[1]
                    for line in (
                        self.source_root / "include/th03/app/new_classic_game_text_ids.inc"
                    ).read_text(encoding="utf-8").splitlines()
                    if line.strip()
                },
            )
            for payload in first_files[:-1]:
                self.assertTrue(payload.startswith(b"P6\n640 400\n255\n"))
                self.assertEqual(len(payload), len(b"P6\n640 400\n255\n") + 640 * 400 * 3)

            pack = root / "en-US.t3gtl"
            compiled = subprocess.run(
                [
                    sys.executable,
                    str(self.source_root / "tools/localization/compile_game_text_pack.py"),
                    "compile",
                    "--schema", str(self.source_root / "localization/game-text-schema.json"),
                    "--source", str(self.source_root / "localization/game-text-en-US.json"),
                    "--output", str(pack),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            custom_root = root / "custom"
            custom = self.run_preview(
                custom_root, "--game-text-pack", str(pack)
            )
            self.assertEqual(custom.returncode, 0, custom.stderr)
            custom_files = [
                (custom_root / "preview" / name).read_bytes() for name in names
            ]
            self.assertEqual(first_files, custom_files)

            digest = hashlib.sha256(b"".join(first_files)).hexdigest().upper()
            self.assertEqual(len(digest), 64)

    def test_missing_font_and_malformed_pack_fail_without_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            missing = subprocess.run(
                [str(self.tool), "--font", str(root / "missing.rom"),
                 "--output", str(root / "missing-output")],
                text=True, capture_output=True, check=False,
            )
            self.assertNotEqual(missing.returncode, 0)
            self.assertIn("stage=font-read", missing.stderr)
            malformed = root / "bad.t3gtl"
            malformed.write_bytes(b"T3GTL001")
            failed = self.run_preview(root / "bad-run", "--game-text-pack", str(malformed))
            self.assertNotEqual(failed.returncode, 0)
            self.assertIn("stage=game-text-pack", failed.stderr)
            self.assertFalse((root / "bad-run/preview").exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    parser.add_argument("--source-root", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
