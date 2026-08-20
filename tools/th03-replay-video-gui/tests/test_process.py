from __future__ import annotations

import ctypes
import os
import sys
import time
import unittest

from ctypes import wintypes

from th03_replay_video_gui.process import RendererProcess


class RendererProcessTests(unittest.TestCase):
    def test_terminate_stops_owned_child(self) -> None:
        child = RendererProcess([sys.executable, "-c", "import time; time.sleep(30)"])
        child.terminate()
        deadline = time.monotonic() + 5
        while child.process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.02)
        self.assertIsNotNone(child.process.poll())
        child.wait()

    @unittest.skipUnless(os.name == "nt", "Windows Job Object contract")
    def test_terminate_stops_ffmpeg_like_descendant(self) -> None:
        script = (
            "import subprocess,sys,time; "
            "p=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)']); "
            "print(p.pid,flush=True); time.sleep(30)"
        )
        child = RendererProcess([sys.executable, "-c", script])
        descendant = int(next(child.lines()))
        child.terminate()
        child.wait()

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        kernel32.OpenProcess.restype = wintypes.HANDLE
        kernel32.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
        kernel32.WaitForSingleObject.restype = wintypes.DWORD
        kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
        kernel32.CloseHandle.restype = wintypes.BOOL
        handle = kernel32.OpenProcess(0x00100000, False, descendant)
        if handle:
            try:
                self.assertEqual(kernel32.WaitForSingleObject(handle, 5000), 0)
            finally:
                kernel32.CloseHandle(handle)


if __name__ == "__main__":
    unittest.main()
