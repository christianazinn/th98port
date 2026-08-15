#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("compile_language_pack.py")
SPEC = importlib.util.spec_from_file_location("compile_language_pack", MODULE_PATH)
assert SPEC and SPEC.loader
pack = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = pack
SPEC.loader.exec_module(pack)


class LanguagePackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.root = Path(__file__).resolve().parents[2]
        self.schema = pack.load_schema(self.root / "localization/schema.json")

    def author(self, name: str):
        return pack.load_author(self.root / f"localization/{name}.json", self.schema)

    def test_deterministic_binary_and_fallback(self) -> None:
        english = self.author("en-US")
        japanese = self.author("ja-JP")
        first = pack.compile_pack(self.schema, english)
        second = pack.compile_pack(self.schema, english)
        self.assertEqual(first, second)
        self.assertEqual(first[:8], b"T3LNG001")
        self.assertEqual(hashlib.sha256(first).hexdigest(),
                         hashlib.sha256(second).hexdigest())
        authors = {author.locale: author for author in (english, japanese)}
        self.assertEqual(
            pack.resolve_author(japanese, authors, self.schema),
            pack.resolve_author(english, authors, self.schema),
        )

    def test_unknown_missing_placeholder_and_glyph_fail_closed(self) -> None:
        original = json.loads((self.root / "localization/en-US.json").read_text())
        cases = []
        unknown = json.loads(json.dumps(original))
        unknown["messages"]["unknown.message"] = "No"
        cases.append(unknown)
        missing = json.loads(json.dumps(original))
        del missing["messages"]["title.start"]
        cases.append(missing)
        placeholder = json.loads(json.dumps(original))
        placeholder["messages"]["value.percent"] = "percent"
        cases.append(placeholder)
        glyph = json.loads(json.dumps(original))
        glyph["messages"]["title.start"] = "Stärt"
        cases.append(glyph)
        with tempfile.TemporaryDirectory() as temporary:
            for index, case in enumerate(cases):
                path = Path(temporary) / f"case-{index}.json"
                path.write_text(json.dumps(case), encoding="utf-8")
                with self.assertRaises(pack.PackError):
                    pack.load_author(path, self.schema)

    def test_generated_sources_are_current(self) -> None:
        authors = [self.author(name) for name in ("en-US", "ja-JP", "qps-ploc")]
        with tempfile.TemporaryDirectory() as temporary:
            ids = Path(temporary) / "ids.inc"
            data = Path(temporary) / "data.inc"
            pack.generate_cpp(self.schema, authors, ids, data)
            self.assertEqual(
                ids.read_bytes(),
                (self.root / "include/th03/app/new_classic_message_ids.inc").read_bytes(),
            )
            self.assertEqual(
                data.read_bytes(),
                (self.root / "src/app/new_classic_messages_generated.inc").read_bytes(),
            )


if __name__ == "__main__":
    unittest.main()
