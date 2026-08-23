#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import sys
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("make_japanese_game_text_author.py")
SPEC = importlib.util.spec_from_file_location("make_japanese_game_text_author", MODULE_PATH)
assert SPEC and SPEC.loader
author = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = author
SPEC.loader.exec_module(author)


class JapaneseGameTextAuthorTests(unittest.TestCase):
    def test_source_profiles_make_complete_exact_catalog(self) -> None:
        root = Path(__file__).resolve().parents[2]
        originals = root.parent / "originals" / "th03"
        if not originals.is_dir():
            self.skipTest("authenticated Japanese executable profiles unavailable")
        result = author.build_author(
            root / "localization/game-text-schema.json",
            root / "localization/game-fixed-text-ja-JP.json",
            originals / "OP.EXE", originals / "MAINL.EXE", originals / "MAIN.EXE",
        )
        tracked = json.loads(
            (root / "localization/game-text-ja-JP.json").read_text(encoding="utf-8")
        )
        self.assertEqual(result, tracked)
        self.assertEqual(len(result["messages"]), 743)
        self.assertEqual(result["messages"]["music.track.01"].encode("cp932"),
                         b"NO.1     \x81\x40\x96\xb2\x82\xcd\x8e\x9e\x8b\xf3\x82\xf0\x89\x7a\x82\xa6\x82\xc4     ")
        self.assertEqual(len(result["messages"]["stage.name.08"].encode("cp932")), 28)
        self.assertEqual(len(result["messages"]["result.lives-left"].encode("cp932")), 26)
        self.assertTrue(result["messages"]["win.quote.00.00"].startswith("博麗"))
        self.assertEqual(result["messages"]["hud.banner.start"].rstrip(), "開始")

    def test_byte_padding_rejects_overflow(self) -> None:
        self.assertEqual(author.pad_cp932("夢", 4, "x").encode("cp932"), b"\x96\xb2  ")
        with self.assertRaises(author.AuthorError):
            author.pad_cp932("夢夢夢", 4, "x")


if __name__ == "__main__":
    unittest.main()
