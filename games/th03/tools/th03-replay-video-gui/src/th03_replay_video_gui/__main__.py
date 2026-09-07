"""TH03 Replay Renderer entry point."""

from __future__ import annotations

import subprocess
import sys

from .gui import run_gui
from .paths import find_renderer


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--gui-smoke":
        renderer = find_renderer()
        if renderer is None:
            return 2
        result = subprocess.run(
            [str(renderer), "--help"], capture_output=True, text=True,
            timeout=15, check=False,
        )
        return 0 if result.returncode == 0 and "--machine-progress" in result.stdout else 3
    run_gui()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
