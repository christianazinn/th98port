#!/usr/bin/env python3
"""Create and finalize source-free T3PTP001 transparent-overlay projects."""

from __future__ import annotations

import argparse
import binascii
import json
import os
import shutil
import struct
import sys
import zlib
from pathlib import Path

import compile_picture_patch_pack as pack


class ScaffoldError(ValueError):
    pass


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF))


def transparent_overlay(source: pack.DecodedSource) -> bytes:
    palette = bytes(
        channel * 17
        for color in source.palette
        for channel in color
    ) + b"\0\0\0"
    alpha = bytes([255] * 16 + [0])
    rows = (b"\0" + bytes([16]) * pack.WIDTH) * pack.HEIGHT
    ihdr = struct.pack(
        ">IIBBBBB", pack.WIDTH, pack.HEIGHT, 8, 3, 0, 0, 0
    )
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", ihdr) +
            png_chunk(b"PLTE", palette) + png_chunk(b"tRNS", alpha) +
            png_chunk(b"IDAT", zlib.compress(rows, 9)) +
            png_chunk(b"IEND", b""))


def relative_path(target: Path, base: Path) -> str:
    try:
        return Path(os.path.relpath(target, base)).as_posix()
    except ValueError as error:
        raise ScaffoldError(
            f"author project and source must share a filesystem: {target}"
        ) from error


def discover_sources(source_root: Path) -> dict[str, Path]:
    try:
        candidates = [
            path for path in source_root.iterdir()
            if path.is_file() and path.suffix.lower() == ".bmp"
        ]
    except OSError as error:
        raise ScaffoldError(f"{source_root}: {error}") from error
    sources: dict[str, Path] = {}
    for path in candidates:
        member = pack.canonical_member(path.stem + ".PI")
        if member in sources:
            raise ScaffoldError(f"duplicate source member {member}")
        sources[member] = path.resolve()
    return sources


def initialize_project(
    locale: str,
    source_root: Path,
    output_root: Path,
    requested_members: list[str],
) -> Path:
    locale = pack.valid_locale(locale)
    source_root = source_root.resolve()
    output_root = output_root.resolve()
    if output_root.exists():
        raise ScaffoldError(
            f"refusing to overwrite existing author project {output_root}"
        )
    temporary = output_root.with_name(output_root.name + ".tmp")
    if temporary.exists():
        raise ScaffoldError(f"stale temporary author project exists: {temporary}")
    sources = discover_sources(source_root)
    if requested_members:
        members = [pack.canonical_member(value) for value in requested_members]
        if len(set(members)) != len(members):
            raise ScaffoldError("requested picture members must be unique")
        missing = sorted(set(members) - set(sources))
        if missing:
            raise ScaffoldError("missing source BMPs: " + ", ".join(missing))
    else:
        members = list(sources)
    members.sort()
    if not members:
        raise ScaffoldError(f"{source_root}: no source BMP previews found")

    decoded = {member: pack.load_source_bmp(sources[member]) for member in members}
    records = []
    for member in members:
        overlay = Path("overlays") / (Path(member).stem + ".png")
        records.append({
            "member": member,
            "source_bmp": relative_path(sources[member], output_root),
            "overlay_png": overlay.as_posix(),
            "bounds": None,
        })
    document = {
        "format": pack.AUTHOR_FORMAT,
        "locale": locale,
        "pictures": records,
    }
    try:
        temporary.mkdir(parents=True)
        for record in records:
            member = record["member"]
            destination = temporary / record["overlay_png"]
            pack.write_if_changed(destination, transparent_overlay(decoded[member]))
        author_name = f"picture-patch-{locale}.json"
        pack.write_if_changed(
            temporary / author_name,
            (json.dumps(document, ensure_ascii=False, indent=2) + "\n").encode(
                "utf-8"
            ),
        )
        temporary.replace(output_root)
    except Exception:
        if temporary.exists():
            shutil.rmtree(temporary)
        raise
    return output_root / f"picture-patch-{locale}.json"


def author_shape(path: Path) -> tuple[str, list[dict[str, object]]]:
    document = pack.load_json(path)
    if not isinstance(document, dict) or set(document) != {
        "format", "locale", "pictures"
    } or document.get("format") != pack.AUTHOR_FORMAT:
        raise ScaffoldError("author fields do not match T3PTP-AUTHOR-1")
    locale = pack.valid_locale(document.get("locale"))
    pictures = document.get("pictures")
    if not isinstance(pictures, list) or len(pictures) > 24:
        raise ScaffoldError("pictures must be an array of at most 24 entries")
    return locale, pictures


def finalize_author(author: Path, output: Path) -> int:
    author = author.resolve()
    output = output.resolve()
    locale, records = author_shape(author)
    finalized = []
    observed: set[str] = set()
    for record in records:
        if not isinstance(record, dict) or set(record) != {
            "member", "source_bmp", "overlay_png", "bounds"
        }:
            raise ScaffoldError("picture fields do not match T3PTP-AUTHOR-1")
        member = pack.canonical_member(record.get("member"))
        if member in observed:
            raise ScaffoldError(f"duplicate picture member {member}")
        observed.add(member)
        source_name = record.get("source_bmp")
        overlay_name = record.get("overlay_png")
        if not isinstance(source_name, str) or not source_name or (
            not isinstance(overlay_name, str) or not overlay_name
        ):
            raise ScaffoldError(f"{member}: source_bmp/overlay_png must be paths")
        source_path = (author.parent / source_name).resolve()
        overlay_path = (author.parent / overlay_name).resolve()
        source = pack.load_source_bmp(source_path)
        picture = pack.compile_picture_runs(member, source, overlay_path)
        finalized.append({
            "member": member,
            "source_bmp": relative_path(source_path, output.parent),
            "overlay_png": relative_path(overlay_path, output.parent),
            "bounds": list(picture.bounds),
        })
    finalized.sort(key=lambda record: str(record["member"]))
    document = {
        "format": pack.AUTHOR_FORMAT,
        "locale": locale,
        "pictures": finalized,
    }
    pack.write_if_changed(
        output,
        (json.dumps(document, ensure_ascii=False, indent=2) + "\n").encode(
            "utf-8"
        ),
    )
    return len(finalized)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    initialize = commands.add_parser(
        "init", help="create a new all-transparent overlay project"
    )
    initialize.add_argument("--locale", required=True)
    initialize.add_argument("--source-root", type=Path, required=True)
    initialize.add_argument("--output-root", type=Path, required=True)
    initialize.add_argument("--member", action="append", default=[])
    finalize = commands.add_parser(
        "finalize", help="derive exact changed bounds after editing overlays"
    )
    finalize.add_argument("--author", type=Path, required=True)
    finalize.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        if arguments.command == "init":
            author = initialize_project(
                arguments.locale,
                arguments.source_root,
                arguments.output_root,
                arguments.member,
            )
            print(f"picture patch author: initialized {author}")
        else:
            count = finalize_author(arguments.author, arguments.output)
            print(
                f"picture patch author: finalized pictures={count} "
                f"output={arguments.output.resolve()}"
            )
    except (pack.PackError, ScaffoldError, OSError) as error:
        print(f"picture patch author: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
