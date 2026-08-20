#!/usr/bin/env python3
"""Compile deterministic New Classic T3GTL001 in-game text packs."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

MAGIC = b"T3GTL001"
HEADER_SIZE = 96
ENTRY_SIZE = 8
SCHEMA_FORMAT = "T3GTL-SCHEMA-1"
AUTHOR_FORMAT = "T3GTL-AUTHOR-1"
LOCALE_RE = re.compile(r"^[A-Za-z0-9]{2,8}(?:-[A-Za-z0-9]{1,8})*$")
ENUM_RE = re.compile(r"^[a-z][a-z0-9_]*$")
ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$")


class PackError(ValueError):
    pass


@dataclass(frozen=True)
class Message:
    enum: str
    id: str
    exact_bytes: int


@dataclass(frozen=True)
class Schema:
    messages: tuple[Message, ...]
    canonical: bytes
    digest: bytes


@dataclass(frozen=True)
class Author:
    locale: str
    fallback: str
    messages: dict[str, str]


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PackError(f"{path}: {error}") from error


def canonical_json(value: object) -> bytes:
    return (json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ) + "\n").encode("utf-8")


def parse_message(entry: object, location: str) -> Message:
    if not isinstance(entry, dict) or set(entry) != {
        "enum", "id", "encoding", "exact_bytes"
    }:
        raise PackError(f"invalid schema message {location}")
    enum = entry.get("enum")
    identifier = entry.get("id")
    encoding = entry.get("encoding")
    exact_bytes = entry.get("exact_bytes")
    if not isinstance(enum, str) or not ENUM_RE.fullmatch(enum):
        raise PackError(f"invalid enum at {location}")
    if not isinstance(identifier, str) or not ID_RE.fullmatch(identifier):
        raise PackError(f"invalid id at {location}")
    if encoding != "cp932":
        raise PackError(f"unsupported encoding for {identifier}")
    if not isinstance(exact_bytes, int) or not 1 <= exact_bytes <= 512:
        raise PackError(f"invalid exact byte length for {identifier}")
    return Message(enum, identifier, exact_bytes)


def load_schema(path: Path) -> Schema:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != SCHEMA_FORMAT:
        raise PackError("unsupported schema format")
    if set(raw) != {"format", "groups", "messages"}:
        raise PackError("schema fields do not match T3GTL-SCHEMA-1")
    groups = raw.get("groups")
    entries = raw.get("messages")
    if not isinstance(groups, list) or not isinstance(entries, list):
        raise PackError("schema groups/messages must be arrays")
    messages: list[Message] = []
    for group_index, group in enumerate(groups):
        if not isinstance(group, dict) or set(group) != {
            "enum_prefix", "id_prefix", "first", "count", "digits",
            "encoding", "exact_bytes"
        }:
            raise PackError(f"invalid schema group {group_index}")
        prefix = group.get("enum_prefix")
        id_prefix = group.get("id_prefix")
        first = group.get("first")
        count = group.get("count")
        digits = group.get("digits")
        encoding = group.get("encoding")
        exact_bytes = group.get("exact_bytes")
        if (not isinstance(prefix, str) or not ENUM_RE.fullmatch(prefix + "x") or
                not isinstance(id_prefix, str) or not ID_RE.fullmatch(id_prefix + "x") or
                not isinstance(first, int) or first < 0 or
                not isinstance(count, int) or not 1 <= count <= 256 or
                not isinstance(digits, int) or not 1 <= digits <= 3):
            raise PackError(f"invalid schema group {group_index}")
        for offset in range(count):
            number = f"{first + offset:0{digits}d}"
            messages.append(parse_message({
                "enum": prefix + number,
                "id": id_prefix + number,
                "encoding": encoding,
                "exact_bytes": exact_bytes,
            }, f"group {group_index}/{number}"))
    messages.extend(parse_message(entry, str(index)) for index, entry in enumerate(entries))
    if not messages:
        raise PackError("schema must contain at least one message")
    if len({message.enum for message in messages}) != len(messages):
        raise PackError("duplicate schema enum")
    if len({message.id for message in messages}) != len(messages):
        raise PackError("duplicate schema id")
    canonical = canonical_json(raw)
    return Schema(tuple(messages), canonical, hashlib.sha256(canonical).digest())


def validate_locale(locale: str, field: str, allow_empty: bool = False) -> None:
    if allow_empty and not locale:
        return
    if not LOCALE_RE.fullmatch(locale) or len(locale.encode("ascii")) > 31:
        raise PackError(f"invalid {field}: {locale!r}")


def validate_message(message: Message, text: str) -> bytes:
    if "\0" in text or "\r" in text or "\n" in text:
        raise PackError(f"{message.id}: control characters are not allowed")
    try:
        encoded = text.encode("cp932")
    except UnicodeEncodeError as error:
        raise PackError(f"{message.id}: text is not representable in CP932") from error
    if len(encoded) != message.exact_bytes:
        raise PackError(
            f"{message.id}: expected {message.exact_bytes} CP932 bytes, got {len(encoded)}"
        )
    return encoded


def load_author(path: Path, schema: Schema) -> Author:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != AUTHOR_FORMAT:
        raise PackError(f"{path}: unsupported authoring format")
    if set(raw) != {"format", "locale", "fallback", "messages"}:
        raise PackError(f"{path}: author fields do not match T3GTL-AUTHOR-1")
    locale = raw.get("locale")
    fallback = raw.get("fallback")
    authored = raw.get("messages")
    if not isinstance(locale, str) or not isinstance(fallback, str):
        raise PackError(f"{path}: locale/fallback must be strings")
    validate_locale(locale, "locale")
    validate_locale(fallback, "fallback", allow_empty=True)
    if fallback == locale:
        raise PackError(f"{path}: locale cannot fall back to itself")
    if not isinstance(authored, dict) or any(
        not isinstance(key, str) or not isinstance(value, str)
        for key, value in authored.items()
    ):
        raise PackError(f"{path}: messages must map strings to strings")
    known = {message.id for message in schema.messages}
    unknown = sorted(set(authored) - known)
    if unknown:
        raise PackError(f"{path}: unknown message {unknown[0]}")
    if not fallback:
        missing = [message.id for message in schema.messages if message.id not in authored]
        if missing:
            raise PackError(f"{path}: missing required message {missing[0]}")
    for message in schema.messages:
        if message.id in authored:
            validate_message(message, authored[message.id])
    return Author(locale, fallback, dict(authored))


def resolve_author(
    author: Author,
    authors: dict[str, Author],
    schema: Schema,
    chain: tuple[str, ...] = (),
) -> tuple[str, ...]:
    if author.locale in chain:
        raise PackError(f"fallback cycle at {author.locale}")
    fallback_values: tuple[str, ...] | None = None
    if author.fallback:
        fallback = authors.get(author.fallback)
        if fallback is None:
            raise PackError(f"missing fallback locale {author.fallback}")
        fallback_values = resolve_author(fallback, authors, schema, chain + (author.locale,))
    values: list[str] = []
    for index, message in enumerate(schema.messages):
        value = author.messages.get(message.id)
        if value is None:
            if fallback_values is None:
                raise PackError(f"missing required message {message.id}")
            value = fallback_values[index]
        validate_message(message, value)
        values.append(value)
    return tuple(values)


def compile_pack(schema: Schema, author: Author) -> bytes:
    locale = author.locale.encode("ascii")
    fallback = author.fallback.encode("ascii")
    payload = bytearray()
    directory = bytearray()
    for message in schema.messages:
        text = author.messages.get(message.id)
        if text is None:
            directory.extend(struct.pack("<IHH", 0, 0, 0))
            continue
        encoded = validate_message(message, text)
        directory.extend(struct.pack("<IHH", len(payload), len(encoded), 0))
        payload.extend(encoded)
    body = bytes(locale + fallback + directory + payload)
    header = bytearray(HEADER_SIZE)
    header[0:8] = MAGIC
    struct.pack_into("<HHHBBBBxxII", header, 8,
                     1, HEADER_SIZE, len(schema.messages), 1,
                     len(locale), len(fallback), 0,
                     len(directory), len(payload))
    header[32:64] = schema.digest
    header[64:96] = hashlib.sha256(body).digest()
    return bytes(header) + body


def cpp_byte_array(values: bytes) -> str:
    return ", ".join(f"0x{value:02X}U" for value in values)


def cpp_bytes(values: bytes) -> str:
    return '"' + "".join(f"\\x{value:02X}" for value in values) + '"'


def generate_cpp(
    schema: Schema,
    authors: Iterable[Author],
    ids_output: Path,
    data_output: Path,
) -> None:
    author_map = {author.locale: author for author in authors}
    if set(author_map) != {"en-US"}:
        raise PackError("C++ generation currently requires exactly en-US")
    ids = "".join(
        f'TH03_NEW_CLASSIC_GAME_TEXT({message.enum}, "{message.id}", '
        f'{message.exact_bytes}U)\n'
        for message in schema.messages
    )
    values = resolve_author(author_map["en-US"], author_map, schema)
    lines = [
        "// Generated by tools/localization/compile_game_text_pack.py.\n",
        f"constexpr std::array<std::uint8_t, 32U> game_text_schema_sha256{{{{{cpp_byte_array(schema.digest)}}}}};\n",
        "constexpr std::array<std::string_view, game_text_count> builtin_english_game_text{\n",
    ]
    for message, value in zip(schema.messages, values, strict=True):
        lines.append(
            f"\tstd::string_view{{{cpp_bytes(validate_message(message, value))}, "
            f"{message.exact_bytes}U}},\n"
        )
    lines.append("};\n")
    ids_output.write_text(ids, encoding="utf-8", newline="\n")
    data_output.write_text("".join(lines), encoding="utf-8", newline="\n")


def write_if_changed(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    compile_parser = subparsers.add_parser("compile")
    compile_parser.add_argument("--schema", type=Path, required=True)
    compile_parser.add_argument("--source", type=Path, required=True)
    compile_parser.add_argument("--output", type=Path, required=True)
    generate_parser = subparsers.add_parser("generate-cpp")
    generate_parser.add_argument("--schema", type=Path, required=True)
    generate_parser.add_argument("--source", type=Path, action="append", required=True)
    generate_parser.add_argument("--ids-output", type=Path, required=True)
    generate_parser.add_argument("--data-output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        schema = load_schema(arguments.schema)
        if arguments.command == "compile":
            author = load_author(arguments.source, schema)
            write_if_changed(arguments.output, compile_pack(schema, author))
        else:
            authors = [load_author(path, schema) for path in arguments.source]
            generate_cpp(schema, authors, arguments.ids_output, arguments.data_output)
    except PackError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
