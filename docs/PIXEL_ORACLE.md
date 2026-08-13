# Classic / New Classic pixel verification

The port has two maintained host-side presentation oracles and one deliberately
separate PC-98 raw-capture plan. The compact state stream and exact published
RGB stream are implemented and automated. Lossless DOSBox-X video can be
normalized into the same exact-frame comparator. The publication-aligned guest
producer is the remaining emulator-instrumentation parcel; this document does
not claim pixel parity until that producer aligns the two timelines.

## Host-publication fingerprints (`T3PXH2`)

Set `TH03_PIXEL_ORACLE_LOG` to an ignored output path before launching
`th03-app`. Every complete surface successfully published through SDL is
recorded as one TSV row:

```powershell
$env:TH03_PIXEL_ORACLE_LOG = "$PWD\.scratch\pix\classic-msvc.t3pxh"
.\out\build\msvc-x64-sdl\Release\th03-app.exe --classic <normal arguments>
```

The exact fourteen-column schema is:

```text
stream schema publication logical_tick coalesced_ticks overlay reduced_flash source_fnv64 raster_fnv64 tram_fnv64 shown_page raster_mode scroll_y tone
```

Fields are tab-separated. `stream=T3PXH` and `schema=2` identify this contract.
`publication` is contiguous host output order. Source-driven publications also
carry their rational scheduler `logical_tick`; product-owned or initial host
surfaces use `na`. While capture is enabled, the application publishes each
surface-changing logical tick separately instead of allowing wall-clock
catch-up to hide an intermediate source publication. `coalesced_ticks` records
how many changed logical surfaces the publication represents and is currently
one for those source rows.

`source_fnv64` fingerprints the complete published presentation state, not
only final RGBA pixels. `raster_fnv64` and `tram_fnv64` separately fingerprint
guest Raster and visible TRAM state; host-only surfaces use `na`. `overlay`
marks product or Replay Patch host drawing over a source surface, and
`reduced_flash` states whether New Classic's final reduced-flash policy was
applied. Overlay surfaces retain their base surface's page, raster, scroll,
tone, and guest hashes, so mixed-domain changes remain attributable.

The Raster/TRAM hashes are computed only while an SDL oracle owner is active;
normal shipping runs pay no full-surface hash cost.

Compare captures with:

```powershell
pwsh -NoProfile -File tools\compare_pixel_oracle.ps1 `
  -Base .scratch\pix\classic-gcc.t3pxh `
  -Candidate .scratch\pix\classic-msvc.t3pxh `
  -ReportPath .scratch\pix\classic-compiler-diff.json
```

The comparator requires the exact header and schema, equal-width rows,
contiguous ordinals, symmetric `na` fields, and equal source identity. It
normalizes hexadecimal letter case, reports the first differing field and
logical tick, and rejects unexplained prefixes or suffixes. Deliberately
bounded prefix comparisons must opt into `-AllowCandidateSuffix` or
`-AllowBaseSuffix`. Even malformed inputs produce a BOM-free structured JSON
report with `status=malformed`.

`tests\test_compare_pixel_oracle.ps1` freezes case normalization, asymmetric
`na`, length and suffix policy, structured malformed output, and UTF-8 encoding.
The SDL platform test freezes the producer's exact header and row identity.
An authentic 80-tick no-input Classic gate additionally produced byte-identical
81-row logs across two GCC runs and between GCC and MSVC Release on 2026-08-12.

This layer is useful for:

- repeat-run and GCC-versus-MSVC presentation determinism;
- locating the first Classic/New Classic source-state or host-overlay change;
- separating Raster, TRAM, selector, reduced-flash, and composition faults;
- driving an interactive or replay-locked capture while preserving source tick
  identity.

It is intentionally not called collision-proof pixel evidence. A differing
field decisively proves a published-state difference, but a finite hash match
does not prove equality of every pixel. Use the same content, seed, language,
reduced-flash policy, input/replay, and audio-status timeline on both sides.

## Exact host RGB publications (`T3RGB1`)

Set `TH03_PIXEL_ORACLE_RGB_DIR` to a new ignored directory before launching
`th03-app`. Each successful SDL publication is committed as one binary PPM
(`P6`, 640x400, RGB24) plus one manifest row. The bytes are copied from the
completed 640x400 conversion buffer before SDL scaling or texture upload, so
window size, monitor scaling, GPU backend, and screenshot timing do not enter
the evidence.

```powershell
$env:TH03_PIXEL_ORACLE_RGB_DIR = "$PWD\.scratch\pix\classic-rgb"
.\out\build\msvc-x64-sdl\Release\th03-app.exe --classic <normal arguments>
```

`T3RGB1.tsv` carries the same publication/tick/overlay/page/palette identity as
`T3PXH2`, the FNV-1a/64 of the actual RGB payload, and the relative PPM path.
Frames are written to a temporary file before present and renamed only after a
successful `SDL_RenderPresent`; failed publications cannot create manifest
gaps or stale accepted frames. The output directory must be empty so
independent captures cannot be mixed.

Compare two exact streams with:

```powershell
python tools\compare_pixel_frames.py `
  --base .scratch\pix\classic-rgb\T3RGB1.tsv `
  --candidate .scratch\pix\new-classic-rgb\T3RGB1.tsv `
  --report .scratch\pix\classic-vs-new-classic.json `
  --diff-directory .scratch\pix\classic-vs-new-classic-diff
