#!/usr/bin/env python3
"""Compile deterministic New Classic T3LNG001 language packs."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

MAGIC = b"T3LNG001"
HEADER_SIZE = 96
ENTRY_SIZE = 8
SCHEMA_FORMAT = "T3LANG-SCHEMA-1"
AUTHOR_FORMAT = "T3LANG-AUTHOR-1"
LOCALE_RE = re.compile(r"^[A-Za-z0-9]{2,8}(?:-[A-Za-z0-9]{1,8})*$")
ENUM_RE = re.compile(r"^[a-z][a-z0-9_]*$")
ID_RE = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$")
PLACEHOLDER_RE = re.compile(r"\{([a-z][a-z0-9_]*)\}")


class PackError(ValueError):
    pass


@dataclass(frozen=True)
class Placeholder:
    name: str
    width_value: str


@dataclass(frozen=True)
class Message:
    enum: str
    id: str
    max_width: int
    placeholders: tuple[Placeholder, ...]


@dataclass(frozen=True)
class Schema:
    messages: tuple[Message, ...]
    canonical: bytes
    digest: bytes


@dataclass(frozen=True)
class Author:
    locale: str
    fallback: str
    asset_profile: int
    encoding: int
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


def load_schema(path: Path) -> Schema:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != SCHEMA_FORMAT:
        raise PackError("unsupported schema format")
    entries = raw.get("messages")
    if not isinstance(entries, list) or not entries:
        raise PackError("schema messages must be a non-empty array")
    messages: list[Message] = []
    enums: set[str] = set()
    identifiers: set[str] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            raise PackError(f"schema message {index} is not an object")
        enum = entry.get("enum")
        identifier = entry.get("id")
        domain = entry.get("domain")
        max_width = entry.get("max_width")
        placeholders = entry.get("placeholders", [])
        if not isinstance(enum, str) or not ENUM_RE.fullmatch(enum):
            raise PackError(f"invalid enum at schema message {index}")
        if not isinstance(identifier, str) or not ID_RE.fullmatch(identifier):
            raise PackError(f"invalid id at schema message {index}")
        if enum in enums or identifier in identifiers:
            raise PackError(f"duplicate schema message {identifier}")
        if domain != "menu-font":
            raise PackError(f"unsupported domain for {identifier}")
        if not isinstance(max_width, int) or not 1 <= max_width <= 640:
            raise PackError(f"invalid maximum width for {identifier}")
        if not isinstance(placeholders, list):
            raise PackError(f"invalid placeholders for {identifier}")
        parsed_placeholders: list[Placeholder] = []
        for placeholder in placeholders:
            if not isinstance(placeholder, dict) or set(placeholder) != {
                "name", "width_value"
            }:
                raise PackError(f"invalid placeholder for {identifier}")
            name = placeholder.get("name")
            width_value = placeholder.get("width_value")
            if (not isinstance(name, str) or not ENUM_RE.fullmatch(name) or
                    not isinstance(width_value, str) or not width_value or
                    "|" in width_value):
                raise PackError(f"invalid placeholder for {identifier}")
            try:
                encoded_width = width_value.encode("ascii")
            except UnicodeEncodeError as error:
                raise PackError(
                    f"invalid placeholder width value for {identifier}"
                ) from error
            if any(value < 0x20 or value > 0x7E for value in encoded_width):
                raise PackError(f"invalid placeholder for {identifier}")
            parsed_placeholders.append(Placeholder(name, width_value))
        parsed_placeholders.sort(key=lambda item: item.name)
        if len({item.name for item in parsed_placeholders}) != len(
            parsed_placeholders
        ):
            raise PackError(f"duplicate placeholder for {identifier}")
        enums.add(enum)
        identifiers.add(identifier)
        messages.append(Message(
            enum=enum,
            id=identifier,
            max_width=max_width,
            placeholders=tuple(parsed_placeholders),
        ))
    canonical = canonical_json(raw)
    return Schema(tuple(messages), canonical, hashlib.sha256(canonical).digest())


def validate_locale(locale: str, field: str, allow_empty: bool = False) -> None:
    if allow_empty and not locale:
        return
    if not LOCALE_RE.fullmatch(locale):
        raise PackError(f"invalid {field}: {locale!r}")
    if len(locale.encode("ascii")) > 31:
        raise PackError(f"{field} is too long")


def placeholders(text: str) -> tuple[str, ...]:
    found = tuple(sorted(PLACEHOLDER_RE.findall(text)))
    stripped = PLACEHOLDER_RE.sub("", text)
    if "{" in stripped or "}" in stripped or len(set(found)) != len(found):
        raise PackError(f"malformed or duplicate placeholder in {text!r}")
    return found


def unicode_noncharacter(value: int) -> bool:
    return 0xFDD0 <= value <= 0xFDEF or (
        value <= 0x10FFFF and value & 0xFFFF >= 0xFFFE
    )


def unicode_unsupported_format(value: int) -> bool:
    return value in (0x00AD, 0x061C, 0xFEFF) or (
        0x200B <= value <= 0x200F or 0x202A <= value <= 0x202E or
        0x2060 <= value <= 0x206F or 0xFFF9 <= value <= 0xFFFB or
        0xE0000 <= value <= 0xE007F
    )


def validate_message(message: Message, text: str, encoding: int) -> bytes:
    try:
        codec = ("ascii", "cp932", "utf-8")[encoding]
        encoded = text.encode(codec)
    except UnicodeEncodeError as error:
        raise PackError(
            f"{message.id}: text is not representable by its selected font encoding"
        ) from error
    if not encoded:
        raise PackError(f"{message.id}: empty menu-font text")
    if encoding == 2:
        if any(
            ord(character) < 0x20 or ord(character) == 0x7F or
            0x80 <= ord(character) <= 0x9F or
            unicode_noncharacter(ord(character)) or
            unicode_unsupported_format(ord(character))
            for character in text
        ):
            raise PackError(f"{message.id}: Unicode text contains a control")
        if placeholders(text) != tuple(item.name for item in message.placeholders):
            raise PackError(f"{message.id}: placeholder set does not match schema")
        if len(encoded) > 0xFFFF:
            raise PackError(f"{message.id}: text is too long")
        return encoded
    offset = 0
    while offset < len(encoded):
        value = encoded[offset]
        if 0x20 <= value <= 0x7E:
            offset += 1
            continue
        if encoding == 1 and 0xA1 <= value <= 0xDF:
            offset += 1
            continue
        lead = encoding == 1 and (0x81 <= value <= 0x9F or 0xE0 <= value <= 0xFC)
        if not lead or offset + 1 >= len(encoded):
            raise PackError(f"{message.id}: invalid CP932 glyph")
        trail = encoded[offset + 1]
        if not (0x40 <= trail <= 0x7E or 0x80 <= trail <= 0xFC):
            raise PackError(f"{message.id}: invalid CP932 glyph")
        offset += 2
    if placeholders(text) != tuple(item.name for item in message.placeholders):
        raise PackError(f"{message.id}: placeholder set does not match schema")
    if len(encoded) > 0xFFFF:
        raise PackError(f"{message.id}: text is too long")
    return encoded


def load_author(path: Path, schema: Schema) -> Author:
    raw = load_json(path)
    if not isinstance(raw, dict) or raw.get("format") != AUTHOR_FORMAT:
        raise PackError(f"{path}: unsupported authoring format")
    locale = raw.get("locale")
    fallback = raw.get("fallback")
    profile = raw.get("asset_profile")
    authored = raw.get("messages")
    if not isinstance(locale, str) or not isinstance(fallback, str):
        raise PackError(f"{path}: locale/fallback must be strings")
    validate_locale(locale, "locale")
    validate_locale(fallback, "fallback", allow_empty=True)
    if fallback == locale:
        raise PackError(f"{path}: locale cannot fall back to itself")
    if profile not in ("japanese", "english"):
        raise PackError(f"{path}: unsupported asset profile")
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
    requested_encoding = raw.get("encoding")
    if requested_encoding is None:
        encoding = 1 if profile == "japanese" else 0
    elif requested_encoding == "utf-8" and profile == "english":
        encoding = 2
    elif requested_encoding == "cp932" and profile == "japanese":
        encoding = 1
    elif requested_encoding == "ascii" and profile == "english":
        encoding = 0
    else:
        raise PackError(f"{path}: encoding does not match its asset profile")
    for message in schema.messages:
        if message.id in authored:
            validate_message(message, authored[message.id], encoding)
    return Author(
        locale=locale,
        fallback=fallback,
        asset_profile=0 if profile == "japanese" else 1,
        encoding=encoding,
        messages=dict(authored),
    )


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
        fallback_values = resolve_author(
            fallback, authors, schema, chain + (author.locale,)
        )
    values: list[str] = []
    for index, message in enumerate(schema.messages):
        value = author.messages.get(message.id)
        if value is None:
            if fallback_values is None:
                raise PackError(f"missing required message {message.id}")
            value = fallback_values[index]
        validate_message(message, value, author.encoding)
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
        encoded = validate_message(message, text, author.encoding)
        directory.extend(struct.pack("<IHH", len(payload), len(encoded), 0))
        payload.extend(encoded)
    body = bytes(locale + fallback + directory + payload)
    header = bytearray(HEADER_SIZE)
    header[0:8] = MAGIC
    struct.pack_into("<HHHBBBBxxII", header, 8,
                     1, HEADER_SIZE, len(schema.messages), author.asset_profile,
                     len(locale), len(fallback), author.encoding,
                     len(directory), len(payload))
    header[32:64] = schema.digest
    header[64:96] = hashlib.sha256(body).digest()
    return bytes(header) + body


def cpp_byte_array(values: bytes) -> str:
    return ", ".join(f"0x{value:02X}U" for value in values)


def cpp_string_view(value: str, encoding: int) -> str:
    encoded = value.encode(("ascii", "cp932", "utf-8")[encoding])
    escaped = "".join(f"\\x{byte:02X}" for byte in encoded)
    return f'std::string_view{{"{escaped}", {len(encoded)}U}}'


def generate_cpp(
    schema: Schema,
    authors: Iterable[Author],
    ids_output: Path,
    data_output: Path,
) -> None:
    author_map = {author.locale: author for author in authors}
    if set(author_map) != {"en-US", "ja-JP", "es-TEST", "qps-ploc"}:
        raise PackError(
            "C++ generation requires en-US, ja-JP, es-TEST, and qps-ploc"
        )
    ids = "".join(
        f'TH03_NEW_CLASSIC_MESSAGE({message.enum}, "{message.id}", '
        f'{message.max_width}U, '
        f'"{",".join(item.name for item in message.placeholders)}", '
        f'"{"|".join(item.width_value for item in message.placeholders)}")\n'
        for message in schema.messages
    )
    resolved = {
        locale: (
            resolve_author(author_map[locale], author_map, schema),
            author_map[locale].encoding,
        )
        for locale in ("en-US", "ja-JP", "es-TEST", "qps-ploc")
    }
    lines = [
        "// Generated by tools/localization/compile_language_pack.py.\n",
        f"constexpr std::array<std::uint8_t, 32U> message_schema_sha256{{{{{cpp_byte_array(schema.digest)}}}}};\n",
    ]
    for locale, symbol in (("en-US", "builtin_english"),
                           ("ja-JP", "builtin_japanese"),
                           ("es-TEST", "builtin_spanish_test"),
                           ("qps-ploc", "builtin_width_test")):
        lines.append(
            f"constexpr std::array<std::string_view, message_count> {symbol}{{{{\n"
        )
        values, encoding = resolved[locale]
        for value in values:
            lines.append(f"\t{cpp_string_view(value, encoding)},\n")
        lines.append("}};\n")
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
