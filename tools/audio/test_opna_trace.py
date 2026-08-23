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

    def test_compares_controlled_effect_updates_and_duration(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            markers = root / "markers.tsv"
            markers.write_text(
                "ordinal\tpic_ms\tcode\n"
                "0\t0.000\t61\n"
                "1\t100.000\t81\n",
                encoding="ascii",
            )
            pc = root / "pc.tsv"
            pc.write_text(
                "sequence\tsource_ordinal\tpic_ms\taddress\tvalue\n"
                "1\t0\t1.000\t028\t06\n"
                "2\t1\t1.100\t1A2\t11\n"
                "3\t2\t1.200\t1A6\t22\n"
                "4\t3\t1.300\t028\tF6\n"
                "5\t4\t11.200\t028\t06\n"
                "6\t5\t11.300\t1B6\tC0\n",
                encoding="ascii",
            )
            native = root / "native"
            native.mkdir()
            registers = native / "effect-01-registers.tsv"
            registers.write_text(
                "sequence\tchip_clock\taddress\tvalue\n"
                "1\t0\t028\t06\n"
                "2\t0\t1A2\t11\n"
                "3\t0\t1A6\t22\n"
                "4\t0\t028\tF6\n"
                "5\t79872\t028\t06\n"
                "6\t79872\t1B6\tC0\n",
                encoding="ascii",
            )
            (native / "effect-01-events.tsv").write_text(
                "event\tsequence\tchip_clock\n"
                "effect-call\t0\t0\n"
                "effect-started\t4\t0\n"
                "effect-ended\t6\t79872\n",
                encoding="ascii",
            )
            matched = self.invoke(
                "compare-effects", "--markers", markers,
                "--pc-registers", pc, "--native-directory", native,
                "--effect", 1,
            )
            self.assertEqual(matched.returncode, 0, matched.stdout + matched.stderr)
            self.assertIn("effect=01 status=match", matched.stdout)
            self.assertIn("pc_groups=2 native_groups=2", matched.stdout)

            registers.write_text(
                registers.read_text("ascii").replace("1A2\t11", "1A2\t12"),
                encoding="ascii",
            )
            different = self.invoke(
                "compare-effects", "--markers", markers,
                "--pc-registers", pc, "--native-directory", native,
                "--effect", 1,
            )
            self.assertEqual(different.returncode, 1)
            self.assertIn("effect=01 status=different", different.stdout)
            self.assertIn("effect=01 first_group=1", different.stdout)
            self.assertIn("effect=01 pc=1A2:11 native=1A2:12", different.stdout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])
