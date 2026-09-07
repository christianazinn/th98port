#!/usr/bin/env python3
"""Hermetic CLI coverage for th03-resource-preview."""

from __future__ import annotations

import argparse
import hashlib
import struct
import subprocess
import tempfile
from pathlib import Path


def run(tool: Path, *arguments: str, expected: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [str(tool), *arguments],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != expected:
        raise AssertionError(
            f"exit {result.returncode}, expected {expected}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def palette_bytes() -> bytes:
    return bytes(
        component
        for color in range(16)
        for component in (color, 15 - color, color ^ 5)
    )


def cdg_bytes() -> bytes:
    width = 32
    height = 2
    plane_size = (width // 8) * height
    header = struct.pack(
        "<HHHhHBbHH",
        plane_size,
        width,
        height,
        80 * (height - 1),
        width // 32,
        0,
        0,
        0,
        0,
    )
    return header + bytes([0xFF] * plane_size) + bytes([0xFF] * plane_size) + bytes(
        plane_size * 3
    )


def bfnt_header(
    color_flags: int,
    width: int,
    height: int,
    first: int,
    last: int,
) -> bytes:
    return b"".join(
        (
            b"BFNT\x1a",
            bytes((color_flags, 0x16, 0)),
            struct.pack("<HHHH", width, height, first, last),
            b"PREVIEW\0",
            struct.pack("<IHH", 0, 0, 0),
        )
    )


def superimpose_bfnt_bytes() -> bytes:
    palette = bytes(
        component
        for color in range(16)
        for component in ((color ^ 5) << 4, color << 4, (15 - color) << 4)
    )
    return (
        bfnt_header(0x83, 8, 2, 0, 1)
        + palette
        + bytes([0x12] * 8)
        + bytes([0x34] * 8)
    )


def gaiji_bfnt_bytes() -> bytes:
    glyph_bytes = (16 // 8) * 16
    payload = bytes(
        (glyph + offset) & 0xFF
        for glyph in range(256)
        for offset in range(glyph_bytes)
    )
    return bfnt_header(0, 16, 16, 0, 255) + payload


def bmp_geometry(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:2] != b"BM" or len(data) < 118:
        raise AssertionError("output is not a complete 16-color BMP")
    return (
        struct.unpack_from("<I", data, 18)[0],
        struct.unpack_from("<I", data, 22)[0],
        struct.unpack_from("<H", data, 28)[0],
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool", required=True, type=Path)
    arguments = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="th03-resource-preview-") as temporary:
        root = Path(temporary)
        palette = root / "fixture.RGB"
        cdg = root / "fixture.CDG"
        superimpose = root / "fixture.BFT"
        gaiji = root / "gaiji.BFT"
        palette.write_bytes(palette_bytes())
        cdg.write_bytes(cdg_bytes())
        superimpose.write_bytes(superimpose_bfnt_bytes())
        gaiji.write_bytes(gaiji_bfnt_bytes())

        cdg_output = root / "cdg.bmp"
        complete = run(
            arguments.tool,
            "--input", str(cdg),
            "--type", "cdg",
            "--palette", str(palette),
            "--discard-alpha",
            "--output", str(cdg_output),
        )
        assert "status=complete" in complete.stdout
        assert bmp_geometry(cdg_output) == (32, 2, 4)
        cdg_hash = hashlib.sha256(cdg_output.read_bytes()).digest()
        second_output = root / "cdg-second.bmp"
        run(
            arguments.tool,
            "--input", str(cdg),
            "--type", "cdg",
            "--palette", str(palette),
            "--discard-alpha",
            "--output", str(second_output),
        )
        assert hashlib.sha256(second_output.read_bytes()).digest() == cdg_hash

        run(
            arguments.tool,
            "--input", str(cdg),
            "--type", "cdg",
            "--output", str(root / "missing-palette.bmp"),
            expected=1,
        )
        run(
            arguments.tool,
            "--input", str(cdg),
            "--type", "cdg",
            "--palette", str(palette),
            "--discard-alpha",
            "--output", str(cdg_output),
            expected=1,
        )

        superimpose_output = root / "superimpose.bmp"
        run(
            arguments.tool,
            "--input", str(superimpose),
            "--type", "bfnt-superimpose",
            "--output", str(superimpose_output),
        )
        assert bmp_geometry(superimpose_output) == (16, 2, 4)

        gaiji_output = root / "gaiji.bmp"
        run(
            arguments.tool,
            "--input", str(gaiji),
            "--type", "bfnt-gaiji",
            "--palette", str(palette),
            "--background", "3",
            "--output", str(gaiji_output),
        )
        assert bmp_geometry(gaiji_output) == (640, 112, 4)

        malformed = root / "malformed.PI"
        malformed.write_bytes(b"Pi")
        malformed_output = root / "malformed.bmp"
        run(
            arguments.tool,
            "--input", str(malformed),
            "--type", "pi",
            "--output", str(malformed_output),
            expected=1,
        )
        assert not malformed_output.exists()
        assert not Path(str(malformed_output) + ".partial-T3RSP001").exists()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
