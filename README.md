# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. Phase 2's complete TH3R03 round remains a permanent
gate, and Phase 3 now verifies 19 locked V13 conversions across Story,
Practice, and ordinary VS, plus a separate title-Demo gate, on top of the Phase
1 format/session runway. A
strict V13 span adapter and the first stdlib-only asset/indexed-raster
foundations now sit beside that core. There is intentionally no playable
application, scene renderer, audio target, networking, or CI configuration.

## Build and test

Requirements are CMake 3.25 or newer and a C++23 compiler. There are no
third-party dependencies.

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
- `th03_app_runtime` owns dependency-free host policy: explicit rational
  logical deadlines, timestamped two-player input accumulation, exact
  SourceSurface-to-RGBA expansion into caller storage, and centered integer
  640x400 letterboxing. It assumes no source cadence, SDL type, key binding,
  window, or replay UI; those remain application/platform decisions.
- `th03_app_resources` owns the checked external-content root and immutable
  byte cache. Original/extracted files remain local inputs and never enter the
  repository.
- `th03_app_content` owns authenticated process-lifetime content that is not a
  standalone archive resource: compiled MAIN score/pellet/text tables, the
  prepared GAMEFT gaiji bank, and the external PC-98 ANK/JIS font provider.
  Production MAIN import accepts only the complete supported executable
  profile; default tests use synthetic bytes and optional local gates remain
  byte-silent.
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
