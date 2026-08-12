# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. Phase 2's complete TH3R03 round remains a permanent
gate, and Phase 3 now verifies 19 locked V13 conversions across Story,
Practice, and ordinary VS, plus a separate title-Demo gate, on top of the Phase
1 format/session runway. A
strict V13 span adapter and the first stdlib-only asset/indexed-raster
foundations now sit beside that core. An optional SDL 3 application now launches
both the integrated authenticated Classic OP/MAINL/MAIN lifecycle and the earlier
direct-VS developer path. A separate byte-fed PMD/YM2608 engine and TH03
transport/SFX service, bounded `YUME.EFC` playback on FM6, deterministic
166.4-to-48 kHz resampling, and a bounded SDL playback endpoint are now present,
and a platform-neutral Classic session now executes the existing semantic scene
commands over authenticated external content. The Classic launcher binds that
session to the SDL endpoint at one deterministic audio interval per logical
sample. Initial or later device unavailability drops only final delivery while
the deterministic PMD session keeps advancing.
Gameplay-stage music, source-ordered ordinary MAIN SFX emission, and stock
blocking live pause/resume with effect 21 are integrated. The Replay Patch's
richer pause menu and replay UI remain intentionally deferred.

## Build and test

Requirements are CMake 3.25 or newer and a C++23 compiler. The default library
and validation presets have no third-party dependencies.

```powershell
cmake --preset msvc-x64
cmake --build --preset msvc-x64
ctest --preset msvc-x64

cmake --preset gcc
cmake --build --preset gcc
ctest --preset gcc
```

The MSVC preset uses Visual Studio 2022 x64. The GCC preset uses the standard
`MinGW Makefiles` generator.

### Optional Classic and direct-VS application

The SDL target is off by default. Authenticate and extract the pinned SDL
3.4.10 release archive once, then use either SDL preset:

```powershell
tools\bootstrap_sdl3.ps1 -Archive X:\downloads\SDL3-3.4.10.tar.gz
cmake --preset msvc-x64-sdl
cmake --build --preset msvc-x64-sdl
ctest --preset msvc-x64-sdl
```

`th03-app` reads an external structured extraction containing `YUME1.DAT/`
and `YUME2.DAT/`, a supported Japanese `MAIN.EXE` compiled-content profile,
supported unpacked Japanese `MAINL.EXE` and `OP.EXE` profiles, and a
caller-supplied 288,768-byte PC-98 `FONT.ROM`, and optionally a caller-supplied
8,192-byte YM2608 ADPCM-A rhythm image. No
original bytes are embedded or copied into the build. Classic and direct VS
default to the nominal PC-9801 24 kHz timing model, reduced exactly to
`526315/9328` Hz (56.423134648... Hz). `--tick-rate` remains an optional
developer/host override:

```powershell
out\build\msvc-x64-sdl\Debug\th03-app.exe `
  --classic --data X:\th03-extracted `
  --main X:\th03\MAIN.EXE --mainl X:\th03\MAINL.EXE `
  --op X:\th03\OP.EXE --font X:\pc98\FONT.ROM `
  --rhythm-rom X:\pc98\YM2608.ROM

out\build\msvc-x64-sdl\Debug\th03-app.exe `
  --vs --data X:\th03-extracted --main X:\th03\MAIN.EXE `
  --font X:\pc98\FONT.ROM `
  --p1 reimu:first --p2 mima:second
```

The rational comes from the standard 21,052,600 Hz dot clock divided by 8,
106 horizontal character periods, and 440 total lines, independently modelled
by DOSBox-X 2026.05.02 and NP2kai and consistent with PC-9801 monitor timing
tables. It is exact for the nominal model, not a claim of zero physical
oscillator tolerance. The first logical sample is immediately due, matching
the source's no-pre-wait process entry; later samples use rational deadlines.
The pinned source and timing evidence is recorded in
[`docs/research/PC98_CADENCE.md`](docs/research/PC98_CADENCE.md).
Kotohime additionally requires the explicit developer flag
`--kotohime-zero-red` until her source-uninitialized Bomb fade byte has an
evidence-backed release policy. The Classic path always executes semantic
commands through the authenticated PMD/OPNA/EFC session and fixed resampler;
SDL availability controls only bounded final playback. Music Off follows fresh
MAIN/MAINL process behavior and disables both music and FM effects. An exact
8 KiB `--rhythm-rom` input is copied into the deterministic engine; omitting it
selects explicit silent-rhythm mode. The repository does not supply or download
that data, so this is not yet a complete redistributable audio-fidelity claim.
`--help` lists
keyboard controls and all launch options.

Validate an external frozen case without copying it into this repository:

```powershell
out\build\msvc-x64\Debug\th03-replay-check.exe --case X:\path\TH3R03.T3C
out\build\msvc-x64\Debug\th03-headless.exe --case X:\path\TH3R03.T3C
```

Verify a V13 replay directly against its embedded round summaries:

```powershell
out\build\msvc-x64\Debug\th03rv.exe X:\path\TH3R00.RPY
```

`th03rv` locates `ENEDAT.DAT` beside the replay or in a development harness.
For a standalone installation, pass `--enedat <file>` or set `TH03_ENEDAT`.
It runs the neutral replay session at unbounded speed, prints every computed
round split, and reports whether all embedded summaries match.

Run the complete externally stored converted V13 corpus, including its frozen
size/SHA-256 lock, with:

```powershell
tools\verify_replay_corpus_external.ps1 `
  -EvidenceRoot X:\path\to\rec98-harness `
  -V13CorpusRoot X:\path\to\converted-v13
