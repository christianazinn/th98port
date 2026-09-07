#!/usr/bin/env python3
"""Build the complete source-authentic Japanese T3GTL author catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

AUTHOR_FORMAT = "T3GTL-AUTHOR-1"
FIXED_FORMAT = "T3GTF-AUTHOR-1"
SCHEMA_FORMAT = "T3GTL-SCHEMA-1"

PROFILES = {
    "op": (63866, "efd858aef69a240af3a27c747a5beae150f55f0b41b8afdd1eda1760a759ecc0"),
    "mainl": (67372, "92600c8858bb407516cac6a9d77538ebdf0c33522c4f6bd9194d569eb927134a"),
    "main": (130882, "f41fde47ea36bf4d985ff9127b67fe93d5ecb58cc36e7cffab86959db7f2ce6b"),
}

OP_TRACK_OFFSET = 0xDE84
OP_TRACK_COUNT = 19
OP_TRACK_BYTES = 32
OP_TRACK_STRIDE = 33

MAINL_STAGE_SPECS = (
    (0xE5C8, 28, 0xE5E5, 13), (0xE5F3, 28, 0xE610, 10),
    (0xE61B, 28, 0xE638, 15), (0xE648, 28, 0xE665, 10),
    (0xE670, 28, 0xE68D, 10), (0xE698, 28, 0xE6B5, 16),
    (0xE6C6, 28, 0xE6E3, 14), (0xE6F2, 28, 0xE70F, 15),
    (0xE71F, 28, 0xE73C, 13),
)
MAINL_REGISTRATION_OFFSET = 0xED72
MAINL_REGISTRATION_COUNT = 10
MAINL_REGISTRATION_BYTES = 12
MAINL_REGISTRATION_STRIDE = 13
MAIN_RESULT_SPECS = (
    ("result.gauge-attack", 0x1DE65, 26),
    ("result.boss-attack", 0x1DE80, 26),
    ("result.boss-flip", 0x1DE9B, 26),
    ("result.boss-panic", 0x1DEB6, 26),
    ("result.lives-left", 0x1DF2E, 26),
)
IMAGE_PRESENTATION_MESSAGES = {
    # These labels are already English in the original Japanese game. New
    # Classic preserves that source presentation in every locale for now.
    "hud.banner.lose": "Lose            ",
    "hud.banner.win": "Win             ",
    "hud.banner.stage": "STAGE           ",
    "hud.banner.round": "Round           ",
    "hud.banner.start": "START           ",
    "continue.question": "Continue?                       ",
    "continue.credit": "CREDIT                          ",
    "continue.yes": "Yes                             ",
    "continue.no": "No                              ",
    "gameover.title": "Game is Over                    ",
    "registration.title": "Registration of your name       ",
    "registration.header.name": "Name    ",
    "registration.header.point": "Point   ",
    "registration.header.character": "Character   ",
    "registration.header.stage": "Stage   ",
    "registration.rank.easy": "Easy            ",
    "registration.rank.normal": "Normal          ",
    "registration.rank.hard": "Hard            ",
    "registration.rank.lunatic": "Lunatic         ",
    "registration.key.space": "SP  ",
    "registration.key.backspace": "BS  ",
    "registration.key.end": "END ",
    "registration.stage.all": "All ",
    "staff.title.line-1": "東方夢時空                      ",
    "staff.title.line-2": "                                ",
    "staff.subtitle": "The Phantasmagoria of Dim. Dream...             ",
    "staff.role.line-1": "プログラム・グラフィック                ",
    "staff.role.line-2": "音楽                                    ",
    "staff.creator": "ZUN（太田 順也）                ",
    "staff.thanks.heading": "謝辞                            ",
    "staff.thanks.aotaka": "AOTAKA          ",
    "staff.thanks.amusement-makers": "東京電機大学 Amusement Makers                                   ",
    "staff.thanks.pmd": "P.M.D 提供 M.Kajihara (KAJA)                                    ",
    "staff.copyright": "C53 version   (c) ZUN Soft 1997                 ",
    "staff.verdict.title": "今回の記録              ",
    "staff.verdict.character": "キャラクター    ",
    "staff.verdict.rank": "難易度          ",
    "staff.verdict.score": "得点            ",
    "staff.verdict.continue": "コンティニュー  ",
    "staff.verdict.skill": "あなたの腕前    ",
    "staff.verdict.point": "点  ",
}


class AuthorError(ValueError):
    pass


def read_profile(path: Path, role: str) -> tuple[bytes, int]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise AuthorError(f"{path}: {error}") from error
    expected_size, expected_sha = PROFILES[role]
    if len(data) != expected_size or hashlib.sha256(data).hexdigest() != expected_sha:
        raise AuthorError(f"{path}: unsupported {role.upper()} profile")
    if len(data) < 28 or data[:2] != b"MZ":
        raise AuthorError(f"{path}: invalid MZ header")
    header = struct.unpack_from("<H", data, 8)[0] * 16
    if header < 28 or header >= len(data):
        raise AuthorError(f"{path}: invalid MZ header size")
    return data, header


def decode_exact(data: bytes, offset: int, size: int, location: str) -> str:
    payload = data[offset : offset + size]
    if len(payload) != size:
        raise AuthorError(f"{location}: source range is truncated")
    try:
        return payload.decode("cp932", errors="strict")
    except UnicodeDecodeError as error:
        raise AuthorError(f"{location}: invalid CP932") from error


def pad_cp932(text: str, exact_bytes: int, location: str) -> str:
    encoded = text.encode("cp932")
    if len(encoded) > exact_bytes:
        raise AuthorError(f"{location}: source text exceeds the destination field")
    return (encoded + (b" " * (exact_bytes - len(encoded)))).decode("cp932")


def extract_executable_messages(
    op: tuple[bytes, int], mainl: tuple[bytes, int], main: tuple[bytes, int]
) -> dict[str, str]:
    op_bytes, op_header = op
    mainl_bytes, mainl_header = mainl
    main_bytes, main_header = main
    messages: dict[str, str] = {}
    for index in range(OP_TRACK_COUNT):
        identifier = f"music.track.{index + 1:02d}"
        messages[identifier] = decode_exact(
            op_bytes,
            op_header + OP_TRACK_OFFSET + index * OP_TRACK_STRIDE,
            OP_TRACK_BYTES,
            identifier,
        )
    for index, (title_offset, title_bytes, name_offset, name_bytes) in enumerate(
        MAINL_STAGE_SPECS
    ):
        title_id = f"stage.title-first.{index:02d}"
        name_id = f"stage.name.{index:02d}"
        messages[title_id] = decode_exact(
            mainl_bytes, mainl_header + title_offset, title_bytes, title_id
        )
        messages[f"stage.title-second.{index:02d}"] = " " * 28
        messages[name_id] = pad_cp932(
            decode_exact(
                mainl_bytes, mainl_header + name_offset, name_bytes, name_id
            ),
            28,
            name_id,
        )
    for index in range(MAINL_REGISTRATION_COUNT):
        identifier = f"registration.character.{index:02d}"
        messages[identifier] = decode_exact(
            mainl_bytes,
            mainl_header + MAINL_REGISTRATION_OFFSET +
            index * MAINL_REGISTRATION_STRIDE,
            MAINL_REGISTRATION_BYTES,
            identifier,
        )
    for identifier, offset, size in MAIN_RESULT_SPECS:
        messages[identifier] = decode_exact(
            main_bytes, main_header + offset, size, identifier
        )
    return messages


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise AuthorError(f"{path}: {error}") from error


def schema_ids(path: Path) -> tuple[str, ...]:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != SCHEMA_FORMAT:
        raise AuthorError("unsupported T3GTL schema")
    result: list[str] = []
    for group in raw.get("groups", []):
        for offset in range(group["count"]):
            result.append(
                group["id_prefix"] +
                f"{group['first'] + offset:0{group['digits']}d}"
            )
    result.extend(entry["id"] for entry in raw.get("messages", []))
    return tuple(result)


def build_author(
    schema: Path, fixed: Path, op: Path, mainl: Path, main: Path
) -> dict[str, object]:
    fixed_raw = load_json(fixed)
    if not isinstance(fixed_raw, dict) or fixed_raw.get("format") != FIXED_FORMAT or \
            fixed_raw.get("locale") != "ja-JP" or not isinstance(
                fixed_raw.get("messages"), dict
            ):
        raise AuthorError("fixed Japanese author has the wrong format")
    messages = extract_executable_messages(
        read_profile(op, "op"), read_profile(mainl, "mainl"),
        read_profile(main, "main")
    )
    overlap = set(messages) & set(fixed_raw["messages"])
    if overlap:
        raise AuthorError(f"duplicate source message {sorted(overlap)[0]}")
    messages.update(fixed_raw["messages"])
    overlap = set(messages) & set(IMAGE_PRESENTATION_MESSAGES)
    if overlap:
        raise AuthorError(f"duplicate localized HUD message {sorted(overlap)[0]}")
    messages.update(IMAGE_PRESENTATION_MESSAGES)
    identifiers = schema_ids(schema)
    missing = set(identifiers) - set(messages)
    extra = set(messages) - set(identifiers)
    if missing or extra:
        raise AuthorError(
            f"catalog mismatch: missing={sorted(missing)[:1]} extra={sorted(extra)[:1]}"
        )
    return {
        "format": AUTHOR_FORMAT,
        "locale": "ja-JP",
        "fallback": "",
        "messages": {identifier: messages[identifier] for identifier in identifiers},
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", type=Path, required=True)
    parser.add_argument("--fixed", type=Path, required=True)
    parser.add_argument("--op", type=Path, required=True)
    parser.add_argument("--mainl", type=Path, required=True)
    parser.add_argument("--main", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        author = build_author(
            arguments.schema, arguments.fixed, arguments.op,
            arguments.mainl, arguments.main
        )
        output = (
            json.dumps(author, ensure_ascii=False, indent=2) + "\n"
        ).encode("utf-8")
        if not arguments.output.exists() or arguments.output.read_bytes() != output:
            arguments.output.parent.mkdir(parents=True, exist_ok=True)
            arguments.output.write_bytes(output)
    except AuthorError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
