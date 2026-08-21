#!/usr/bin/env python3
"""Convert a VM-derived T3CTR001 inventory into a UTF-8 author scaffold."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

INVENTORY_FORMAT = "T3CTR001"
AUTHOR_FORMAT = "T3CTR-AUTHOR-1"
HEX64_RE = re.compile(r"^[0-9A-F]{16}$")
LOCALE_RE = re.compile(r"^[A-Za-z0-9]{2,8}(?:-[A-Za-z0-9]{1,8})*$")
RUN_ID_RE = re.compile(
    r"^cutscene\.[a-z0-9]+(?:[.-][a-z0-9]+)*\.run\.[0-9]{3}$"
)
SOURCE_PROFILE_RE = re.compile(r"^th03-(?:original|english-overlay)-28$")


class AuthorError(ValueError):
    pass


def decode_vm_cells(encoded: bytes) -> str:
    normalized = bytearray()
    for offset in range(0, len(encoded), 2):
        cell = encoded[offset:offset + 2]
        if len(cell) != 2:
            raise UnicodeDecodeError("cp932", encoded, offset, len(encoded), "odd VM cell")
        first, second = cell
        double = (
            ((0x81 <= first <= 0x9F) or (0xE0 <= first <= 0xFC))
            and ((0x40 <= second <= 0x7E) or (0x80 <= second <= 0xFC))
        )
        if double:
            normalized.extend(cell)
        else:
            normalized.extend(0x20 if value == 0xA0 else value for value in cell)
    return bytes(normalized).decode("cp932")


def load_inventory(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise AuthorError(f"{path}: {error}") from error
    if not isinstance(value, dict) or set(value) != {
        "format", "source_profile", "source_profile_fnv1a64", "scripts"
    }:
        raise AuthorError("inventory fields do not match T3CTR001")
    if value.get("format") != INVENTORY_FORMAT:
        raise AuthorError("unsupported inventory format")
    profile_name = value.get("source_profile")
    if not isinstance(profile_name, str) or not SOURCE_PROFILE_RE.fullmatch(profile_name):
        raise AuthorError("unsupported source profile")
    profile = value.get("source_profile_fnv1a64")
    if not isinstance(profile, str) or not HEX64_RE.fullmatch(profile):
        raise AuthorError("invalid source profile digest")
    scripts = value.get("scripts")
    if not isinstance(scripts, list) or len(scripts) != 28:
        raise AuthorError("inventory must contain all 28 scripts")
    return value


def validate_locale(locale: str, field: str, allow_empty: bool = False) -> None:
    if allow_empty and not locale:
        return
    if not LOCALE_RE.fullmatch(locale) or len(locale.encode("ascii")) > 31:
        raise AuthorError(f"invalid {field}: {locale!r}")


def make_author(
    inventory: dict[str, object], locale: str, fallback: str
) -> dict[str, object]:
    validate_locale(locale, "locale")
    validate_locale(fallback, "fallback", allow_empty=True)
    if fallback == locale:
        raise AuthorError("locale cannot fall back to itself")

    scripts = inventory.get("scripts")
    if not isinstance(scripts, list) or len(scripts) != 28:
        raise AuthorError("inventory must contain all 28 scripts")

    messages: dict[str, dict[str, object]] = {}
    script_ids: set[str] = set()
    for script_index, script in enumerate(scripts):
        if not isinstance(script, dict) or set(script) != {
            "id", "resource", "source_bytes", "source_fnv1a64", "runs"
        }:
            raise AuthorError(f"invalid script {script_index}")
        script_id = script.get("id")
        resource = script.get("resource")
        source_fnv = script.get("source_fnv1a64")
        source_bytes = script.get("source_bytes")
        runs = script.get("runs")
        if (not isinstance(script_id, str) or not script_id or
                script_id in script_ids or
                not isinstance(resource, str) or not resource or
                not isinstance(source_fnv, str) or not HEX64_RE.fullmatch(source_fnv) or
                not isinstance(source_bytes, int) or source_bytes <= 0 or
                not isinstance(runs, list) or not runs):
            raise AuthorError(f"invalid script identity {script_index}")
        script_ids.add(script_id)
        for ordinal, run in enumerate(runs):
            if not isinstance(run, dict) or set(run) != {
                "id", "ordinal", "source_offset", "glyphs", "first", "last",
                "source_fnv1a64", "cp932_hex"
            }:
                raise AuthorError(f"invalid run {script_id}/{ordinal}")
            identifier = run.get("id")
            glyphs = run.get("glyphs")
            encoded_hex = run.get("cp932_hex")
            first = run.get("first")
            last = run.get("last")
            if (not isinstance(identifier, str) or
                    not RUN_ID_RE.fullmatch(identifier) or
                    not identifier.startswith(f"cutscene.{script_id}.run.") or
                    identifier in messages or run.get("ordinal") != ordinal or
                    not isinstance(glyphs, int) or glyphs <= 0 or glyphs > 108 or
                    not isinstance(encoded_hex, str) or
                    len(encoded_hex) != glyphs * 4 or
                    not isinstance(run.get("source_offset"), int) or
                    not isinstance(run.get("source_fnv1a64"), str) or
                    not HEX64_RE.fullmatch(run["source_fnv1a64"]) or
                    not isinstance(first, list) or len(first) != 2 or
                    not isinstance(last, list) or len(last) != 2 or
                    any(not isinstance(value, int) for value in first + last)):
                raise AuthorError(f"invalid run contract {script_id}/{ordinal}")
            try:
                encoded = bytes.fromhex(encoded_hex)
                # The authenticated English overlay uses 0xA0 as an 8-pixel
                # blank. The renderer deliberately maps this otherwise
                # undefined single byte to its blank glyph; Python exposes it
                # as an unencodable private-use code point. Present translators
                # with an ordinary space, which has identical runtime width and
                # pixels and compiles back into the same two-byte VM cell shape.
                text = decode_vm_cells(encoded)
            except (ValueError, UnicodeDecodeError) as error:
                raise AuthorError(
                    f"invalid CP932 text {script_id}/{ordinal}: {error}"
                ) from error
            if "\0" in text or "\r" in text or "\n" in text:
                raise AuthorError(f"control character in {script_id}/{ordinal}")
            messages[identifier] = {
                "exact_glyphs": glyphs,
                "text": text,
            }

    return {
        "format": AUTHOR_FORMAT,
        "source_profile": inventory["source_profile"],
        "source_profile_fnv1a64": inventory["source_profile_fnv1a64"],
        "locale": locale,
        "fallback": fallback,
        "messages": messages,
    }


def write_author(path: Path, author: dict[str, object]) -> None:
    encoded = (json.dumps(
        author, ensure_ascii=False, indent=2, sort_keys=False
    ) + "\n").encode("utf-8")
    if path.exists() and path.read_bytes() == encoded:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--locale", required=True)
    parser.add_argument("--fallback", default="")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        inventory = load_inventory(arguments.inventory)
        author = make_author(inventory, arguments.locale, arguments.fallback)
        write_author(arguments.output, author)
    except AuthorError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