```

The current V13 lock contains 19 files converted by TH03 Data Manager 0.4.2
from the frozen six-file V11 and thirteen-file V12 inputs. It contains 173
embedded round summaries and 152 preserved checkpoints across Classic Story,
Replay Patch Practice, and ordinary VS. All 19 complete at unbounded speed and
match every summary and stored checkpoint on GCC and MSVC. Historical replay
files are converted outside this repository; the native V11/V12 parsers are
retired.

Pass the calibrated ENEDAT and frozen Anniversary split to execute the Phase 2
gate:

```powershell
out\build\msvc-x64\Debug\th03-headless.exe `
  --case X:\path\TH3R03.T3C `
  --enedat X:\path\ENEDAT.DAT `
  --oracle X:\path\T3SPLIT.BIN
```

The first MAIN process remains the stable Phase 2 contract: 46 rows, 6,773
logical samples, route 2, and final scores `01090503`/`00434365`. Phase 3 now
continues across process and repeat-round boundaries. TH3R00 matches 715/715
rows, TH3R01 964/964, TH3R02 626/626, TH3R03 186/186, TH3R04 627/627, and
TH3R05 783/783 through their natural terminal boundaries.

Run the complete external Phase 0 lock with:

```powershell
tools\verify_phase0_external.ps1 -EvidenceRoot X:\path\to\rec98-harness
```

The evidence root must contain the ignored six-file V11 corpus, normalized
T3CASE cases, and final split artifacts. The script checks frozen SHA-256 and
sample counts, parses all twelve replay documents, and reruns all twelve native
split comparisons. Original game data and replay captures remain external.

## Boundaries

- `th03_core` owns deterministic state and stepping. It has no filesystem,
  platform, rendering, audio, or network dependency.
- `th03_replay` owns recorded pause/process coordination at unbounded speed.
- `th03_formats` owns byte-level formats and adapts them to a neutral
  `ReplaySession`. It operates on byte spans and performs no file I/O.
- `th03_assets` decodes caller-owned RGB, PI, CDG/CD2, BFNT, and MRS byte spans
  into immutable models. Original files, paths, caches, and platform resources
  remain outside the library.
- `th03_present` owns the PC-98 page, planar, palette, TRAM, cel, Sprite16
  staging/drawing, prepared BFNT superimpose resources, text composition,
  staff flakes, exact GRCG line/circle primitives, and the source-ordered
  MAIN presenter: backgrounds, Bosses, Bombs, shots, enemies, fireballs,
  hit circles, Extra Attacks, Charge Shots, players, common effects, bullets,
  Warning/Gauge, dynamic HUD, scores, combos, result banners, and result
  transitions. It depends inward on `th03_assets` and the neutral observation
  API in `th03_core`; the core has no reciprocal dependency, and `th03rv`
  links neither presentation nor assets. Captured PC-98 pixel parity remains a
  separate external-data acceptance gate.
- `th03_app_runtime` owns dependency-free host policy: caller-selected rational
  logical deadlines, timestamped two-player input accumulation, exact
  SourceSurface-to-RGBA expansion into caller storage, and centered integer
  640x400 letterboxing. Its scheduler assumes no source cadence, SDL type, key binding,
  window, or replay UI; those remain application/platform decisions.
- `th03_app_main` also contains the source-private owning Classic Story route
  over Stage, retained MAIN, Win, Continue, Game Over, both interstage
  cutscenes, the primary character endings, and the staff/verdict scene. The
  owning cutscene scene binds
  the complete typed script VM and exact Master/upstream two-page Raster
  presenter to authenticated script/PI resources, explicit source input and
  wait boundaries, and semantic audio commands. Its completed Raster is
  transactionally handed to the next Stage or to the authenticated staff
  driver. Staff preserves the source CDG/flakes/page schedule, exact volume and
  measure waits, adaptive 80-to-50 downgrade, verdict input lock, and final
  tone ramp before returning its Raster. The same owner now runs the complete
  MAINL high-score registration transaction over that retained Raster: exact
  qualification/name-grid input, page-1 unblits, source RNG codec fields,
  conditional audio fade, atomic score commit, route-selected next-screen
  preload, and the conditional post-registration extra ending. The complete
  Classic Story route is now internally owned; no second campaign or
  resident-state model is exposed. The same source-private app owner now
  composes the OP title, retained Options and Music Room, read-only HiScore,
  Story/all-VS character selection, Story, MAINL/MAIN VS, winner return, and
  the exact four-recipe automatic Demo loop. Every source OP route is now
  internally owned without adding replay UI.
