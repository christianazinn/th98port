#!/usr/bin/env python3
"""Compile transparent indexed-PNG overlays into deterministic T3PTP001 packs."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"T3PTP001"
AUTHOR_FORMAT = "T3PTP-AUTHOR-1"
HEADER_SIZE = 96
PICTURE_ENTRY_SIZE = 80
RUN_ENTRY_SIZE = 12
WIDTH = 640
HEIGHT = 400


class PackError(ValueError):
    pass


@dataclass(frozen=True)
class DecodedSource:
    palette: tuple[tuple[int, int, int], ...]
    indices: bytes
    sha256: bytes


@dataclass(frozen=True)
class Run:
    pixel_offset: int
    indices: bytes


@dataclass(frozen=True)
class Picture:
    member: str
    source_sha256: bytes
    bounds: tuple[int, int, int, int]
    runs: tuple[Run, ...]


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PackError(f"{path}: {error}") from error


def canonical_member(value: object) -> str:
    if not isinstance(value, str):
        raise PackError("picture member must be a string")
    member = value.upper()
    if not (4 <= len(member) <= 15 and member.endswith(".PI")) or any(
        not (character.isascii() and (character.isupper() or character.isdigit()
                                     or character in "_."))
        for character in member
    ):
        raise PackError(f"invalid picture member {value!r}")
    return member


def valid_locale(value: object) -> str:
    if not isinstance(value, str) or not (1 <= len(value) <= 15) or any(
        not (character.isascii() and (character.isalnum() or character == "-"))
        for character in value
    ):
        raise PackError("invalid locale")
    return value


def load_source_bmp(path: Path) -> DecodedSource:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise PackError(f"{path}: {error}") from error
    if len(data) < 14 + 40 or data[:2] != b"BM":
        raise PackError(f"{path}: not a Windows BMP")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40 or len(data) < 14 + dib_size:
        raise PackError(f"{path}: unsupported BMP header")
    width, signed_height = struct.unpack_from("<ii", data, 18)
    planes, bits = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    colors_used = struct.unpack_from("<I", data, 46)[0]
    if width != WIDTH or abs(signed_height) != HEIGHT or planes != 1 or (
        bits != 4 or compression != 0
    ):
        raise PackError(f"{path}: expected an uncompressed 640x400 4-bpp BMP")
    palette_count = colors_used or 16
    palette_offset = 14 + dib_size
    if palette_count != 16 or palette_offset + 64 > len(data):
        raise PackError(f"{path}: expected exactly 16 palette entries")
    palette: list[tuple[int, int, int]] = []
    for index in range(16):
        blue, green, red, reserved = data[palette_offset + index * 4:
                                          palette_offset + index * 4 + 4]
        if reserved != 0 or red % 17 or green % 17 or blue % 17:
            raise PackError(f"{path}: palette is not exact RGB4")
        palette.append((red // 17, green // 17, blue // 17))
    stride = (((WIDTH + 1) // 2) + 3) & ~3
    if pixel_offset + stride * HEIGHT != len(data):
        raise PackError(f"{path}: malformed BMP pixel extent")
    rows: list[bytes] = []
    for stored_y in range(HEIGHT):
        row_start = pixel_offset + stored_y * stride
        row = bytearray()
        for value in data[row_start:row_start + WIDTH // 2]:
            row.extend((value >> 4, value & 0x0F))
        rows.append(bytes(row))
    if signed_height > 0:
        rows.reverse()
    indices = b"".join(rows)
    canonical = struct.pack("<HH", WIDTH, HEIGHT) + bytes(
        channel for color in palette for channel in color
    ) + indices
    return DecodedSource(tuple(palette), indices, hashlib.sha256(canonical).digest())


def paeth(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def load_overlay_png(path: Path) -> tuple[bytes, tuple[tuple[int, int, int], ...], bytes]:
    try:
        data = path.read_bytes()
    except OSError as error:
        raise PackError(f"{path}: {error}") from error
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise PackError(f"{path}: not a PNG")
    offset = 8
    ihdr: bytes | None = None
    palette: bytes | None = None
    alpha: bytes | None = None
    compressed = bytearray()
    ended = False
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload_end = offset + 8 + length
        if payload_end + 4 > len(data):
            raise PackError(f"{path}: truncated PNG chunk")
        payload = data[offset + 8:payload_end]
        stored_crc = struct.unpack_from(">I", data, payload_end)[0]
        if binascii.crc32(kind + payload) & 0xFFFFFFFF != stored_crc:
            raise PackError(f"{path}: PNG chunk checksum mismatch")
        if kind == b"IHDR":
            if ihdr is not None:
                raise PackError(f"{path}: duplicate IHDR")
            ihdr = payload
        elif kind == b"PLTE":
            palette = payload
        elif kind == b"tRNS":
            alpha = payload
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            ended = True
            offset = payload_end + 4
            break
        offset = payload_end + 4
    if not ended or offset != len(data) or ihdr is None or len(ihdr) != 13:
        raise PackError(f"{path}: malformed PNG container")
    width, height, depth, color, compression, filtering, interlace = struct.unpack(
        ">IIBBBBB", ihdr
    )
    if (width, height, depth, color, compression, filtering, interlace) != (
        WIDTH, HEIGHT, 8, 3, 0, 0, 0
    ):
        raise PackError(f"{path}: expected a non-interlaced 640x400 indexed PNG")
    if palette is None or len(palette) == 0 or len(palette) % 3 or len(palette) > 768:
        raise PackError(f"{path}: malformed PNG palette")
    if alpha is None:
        raise PackError(f"{path}: overlay has no transparency table")
    colors = tuple(
        tuple(palette[index:index + 3])
        for index in range(0, len(palette), 3)
    )
    alphas = alpha + bytes([255]) * (len(colors) - len(alpha))
    if len(alpha) > len(colors) or any(value not in (0, 255) for value in alphas):
        raise PackError(f"{path}: overlay alpha must be fully transparent or opaque")
    try:
        filtered = zlib.decompress(bytes(compressed))
    except zlib.error as error:
        raise PackError(f"{path}: invalid compressed PNG pixels") from error
    if len(filtered) != HEIGHT * (WIDTH + 1):
        raise PackError(f"{path}: malformed PNG scanline extent")
    decoded = bytearray()
    previous = bytes(WIDTH)
    cursor = 0
    for _ in range(HEIGHT):
        method = filtered[cursor]
        source = filtered[cursor + 1:cursor + 1 + WIDTH]
        cursor += WIDTH + 1
        if method > 4:
            raise PackError(f"{path}: unsupported PNG scanline filter")
        row = bytearray(WIDTH)
        for x, value in enumerate(source):
            left = row[x - 1] if x else 0
            above = previous[x]
            upper_left = previous[x - 1] if x else 0
            predictor = (0, left, above, (left + above) // 2,
                         paeth(left, above, upper_left))[method]
            row[x] = (value + predictor) & 0xFF
        if any(value >= len(colors) for value in row):
            raise PackError(f"{path}: PNG pixel exceeds its palette")
        decoded.extend(row)
        previous = row
    return bytes(decoded), colors, alphas


def compile_picture_runs(
    member: str,
    source: DecodedSource,
    overlay_path: Path,
) -> Picture:
    overlay, colors, alphas = load_overlay_png(overlay_path)
    expanded_source = tuple(tuple(channel * 17 for channel in color)
                            for color in source.palette)
    mapped: list[int | None] = []
    for index, color in enumerate(colors):
        if alphas[index] == 0:
            mapped.append(None)
            continue
        matches = [source_index for source_index, source_color in
                   enumerate(expanded_source) if source_color == color]
        if len(matches) != 1:
            raise PackError(
                f"{overlay_path}: opaque palette entry {index} does not map "
                "uniquely to the source RGB4 palette"
            )
        mapped.append(matches[0])

    runs: list[Run] = []
    changed_positions: list[int] = []
    for y in range(HEIGHT):
        x = 0
        while x < WIDTH:
            palette_index = overlay[y * WIDTH + x]
            replacement = mapped[palette_index]
            if replacement is None:
                x += 1
                continue
            start = x
            values = bytearray()
            while x < WIDTH:
                palette_index = overlay[y * WIDTH + x]
                replacement = mapped[palette_index]
                if replacement is None:
                    break
                pixel = y * WIDTH + x
                if replacement == source.indices[pixel]:
                    raise PackError(
                        f"{overlay_path}: opaque pixel ({x}, {y}) does not "
                        "change the authenticated source"
                    )
                values.append(replacement)
                changed_positions.append(pixel)
                x += 1
            runs.append(Run(y * WIDTH + start, bytes(values)))
    if not changed_positions:
        raise PackError(f"{overlay_path}: overlay changes no pixels")
    xs = [position % WIDTH for position in changed_positions]
    ys = [position // WIDTH for position in changed_positions]
    bounds = (min(xs), min(ys), max(xs) + 1, max(ys) + 1)
    return Picture(member, source.sha256, bounds, tuple(runs))


def compile_picture(
    member: str,
    source: DecodedSource,
    overlay_path: Path,
    declared_bounds: object,
) -> Picture:
    picture = compile_picture_runs(member, source, overlay_path)
    if not (isinstance(declared_bounds, list) and len(declared_bounds) == 4 and
            all(isinstance(value, int) and not isinstance(value, bool)
                for value in declared_bounds)) or (
                    tuple(declared_bounds) != picture.bounds
                ):
        raise PackError(
            f"{member}: declared bounds {declared_bounds!r} do not match "
            f"{list(picture.bounds)}"
        )
    return picture


def load_author(path: Path) -> tuple[str, tuple[Picture, ...]]:
    document = load_json(path)
    if not isinstance(document, dict) or set(document) != {
        "format", "locale", "pictures"
    } or document.get("format") != AUTHOR_FORMAT:
        raise PackError("author fields do not match T3PTP-AUTHOR-1")
    locale = valid_locale(document.get("locale"))
    records = document.get("pictures")
    if not isinstance(records, list) or len(records) > 24:
        raise PackError("pictures must be an array of at most 24 entries")
    pictures: list[Picture] = []
    base = path.parent
    for record in records:
        if not isinstance(record, dict) or set(record) != {
            "member", "source_bmp", "overlay_png", "bounds"
        }:
            raise PackError("picture fields do not match T3PTP-AUTHOR-1")
        member = canonical_member(record.get("member"))
        source_name = record.get("source_bmp")
        overlay_name = record.get("overlay_png")
        if not isinstance(source_name, str) or not source_name or (
            not isinstance(overlay_name, str) or not overlay_name
        ):
            raise PackError(f"{member}: source_bmp/overlay_png must be paths")
        source = load_source_bmp(base / source_name)
        pictures.append(compile_picture(
            member, source, base / overlay_name, record.get("bounds")
        ))
    pictures.sort(key=lambda picture: picture.member)
    for left, right in zip(pictures, pictures[1:]):
        if left.member == right.member:
            raise PackError(f"duplicate picture member {left.member}")
    return locale, tuple(pictures)


def pack_indices(indices: bytes) -> bytes:
    result = bytearray()
    for offset in range(0, len(indices), 2):
        high = indices[offset]
        low = indices[offset + 1] if offset + 1 < len(indices) else 0
        result.append((high << 4) | low)
    return bytes(result)


def compile_pack(locale: str, pictures: tuple[Picture, ...]) -> bytes:
    picture_directory = bytearray()
    run_directory = bytearray()
    payload = bytearray()
    first_run = 0
    for picture in pictures:
        entry = bytearray(PICTURE_ENTRY_SIZE)
        member = picture.member.encode("ascii")
        entry[:len(member)] = member
        entry[16:48] = picture.source_sha256
        changed_pixels = sum(len(run.indices) for run in picture.runs)
        struct.pack_into(
            "<IIHHHHI", entry, 48, first_run, len(picture.runs),
            *picture.bounds, changed_pixels,
        )
        picture_directory.extend(entry)
        for run in picture.runs:
            packed = pack_indices(run.indices)
            run_directory.extend(struct.pack(
                "<IHHI", run.pixel_offset, len(run.indices), 0, len(payload)
            ))
            payload.extend(packed)
        first_run += len(picture.runs)
    run_offset = HEADER_SIZE + len(picture_directory)
    payload_offset = run_offset + len(run_directory)
    body = bytes(picture_directory + run_directory + payload)
    header = bytearray(HEADER_SIZE)
    header[:8] = MAGIC
    struct.pack_into(
        "<IIIIIII", header, 8, HEADER_SIZE, len(pictures), first_run,
        HEADER_SIZE, run_offset, HEADER_SIZE + len(body), payload_offset,
    )
    header[36:68] = hashlib.sha256(body).digest()
    encoded_locale = locale.encode("ascii")
    header[68:68 + len(encoded_locale)] = encoded_locale
    return bytes(header) + body


def write_if_changed(path: Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--author", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        locale, pictures = load_author(arguments.author)
        output = compile_pack(locale, pictures)
        write_if_changed(arguments.output, output)
    except PackError as error:
        print(f"picture patch pack: {error}", file=sys.stderr)
        return 2
    print(
        f"picture patch pack: locale={locale} pictures={len(pictures)} "
        f"bytes={len(output)} sha256={hashlib.sha256(output).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
