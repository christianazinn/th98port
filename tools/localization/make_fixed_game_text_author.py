#!/usr/bin/env python3
"""Extract TH03 fixed-record win and Music Room text for localization."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

FIXED_AUTHOR_FORMAT = "T3GTF-AUTHOR-1"
GAME_AUTHOR_FORMAT = "T3GTL-AUTHOR-1"
WIN_FILES = tuple(f"@{index:02d}TX.TXT" for index in range(9))
MUSIC_FILE = "MUSIC.TXT"
WIN_FILE_BYTES = 11 * 3 * 60
MUSIC_FILE_BYTES = 19 * 20 * 42


class AuthorError(ValueError):
    pass


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def expand_rle(data: bytes, expected_size: int) -> bytes:
    output = bytearray()
    previous = -1
    cursor = 0
    while cursor < len(data):
        value = data[cursor]
        cursor += 1
        output.append(value)
        if value == previous:
            if cursor >= len(data):
                raise AuthorError("compressed member has no RLE count byte")
            output.extend((value,) * data[cursor])
            cursor += 1
        previous = value
        if len(output) > expected_size:
            raise AuthorError("compressed member expands past its declared size")
    if len(output) != expected_size:
        raise AuthorError("compressed member does not match its declared size")
    return bytes(output)


def read_archive(path: Path) -> dict[str, bytes]:
    try:
        archive = path.read_bytes()
    except OSError as error:
        raise AuthorError(f"{path}: {error}") from error
    if len(archive) < 16:
        raise AuthorError("archive header is truncated")
    directory_size, _reserved, entry_count, directory_key = struct.unpack_from(
        "<HHHH", archive, 0
    )
    if (
        directory_size == 0
        or directory_size % 32
        or entry_count > directory_size // 32
        or 16 + directory_size > len(archive)
    ):
        raise AuthorError("archive directory layout is invalid")
    directory = bytearray(directory_size)
    key = directory_key & 0xFF
    for index, value in enumerate(archive[16 : 16 + directory_size]):
        decoded = value ^ key
        directory[index] = decoded
        key = (key - decoded) & 0xFF
    result: dict[str, bytes] = {}
    for index in range(entry_count):
        offset = index * 32
        raw_name = bytes(directory[offset + 3 : offset + 16]).split(b"\0", 1)[0]
        try:
            name = raw_name.decode("ascii").upper()
        except UnicodeDecodeError as error:
            raise AuthorError("archive member name is not ASCII") from error
        member_type, auxiliary = struct.unpack_from("<HB", directory, offset)
        packed_size, original_size, member_offset = struct.unpack_from(
            "<HHI", directory, offset + 16
        )
        if member_type not in (0x9595, 0xF388) or auxiliary != 0:
            raise AuthorError(f"{name} uses an unsupported member type")
        if member_offset + packed_size > len(archive):
            raise AuthorError(f"{name} extends past the archive")
        if name in result:
            raise AuthorError(f"archive contains duplicate member {name}")
        packed = archive[member_offset : member_offset + packed_size]
        if member_type == 0xF388:
            if packed_size != original_size:
                raise AuthorError(f"raw member {name} has inconsistent sizes")
            result[name] = packed
        else:
            result[name] = expand_rle(packed, original_size)
    return result


def read_root(path: Path) -> dict[str, bytes]:
    if not path.is_dir():
        raise AuthorError(f"not a fixed-text directory: {path}")
    available = {entry.name.upper(): entry for entry in path.iterdir() if entry.is_file()}
    result: dict[str, bytes] = {}
    for name in (*WIN_FILES, MUSIC_FILE):
        source = available.get(name)
        if source is None:
            raise AuthorError(f"missing fixed-text member {name}")
        try:
            result[name] = source.read_bytes()
        except OSError as error:
            raise AuthorError(f"{source}: {error}") from error
    return result


def fnv1a64(members: dict[str, bytes]) -> int:
    value = 14695981039346656037
    for name in (*WIN_FILES, MUSIC_FILE):
        for byte in members[name]:
            value ^= byte
            value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def decode_row(data: bytes, expected: int, location: str) -> str:
    if len(data) != expected:
        raise AuthorError(f"{location}: fixed row geometry is invalid")
    try:
        return data.decode("cp932", errors="strict")
    except UnicodeDecodeError as error:
        raise AuthorError(f"{location}: invalid CP932") from error


def fixed_rows(data: bytes, count: int, text_bytes: int, location: str) -> tuple[bytes, ...]:
    ordinary_size = count * (text_bytes + 2)
    if len(data) not in (ordinary_size, ordinary_size - 2):
        raise AuthorError(f"{location}: fixed table size is invalid")
    rows: list[bytes] = []
    cursor = 0
    for row in range(count):
        end = cursor + text_bytes
        if end > len(data):
            raise AuthorError(f"{location}/{row}: fixed row is truncated")
        rows.append(data[cursor:end])
        cursor = end
        if cursor == len(data) and row + 1 == count:
            break
        if cursor + 2 > len(data) or data[cursor : cursor + 2] != b"\r\n":
            raise AuthorError(f"{location}/{row}: CRLF separator is invalid")
        cursor += 2
    if cursor != len(data):
        raise AuthorError(f"{location}: trailing fixed-table bytes")
    return tuple(rows)


def extract_messages(members: dict[str, bytes]) -> dict[str, str]:
    messages: dict[str, str] = {}
    for character, name in enumerate(WIN_FILES):
        data = members.get(name)
        if data is None:
            raise AuthorError(f"missing fixed-text member {name}")
        for row, source in enumerate(fixed_rows(data, 33, 58, name)):
            identifier = f"win.quote.{character:02d}.{row:02d}"
            messages[identifier] = decode_row(source, 58, identifier)
    music = members.get(MUSIC_FILE)
    if music is None:
        raise AuthorError(f"missing fixed-text member {MUSIC_FILE}")
    for index, source in enumerate(fixed_rows(music, 19 * 20, 40, MUSIC_FILE)):
        track, row = divmod(index, 20)
        identifier = f"music.comment.{track + 1:02d}.{row:02d}"
        messages[identifier] = decode_row(source, 40, identifier)
    return messages


def load_base(path: Path, locale: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise AuthorError(f"{path}: {error}") from error
    if (
        not isinstance(value, dict)
        or value.get("format") != GAME_AUTHOR_FORMAT
        or value.get("locale") != locale
        or not isinstance(value.get("messages"), dict)
    ):
        raise AuthorError("base author has the wrong format or locale")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--root", type=Path)
    source.add_argument("--archive", type=Path)
    parser.add_argument("--locale", required=True)
    parser.add_argument("--base-author", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        members = read_root(arguments.root) if arguments.root else read_archive(arguments.archive)
        messages = extract_messages(members)
        if arguments.base_author:
            output = load_base(arguments.base_author, arguments.locale)
            authored = dict(output["messages"])
            overlap = set(authored) & set(messages)
            if overlap:
                raise AuthorError(f"base author already contains {sorted(overlap)[0]}")
            authored.update(messages)
            output["messages"] = authored
        else:
            output = {
                "format": FIXED_AUTHOR_FORMAT,
                "locale": arguments.locale,
                "source_profile_fnv1a64": f"{fnv1a64(members):016X}",
                "messages": messages,
            }
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        data = canonical_json(output)
        if not arguments.output.exists() or arguments.output.read_bytes() != data:
            arguments.output.write_bytes(data)
    except AuthorError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