- The same boundary authenticates the supported unpacked Japanese `MAINL.EXE`
  and owns its exact Story title/name table, staff verdict strings, and flake
  masks. Packed DIET executable support is not yet provided; original bytes
  remain external and uncommitted.
- `th03_app_resources` owns the checked external-content root and immutable
  byte cache. Original/extracted files remain local inputs and never enter the
  repository.
- `th03_app_persistence` owns exact, allocation-free Classic persistent-value
  codecs, source mutation rules, and checked `YUME.NEM` / `YUME.CFG`
  filesystem transactions. It preserves all four independently encrypted
  rank sections, the encrypted checksum, explicit codec keys, stock decoy
  rows, tie-sinking insertion, and backwards name storage. Missing or
  malformed files reproduce the source's all-rank stock recovery, while
  commits use one same-directory, flushed, read-back-validated atomic
  replacement. It draws no registration UI and receives codec fields rather
  than owning gameplay RNG; the owning MAINL registration scene supplies both
  at the source-ordered commit boundary. The configuration codec also
  preserves the exact eight-byte Classic layout while native writes zero
  DOS-only resident/debug/unused metadata.
- `th03_app_content` owns authenticated process-lifetime content that is not a
  standalone archive resource: compiled MAIN score/pellet/text tables, the
  prepared GAMEFT gaiji bank, and the external PC-98 ANK/JIS font provider.
  Production MAIN import accepts only the complete supported executable
  profile; default tests use synthetic bytes and optional local gates remain
  byte-silent.
- `th03_app_main` owns one stable-address MAIN process plus source-private
  Story stage, winner, Continue, and Game Over scene boundaries. It
  reauthenticates and decodes the selected external batch, performs source-
  ordered background/static/palette/atlas startup, retains every presenter and
  backing span, and guards all Game lifecycle calls through MainRound. Its
  platform-neutral runner is used by the optional `th03-app` executable.
- `th03_platform_sdl` is the optional pinned SDL 3 lifetime, input-event,
  timestamp, and indexed-source-surface upload boundary. It owns no gameplay,
  resource decoding, cadence constant, or scene policy.
- MAIN palette primitives preserve the source's logical RGB8 values separately
  from applied RGB4 DAC output, including its process baseline, raw tone, and
  explicit deferred-apply latch. Neutral Bomb/Warning/defeat/footer lifecycle
  facts keep this ordering out of deterministic gameplay state.
- `th03_core` exposes an optional synchronous observer for immutable gameplay
  facts and borrowed source-phase views. Transient lifecycle facts preserve shot
  cel reset, player pose/defeat ordering, Bullet16 construction, hit-circle
  allocation, common-effect allocation/advance timing, and Bomb/Warning/footer
  scheduling. Later views and facts preserve the literal Boss, Extra/Charge,
  result, Gauge, score/extend, HUD-start, combo, and transition order without
  adding visual state to gameplay. The pointer is borrowed per call and never enters
  deterministic state; `th03rv` uses the null path.
- `th03-headless`, `th03-replay-check`, and `th03rv` are independent
  filesystem/CLI clients of those libraries.
- V13 produces the same neutral document/session types without changing the
  core or runner. Its structural tests and converted 19-file corpus gate are
  green on both compilers. Later compact checkpoints and their raw V13 reset
  recipes are preserved in the neutral document, but complete-state comparison
  and application-side selection remain a separate preroll acceptance gate.

See `docs/ARCHITECTURE.md`, `docs/PARITY_SOURCE_COVERAGE.md`,
`docs/REPLAY_VERIFICATION.md`, `docs/ORACLE_CONTRACT.md`, and
`docs/SOURCES.lock` before expanding corpus and mode coverage.
`docs/FRAME_PARTITIONS.md` describes the behavior-free implementation split
inside the gameplay frame translation unit.
`docs/V13_INTEGRATION.md` records V13 authority and the historical-format
retirement gate. `docs/POST_CORE_ROADMAP.md` records the active-tip branch
workflow and the asset, presentation, audio, platform, and application layers
that follow the deterministic gameplay core. `docs/PLAYABLE_PORT_ROADMAP.md`
turns those boundaries into the acceptance-gated path to the first direct-launch
local VS build and then the remaining source scenes.
`docs/research/README.md` indexes the presentation, asset, and audio research
contracts prepared for those later layers.
