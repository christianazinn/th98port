#!/usr/bin/env python3
"""Hermetic tests for the complete T3CTP001 compiler."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "compile_cutscene_text_pack",
    ROOT / "tools/localization/compile_cutscene_text_pack.py",
)
assert SPEC is not None and SPEC.loader is not None
PACK = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PACK
SPEC.loader.exec_module(PACK)


def fixture() -> tuple[dict[str, object], dict[str, object]]:
    scripts: list[dict[str, object]] = []
    messages: dict[str, object] = {}
    for index in range(28):
        script_id = f"dm{index}.reimu"
        identifier = f"cutscene.{script_id}.run.000"
        scripts.append({
            "id": script_id,
            "resource": f"SCRIPT{index:02}.TXT",
            "source_bytes": 4,
            "source_fnv1a64": f"{index + 1:016X}",
            "runs": [{
                "id": identifier,
                "ordinal": 0,
                "source_offset": 0,
                "glyphs": 2,
                "first": [80, 320],
                "last": [96, 320],
                "source_fnv1a64": f"{index + 101:016X}",
                "cp932_hex": "82A082A2",
            }],
        })
        messages[identifier] = {"exact_glyphs": 2, "text": "うえ"}
    inventory = {
        "format": "T3CTR001",
        "source_profile": "th03-original-28",
        "source_profile_fnv1a64": "0123456789ABCDEF",
        "scripts": scripts,
    }
    author = {
        "format": "T3CTR-AUTHOR-1",
        "source_profile": "th03-original-28",
        "source_profile_fnv1a64": "0123456789ABCDEF",
        "locale": "test",
        "fallback": "",
        "messages": messages,
    }
    return inventory, author


class CutsceneTextPackTests(unittest.TestCase):
    def load(self, inventory: dict[str, object], author: dict[str, object]):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            inventory_path = root / "inventory.json"
            author_path = root / "author.json"
            inventory_path.write_text(json.dumps(inventory), encoding="utf-8")
            author_path.write_text(json.dumps(author), encoding="utf-8")
            return PACK.load_catalog(inventory_path, author_path)

    def test_complete_pack_is_deterministic(self) -> None:
        inventory, author = fixture()
        catalog = self.load(inventory, author)
        first = PACK.compile_pack(catalog)
        self.assertEqual(first, PACK.compile_pack(catalog))
        self.assertEqual(first[:8], b"T3CTP001")
        self.assertEqual(int.from_bytes(first[12:14], "little"), 28)
        self.assertEqual(int.from_bytes(first[20:24], "little"), 28)

    def test_accepts_two_halfwidth_glyphs_per_vm_cell(self) -> None:
        inventory, author = fixture()
        halfwidth = copy.deepcopy(author)
        first = next(iter(halfwidth["messages"].values()))
        first["text"] = "ABCD"
        catalog = self.load(inventory, halfwidth)
        self.assertEqual(catalog.scripts[0].runs[0].replacement, b"ABCD")

    def test_rejects_partial_mismatched_and_split_cells(self) -> None:
        inventory, author = fixture()
        partial = copy.deepcopy(author)
        partial["messages"].pop(next(iter(partial["messages"])))
        mismatched = copy.deepcopy(author)
        first = next(iter(mismatched["messages"].values()))
        first["exact_glyphs"] = 1
        split_cell = copy.deepcopy(author)
        first = next(iter(split_cell["messages"].values()))
        first["text"] = "AあB"
        fallback = copy.deepcopy(author)
        fallback["fallback"] = "en-US"
        for invalid in (partial, mismatched, split_cell, fallback):
            with self.subTest():
                with self.assertRaises(PACK.PackError):
                    self.load(inventory, invalid)

    def test_rejects_profile_and_unknown_message(self) -> None:
        inventory, author = fixture()
        profile = copy.deepcopy(author)
        profile["source_profile_fnv1a64"] = "FEDCBA9876543210"
        unknown = copy.deepcopy(author)
        unknown["messages"]["cutscene.unknown.run.000"] = {
            "exact_glyphs": 2, "text": "うえ"
        }
        for invalid in (profile, unknown):
            with self.subTest():
                with self.assertRaises(PACK.PackError):
                    self.load(inventory, invalid)


if __name__ == "__main__":
    unittest.main()
