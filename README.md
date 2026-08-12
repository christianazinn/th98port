# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. Phase 2's complete TH3R03 round remains a permanent
gate, and Phase 3 now verifies 19 locked V13 conversions across Story,
Practice, and ordinary VS, plus a separate title-Demo gate, on top of the Phase
1 format/session runway. A
strict V13 span adapter and the first stdlib-only asset/indexed-raster
foundations now sit beside that core. An optional SDL 3 application now opens a
shared host menu for the authenticated Classic lifecycle and the developing
Replay Patch 0.4.10-derived New Classic experience, alongside the earlier
direct-VS developer path. Both product identities reuse the same gameplay,
presentation, content, and audio owners; title Exit returns to the host menu.
A separate byte-fed PMD/YM2608 engine and TH03
transport/SFX service, bounded `YUME.EFC` playback on FM6, deterministic
166.4-to-48 kHz resampling, and a bounded SDL playback endpoint are now present,
and a platform-neutral Classic session now executes the existing semantic scene
commands over authenticated external content. The Classic launcher binds that
session to the SDL endpoint at one deterministic audio interval per logical
sample. Initial or later device unavailability drops only final delivery while
the deterministic PMD session keeps advancing.
Gameplay-stage music, source-ordered ordinary MAIN SFX emission, and stock
blocking live pause/resume with effect 21 are integrated. New Classic's host
identity/lifecycle is present; its V13 browser, recording, richer pause menu,
Practice, KeyConfig, and remaining Replay Patch QoL are the active next port.

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

#### Maintained one-file Classic/New Classic test build

For ongoing human testing, the repository has a local-only packager that
rebuilds the current Release application, embeds its SDL runtime and the
caller's ignored original test content, and smoke-tests one self-contained
Windows x64 executable. Run it from PowerShell 7 so the pinned ZIP/runtime
pipeline produces one stable cache identity:

```powershell
tools\build_classic_standalone.ps1
```

With the standard ignored workspace layout, the stable result is
`out\standalone\TH03-Classic-Test.exe`. It can be copied elsewhere and launched
by double-clicking; it needs no adjacent DLL/content tree, installed .NET
runtime, or command-line arguments. It opens the shared Classic/New Classic
menu; both entries currently run the complete shared Classic lifecycle while
New Classic's Replay Patch-owned features are brought online incrementally.
Exiting either title returns to this menu. Its first launch extracts a content-hash
cache below `%LOCALAPPDATA%\TH03 Classic Native Test`, while configuration,
scores, and the last-run diagnostic log live in the separate `user` directory
there and survive artifact updates. New Classic V13 files live in its
`user\REPLAY` directory and likewise survive artifact updates.

The no-argument script is the maintained convenience path and should be rerun
after changes intended for human testing. It rebuilds and overwrites the stable
artifact rather than publishing versioned copies. Alternate ignored inputs can
be supplied with `-DataRoot`, `-MainExecutable`, `-MainlExecutable`,
`-OpExecutable`, `-FontRom`, and optional `-RhythmRom`. The packager also accepts
`-T98Sound86`; on this development machine the no-argument path automatically
recognizes the authenticated T98-Next 13.1th `DEVICE\Sound86.nhw`, extracts its
six decoded rhythm voices into the private payload, and never executes the
device module. The tracked launcher and packager contain no original bytes; the
generated executable does, so it remains
ignored and is strictly a private testing artifact pending the asset/license
and public-package work.

`th03-app` reads an external structured extraction containing `YUME1.DAT/`
and `YUME2.DAT/`, a supported Japanese `MAIN.EXE` compiled-content profile,
the untouched stock DIET-packed or authenticated unpacked Japanese
`MAINL.EXE` and `OP.EXE` profiles, and a
caller-supplied 288,768-byte PC-98 `FONT.ROM`, and optionally a caller-supplied
8,192-byte YM2608 ADPCM-A rhythm image or the authenticated decoded T98 profile.
No
original bytes are embedded or copied into the build. The repository does not
yet include a DAT extractor or stock-install bootstrap; `--data` must point at
the already extracted layout documented in
[`docs/MAIN_RESOURCE_MANIFEST.md`](docs/MAIN_RESOURCE_MANIFEST.md). Classic and direct VS
default to the nominal PC-9801 24 kHz timing model, reduced exactly to
`526315/9328` Hz (56.423134648... Hz). `--tick-rate` remains an optional
developer/host override:

