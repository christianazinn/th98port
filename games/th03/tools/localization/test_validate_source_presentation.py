#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("validate_source_presentation.py")
SPEC = importlib.util.spec_from_file_location("validate_source_presentation", MODULE_PATH)
assert SPEC and SPEC.loader
policy = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = policy
SPEC.loader.exec_module(policy)


class SourcePresentationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.source_root = Path(__file__).resolve().parents[2]

    def test_repository_catalogs_obey_policy(self) -> None:
        invariants, surfaces = policy.validate(self.source_root)
        self.assertEqual(invariants, 23)
        self.assertEqual(surfaces, 4)

    def test_translated_source_english_label_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            localization = root / "localization"
            localization.mkdir()
            for path in self.source_root.joinpath("localization").glob(
                "game-text-*.json"
            ):
                shutil.copy2(path, localization / path.name)
            shutil.copy2(
                self.source_root / "localization/source-presentation-policy.json",
                localization / "source-presentation-policy.json",
            )
            japanese_path = localization / "game-text-ja-JP.json"
            japanese = json.loads(japanese_path.read_text(encoding="utf-8"))
            japanese["messages"]["hud.banner.start"] = "開始            "
            japanese_path.write_text(
                json.dumps(japanese, ensure_ascii=False), encoding="utf-8"
            )
            with self.assertRaisesRegex(
                policy.PolicyError, "source-English invariant changed"
            ):
                policy.validate(root)


if __name__ == "__main__":
    unittest.main()
