#!/usr/bin/env python3
"""Compile complete T3CTR author catalogs into deterministic T3CTP001 packs."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"T3CTP001"
HEADER_SIZE = 128
SCRIPT_ENTRY_SIZE = 24
RUN_ENTRY_SIZE = 24
SCRIPT_COUNT = 28
AUTHOR_FORMAT = "T3CTR-AUTHOR-1"

MODULE_PATH = Path(__file__).with_name("make_cutscene_text_author.py")
SPEC = importlib.util.spec_from_file_location("cutscene_text_author", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
AUTHOR_MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUTHOR_MODULE
SPEC.loader.exec_module(AUTHOR_MODULE)


class PackError(ValueError):
    pass


@dataclass(frozen=True)
class Run:
    source_offset: int
    glyphs: int
    source_fnv1a64: int
    replacement: bytes


@dataclass(frozen=True)
class Script:
    source_bytes: int
    source_fnv1a64: int
    runs: tuple[Run, ...]


@dataclass(frozen=True)
class Catalog:
    locale: str
    source_profile_fnv1a64: int
    scripts: tuple[Script, ...]


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PackError(f"{path}: {error}") from error


def parse_hex64(value: object, field: str) -> int:
    if not isinstance(value, str) or not AUTHOR_MODULE.HEX64_RE.fullmatch(value):
        raise PackError(f"invalid {field}")
    return int(value, 16)


def encode_glyph_pairs(identifier: str, text: object, glyphs: int) -> bytes:
    if not isinstance(text, str) or any(character in text for character in "\0\r\n"):
        raise PackError(f"{identifier}: invalid text")
    try:
        encoded = text.encode("cp932")
    except UnicodeEncodeError as error:
        raise PackError(f"{identifier}: text is not representable in CP932") from error
    if len(encoded) != glyphs * 2:
        raise PackError(
            f"{identifier}: expected {glyphs * 2} CP932 bytes, got {len(encoded)}"
        )
    def single(value: int) -> bool:
        return (0x20 <= value <= 0x7E) or (0xA1 <= value <= 0xDF)

    for offset in range(0, len(encoded), 2):
        first, second = encoded[offset:offset + 2]
        double = (
            ((0x81 <= first <= 0x9F) or (0xE0 <= first <= 0xFC))
            and ((0x40 <= second <= 0x7E) or (0x80 <= second <= 0xFC))
        )
        if not double and not (single(first) and single(second)):
            raise PackError(
                f"{identifier}: glyph {offset // 2} is not one valid "
                "16-pixel CP932 VM cell"
            )
    return encoded


def load_catalog(inventory_path: Path, author_path: Path) -> Catalog:
    try:
        inventory = AUTHOR_MODULE.load_inventory(inventory_path)
        # make_author() performs the complete per-script/run shape validation.
        AUTHOR_MODULE.make_author(inventory, "zxx", "")
    except AUTHOR_MODULE.AuthorError as error:
        raise PackError(str(error)) from error
    author = load_json(author_path)
    if not isinstance(author, dict) or set(author) != {
        "format", "source_profile", "source_profile_fnv1a64", "locale",
        "fallback", "messages"
    } or author.get("format") != AUTHOR_FORMAT:
        raise PackError("author fields do not match T3CTR-AUTHOR-1")
    if author.get("source_profile") != inventory["source_profile"] or (
        author.get("source_profile_fnv1a64") != inventory["source_profile_fnv1a64"]
    ):
        raise PackError("author source profile does not match inventory")
    locale = author.get("locale")
    fallback = author.get("fallback")
    messages = author.get("messages")
    if not isinstance(locale, str) or not isinstance(fallback, str):
        raise PackError("author locale/fallback must be strings")
    try:
        AUTHOR_MODULE.validate_locale(locale, "locale")
    except AUTHOR_MODULE.AuthorError as error:
        raise PackError(str(error)) from error
    if fallback:
        raise PackError("T3CTP001 requires a complete catalog without fallback")
    if not isinstance(messages, dict):
        raise PackError("author messages must be an object")

    known: set[str] = set()
    scripts: list[Script] = []
    for script in inventory["scripts"]:
        runs: list[Run] = []
        for run in script["runs"]:
            identifier = run["id"]
            known.add(identifier)
            message = messages.get(identifier)
            if not isinstance(message, dict) or set(message) != {
                "exact_glyphs", "text"
            } or message.get("exact_glyphs") != run["glyphs"]:
                raise PackError(f"missing or mismatched message {identifier}")
            runs.append(Run(
                source_offset=run["source_offset"],
                glyphs=run["glyphs"],
                source_fnv1a64=parse_hex64(run["source_fnv1a64"], identifier),
                replacement=encode_glyph_pairs(
                    identifier, message.get("text"), run["glyphs"]
                ),
            ))
        scripts.append(Script(
            source_bytes=script["source_bytes"],
            source_fnv1a64=parse_hex64(
                script["source_fnv1a64"], f"{script['id']} source digest"
            ),
            runs=tuple(runs),
        ))
    unknown = sorted(set(messages) - known)
    if unknown:
        raise PackError(f"unknown message {unknown[0]}")
    if len(messages) != len(known):
        missing = next(identifier for identifier in known if identifier not in messages)
        raise PackError(f"missing message {missing}")
    return Catalog(
        locale=locale,
        source_profile_fnv1a64=parse_hex64(
            inventory["source_profile_fnv1a64"], "source profile digest"
        ),
        scripts=tuple(scripts),
    )


def compile_pack(catalog: Catalog) -> bytes:
    if len(catalog.scripts) != SCRIPT_COUNT:
        raise PackError("catalog must contain all 28 scripts")
    locale = catalog.locale.encode("ascii")
    scripts = bytearray()
    runs = bytearray()
    payload = bytearray()
    first_run = 0
    for script in catalog.scripts:
        scripts.extend(struct.pack(
            "<IIIIQ", script.source_bytes, len(script.runs), first_run, 0,
            script.source_fnv1a64,
        ))
        for run in script.runs:
            runs.extend(struct.pack(
                "<IHHQIHH", run.source_offset, run.glyphs, 0,
                run.source_fnv1a64, len(payload), len(run.replacement), 0,
            ))
            payload.extend(run.replacement)
        first_run += len(script.runs)
    body = bytes(locale + scripts + runs + payload)
    header = bytearray(HEADER_SIZE)
    header[:8] = MAGIC
    struct.pack_into(
        "<HHHBB4xIIIIIQ", header, 8, 1, HEADER_SIZE, SCRIPT_COUNT, 1,
        len(locale), first_run, len(scripts), len(runs), len(payload), 0,
        catalog.source_profile_fnv1a64,
    )
    header[48:80] = hashlib.sha256(body).digest()
    return bytes(header) + body


def write_if_changed(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        write_if_changed(
            arguments.output,
            compile_pack(load_catalog(arguments.inventory, arguments.source)),
        )
    except PackError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
