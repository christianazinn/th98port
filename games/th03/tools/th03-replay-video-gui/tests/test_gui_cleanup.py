from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from th03_replay_video_gui.gui import ReplayRendererApp
from th03_replay_video_gui.model import RenderJob


class GuiCleanupTests(unittest.TestCase):
    def test_cancel_preserves_destination_and_removes_only_owned_temporaries(self) -> None:
        with tempfile.TemporaryDirectory(prefix="th03 gui cleanup ") as temporary:
            root = Path(temporary)
            output = root / "existing.mkv"
            staging = root / ".existing.partial.mkv"
            audio = Path(str(output) + ".th03-audio.flac")
            output.write_bytes(b"previous complete video")
            staging.write_bytes(b"partial video")
            audio.write_bytes(b"partial audio")
            job = RenderJob(
                renderer=root / "renderer.exe",
                replay=root / "replay.rpy",
                data=root,
                output=output,
                ffmpeg="ffmpeg",
                staging_output=staging,
            )

            ReplayRendererApp._remove_partial(job)

            self.assertEqual(output.read_bytes(), b"previous complete video")
            self.assertFalse(staging.exists())
            self.assertFalse(audio.exists())


if __name__ == "__main__":
    unittest.main()
