#!/usr/bin/env python3
"""Strict byte-for-byte comparison of T3RGB1/T3EMU1 pixel streams."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
import shutil
import sys


WIDTH = 640
HEIGHT = 400
RGB_BYTES = WIDTH * HEIGHT * 3
T3RGB1_HEADER = [
    "stream", "schema", "publication", "logical_tick", "coalesced_ticks",
    "overlay", "reduced_flash", "source_fnv64", "raster_fnv64",
    "tram_fnv64", "shown_page", "raster_mode", "scroll_y", "tone",
    "rgb_fnv64", "file",
]
T3EMU1_HEADER = [
    "stream", "schema", "publication", "source_frame", "rgb_sha256", "file",
]


class MalformedCapture(ValueError):
    pass


@dataclass(frozen=True)
class Frame:
    ordinal: int
    source_identity: str
    path: Path
    rgb: bytes
    sha256: str


@dataclass(frozen=True)
class Capture:
    stream: str
    rows: int
    frames: tuple[Frame, ...]


def fnv1a64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}"


def parse_ppm(path: Path) -> bytes:
    raw = path.read_bytes()
    expected = b"P6\n640 400\n255\n"
    if not raw.startswith(expected):
        raise MalformedCapture(f"unsupported PPM header: {path}")
    rgb = raw[len(expected):]
    if len(rgb) != RGB_BYTES:
        raise MalformedCapture(
            f"PPM payload has {len(rgb)} bytes, expected {RGB_BYTES}: {path}"
        )
    return rgb


def resolve_frame(manifest: Path, relative: str) -> Path:
    if not relative or Path(relative).is_absolute():
        raise MalformedCapture(f"frame path must be relative: {relative!r}")
    root = manifest.parent.resolve()
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root)
    except ValueError as error:
        raise MalformedCapture(f"frame path escapes manifest directory: {relative}") from error
    if not candidate.is_file():
        raise MalformedCapture(f"frame file does not exist: {candidate}")
    return candidate


def read_capture(manifest: Path, dedupe: bool) -> Capture:
    manifest = manifest.resolve()
    if not manifest.is_file():
        raise MalformedCapture(f"manifest does not exist: {manifest}")
    with manifest.open("r", encoding="utf-8-sig", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    if not rows:
        raise MalformedCapture(f"manifest is empty: {manifest}")
    header = rows[0]
    if header == T3RGB1_HEADER:
        stream, schema, identity_column, hash_column, file_column = (
            "T3RGB", "1", 3, 14, 15
        )
    elif header == T3EMU1_HEADER:
        stream, schema, identity_column, hash_column, file_column = (
            "T3EMU", "1", 3, 4, 5
        )
    else:
        raise MalformedCapture(f"unsupported pixel manifest header: {manifest}")
    if len(rows) == 1:
        raise MalformedCapture(f"manifest contains no frame rows: {manifest}")

    frames: list[Frame] = []
    for ordinal, row in enumerate(rows[1:]):
        if len(row) != len(header):
            raise MalformedCapture(
                f"row {ordinal + 2} has {len(row)} fields, expected {len(header)}: {manifest}"
            )
        if row[0] != stream or row[1] != schema or row[2] != str(ordinal):
            raise MalformedCapture(
                f"row {ordinal + 2} has invalid stream/schema/ordinal: {manifest}"
            )
        path = resolve_frame(manifest, row[file_column])
        rgb = parse_ppm(path)
        sha = hashlib.sha256(rgb).hexdigest()
        declared = row[hash_column].lower()
        if stream == "T3RGB":
            if declared != fnv1a64(rgb):
                raise MalformedCapture(f"RGB FNV mismatch at row {ordinal}: {manifest}")
        elif declared != sha:
            raise MalformedCapture(f"RGB SHA-256 mismatch at row {ordinal}: {manifest}")
        frame = Frame(ordinal, row[identity_column], path, rgb, sha)
        if dedupe and frames and frames[-1].rgb == rgb:
            continue
        frames.append(frame)
    return Capture(stream, len(rows) - 1, tuple(frames))


def pixel_difference(base: bytes, candidate: bytes) -> dict[str, object]:
    differing = 0
    first = None
    left = WIDTH
    top = HEIGHT
    right = -1
    bottom = -1
    total_delta = 0
    max_delta = 0
    diff = bytearray(RGB_BYTES)
    for offset in range(0, RGB_BYTES, 3):
        lhs = base[offset:offset + 3]
        rhs = candidate[offset:offset + 3]
        if lhs == rhs:
            continue
        pixel = offset // 3
        x = pixel % WIDTH
        y = pixel // WIDTH
        if first is None:
            first = {
                "x": x, "y": y,
                "base_rgb": list(lhs), "candidate_rgb": list(rhs),
            }
        differing += 1
        left = min(left, x)
        top = min(top, y)
        right = max(right, x)
        bottom = max(bottom, y)
        delta = sum(abs(lhs[channel] - rhs[channel]) for channel in range(3))
        total_delta += delta
        max_delta = max(max_delta, *(abs(lhs[channel] - rhs[channel]) for channel in range(3)))
        diff[offset:offset + 3] = bytes(
            min(255, abs(lhs[channel] - rhs[channel]) * 4)
            for channel in range(3)
        )
    return {
        "differing_pixels": differing,
        "first_pixel": first,
        "bounding_box": None if first is None else {
            "left": left, "top": top, "right": right, "bottom": bottom,
        },
        "maximum_channel_delta": max_delta,
        "mean_absolute_channel_delta_per_differing_pixel": (
            0.0 if differing == 0 else total_delta / (differing * 3)
        ),
        "diff_rgb": bytes(diff),
    }


def write_ppm(path: Path, rgb: bytes) -> None:
    path.write_bytes(b"P6\n640 400\n255\n" + rgb)


def compare(args: argparse.Namespace) -> tuple[dict[str, object], int]:
    base = read_capture(args.base, not args.keep_duplicates)
    candidate = read_capture(args.candidate, not args.keep_duplicates)
    if args.base_start < 0 or args.candidate_start < 0:
        raise MalformedCapture("frame starts must be nonnegative")
    base_frames = base.frames[args.base_start:]
    candidate_frames = candidate.frames[args.candidate_start:]
    compared = min(len(base_frames), len(candidate_frames))
    mismatch = None
    diff_data = None
    for index in range(compared):
        if base_frames[index].rgb == candidate_frames[index].rgb:
            continue
        detail = pixel_difference(base_frames[index].rgb, candidate_frames[index].rgb)
        diff_data = detail.pop("diff_rgb")
        mismatch = {
            "comparison_index": index,
            "base_ordinal": base_frames[index].ordinal,
            "candidate_ordinal": candidate_frames[index].ordinal,
            "base_identity": base_frames[index].source_identity,
            "candidate_identity": candidate_frames[index].source_identity,
            **detail,
        }
        if args.diff_directory:
            args.diff_directory.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(base_frames[index].path, args.diff_directory / "first-mismatch-base.ppm")
            shutil.copyfile(candidate_frames[index].path, args.diff_directory / "first-mismatch-candidate.ppm")
            write_ppm(args.diff_directory / "first-mismatch-diff.ppm", diff_data)
        break

    length_accepted = (
        len(base_frames) == len(candidate_frames)
        or (args.allow_candidate_suffix and len(candidate_frames) > len(base_frames))
        or (args.allow_base_suffix and len(base_frames) > len(candidate_frames))
    )
    if mismatch is not None:
        status = "mismatch"
    elif not length_accepted:
        status = "length-mismatch"
    elif len(base_frames) == len(candidate_frames):
        status = "same"
    else:
        status = "same-prefix"
    report = {
        "status": status,
        "base_stream": base.stream,
        "candidate_stream": candidate.stream,
        "base_rows": base.rows,
        "candidate_rows": candidate.rows,
        "base_unique_frames": len(base.frames),
        "candidate_unique_frames": len(candidate.frames),
        "base_start": args.base_start,
        "candidate_start": args.candidate_start,
        "compared_frames": compared,
        "first_mismatch": mismatch,
        "error": None,
    }
    return report, 0 if status in {"same", "same-prefix"} else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--diff-directory", type=Path)
    parser.add_argument("--base-start", type=int, default=0)
    parser.add_argument("--candidate-start", type=int, default=0)
    parser.add_argument("--keep-duplicates", action="store_true")
    parser.add_argument("--allow-candidate-suffix", action="store_true")
    parser.add_argument("--allow-base-suffix", action="store_true")
    args = parser.parse_args()
    try:
        report, code = compare(args)
    except (MalformedCapture, OSError, csv.Error) as error:
        report = {
            "status": "malformed", "base_rows": 0, "candidate_rows": 0,
            "compared_frames": 0, "first_mismatch": None, "error": str(error),
        }
        code = 1
    encoded = json.dumps(report, indent=2, sort_keys=False) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded, encoding="utf-8", newline="\n")
    sys.stdout.write(encoded)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
