from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from th03_replay_video_gui.model import JobError, RenderJob


class RenderJobTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="th03 replay gui ü ")
        self.root = Path(self.temporary.name)
        self.renderer = self.root / "renderer with spaces.exe"
        self.renderer.write_bytes(b"fixture")
        self.replay = self.root / "世界 record 06.rpy"
        self.replay.write_bytes(b"T3RPLY14")
        self.game_text = self.root / "game text.t3gtl"
        self.game_text.write_bytes(b"T3GTL001")
        self.cutscene_text = self.root / "cutscene text.t3ctp"
        self.cutscene_text.write_bytes(b"T3CTP001")
        self.picture_patch = self.root / "picture patch.t3ptp"
        self.picture_patch.write_bytes(b"T3PTP001")
        self.data = self.root / "content files"
        self.data.mkdir()
        for name in ("MAIN.EXE", "MAINL.EXE", "FONT.ROM"):
            (self.data / name).write_bytes(b"fixture")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def job(self, **overrides: object) -> RenderJob:
        values = {
            "renderer": self.renderer,
            "replay": self.replay,
            "data": self.data,
            "output": self.root / "output video.mkv",
            "ffmpeg": str(self.root / "ffmpeg path.exe"),
        }
        values.update(overrides)
        return RenderJob(**values)

    def test_exact_argument_projection_preserves_unicode_and_spaces(self) -> None:
        job = self.job(
            language="en", reduced_flash=False, master_volume=95,
            music_driver="midi", music_volume=90, sfx_volume=85,
            encoder_label="HEVC NVIDIA NVENC",
            encoder_preset="p1", overwrite=True,
            staging_output=self.root / ".世界 partial.mkv",
            game_text_pack=self.game_text,
            cutscene_text_pack=self.cutscene_text,
            picture_patch_pack=self.picture_patch,
        )
        command = job.command()
        self.assertEqual(command[0], str(self.renderer))
        self.assertEqual(command[command.index("--replay") + 1], str(self.replay))
        self.assertIn("--machine-progress", command)
        self.assertIn("--use-installed-settings", command)
        self.assertEqual(command[command.index("--encoder") + 1], "hevc_nvenc")
        self.assertEqual(command[command.index("--sfx-volume") + 1], "85")
        self.assertEqual(command[command.index("--music-driver") + 1], "midi")
        self.assertEqual(
            command[command.index("--cutscene-text-pack") + 1],
            str(self.cutscene_text),
        )
        self.assertEqual(
            command[command.index("--picture-patch-pack") + 1],
            str(self.picture_patch),
        )
        self.assertEqual(
            command[command.index("--staging-output") + 1],
            str(self.root / ".世界 partial.mkv"),
        )
        self.assertIn('"', job.command_preview())

    def test_explicit_policy_without_installed_settings(self) -> None:
        command = self.job(use_installed_settings=False, language="ja").command()
        self.assertNotIn("--use-installed-settings", command)
        self.assertEqual(command[command.index("--language") + 1], "ja")

    def test_rejects_non_8_to_5_resolution_and_volume_steps(self) -> None:
        with self.assertRaisesRegex(JobError, "8:5"):
            self.job(resolution="1920x1080").validate()
        with self.assertRaisesRegex(JobError, "5%"):
            self.job(sfx_volume=84).validate()

    def test_requires_complete_content_boundary(self) -> None:
        (self.data / "FONT.ROM").unlink()
        with self.assertRaisesRegex(JobError, "FONT.ROM"):
            self.job().validate()


if __name__ == "__main__":
    unittest.main()