```powershell
out\build\msvc-x64-sdl\Debug\th03-app.exe `
  --shell --data X:\th03-extracted `
  --main X:\th03\MAIN.EXE --mainl X:\th03\MAINL.EXE `
  --op X:\th03\OP.EXE --font X:\pc98\FONT.ROM `
  --rhythm-rom X:\pc98\YM2608.ROM

# Private T98-Next reference alternative to --rhythm-rom:
out\build\msvc-x64-sdl\Debug\th03-app.exe `
  --classic --data X:\th03-extracted `
  --main X:\th03\MAIN.EXE --mainl X:\th03\MAINL.EXE `
  --op X:\th03\OP.EXE --font X:\pc98\FONT.ROM `
  --rhythm-pcm X:\private\T98RHYTHM.PCM

# Developer bypasses for one experience without the shared menu:
# replace --shell with --classic or --new-classic.

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
SDL presentation independently requires interval-one VSync so a complete
source frame cannot be replaced mid-scanout on the native renderer. A host
display whose refresh rate differs from 56.423 Hz can still repeat logical
frames or skip intermediate catch-up frames, but that physical scheduling never
changes the PC-98 gameplay/PMD timeline. The audio endpoint starts paused after
the initial blocking present, receives a three-interval prefill, and only then
resumes. The maintained T98-Next profile consumes ordinary FM key edges
immediately, matching FMGEN and preventing same-sample PMD key-off/key-on pairs
from collapsing. Its SSG lane is bipolar and weighted by `725/2048`, and its FM
lane is scaled by `5/2`, before the signed-16-scale mix is promoted to the
declared Q31/S32 domain and resampled. These values come from driving the
authenticated Sound86 mixer over all 21 songs; they are a practical
emulator-reference policy, while
physical PC-98 analog gain remains a later capture gate.
Kotohime's source-uninitialized late Bomb local is normalized from the
contemporaneous fade byte, following adjacent Kana. Kana's generated path and
the normalized Kotohime output are `{2*fade, 2*fade, fade}`; stock Kotohime
instead stores `{2*stale, 2*stale, fade}`. This deterministic policy supports
ordinary routes but is not a claim about undefined PC-98 stack residue. The
Classic path always executes semantic commands through the authenticated
PMD/OPNA/EFC session and fixed resampler;
SDL availability controls only bounded final playback. Music Off follows fresh
MAIN/MAINL process behavior and disables both music and FM effects. An exact
8 KiB `--rhythm-rom` input is copied into the deterministic engine. The private
standalone builder can instead authenticate the supplied T98-Next Sound86 module
and package its six decoded 44.1 kHz voices for `--rhythm-pcm`. Omitting both
suppresses the YM2608 ADPCM-A decoder; zero-filled external data is not used as
silence because zero nibbles decode into a loud waveform. The repository does
not supply or download
that data, so this is not yet a complete redistributable audio-fidelity claim.
`--help` lists
keyboard controls and all launch options.
Unless overridden with `--config`, `--score`, and `--replay-root`, Classic
stores `YUME.CFG`, `YUME.NEM`, and New Classic's `REPLAY` directory under the
selected `--data` root. This avoids dependence on the launcher working
directory; use explicit writable paths when the extracted content root is
read-only.

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
- `th03_app_runtime` owns dependency-free host policy: the minimal
  Classic/New Classic/Exit product menu, caller-selected rational
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
  internally owned without adding replay UI. A supplied-original integration
  matrix drives the production owner through a Reimu clear, used-credit
  clear, Continue Yes/No, exhausted credits, MAIN route 0, score persistence,
  fresh-title return, and real Quit, plus all three VS modes through Win,
  reselection, route 0, and Cancel, while composing every changed surface and
  draining every audio command on GCC and MSVC. Each scripted terminal is
  substituted only after one real MAIN frame; the locked V13 corpus separately
  validates complete deterministic gameplay through natural terminal
  boundaries. A human full Story clear and two-controller VS match remain
  release QA rather than claims made by this automated route gate.
- The same boundary authenticates either untouched stock DIET-packed or exact
  unpacked Japanese `MAINL.EXE` and `OP.EXE`. Packed input is authenticated,
  boundedly reconstructed in memory, authenticated again as the known unpacked
  profile, and passed to the same exact Story/staff/Music Room importers.
  Original bytes remain external and uncommitted.
- The eight-byte Classic config codec still accepts the source MIDI value, but
  a production session normalizes it to Off. Stock TH03 ships no MIDI/MMD song
  set or MMD launch path, and the native engine must not play FM while the
  Options screen claims MIDI. Off/FM remain the supported runtime choices.
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
