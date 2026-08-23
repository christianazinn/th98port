#!/usr/bin/env python3
"""Tests for the strict PC-98 OPNA trace normalizer and comparator."""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class OpnaTraceTests(unittest.TestCase):
    tool: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)

    def invoke(self, *arguments: object) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(self.tool), *(str(value) for value in arguments)],
            text=True, capture_output=True, check=False,
        )

    def test_normalizes_both_banks_and_compares_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            raw = root / "raw.tsv"
            raw.write_text(
                "ordinal\tpic_ms\tport\tvalue\n"
                "0\t1.000000000\t0188\t28\n"
                "1\t1.100000000\t018A\tF6\n"
                "2\t1.200000000\t018C\tB4\n"
                "3\t1.300000000\t018E\tC0\n",
                encoding="ascii",
            )
            normalized = root / "normalized.tsv"
            completed = self.invoke(
                "normalize", "--input", raw, "--output", normalized
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(
                normalized.read_text("ascii"),
                "sequence\tsource_ordinal\tpic_ms\taddress\tvalue\n"
                "1\t1\t1.100000000\t028\tF6\n"
                "2\t3\t1.300000000\t1B4\tC0\n",
            )
            matched = self.invoke(
                "compare", "--reference", normalized,
                "--candidate", normalized,
            )
            self.assertEqual(matched.returncode, 0, matched.stdout + matched.stderr)
            self.assertIn("status=match", matched.stdout)

            changed = root / "changed.tsv"
            changed.write_text(
                normalized.read_text("ascii").replace("1B4\tC0", "1B4\tC1"),
                encoding="ascii",
            )
            different = self.invoke(
                "compare", "--reference", normalized, "--candidate", changed
            )
            self.assertEqual(different.returncode, 1)
            self.assertIn("first_sequence=2", different.stdout)
            self.assertIn("reference=1B4:C0", different.stdout)
            self.assertIn("candidate=1B4:C1", different.stdout)

    def test_rejects_unlatched_data_without_publishing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            raw = root / "raw.tsv"
            raw.write_text(
                "ordinal\tpic_ms\tport\tvalue\n"
                "0\t1.0\t018A\t00\n",
                encoding="ascii",
            )
            output = root / "normalized.tsv"
            failed = self.invoke("normalize", "--input", raw, "--output", output)
            self.assertEqual(failed.returncode, 2)
            self.assertIn("has no bank-0 address", failed.stderr)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