```

The comparator validates every declared payload hash and compares the RGB
bytes themselves. On the first mismatch it reports the differing-pixel count,
first `(x,y)` and both RGB triples, bounding box, maximum channel delta, and
mean absolute channel delta; optional PPMs retain the base, candidate, and
amplified absolute diff. Consecutive duplicate scanouts are collapsed by
default so an emulator that submits unchanged frames can be compared with the
port's change-only publication stream. Prefix/suffix comparisons require an
explicit flag.

The exact stream is intentionally opt-in: one frame occupies 768,015 bytes,
so a long full-game capture belongs under ignored scratch storage.

## Lossless emulator-video ingestion (`T3EMU1`)

Both `tools\emu\run_replay.ps1` and `run_t3case.ps1` accept `-VideoCapture`.
The switch requires source-rate rendering (no `-Turbo` or force-stop split
target), forces the visible SDL renderer even for unattended runs, and asks the
pinned DOSBox-X build for ZMBV AVI capture around the guest command. Capture
artifacts are SHA-256 inventoried beside the ordinary oracle outputs.
Pass the same authenticated `FONT.ROM` used by the native run with
`-FontRomPath`; otherwise DOSBox-X may silently fall back to FREECG98 and make
all guest TRAM text pixels incomparable.

Normalize an AVI without scaling or lossy conversion:

```powershell
python th03-port\tools\import_emulator_video.py `
  --video .scratch\emu\t3case\<run>\capture\command_000.avi `
  --output .scratch\pix\guest-th3r03
```

The importer requires an exact 640x400 stream, decodes losslessly through
`ffmpeg`, verifies every PPM, collapses consecutive duplicate scanouts, and
writes `T3EMU1.tsv` plus source metadata and SHA-256. `compare_pixel_frames.py`
accepts `T3EMU1` and `T3RGB1` on either side.

The wrappers require an AVI to exist before reporting capture success. They
record `video_capture_status=ready|missing|prepared` in `run-info.json`, append
`EXIT` after the synchronous `DX-CAPTURE` command so a completed guest command
closes the emulator cleanly, and report `result=capture-missing` if the selected
DOSBox-X binary silently accepts `DX-CAPTURE` without an encoder. The two
installed 2026.05.02 MinGW binaries on the development workstation currently
take that `missing` path; a scratch build whose configure report explicitly
enables screenshots/movie recording is being used for the external gate.

This provides a useful review and first-divergence mechanism, but an AVI frame
still lacks an authenticated TH03 publication identity. It must not be aligned
by guessed wall-clock time and called parity. The raw guest producer below is
what turns the RGB comparison into a source-boundary gate across title, menus,
selection, MAINL, gameplay, result, and ending scenes.

## Raw PC-98 oracle (`T3PIX1`)

The authoritative source-side producer is implemented in the companion ReC98
authority tree as a devtools-only profile. It builds separate
`bin/th03/pixcap/{op,main,mainl}.exe` binaries and writes a versioned raw
capture into an emulator-mounted host directory. Release and combined-debug
OP/MAIN/MAINL remain byte-identical to the preserved 0.4.11 artifacts. The
frozen stream records, at matching source publication acts:

- all four planar VRAM planes for the shown page, plus selected hidden-page
  diagnostics;
- shown/access page, 400/200-line raster mode, vertical scroll, and raw tone;
- applied and pending palettes plus the deferred-palette flag;
- raw TRAM code/attribute words; and
- process, scene, publication, logical-sample, and gameplay-frame identities.

Dense rows will contain fixed scalar state and FNV-1a/32 layer hashes. Selected
ordinals will additionally contain raw planar/TRAM payloads, allowing an
offline comparator to localize the first mismatch to a plane byte and physical
pixel. Indexed and RGBA review images are derived artifacts; raw planar,
palette, TRAM, and selector equality are the actual gate.

The completed source parcel includes the strict `tools/port/t3pix1.py`
parser/control writer, planar PPM renderer, synthetic tests, and the frozen
`state/port/T3PIX1_FORMAT.md` contract. The remaining acceptance sequence is:

1. run the pixcap OP/MAINL/MAIN binaries at Replay Patch 0.4.11 publication
   boundaries, then add the equivalent stock/Anniversary profiles;
2. run locked no-input and V13 cases through `tools\emu`, correlate each raw
   guest publication with the lossless scanout frame, and compare against
   `T3RGB1`; and
3. track only identities, SHA-256 digests, verdicts, and first-divergence
   metadata, never original capture payloads.

Guest instrumentation must remain devtools-only on a disposable branch. It
must not enter the shipping Replay Patch executable or become implementation
authority. Runtime guest acceptance is deliberately still open: it must use a
silent/headless Turbo emulator route, and final TRAM composition still needs
the authenticated FONT.ROM/gaiji provider. `T3PXH2` plus `T3RGB1` remain the
practical daily regression mechanisms until that external matrix is captured.

## Why not screenshots?

An emulator screenshot is not reliably tied to a source page/palette
publication act, and host RGB conversion mixes several independent domains.
Screenshots remain useful for human review, but cannot identify whether the
first fault was VRAM, TRAM, a palette apply, page selection, scroll, or the host
converter. The layered contract preserves those distinctions.
