"""Validated replay-renderer job model and deterministic CLI projection."""

from __future__ import annotations

import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


ENCODERS = {
    "Automatic (NVIDIA, then software)": "auto",
    "H.264 NVIDIA NVENC": "h264_nvenc",
    "HEVC NVIDIA NVENC": "hevc_nvenc",
    "AV1 NVIDIA NVENC": "av1_nvenc",
    "H.264 software (libx264)": "libx264",
}
PRESETS = tuple(f"p{index}" for index in range(1, 8))
VOLUMES = (None, *range(0, 101, 5))
RESOLUTION_PATTERN = re.compile(r"^(\d+)[xX](\d+)$")


class JobError(ValueError):
    """The requested job cannot be represented safely by the renderer CLI."""


@dataclass(frozen=True)
class RenderJob:
    renderer: Path
    replay: Path
    data: Path
    output: Path
    ffmpeg: str
    resolution: str = "1920x1200"
    encoder_label: str = "Automatic (NVIDIA, then software)"
    encoder_preset: str = "p4"
    checkpoint: int = 0
    use_installed_settings: bool = True
    language: str | None = None
    reduced_flash: bool | None = None
    master_volume: int | None = None
    music_volume: int | None = None
    sfx_volume: int | None = None
    game_text_pack: Path | None = None
    cutscene_text_pack: Path | None = None
    staging_output: Path | None = None
    overwrite: bool = False

    def validate(self) -> None:
        if not self.renderer.is_file():
            raise JobError("The bundled replay renderer is missing.")
        if not self.replay.is_file():
            raise JobError("Choose an existing finalized V13 replay.")
        if not self.data.is_dir():
            raise JobError("Choose the extracted game-data directory.")
        for filename in ("MAIN.EXE", "MAINL.EXE", "FONT.ROM"):
            if not (self.data / filename).is_file():
                raise JobError(f"The game-data directory is missing {filename}.")
        if self.output.suffix.lower() != ".mkv":
            raise JobError("The output filename must end in .mkv.")
        match = RESOLUTION_PATTERN.fullmatch(self.resolution.strip())
        if not match:
            raise JobError("Resolution must use WIDTHxHEIGHT.")
        width, height = (int(value) for value in match.groups())
        if width <= 0 or height <= 0 or width * 5 != height * 8:
            raise JobError("Resolution must be a nonzero 8:5 size.")
        if self.encoder_label not in ENCODERS:
            raise JobError("Choose a supported video encoder.")
        if self.encoder_preset not in PRESETS:
            raise JobError("Choose an encoder preset from p1 through p7.")
        if self.checkpoint < 0:
            raise JobError("Checkpoint cannot be negative.")
        if not self.ffmpeg.strip():
            raise JobError("Choose FFmpeg or make it available on PATH.")
        if self.language not in (None, "ja", "en"):
            raise JobError("Language must be inherited, Japanese, or English.")
        for label, value in (
            ("Master volume", self.master_volume),
            ("Music volume", self.music_volume),
            ("SFX volume", self.sfx_volume),
        ):
            if value is not None and (value not in range(0, 101, 5)):
                raise JobError(f"{label} must use 5% steps from 0% through 100%.")
        if self.game_text_pack is not None and not self.game_text_pack.is_file():
            raise JobError("The selected T3GTL001 game-text pack does not exist.")
        if self.cutscene_text_pack is not None and not self.cutscene_text_pack.is_file():
            raise JobError("The selected T3CTP001 cutscene-text pack does not exist.")
        if self.staging_output is not None:
            if self.staging_output == self.output:
                raise JobError("The staging output must differ from the destination.")
            if self.staging_output.parent != self.output.parent:
                raise JobError("The staging output must be beside the destination.")
            if self.staging_output.exists():
                raise JobError("The private staging output already exists.")

    def command(self) -> list[str]:
        self.validate()
        command = [
            str(self.renderer),
            "--replay", str(self.replay),
            "--data", str(self.data),
            "--output", str(self.output),
            "--resolution", self.resolution.lower(),
            "--encoder", ENCODERS[self.encoder_label],
            "--encoder-preset", self.encoder_preset,
            "--checkpoint", str(self.checkpoint),
            "--ffmpeg", self.ffmpeg,
            "--machine-progress",
        ]
        if self.use_installed_settings:
            command.append("--use-installed-settings")
        if self.language is not None:
            command.extend(("--language", self.language))
        if self.reduced_flash is not None:
            command.append("--reduced-flash" if self.reduced_flash else "--full-flash")
        for option, value in (
            ("--master-volume", self.master_volume),
            ("--music-volume", self.music_volume),
            ("--sfx-volume", self.sfx_volume),
        ):
            if value is not None:
                command.extend((option, str(value)))
        if self.game_text_pack is not None:
            command.extend(("--game-text-pack", str(self.game_text_pack)))
        if self.cutscene_text_pack is not None:
            command.extend(("--cutscene-text-pack", str(self.cutscene_text_pack)))
        if self.staging_output is not None:
            command.extend(("--staging-output", str(self.staging_output)))
        if self.overwrite:
            command.append("--overwrite")
        return command

    def command_preview(self) -> str:
        return subprocess.list2cmdline(self.command())
