#!/usr/bin/env python3
"""Hermetic contract tests for the production resource dependency audit."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class ContentAuditTests(unittest.TestCase):
    tool: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)

    def run_tool(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.tool), *arguments],
            text=True,
            capture_output=True,
            check=False,
        )

    def test_manifest_is_complete_and_deterministic(self) -> None:
        first = self.run_tool()
        second = self.run_tool()
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(first.stdout, second.stdout)
        report = json.loads(first.stdout)
        self.assertEqual(report["format"], "T3CDA001")
        self.assertEqual(report["mode"], "manifest")
        self.assertTrue(report["manifest_valid"])
        self.assertEqual(report["resource_count"], 271)
        self.assertEqual(report["resource_count"], len(report["resources"]))
        self.assertEqual(report["manifest_fnv1a64"], "109855200A365C86")
        self.assertEqual(report["loaded_count"], 0)
        self.assertEqual(report["failure_count"], 0)
        self.assertEqual(len(report["scenes"]), 11)
        self.assertEqual(len(report["native_semantic_surfaces"]), 9)
        self.assertTrue(all(
            item["source_backing_required"]
            for item in report["native_semantic_surfaces"]
        ))
        self.assertEqual(
            hashlib.sha256(first.stdout.encode("utf-8")).hexdigest().upper(),
            "0E36644E0BCE4105C4B22A02C9933B5DB7289107983134D3A4B6D1985F2946E4",
        )

    def test_invalid_content_and_overlay_contract_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            invalid = self.run_tool("--data", str(root / "missing"))
            self.assertNotEqual(invalid.returncode, 0)
            report = json.loads(invalid.stdout)
            self.assertEqual(report["mode"], "validated")
            self.assertEqual(report["failure_count"], report["resource_count"])
            self.assertIn("stage=resource-validation", invalid.stderr)

            overlay_only = self.run_tool(
                "--language-overlay", str(root / "TH3EN.DAT")
            )
            self.assertNotEqual(overlay_only.returncode, 0)
            self.assertEqual(overlay_only.stdout, "")
            self.assertIn("stage=language-overlay-requires-data", overlay_only.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
