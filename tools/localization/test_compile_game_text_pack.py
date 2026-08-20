#!/usr/bin/env python3
"""Hermetic tests for the T3GTL001 compiler."""

from __future__ import annotations

import copy
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "compile_game_text_pack",
    ROOT / "tools/localization/compile_game_text_pack.py",
)
assert SPEC and SPEC.loader
PACK = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PACK
SPEC.loader.exec_module(PACK)


class GameTextPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.schema_path = ROOT / "localization/game-text-schema.json"
        self.author_path = ROOT / "localization/game-text-en-US.json"
        self.schema = PACK.load_schema(self.schema_path)
        self.author = PACK.load_author(self.author_path, self.schema)

    def write_author(self, root: Path, raw: dict[str, object]) -> Path:
        path = root / "author.json"
        path.write_text(json.dumps(raw, ensure_ascii=False), encoding="utf-8")
        return path

    def test_builtin_compiles_deterministically(self) -> None:
        first = PACK.compile_pack(self.schema, self.author)
        second = PACK.compile_pack(self.schema, self.author)
        self.assertEqual(first, second)
        self.assertEqual(first[:8], b"T3GTL001")
        self.assertEqual(struct.unpack_from("<H", first, 12)[0], 61)
        self.assertEqual(first[14], 1)
        self.assertEqual(len(self.schema.digest), 32)

    def test_checked_in_artifacts_match_compiler(self) -> None:
        compiled = PACK.compile_pack(self.schema, self.author)
        self.assertEqual(
            compiled,
            (ROOT / "localization/game-text-en-US.t3gtl").read_bytes(),
        )
        pseudo_path = ROOT / "localization/game-text-qps-ploc.json"
        pseudo = PACK.load_author(pseudo_path, self.schema)
        self.assertEqual(
            PACK.compile_pack(self.schema, pseudo),
            (ROOT / "localization/game-text-qps-ploc.t3gtl").read_bytes(),
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ids = root / "ids.inc"
            data = root / "data.inc"
            PACK.generate_cpp(self.schema, [self.author], ids, data)
            self.assertEqual(
                ids.read_bytes(),
                (ROOT / "include/th03/app/new_classic_game_text_ids.inc").read_bytes(),
            )
            self.assertEqual(
                data.read_bytes(),
                (ROOT / "src/app/new_classic_game_text_generated.inc").read_bytes(),
            )

    def test_author_is_complete_and_exact(self) -> None:
        self.assertEqual(len(self.schema.messages), 61)
        resolved = PACK.resolve_author(
            self.author, {self.author.locale: self.author}, self.schema
        )
        self.assertEqual(len(resolved), 61)
        for message, value in zip(self.schema.messages, resolved, strict=True):
            self.assertEqual(len(PACK.validate_message(message, value)), message.exact_bytes)

    def test_rejects_unencodable_and_wrong_width_text(self) -> None:
        raw = json.loads(self.author_path.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            wrong = copy.deepcopy(raw)
            wrong["messages"]["music.track.01"] = "short"
            with self.assertRaisesRegex(PACK.PackError, "expected 32"):
                PACK.load_author(self.write_author(root, wrong), self.schema)
            unencodable = copy.deepcopy(raw)
            unencodable["messages"]["music.track.01"] = "😀" * 32
            with self.assertRaisesRegex(PACK.PackError, "not representable"):
                PACK.load_author(self.write_author(root, unencodable), self.schema)

    def test_fallback_entries_are_encoded_as_empty_directory_rows(self) -> None:
        raw = {
            "format": "T3GTL-AUTHOR-1",
            "locale": "test",
            "fallback": "en-US",
            "messages": {},
        }
        with tempfile.TemporaryDirectory() as temporary:
            author = PACK.load_author(
                self.write_author(Path(temporary), raw), self.schema
            )
        compiled = PACK.compile_pack(self.schema, author)
        directory = PACK.HEADER_SIZE + len("test") + len("en-US")
        self.assertEqual(compiled[directory:directory + PACK.ENTRY_SIZE], b"\0" * 8)

    def test_schema_rejects_duplicate_generated_identifiers(self) -> None:
        raw = json.loads(self.schema_path.read_text(encoding="utf-8"))
        raw["messages"].append({
            "enum": "duplicate",
            "id": "music.track.01",
            "encoding": "cp932",
            "exact_bytes": 32,
        })
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "schema.json"
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(PACK.PackError, "duplicate schema id"):
                PACK.load_schema(path)


if __name__ == "__main__":
    unittest.main()
