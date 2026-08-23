#!/usr/bin/env python3
"""End-to-end tests for deterministic native PMD register traces."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
import unittest
from pathlib import Path


def set_le16(data: bytearray, offset: int, value: int) -> None:
    data[offset:offset + 2] = value.to_bytes(2, "little")


def synthetic_song() -> bytes:
    data = bytearray(1 + 0x1A)

    def emit(values: bytes) -> int:
        offset = len(data) - 1
        data.extend(values)
        return offset

    empty = emit(bytes((0x80,)))
    fm6 = emit(bytes((0xFF, 0x00, 0x40, 0x60, 0x80)))
    rhythm = emit(bytes((0x80,)))
    parts = [empty] * 11
    parts[5] = fm6
    for index, offset in enumerate(parts):
        set_le16(data, 1 + index * 2, offset)
    set_le16(data, 1 + 0x16, rhythm)
    tone = len(data) - 1
    set_le16(data, 1 + 0x18, tone)
    data.extend(bytes((
        0x00,
        0x01, 0x01, 0x01, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x1F, 0x1F, 0x1F, 0x1F,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x21,
    )))
    return bytes(data)


def synthetic_efc() -> bytes:
    data = bytearray(128 * 2)
    for effect in range(127):
        set_le16(data, effect * 2, len(data))
        data.extend(bytes((0xFF, 0x00, 0xFD, 0x7F, 0x40, 0x01, 0x80)))
    set_le16(data, 127 * 2, len(data))
    data.extend(bytes((
        0x00,
        0x01, 0x01, 0x01, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x1F, 0x1F, 0x1F, 0x1F,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x0F, 0x0F, 0x0F, 0x0F,
        0x07,
    )))
    return bytes(data)


class PmdTraceTests(unittest.TestCase):
    tool: Path

    @classmethod
    def setUpClass(cls) -> None:
        cls.tool = Path(OPTIONS.tool)

    def invoke(
        self, song: Path, efc: Path, output: Path, *extra: str
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                str(self.tool), "--song", str(song), "--efc", str(efc),
                "--output", str(output), *extra,
            ],
            text=True, capture_output=True, check=False,
        )

    @staticmethod
    def tree(root: Path) -> dict[str, bytes]:
        return {
            path.relative_to(root).as_posix(): path.read_bytes()
            for path in root.rglob("*") if path.is_file()
        }

    def test_all_effects_are_complete_and_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            song = root / "OP.M"
            efc = root / "YUME.EFC"
            song.write_bytes(synthetic_song())
            efc.write_bytes(synthetic_efc())
            first = root / "first"
            second = root / "second"
            first_run = self.invoke(song, efc, first)
            second_run = self.invoke(song, efc, second)
            self.assertEqual(first_run.returncode, 0, first_run.stderr)
            self.assertEqual(second_run.returncode, 0, second_run.stderr)
            self.assertEqual(self.tree(first), self.tree(second))
            manifest = json.loads((first / "manifest.json").read_text("utf-8"))
            self.assertEqual(manifest["format"], "T3ATR001")
            self.assertEqual(manifest["opna_clock_hz"], 7_987_200)
            self.assertEqual(len(manifest["scenarios"]), 22)
            self.assertEqual(
                [entry["effect"] for entry in manifest["scenarios"]],
                list(range(1, 23)),
            )
            for entry in manifest["scenarios"]:
                self.assertGreater(entry["register_writes"], 0)
                registers = first / entry["registers"]
                events = first / entry["events"]
                self.assertTrue(registers.read_text("ascii").startswith(
                    "sequence\tchip_clock\taddress\tvalue\n"
                ))
                self.assertEqual(
                    events.read_text("ascii").splitlines()[1].split("\t")[0],
                    "play-complete",
                )

    def test_invalid_effect_fails_before_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            song = root / "OP.M"
            efc = root / "YUME.EFC"
            song.write_bytes(synthetic_song())
            efc.write_bytes(synthetic_efc())
            output = root / "trace"
            failed = self.invoke(song, efc, output, "--effect", "23")
            self.assertNotEqual(failed.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--tool", required=True)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])
