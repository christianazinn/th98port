#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("make_fixed_game_text_author.py")
SPEC = importlib.util.spec_from_file_location("make_fixed_game_text_author", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def row(text_bytes: int, marker: bytes) -> bytes:
    return (marker + (b" " * text_bytes))[:text_bytes] + b"\r\n"


class FixedGameTextAuthorTests(unittest.TestCase):
    def members(self) -> dict[str, bytes]:
        result = {
            name: b"".join(row(58, f"W{character:02d}{line:02d}".encode())
                            for line in range(33))
            for character, name in enumerate(MODULE.WIN_FILES)
        }
        result[MODULE.MUSIC_FILE] = b"".join(
            row(40, f"M{track:02d}{line:02d}".encode())
            for track in range(19) for line in range(20)
        )
        return result

    def test_extracts_every_stable_row(self) -> None:
        messages = MODULE.extract_messages(self.members())
        self.assertEqual(len(messages), (9 * 33) + (19 * 20))
        self.assertTrue(messages["win.quote.08.32"].startswith("W0832"))
        self.assertTrue(messages["music.comment.19.19"].startswith("M1819"))

    def test_rejects_bad_crlf_and_cp932(self) -> None:
        members = self.members()
        members["@00TX.TXT"] = members["@00TX.TXT"][:-1] + b"X"
        with self.assertRaises(MODULE.AuthorError):
            MODULE.extract_messages(members)

    def test_accepts_source_final_row_without_crlf(self) -> None:
        members = self.members()
        members["@03TX.TXT"] = members["@03TX.TXT"][:-2]
        messages = MODULE.extract_messages(members)
        self.assertTrue(messages["win.quote.03.32"].startswith("W0332"))
        members = self.members()
        bad = bytearray(members[MODULE.MUSIC_FILE])
        bad[0] = 0x81
        bad[1] = 0x20
        members[MODULE.MUSIC_FILE] = bytes(bad)
        with self.assertRaises(MODULE.AuthorError):
            MODULE.extract_messages(members)

    def test_base_merge_preserves_existing_messages(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "base.json"
            path.write_text(json.dumps({
                "format": MODULE.GAME_AUTHOR_FORMAT,
                "locale": "en-US",
                "fallback": "",
                "messages": {"existing": "value"},
            }), encoding="utf-8")
            base = MODULE.load_base(path, "en-US")
            merged = dict(base["messages"])
            merged.update(MODULE.extract_messages(self.members()))
            self.assertEqual(merged["existing"], "value")
            self.assertEqual(len(merged), 1 + (9 * 33) + (19 * 20))


if __name__ == "__main__":
    unittest.main()
