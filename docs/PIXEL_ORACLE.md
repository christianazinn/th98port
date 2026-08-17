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

## Typed route/cadence sidecar (`T3ALI1`)

Set `TH03_PIXEL_ORACLE_IDENTITY_LOG` together with either host publication
oracle and provide the authoritative 64-hex route digest in
`TH03_PIXEL_ORACLE_FIXTURE_SHA256`. The sidecar records every native source
boundary, including unchanged boundaries whose `publication` is `na`. Ordinary
changed boundaries are written only after the matching SDL publication
succeeds; an exact sparse capture additionally records deliberately skipped
changed boundaries with `publication=na`.
The frozen columns are:

```text
stream schema event fixture_sha256 process_instance process scene scene_visit logical_sample replay_global_frame round_frame commit boundary source_changed publication
```

`process_instance` advances on every OP/MAINL/MAIN executable transition,
including a resumed OP. `scene_visit` disambiguates repeated menu visits;
`commit` records native publication cadence for diagnostics and is not a
cross-runtime identity. The fixture digest must cover the selected replay/route and checkpoint plus content,
configuration, language, score, and presentation-policy authorities; it is
repeated on every row so unrelated captures cannot be joined accidentally.

The current strict host join admits one raw-selected PC-98 `show-page/state`
publication per stream, plus one narrowly typed MAINL stage-splash
`fade-step/commit`, and fails closed on duplicate source identities. It
joins fixture, process/scene ownership, scene visit, logical sample,
replay-global frame, and round frame; transaction and commit ordinals are
deliberately excluded because the two runtimes publish at different internal
event cadences.

Identity belongs to the source owner that produced the published surface.
Ordinary MAIN advances retain the pre-sample replay identity, an in-consume
transition that exposes an incoming startup surface uses the incoming owner
with a `scene_enter` boundary, and terminal surfaces retain the outgoing owner.
Fast-forward retains the last changed substep. Focused runner/session
regressions freeze the first three page/identity pairs and a Title-to-Options
ownership transition. The aligner rejects bootstrap rows and normally rejects
scene-entry rows as gameplay source ticks. An attested semantic nonzero
checkpoint start instead joins Replay Patch's pre-input accelerator reveal to
the incoming native MAIN `scene_enter`; exact-ordinal and checkpoint-zero
captures remain source-tick-only. Every path fails closed on ambiguity under
the selected key.

For a bounded unattended New Classic capture, pass an exact V13 artifact and
optional checkpoint. The explicit smoke bound is mandatory:

```powershell
th03-app --new-classic <normal arguments> `
  --capture-replay .scratch\route.rpy --capture-checkpoint 10 `
  --smoke-ticks 2000
```

This starts the shared authenticated replay owner directly, without synthetic
UI input, and creates SDL hidden so unattended capture cannot steal focus.
T3PXH2 and T3RGB1 remain unchanged; T3ALI1's successful
`publication` ordinal is the only join to those streams.
Later-round capture reconstructs target semantics at host speed, restores the
anchor MAIN Raster/palette and the T3RACC1 page roles in constant time, then
rebases the host clock before the first visible replay sample. It deliberately
does not reproduce PC-98 decompression latency. The locked TH3R06 corpus covers
its accelerated Stage 8 and Stage 9 Round 2/3/4 entries; their selected reveal
surfaces and following source ticks must pass the independent T3PIX2/T3ALI1
gate before route coverage is claimed.

Long replay routes can select one exact native publication without encoding
every preceding RGB frame. Set `TH03_PIXEL_ORACLE_SELECTOR` to this fixed
pipe-delimited identity:

```text
process_instance|process|scene|scene_visit|logical_sample|
replay_global_frame|round_frame|commit|boundary
```

For example, checkpoint 10's first active frame is
`2|main|gameplay|0|133777|133777|1|0|source_tick`. Optional numeric fields use
the literal `na`. The selector is valid only with direct replay capture,
T3ALI1, and a publication oracle. It runs deterministic replay simulation
unthrottled, does not open or feed an audio device, records every skipped
source boundary in T3ALI1 with `publication=na`, and emits exactly one
T3PXH2/T3RGB1 publication before exiting. Malformed, stale, ambiguous, or
unreached selectors fail closed. This changes capture cost, never guest/native
source timing or the visible frame chosen for comparison.

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

## Raw PC-98 oracle (`T3PIX2`)

The authoritative source-side producer is implemented in the companion ReC98
authority tree as a devtools-only profile. It builds separate
`bin/th03/pixcap/{op,main,mainl}.exe` binaries and writes a versioned raw
capture into an emulator-mounted host directory. Release and combined-debug
OP/MAIN/MAINL remain byte-identical to the preserved 0.4.12 artifacts. The
frozen stream records, at matching source publication acts:

- all four planar VRAM planes for the shown page, plus selected hidden-page
  diagnostics;
- shown/access page, 400/200-line raster mode, vertical scroll, and raw tone;
- applied and pending palettes plus the deferred-palette flag;
- raw TRAM code/attribute words and the full mutable 8,192-byte gaiji bank; and
- process instance, scene visit, event/boundary, commit ordinal, logical
  sample, replay-global frame, and round-frame identities.

Dense rows contain fixed scalar state and FNV-1a/32 layer hashes. Selected
ordinals additionally contain raw planar/TRAM/gaiji payloads, allowing an
offline comparator to localize the first mismatch to a plane byte and physical
pixel. Indexed and RGBA review images are derived artifacts; raw planar,
palette, TRAM, and selector equality are the actual gate.

The completed source parcel includes the strict `tools/port/t3pix1.py`
dual-version parser/control writer, planar PPM renderer, synthetic tests, and
the frozen `state/port/T3PIX2_FORMAT.md` contract. T3FIX1 hashes the exact
size-changing PIXCAP OP/MAINL/MAIN binaries separately from the Replay Patch
release authority named in the stream header. The supported runner verifies
those staged role/length/SHA-256 declarations before accepting a stream.

Runtime acceptance covers OP, MAINL, and MAIN. Silent, hidden, Turbo/max
single-selection runs for guest MAIN publications 3, 4, and 5 join T3ALI1
native `source_tick` events without ordinal or pixel inference. Publication 3
matches full Raster, TRAM, SourceSurface, and RGB state. Publications 4 and 5
match source composition, TRAM, and every output RGB byte; their separate
full-Raster mismatch is confined to the undisplayed lower half of zoomed
200-line VRAM. Guest publication 2 is not accepted: its only same-counter
native surface is the product bootstrap (`logical_tick=na`), which the aligner
explicitly excludes from source-tick joins.

Guest instrumentation remains devtools-only on the dedicated
`codex/t3pix-main-stack-fix` branch. It must not enter the shipping Replay
Patch executable or become implementation authority. Strict guest/native route
acceptance remains open because the selected-publication corpus does not yet
cover every gameplay route and OP/MAINL menu state. The separate complete-VRAM
policy for undisplayed lower-half state also remains explicit. `T3PXH2` plus
`T3RGB1` remain the practical daily regression mechanisms while that external
matrix expands.

## Why not screenshots?

An emulator screenshot is not reliably tied to a source page/palette
publication act, and host RGB conversion mixes several independent domains.
Screenshots remain useful for human review, but cannot identify whether the
first fault was VRAM, TRAM, a palette apply, page selection, scroll, or the host
converter. The layered contract preserves those distinctions.
