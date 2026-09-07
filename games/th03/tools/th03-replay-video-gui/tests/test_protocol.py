from __future__ import annotations

import unittest

from th03_replay_video_gui.protocol import ProgressTracker, parse_progress_line


def line(render_pass: str, index: int, completed: int, event: str = "progress") -> str:
    return (
        '{"protocol":"T3RVP001","event":"' + event + '","pass":"' +
        render_pass + '","passIndex":' + str(index) +
        ',"passCount":2,"completedPermyriad":' + str(completed) +
        ',"sample":100,"firstSample":0,"totalSamples":200,"frames":100,'
        '"elapsedMilliseconds":500,"etaMilliseconds":500,"speedMilli":3500}'
    )


class ProtocolTests(unittest.TestCase):
    def test_two_pass_progress_is_monotonic(self) -> None:
        tracker = ProgressTracker()
        events = [
            parse_progress_line(line("audio", 1, 0)),
            parse_progress_line(line("audio", 1, 5000)),
            parse_progress_line(line("audio", 1, 10_000, "pass-complete")),
            parse_progress_line(line("video", 2, 0)),
            parse_progress_line(line("video", 2, 10_000, "pass-complete")),
        ]
        self.assertTrue(all(event is not None for event in events))
        self.assertEqual([tracker.accept(event) for event in events],
                         [0, 2500, 5000, 5000, 10_000])

    def test_rejects_regression_and_bad_completion(self) -> None:
        tracker = ProgressTracker()
        tracker.accept(parse_progress_line(line("audio", 1, 5000)))
        with self.assertRaisesRegex(ValueError, "regressed"):
            tracker.accept(parse_progress_line(line("audio", 1, 4999)))
        with self.assertRaisesRegex(ValueError, "incomplete"):
            ProgressTracker().accept(
                parse_progress_line(line("audio", 1, 9999, "pass-complete"))
            )

    def test_ignores_logs_and_rejects_malformed_protocol(self) -> None:
        self.assertIsNone(parse_progress_line("Video encoder: h264_nvenc"))
        with self.assertRaisesRegex(ValueError, "incomplete"):
            parse_progress_line('{"protocol":"T3RVP001","event":"progress"}')


if __name__ == "__main__":
    unittest.main()
