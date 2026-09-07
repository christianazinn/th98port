#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile


HEADER = "stream\tschema\tpublication\tsource_frame\trgb_sha256\tfile"
PPM_HEADER = b"P6\n640 400\n255\n"
PIXELS = 640 * 400 * 3
checks = 0


def expect(condition: bool, message: str) -> None:
    global checks
    checks += 1
    if not condition:
        raise AssertionError(message)


def make_capture(root: Path, values: list[int]) -> Path:
    root.mkdir()
    rows = [HEADER]
    for index, value in enumerate(values):
        rgb = bytes([value]) * PIXELS
        name = f"frame-{index:08d}.ppm"
        (root / name).write_bytes(PPM_HEADER + rgb)
        rows.append(
            f"T3EMU\t1\t{index}\t{index}\t{hashlib.sha256(rgb).hexdigest()}\t{name}"
        )
    manifest = root / "T3EMU1.tsv"
    manifest.write_text("\n".join(rows) + "\n", encoding="utf-8", newline="\n")
    return manifest


def invoke(tool: Path, base: Path, candidate: Path, *extra: str) -> tuple[int, dict]:
    result = subprocess.run(
        [sys.executable, str(tool), "--base", str(base), "--candidate", str(candidate), *extra],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8",
    )
    return result.returncode, json.loads(result.stdout)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_compare_pixel_frames.py COMPARATOR")
    tool = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="th03-pixel-frame-test-") as scratch:
        root = Path(scratch)
        base = make_capture(root / "base", [0, 0, 1])
        same = make_capture(root / "same", [0, 1])
        code, report = invoke(tool, base, same)
        expect(code == 0 and report["status"] == "same", "dedupe must ignore repeated scanouts")
        expect(report["base_rows"] == 3 and report["base_unique_frames"] == 2,
               "report must retain raw and deduplicated counts")

        longer = make_capture(root / "longer", [0, 1, 2])
        code, report = invoke(tool, same, longer)
        expect(code == 1 and report["status"] == "length-mismatch",
               "unexplained frame suffix must fail")
        code, report = invoke(tool, same, longer, "--allow-candidate-suffix")
        expect(code == 0 and report["status"] == "same-prefix",
               "explicit suffix policy must pass only the exact prefix")

        mismatch = make_capture(root / "mismatch", [0, 2])
        diff = root / "diff"
        code, report = invoke(tool, same, mismatch, "--diff-directory", str(diff))
        first = report["first_mismatch"]
        expect(code == 1 and report["status"] == "mismatch" and
               first["differing_pixels"] == 640 * 400 and
               first["first_pixel"]["x"] == 0 and first["first_pixel"]["y"] == 0,
               "mismatch must report exact pixel count and first coordinate")
        expect((diff / "first-mismatch-diff.ppm").is_file(),
               "mismatch must emit an inspectable exact diff when requested")

        bad = root / "bad"
        bad_manifest = make_capture(bad, [0])
        (bad / "frame-00000000.ppm").write_bytes(PPM_HEADER + b"short")
        code, report = invoke(tool, same, bad_manifest)
        expect(code == 1 and report["status"] == "malformed" and report["error"],
               "truncated PPM must produce a structured malformed verdict")
    print(f"checks={checks} failures=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
