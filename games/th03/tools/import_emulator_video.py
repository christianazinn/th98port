#!/usr/bin/env python3
"""Import a lossless emulator capture into a strict T3EMU1 RGB stream."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


WIDTH = 640
HEIGHT = 400
PPM_HEADER = b"P6\n640 400\n255\n"
RGB_BYTES = WIDTH * HEIGHT * 3
LOSSLESS_CODECS = {"zmbv", "ffv1"}


def read_ppm(path: Path) -> bytes:
    raw = path.read_bytes()
    if not raw.startswith(PPM_HEADER) or len(raw) != len(PPM_HEADER) + RGB_BYTES:
        raise RuntimeError(f"ffmpeg produced a non-640x400 PPM: {path}")
    return raw[len(PPM_HEADER):]


def run_checked(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command, check=True, text=True, encoding="utf-8",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--ffprobe", default="ffprobe")
    parser.add_argument("--keep-duplicates", action="store_true")
    args = parser.parse_args()

    video = args.video.resolve()
    output = args.output.resolve()
    if not video.is_file():
        raise SystemExit(f"video does not exist: {video}")
    if output.exists():
        raise SystemExit(f"output already exists: {output}")
    temporary = output.with_name(output.name + ".extracting")
    if temporary.exists():
        raise SystemExit(f"temporary output already exists: {temporary}")
    temporary.mkdir(parents=True)
    try:
        probe = run_checked([
            args.ffprobe, "-v", "error", "-select_streams", "v:0",
            "-show_entries", "stream=width,height,codec_name,pix_fmt,r_frame_rate",
            "-of", "json", str(video),
        ])
        metadata = json.loads(probe.stdout)
        streams = metadata.get("streams", [])
        if len(streams) != 1:
            raise RuntimeError("capture must contain exactly one selected video stream")
        stream = streams[0]
        if stream.get("width") != WIDTH or stream.get("height") != HEIGHT:
            raise RuntimeError(
                f"capture is {stream.get('width')}x{stream.get('height')}, expected 640x400"
            )
        if stream.get("codec_name") not in LOSSLESS_CODECS:
            raise RuntimeError(
                f"capture codec {stream.get('codec_name')!r} is not an accepted lossless oracle codec"
            )
        extracted = temporary / "all"
        extracted.mkdir()
        run_checked([
            args.ffmpeg, "-v", "error", "-i", str(video), "-map", "0:v:0",
            "-vsync", "0", "-pix_fmt", "rgb24", str(extracted / "frame-%08d.ppm"),
        ])
        source_frames = sorted(extracted.glob("frame-*.ppm"))
        if not source_frames:
            raise RuntimeError("ffmpeg extracted zero frames")
        manifest_rows = [
            "stream\tschema\tpublication\tsource_frame\trgb_sha256\tfile"
        ]
        previous = None
        publication = 0
        for source_index, source in enumerate(source_frames):
            rgb = read_ppm(source)
            if not args.keep_duplicates and previous == rgb:
                continue
            previous = rgb
            filename = f"frame-{publication:08d}.ppm"
            destination = temporary / filename
            shutil.move(source, destination)
            digest = hashlib.sha256(rgb).hexdigest()
            manifest_rows.append(
                f"T3EMU\t1\t{publication}\t{source_index}\t{digest}\t{filename}"
            )
            publication += 1
        shutil.rmtree(extracted)
        (temporary / "T3EMU1.tsv").write_text(
            "\n".join(manifest_rows) + "\n", encoding="utf-8", newline="\n"
        )
        (temporary / "source.json").write_text(
            json.dumps({
                "schema": "T3EMU1", "video": str(video),
                "video_sha256": sha256_file(video),
                "source_frames": len(source_frames), "published_frames": publication,
                "deduplicated": not args.keep_duplicates, "ffprobe": stream,
            }, indent=2) + "\n",
            encoding="utf-8", newline="\n",
        )
        temporary.rename(output)
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    print(f"manifest={output / 'T3EMU1.tsv'}")
    print(f"frames={publication}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
