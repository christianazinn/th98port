#!/usr/bin/env python3
"""Compile ordinary artwork into a New Classic split 4-bpp menu background."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path
from typing import Sequence


WIDTH = 640
HEIGHT = 400
HALF_HEIGHT = HEIGHT // 2
PACKED_ROW_BYTES = WIDTH // 2
BMP_HEADER_BYTES = 14 + 40 + (16 * 4)
BMP_HALF_BYTES = BMP_HEADER_BYTES + (PACKED_ROW_BYTES * HALF_HEIGHT)
FORMAT = "T3MBG001"
COMPILER = "th03-menu-background-1"

# Retained menu text and repo-owned backgrounds share one indexed PC-98 page.
# Background pixels may use only AVAILABLE_INDICES. Runtime owns the remaining
# six entries regardless of the source artwork's palette.
AVAILABLE_INDICES = (0, 1, 2, 3, 4, 5, 6, 7, 10, 14)
FIXED_COLORS = {
    8: (204, 51, 238),    # reciprocal one-pixel shadow
    9: (204, 170, 255),   # lavender heading/navigation
    11: (255, 170, 255),  # status/advisory pink
    12: (255, 255, 0),    # selected entry
    13: (255, 204, 255),  # light status pink
    15: (255, 255, 255),  # body text
}
RESERVED_INDICES = tuple(sorted(FIXED_COLORS))
NAME_PATTERN = re.compile(r"[A-Z0-9_]{1,7}")


class BackgroundError(ValueError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def valid_name(value: str) -> str:
    name = value.upper()
    if not NAME_PATTERN.fullmatch(name):
        raise BackgroundError(
            "name must contain 1-7 DOS-safe letters, digits, or underscores"
        )
    return name


def rgb4_color(color: Sequence[int]) -> tuple[int, int, int]:
    if len(color) != 3:
        raise BackgroundError("RGB colors must have exactly three channels")
    result = []
    for channel in color:
        if not isinstance(channel, int) or isinstance(channel, bool) or not (
            0 <= channel <= 255
        ):
            raise BackgroundError("RGB channels must be integers from 0 to 255")
        result.append(min(15, (channel + 8) // 17) * 17)
    return tuple(result)  # type: ignore[return-value]


def complete_palette(
    background_colors: Sequence[Sequence[int]],
) -> tuple[tuple[int, int, int], ...]:
    rounded: list[tuple[int, int, int]] = []
    for color in background_colors:
        value = rgb4_color(color)
        if value not in rounded:
            rounded.append(value)
    if not rounded:
        raise BackgroundError("quantization produced no background colors")
    if len(rounded) > len(AVAILABLE_INDICES):
        raise BackgroundError("quantization produced more than ten colors")
    # Duplicate the first real background color into unused background slots.
    # This keeps every candidate in-gamut without accidentally adding black as
    # an eleventh quantization color when a simple source uses fewer colors.
    palette = [rounded[0]] * 16
    for index, color in zip(AVAILABLE_INDICES, rounded, strict=False):
        palette[index] = color
    for index, color in FIXED_COLORS.items():
        palette[index] = color
    return tuple(palette)


def nearest_color(
    color: Sequence[int],
    candidates: Sequence[tuple[int, tuple[int, int, int]]],
) -> tuple[int, tuple[int, int, int]]:
    return min(
        candidates,
        key=lambda item: (
            sum((int(color[channel]) - item[1][channel]) ** 2
                for channel in range(3)),
            item[0],
        ),
    )


def truncate_dividend(value: int, divisor: int) -> int:
    return (abs(value) // divisor) * (-1 if value < 0 else 1)


def map_pixels(
    pixels: Sequence[tuple[int, int, int]],
    palette: Sequence[tuple[int, int, int]],
    dither: str,
    width: int = WIDTH,
    height: int = HEIGHT,
) -> bytes:
    if len(pixels) != width * height:
        raise BackgroundError("pixel count does not match image dimensions")
    if dither not in {"none", "floyd-steinberg"}:
        raise BackgroundError("dither must be none or floyd-steinberg")
    candidates = [(index, palette[index]) for index in AVAILABLE_INDICES]
    if dither == "none":
        return bytes(nearest_color(pixel, candidates)[0] for pixel in pixels)

    # Errors use 8 fractional bits. Division truncates toward zero explicitly,
    # avoiding Python's negative-floor behavior and making the author transform
    # independent of native word size or floating-point modes.
    current = [[0] * (width + 2) for _ in range(3)]
    following = [[0] * (width + 2) for _ in range(3)]
    output = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            source = pixels[(y * width) + x]
            adjusted = tuple(
                min(255, max(0, (source[channel] * 256 +
                    current[channel][x + 1] + 128) // 256))
                for channel in range(3)
            )
            index, chosen = nearest_color(adjusted, candidates)
            output[(y * width) + x] = index
            for channel in range(3):
                error = (source[channel] * 256 +
                    current[channel][x + 1]) - (chosen[channel] * 256)
                current[channel][x + 2] += truncate_dividend(error * 7, 16)
                following[channel][x] += truncate_dividend(error * 3, 16)
                following[channel][x + 1] += truncate_dividend(error * 5, 16)
                following[channel][x + 2] += truncate_dividend(error, 16)
        current, following = following, [[0] * (width + 2) for _ in range(3)]
    return bytes(output)


def pack_bmp_half(
    indices: bytes,
    palette: Sequence[tuple[int, int, int]],
    top: int,
) -> bytes:
    if len(indices) != WIDTH * HEIGHT:
        raise BackgroundError("indexed page must be exactly 640x400")
    if len(palette) != 16:
        raise BackgroundError("palette must contain exactly 16 colors")
    if top not in {0, HALF_HEIGHT}:
        raise BackgroundError("half starts at row 0 or 200")
    if any(index > 15 for index in indices):
        raise BackgroundError("indexed page contains a value above 15")

    pixels = bytearray(PACKED_ROW_BYTES * HALF_HEIGHT)
    destination = 0
    for y in range(top + HALF_HEIGHT - 1, top - 1, -1):
        row = y * WIDTH
        for x in range(0, WIDTH, 2):
            pixels[destination] = (
                (indices[row + x] << 4) | indices[row + x + 1]
            )
            destination += 1
    header = bytearray(BMP_HEADER_BYTES)
    header[:2] = b"BM"
    struct.pack_into("<III", header, 2, BMP_HALF_BYTES, 0, BMP_HEADER_BYTES)
    struct.pack_into(
        "<IiiHHIIiiII", header, 14, 40, WIDTH, HALF_HEIGHT, 1, 4,
        0, len(pixels), 0, 0, 16, 16,
    )
    for index, color in enumerate(palette):
        red, green, blue = rgb4_color(color)
        header[54 + (index * 4):58 + (index * 4)] = bytes(
            (blue, green, red, 0)
        )
    return bytes(header) + bytes(pixels)


def unpack_bmp_half(data: bytes) -> tuple[
    bytes, tuple[tuple[int, int, int], ...]
]:
    if len(data) != BMP_HALF_BYTES or data[:2] != b"BM":
        raise BackgroundError("menu background half has the wrong BMP shape")
    size, reserved, offset = struct.unpack_from("<III", data, 2)
    dib = struct.unpack_from("<IiiHHIIiiII", data, 14)
    if size != BMP_HALF_BYTES or reserved != 0 or offset != BMP_HEADER_BYTES or (
        dib[:7] != (40, WIDTH, HALF_HEIGHT, 1, 4, 0,
                    PACKED_ROW_BYTES * HALF_HEIGHT)
    ) or dib[9:] != (16, 16):
        raise BackgroundError("menu background half has invalid BMP metadata")
    palette = tuple(
        (data[56 + index * 4], data[55 + index * 4], data[54 + index * 4])
        for index in range(16)
    )
    output = bytearray(WIDTH * HALF_HEIGHT)
    source = BMP_HEADER_BYTES
    for file_y in range(HALF_HEIGHT):
        y = HALF_HEIGHT - file_y - 1
        row = y * WIDTH
        for x in range(0, WIDTH, 2):
            value = data[source]
            source += 1
            output[row + x] = value >> 4
            output[row + x + 1] = value & 15
    return bytes(output), palette


def parse_hex_color(value: str) -> tuple[int, int, int]:
    if not re.fullmatch(r"#[0-9A-Fa-f]{6}", value):
        raise BackgroundError("matte must be a color in #RRGGBB form")
    return tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))


def load_and_fit(
    path: Path,
    fit: str,
    matte: tuple[int, int, int],
) -> tuple[list[tuple[int, int, int]], tuple[int, int], str]:
    try:
        from PIL import Image, ImageOps, __version__ as pillow_version
    except ImportError as error:
        raise BackgroundError(
            "Pillow is required (install with: python -m pip install Pillow)"
        ) from error
    try:
        with Image.open(path) as opened:
            if getattr(opened, "n_frames", 1) != 1:
                raise BackgroundError("animated source images are not supported")
            source_size = opened.size
            transposed = ImageOps.exif_transpose(opened)
            rgba = transposed.convert("RGBA")
            backing = Image.new("RGBA", rgba.size, (*matte, 255))
            image = Image.alpha_composite(backing, rgba).convert("RGB")
    except BackgroundError:
        raise
    except (OSError, ValueError) as error:
        raise BackgroundError(f"cannot decode source image: {error}") from error

    resampling = Image.Resampling.LANCZOS
    if fit == "stretch":
        fitted = image.resize((WIDTH, HEIGHT), resampling)
    elif fit == "cover":
        fitted = ImageOps.fit(image, (WIDTH, HEIGHT), resampling, centering=(0.5, 0.5))
    elif fit == "contain":
        contained = ImageOps.contain(image, (WIDTH, HEIGHT), resampling)
        fitted = Image.new("RGB", (WIDTH, HEIGHT), matte)
        fitted.paste(
            contained,
            ((WIDTH - contained.width) // 2, (HEIGHT - contained.height) // 2),
        )
    else:
        raise BackgroundError("fit must be cover, contain, or stretch")
    return list(fitted.getdata()), source_size, pillow_version


def derive_background_colors(
    pixels: Sequence[tuple[int, int, int]],
) -> list[tuple[int, int, int]]:
    try:
        from PIL import Image
    except ImportError as error:
        raise BackgroundError(
            "Pillow is required (install with: python -m pip install Pillow)"
        ) from error
    image = Image.new("RGB", (WIDTH, HEIGHT))
    image.putdata(pixels)
    quantized = image.quantize(
        colors=len(AVAILABLE_INDICES),
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    )
    raw_palette = quantized.getpalette()
    used = sorted(set(quantized.getdata()))
    colors = [
        tuple(raw_palette[index * 3:index * 3 + 3])
        for index in used
    ]
    # Prefer the most frequently useful colors in the unlikely event that RGB4
    # rounding collapses several median-cut entries.
    counts = Counter(quantized.getdata())
    colors.sort(key=lambda color: (
        -sum(counts[index] for index in used
             if tuple(raw_palette[index * 3:index * 3 + 3]) == color),
        color,
    ))
    return colors


def preview_png_bytes(
    indices: bytes,
    palette: Sequence[tuple[int, int, int]],
) -> bytes:
    try:
        from PIL import Image
    except ImportError as error:
        raise BackgroundError("Pillow disappeared during compilation") from error
    image = Image.frombytes("P", (WIDTH, HEIGHT), indices)
    flattened = [channel for color in palette for channel in color]
    image.putpalette(flattened + ([0] * (768 - len(flattened))))
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=False, compress_level=9)
    return output.getvalue()


def compile_background(
    source: Path,
    output_root: Path,
    name: str,
    fit: str = "cover",
    dither: str = "floyd-steinberg",
    matte: tuple[int, int, int] = (0, 0, 0),
) -> Path:
    source = source.resolve()
    output_root = output_root.resolve()
    name = valid_name(name)
    if output_root.exists():
        raise BackgroundError(f"refusing to overwrite output root {output_root}")
    temporary = output_root.with_name(output_root.name + ".tmp")
    if temporary.exists():
        raise BackgroundError(f"stale temporary output exists: {temporary}")
    try:
        source_bytes = source.read_bytes()
    except OSError as error:
        raise BackgroundError(f"cannot read source image: {error}") from error
    pixels, source_size, pillow_version = load_and_fit(source, fit, matte)
    background_colors = derive_background_colors(pixels)
    palette = complete_palette(background_colors)
    indices = map_pixels(pixels, palette, dither)
    used = Counter(indices)
    leaked = sorted(set(indices) & set(RESERVED_INDICES))
    if leaked:
        raise BackgroundError(
            "internal error: background uses reserved indices " +
            ", ".join(map(str, leaked))
        )
    top = pack_bmp_half(indices, palette, 0)
    bottom = pack_bmp_half(indices, palette, HALF_HEIGHT)
    outputs = {
        f"{name}A.BMP": top,
        f"{name}B.BMP": bottom,
        f"{name}-preview.png": preview_png_bytes(indices, palette),
    }
    manifest = {
        "format": FORMAT,
        "compiler": COMPILER,
        "pillow": pillow_version,
        "source": {
            "name": source.name,
            "bytes": len(source_bytes),
            "sha256": sha256_bytes(source_bytes),
            "width": source_size[0],
            "height": source_size[1],
        },
        "transform": {
            "fit": fit,
            "dither": dither,
            "matte": list(matte),
            "width": WIDTH,
            "height": HEIGHT,
        },
        "palette_rgb4": [
            [channel // 17 for channel in color] for color in palette
        ],
        "available_indices": list(AVAILABLE_INDICES),
        "reserved_indices": list(RESERVED_INDICES),
        "index_counts": [used[index] for index in range(16)],
        "outputs": {
            filename: {"bytes": len(data), "sha256": sha256_bytes(data)}
            for filename, data in outputs.items()
        },
    }
    expected = [*outputs, f"{name}.json"]
    try:
        temporary.mkdir(parents=True)
        for filename, data in outputs.items():
            (temporary / filename).write_bytes(data)
        (temporary / f"{name}.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        temporary.replace(output_root)
    except Exception:
        if temporary.exists():
            for filename in expected:
                try:
                    (temporary / filename).unlink(missing_ok=True)
                except OSError:
                    pass
            try:
                temporary.rmdir()
            except OSError:
                pass
        raise
    return output_root / f"{name}.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--name", required=True,
                        help="1-7 character output stem, for example M008")
    parser.add_argument("--fit", choices=("cover", "contain", "stretch"),
                        default="cover")
    parser.add_argument("--dither", choices=("none", "floyd-steinberg"),
                        default="floyd-steinberg")
    parser.add_argument("--matte", default="#000000",
                        help="alpha/contain backing color in #RRGGBB form")
    arguments = parser.parse_args()
    try:
        manifest = compile_background(
            arguments.input,
            arguments.output_root,
            arguments.name,
            arguments.fit,
            arguments.dither,
            parse_hex_color(arguments.matte),
        )
    except (BackgroundError, OSError) as error:
        print(f"menu background compiler: {error}", file=sys.stderr)
        return 2
    print(f"menu background compiler: wrote {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
