# TH03 Replay Renderer

TH03 Replay Renderer is a small Windows desktop frontend for the authoritative
`th03-replay-video` exporter. It never implements gameplay, audio, or encoding
itself: every job exposes the exact CLI command and consumes the renderer's
newline-delimited `T3RVP001` progress stream.

The initial interface selects a finalized V14 replay, extracted game-data
directory, MKV destination, 8:5 resolution, NVENC/software encoder, quality
preset, language, flash policy, and independent 5%-step audio levels. Installed
`PODD.CFG` values are inherited by default and explicit GUI values override
them field by field. Advanced inputs accept compiled `T3GTL001` in-game text,
`T3CTP001` scripted-dialogue, and `T3PTP001` picture-patch packs; the frontend
passes them through to the same authenticated renderer path used by the game.

The game-data directory is the same complete external content root accepted by
the CLI: it contains at least `MAIN.EXE`, `MAINL.EXE`, and `FONT.ROM`, plus the
authenticated archives/resources listed by the renderer when any are missing.
It is not the single-file standalone executable itself.

FFmpeg is discovered in this order: an explicitly selected path, a licensed
binary embedded by `build.ps1`, a side-by-side `ffmpeg.exe`, then `PATH`.
The normal package does not silently redistribute the developer's local
FFmpeg. Pass both `-FfmpegPath` and `-FfmpegLicensePath` to make an explicitly
licensed bundled build.

Development:

```powershell
$env:PYTHONPATH = "$PWD\tools\th03-replay-video-gui\src"
python -m unittest discover -s tools\th03-replay-video-gui\tests -v
python -m th03_replay_video_gui
```

Package next to a maintained Release renderer:

```powershell
tools\th03-replay-video-gui\build.ps1 `
  -RendererPath out\build\msvc-x64-sdl\Release\th03-replay-video.exe
```

The packaged GUI executable is also staged at
`out/standalone/TH03ReplayRenderer.exe` so it stays next to the current game
and CLI-renderer test builds. The versioned directory and ZIP remain the
redistributable package with notices and licenses.

Cancellation owns the renderer and all descendants in a Windows Job Object,
so FFmpeg cannot remain running after the GUI reports the job cancelled. The
CLI renders into a same-directory temporary file and atomically publishes only
after both deterministic passes succeed, preserving an existing destination on
cancel or failure.
