#!/usr/bin/env python3
"""Hermetic tests for the T3CTR001 author-scaffold converter."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools/localization/make_cutscene_text_author.py"
SPEC = importlib.util.spec_from_file_location("cutscene_author", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
cutscene_author = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cutscene_author)


def inventory() -> dict[str, object]:
    scripts: list[dict[str, object]] = []
    for script_index in range(28):
        script_id = f"dm{script_index}.reimu"
        run_id = f"cutscene.{script_id}.run.000"
        scripts.append({
            "id": script_id,
            "resource": f"SCRIPT{script_index:02}.TXT",
            "source_bytes": 4,
            "source_fnv1a64": f"{script_index + 1:016X}",
            "runs": [{
                "id": run_id,
                "ordinal": 0,
                "source_offset": 0,
                "glyphs": 2,
                "first": [80, 320],
                "last": [96, 320],
                "source_fnv1a64": f"{script_index + 101:016X}",
                "cp932_hex": "82A082A2",
            }],
        })
    return {
        "format": "T3CTR001",
        "source_profile": "th03-original-28",
        "source_profile_fnv1a64": "0123456789ABCDEF",
        "scripts": scripts,
    }


class CutsceneAuthorTests(unittest.TestCase):
    def test_complete_inventory_becomes_stable_utf8_authoring(self) -> None:
        source = inventory()
        author = cutscene_author.make_author(source, "ja-JP", "")
        self.assertEqual(author["format"], "T3CTR-AUTHOR-1")
        self.assertEqual(len(author["messages"]), 28)
        first = author["messages"]["cutscene.dm0.reimu.run.000"]
        self.assertEqual(first, {"exact_glyphs": 2, "text": "あい"})
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "author.json"
            cutscene_author.write_author(output, author)
            first_bytes = output.read_bytes()
            cutscene_author.write_author(output, author)
            self.assertEqual(first_bytes, output.read_bytes())
            self.assertIn("あい".encode("utf-8"), first_bytes)

    def test_fail_closed_inventory_contract(self) -> None:
        cases: list[dict[str, object]] = []
        missing = inventory()
        missing["scripts"] = missing["scripts"][:-1]
        cases.append(missing)
        duplicate = inventory()
        duplicate["scripts"][1]["runs"][0]["id"] = (
            duplicate["scripts"][0]["runs"][0]["id"]
        )
        cases.append(duplicate)
        bad_cp932 = inventory()
        bad_cp932["scripts"][0]["runs"][0]["cp932_hex"] = "82FF82A2"
        cases.append(bad_cp932)
        bad_ordinal = inventory()
        bad_ordinal["scripts"][0]["runs"][0]["ordinal"] = 1
        cases.append(bad_ordinal)
        for invalid in cases:
            with self.subTest(case=cases.index(invalid)):
                with self.assertRaises(cutscene_author.AuthorError):
                    cutscene_author.make_author(invalid, "ja-JP", "")

    def test_locale_contract(self) -> None:
        source = inventory()
        with self.assertRaises(cutscene_author.AuthorError):
            cutscene_author.make_author(source, "ja-JP", "ja-JP")
        with self.assertRaises(cutscene_author.AuthorError):
            cutscene_author.make_author(source, "not a locale", "")


if __name__ == "__main__":
    unittest.main()
