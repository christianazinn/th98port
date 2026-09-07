"""Packaged and development dependency discovery."""

from __future__ import annotations

import os
import shutil
import sys
from pathlib import Path


def bundle_root() -> Path:
    return Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[3]))


def find_renderer() -> Path | None:
    override = os.environ.get("TH03_REPLAY_VIDEO")
    candidates = [
        Path(override) if override else None,
        bundle_root() / "th03_native" / "th03-replay-video.exe",
        Path(sys.executable).resolve().with_name("th03-replay-video.exe"),
    ]
    found = shutil.which("th03-replay-video")
    if found:
        candidates.append(Path(found))
    return next((candidate for candidate in candidates if candidate and candidate.is_file()), None)


def find_ffmpeg() -> str:
    override = os.environ.get("TH03_FFMPEG")
    candidates = [
        Path(override) if override else None,
        bundle_root() / "th03_ffmpeg" / "ffmpeg.exe",
        Path(sys.executable).resolve().with_name("ffmpeg.exe"),
    ]
    for candidate in candidates:
        if candidate and candidate.is_file():
            return str(candidate)
    return shutil.which("ffmpeg") or "ffmpeg"
