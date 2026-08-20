"""Strict consumer for the renderer's T3RVP001 JSON-lines progress stream."""

from __future__ import annotations

import json
from dataclasses import dataclass


@dataclass(frozen=True)
class ProgressEvent:
    event: str
    render_pass: str
    pass_index: int
    pass_count: int
    completed_permyriad: int
    sample: int
    first_sample: int
    total_samples: int
    frames: int
    elapsed_milliseconds: int
    eta_milliseconds: int
    speed_milli: int

    @property
    def overall_permyriad(self) -> int:
        numerator = (self.pass_index - 1) * 10_000 + self.completed_permyriad
        return min(10_000, numerator // self.pass_count)


def parse_progress_line(line: str) -> ProgressEvent | None:
    try:
        value = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(value, dict) or value.get("protocol") != "T3RVP001":
        return None
    expected = {
        "event", "pass", "passIndex", "passCount", "completedPermyriad",
        "sample", "firstSample", "totalSamples", "frames",
        "elapsedMilliseconds", "etaMilliseconds", "speedMilli",
    }
    if not expected.issubset(value):
        raise ValueError("incomplete T3RVP001 event")
    if value["event"] not in ("progress", "pass-complete"):
        raise ValueError("unsupported T3RVP001 event")
    if value["pass"] not in ("audio", "video"):
        raise ValueError("unsupported T3RVP001 pass")
    integer_names = expected - {"event", "pass"}
    if any(type(value[name]) is not int or value[name] < 0 for name in integer_names):
        raise ValueError("invalid T3RVP001 integer")
    if (
        value["passCount"] != 2
        or value["passIndex"] not in (1, 2)
        or value["completedPermyriad"] > 10_000
        or value["firstSample"] > value["totalSamples"]
    ):
        raise ValueError("invalid T3RVP001 bounds")
    return ProgressEvent(
        event=value["event"],
        render_pass=value["pass"],
        pass_index=value["passIndex"],
        pass_count=value["passCount"],
        completed_permyriad=value["completedPermyriad"],
        sample=value["sample"],
        first_sample=value["firstSample"],
        total_samples=value["totalSamples"],
        frames=value["frames"],
        elapsed_milliseconds=value["elapsedMilliseconds"],
        eta_milliseconds=value["etaMilliseconds"],
        speed_milli=value["speedMilli"],
    )


class ProgressTracker:
    def __init__(self) -> None:
        self._last_by_pass = {1: 0, 2: 0}
        self._last_overall = 0

    def accept(self, event: ProgressEvent) -> int:
        if event.completed_permyriad < self._last_by_pass[event.pass_index]:
            raise ValueError("T3RVP001 pass progress regressed")
        overall = event.overall_permyriad
        if overall < self._last_overall:
            raise ValueError("T3RVP001 overall progress regressed")
        if event.event == "pass-complete" and event.completed_permyriad != 10_000:
            raise ValueError("incomplete T3RVP001 pass completion")
        self._last_by_pass[event.pass_index] = event.completed_permyriad
        self._last_overall = overall
        return overall
