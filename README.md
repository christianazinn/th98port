# libth03

`libth03` is the architecture-neutral deterministic gameplay core for a
faithful native TH03 port. Phase 2's complete TH3R03 round remains a permanent
gate, and Phase 3 now verifies 19 locked V13 conversions across Story,
Practice, and ordinary VS, plus a separate title-Demo gate, on top of the Phase
1 format/session runway. A
strict V13 span adapter and the first stdlib-only asset/indexed-raster
foundations now sit beside that core. An optional SDL 3 application now opens a
shared host menu for the authenticated Classic lifecycle and the faithful
Replay Patch 0.4.14-derived New Classic experience, alongside the earlier
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
identity/lifecycle, complete shared-owner V13 MAIN/MAINL
recording/browser/playback/save flow and final Splits/Clears/Timers telemetry,
source-rendered pause/restart, Practice setup, three-page KeyConfig, expanded
Options, title Extra unlock, and the complete authenticated Replay Patch
English profile are present. The source-parity audit includes physical-Z Staff/replay
acceleration and no-wait playback of recorded Pause control streams.
Replay Patch 0.4.12's Game Over handoff is also preserved: replay playback of
any recorded P2 win returns through MAINL directly to the patched title without
constructing registration, Continue, Game Over, or VS winner UI. Live recording
still runs the ordinary Game Over flow, and SCORE.M keeps its current playback,
fade, or stopped state while the in-game replay save transaction is open.
Completed Story playback likewise drains its recorded MAINL tail at the donor's
ending entry and returns directly to OP without replaying the ending, staff,
verdict, registration, or save flow.

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

The room-code/WebRTC and direct IPv4/Tailscale New Classic netplay technical
preview, including Windows/Arch setup and the direct fallback commands, is documented in
[`docs/NETPLAY_TESTING.md`](docs/NETPLAY_TESTING.md). Its synchronized Pause
uses each peer's ordinary Story controls and offers Resume or Forfeit; Forfeit
ends both previews without inventing a TH03
winner or publishing the incomplete in-memory replay. Completed matches,
Forfeit, peer departure/timeout, and deterministic proof mismatches now end on
an authenticated source-styled local report instead of disappearing directly
into console output. Missing or unreachable online-service configuration is
also a recoverable report; it cannot terminate the product from Quick Match,
Create Room, or Join Room. Online Match is a retained child of the current
New Classic title: OP.M continues through setup, waiting, and connection-error
screens, every nested Cancel returns one screen, and returning to the title
does not stop or restart the song. Once both character locks are accepted, the
authenticated match-ready surface stops OP.M immediately, plays the source
WIN.M round-clear jingle to its actual PMD end, and drains its submitted audio
tail. Only then does the ordinary VS MAINL character/stage splash run its
source fade/wait cadence before either peer constructs synchronized gameplay
frame zero.
Each peer's Story Autofire choice follows that peer into its negotiated P1/P2
lane; Left Shift contributes both Charge and Shot only when that local choice
is enabled. During Pause, the first ready peer retains the modal with cyan `Waiting
for Opponent`; both peers then see the synchronized cyan 3/2/1 countdown.

### Optional Classic and direct-VS application

The SDL target is off by default. The maintained SDL presets also build the
room-code WebRTC transport. Authenticate and extract the pinned SDL 3.4.10
release archive, libdatachannel 0.24.3 recursive source, and Mbed TLS 3.6.7
archive once, then use either SDL preset:

```powershell
tools\bootstrap_sdl3.ps1 -Archive X:\downloads\SDL3-3.4.10.tar.gz
tools\bootstrap_rtc.ps1 `
  -LibDataChannelSource X:\src\libdatachannel `
  -MbedTlsArchive X:\downloads\mbedtls-3.6.7.tar.bz2
cmake --preset msvc-x64-sdl
cmake --build --preset msvc-x64-sdl
ctest --preset msvc-x64-sdl
```

#### Maintained one-file PoDD Port test build

For ongoing human testing, the repository has a local-only packager that
rebuilds the current Release application, embeds its SDL runtime and the
caller's ignored original test content, and smoke-tests one self-contained
Windows x64 executable. Run it from PowerShell 7 so the pinned ZIP/runtime
pipeline produces one stable cache identity:

```powershell
tools\build_classic_standalone.ps1
```

With the standard ignored workspace layout, the stable result is
`out\standalone\PoDD-Port-Test.exe`. It can be copied elsewhere and launched
by double-clicking; it needs no adjacent DLL/content tree, installed .NET
runtime, or command-line arguments. Its NativeAOT launcher avoids carrying the
ordinary .NET runtime bundle. After the source-faithful startup it boots
directly into New Classic, which records/browses/plays V13 replays through the
shared typed MAIN/MAINL scene owners and owns Replay Patch's four-choice
gameplay pause/restart loop and two-page Practice setup. New Classic's System
Options can temporarily enter sealed Classic, open the persistent user-data
and active replay directories, show factual project/source credits, or open
the directory containing the last-run diagnostic log;
Classic Quit returns to a fresh New Classic
title, while New Classic Quit exits the application. Its first
launch extracts a content-hash
cache below `%LOCALAPPDATA%\PoDD Port`, while configuration,
scores, and the last-run diagnostic log live in the separate `user` directory
there and survive artifact updates. New Classic V13 files instead live in a
visible `REPLAY` directory beside the executable. A read-only installation
falls back to `user\REPLAY`; `last-run.log` records the selected install and
replay roots. The launcher retains only the current and one previous payload
generation, so ordinary artifact refreshes do not accumulate an unbounded
private cache.

The packaged source trees are compared by relative path, length, and SHA-256.
When `YUME1.DAT` and `YUME2.DAT` are entirely identical, only one copy is
embedded. First-launch extraction reconstructs the second tree with per-file
hard links, or checked byte copies on a filesystem without hard-link support.
No non-identical archive is deduplicated.

The no-argument script is the maintained convenience path and should be rerun
after changes intended for human testing. It rebuilds and overwrites the stable
artifact rather than publishing versioned copies. It normalizes payload entry
timestamps, runs both portable and fallback replay-root smokes against the exact
generated executable, verifies reconstructed DAT members, and prints the final
size and SHA-256. An unchanged second build must reproduce that identity; the
maintained identity is recorded in `state/PROGRESS.md` after each refresh.
Alternate ignored inputs can
be supplied with `-DataRoot`, `-MainExecutable`, `-MainlExecutable`,
`-OpExecutable`, `-FontRom`, and optional `-RhythmRom`. The packager also accepts
`-T98Sound86`; on this development machine the no-argument path automatically
recognizes the authenticated T98-Next 13.1th `DEVICE\Sound86.nhw`, extracts its
six decoded rhythm voices into the private payload, and never executes the
device module. The tracked launcher and packager contain no original bytes; the
generated executable does, so it remains
ignored and is strictly a private testing artifact pending the asset/license
and public-package work.

### Manual cross-platform beta builds

The repository's **Manual beta builds** GitHub Actions workflow is a deliberate
button, not per-commit CI. Run it from the Actions tab when a tester build is
needed. It builds and tests source-authenticated SDL 3.4.10,
libdatachannel 0.24.3, and Mbed TLS 3.6.7 configurations for Windows x86-64,
Linux x86-64, macOS Apple Silicon, and macOS Intel. Every job uploads one
archive with the product executable, replay tools, runtime libraries,
dependency licenses, and a SHA-256 build manifest.

Linux jobs use the distribution's OpenSSL-backed libcurl package for bounded
HTTPS update acquisition; macOS jobs use the SDK-provided libcurl, and Windows
uses WinHTTP. These adapters only download authenticated update inputs. The
current beta package still does not replace or execute itself.

The workflow never fetches or packages original TH03 content and never signs,
notarizes, deploys, or creates a GitHub Release. Its artifacts are therefore
development builds that still require the caller-owned inputs described below.
See [the packaged beta notes](docs/BETA_PACKAGE.md) before distributing one.

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

# Developer bypasses for one experience without normal startup/product flow:
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
changes the PC-98 gameplay/PMD timeline. Hidden, minimized, occluded, and
zero-output windows suppress only physical submission: the newest complete
texture is retained and submitted once on restore, while simulation, input,
replay, audio, and source-pixel capture continue. The audio endpoint starts
paused after the initial blocking present, receives a three-interval prefill,
and only then resumes. The maintained T98-Next profile consumes ordinary FM key edges
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
In New Classic gameplay, Escape opens the recorded Pause menu, `R` restarts the
current Story or VS scenario directly, and `Q` exits without saving. Replay
playback reconstructs recorded pause input; live Escape aborts playback and
held physical keyboard `Z` enables fourfold host-paced fast-forward. After a
prior Story clear, holding physical `Z` also accelerates the Staff roll fourfold;
the first clear cannot accelerate itself. Replay details expose the
frozen V13 result/date/score, checkpoint splits, stage clear-bonus counters,
slowdown, and Replay Patch `m:ss.cc` real-frame timers. Clears is read-only;
Splits and Timers retain checkpoint launch.
The active recording lives entirely in memory from its first round through
finalization. Samples, process/checkpoint boundaries, Pause navigation, and
restart/discard never write a replay file. Disk I/O begins only after the
player confirms a numbered Save Replay slot, where the finalized document is
encoded, read-back validated, and atomically published as `TH3Rxx.RPY`.
Fresh launches remove any legacy `REPLAY\TH3LAST.RPY` left by older native
builds before the title; the current recorder never creates that file.
New Classic Practice is entered through the patched title's VS Start route,
1P-vs-CPU selector, and Replay Patch 0.4.12's main and Advanced setup pages.
Its source defaults, exact-VS fallback, fixed-opponent stages, speed/rank/gauge
controls, retry policy, pause/restart behavior, and V13 Practice metadata are
all carried into the shared MAIN runtime.
New Classic also renders the Replay Patch's complete unsigned-byte active
combo count (100-255 adds a hundreds digit); Classic preserves the
source-compatible 99-hit display cap. This presentation choice does not alter
canonical replay hashes, combo timing, scoring, or checkpoint bytes.
The patched title's Option row owns Replay Patch-style KeyConfig and the
source-styled New Classic Options hierarchy. KeyConfig remaps 26 SDL
scancodes across P1, P2, and Story pages, swaps duplicate assignments within a
page, restores page-local defaults, and stores the result atomically in
`TH3KEY.CFG` beside `PODD.CFG`. JoyConfig similarly persists host-neutral
controller button, stick-axis, direction, and 5%-step dead-zone choices in
`TH3JOY.CFG`; SDL conversion and live mapper replacement remain outside the
deterministic gameplay owner. `TH3DEV.CFG` separately persists Automatic,
Disabled, or stable specific-device ownership for P1 and P2. SDL identities
use the controller GUID plus serial where available, otherwise the same-host
device path; reconnecting a selected device restores its owner without writing
transient instance IDs or source slots. The source-styled Input Test reports
aggregate logical words and per-controller contributions. D-pad movement
remains available independently of the selected stick. Story and 1P-vs-CPU
use the dedicated Story movement bindings while retaining P1 actions; the P1
and P2 Autofire selections feed
the recorded logical input boundary. Options persists Rank, audio levels,
KeyConfig, JoyConfig, and the Replay Patch's exact MAIN warning-flash
reduction. With
`--language-overlay <TH3EN.DAT>`, the Language row selects the complete
authenticated English profile: all 52 translated archive members,
Music Room choices, two-line character titles/names, registration names, and
five result labels move together. Without that exact profile the menu retains
Japanese instead of claiming a partial translation. Classic is always
Japanese.
New Classic's source-styled title and hierarchical Option labels additionally
use the versioned `T3LNG001` message boundary. Built-in Japanese/English
catalogs preserve the accepted pixels. Translator and developer builds may
pass `--language-pack <locale.t3lang>`; the loader rejects the package before
startup unless its schema/body hashes, locale/fallback, placeholders,
printable MNUFONT glyphs, and exact proportional row widths all validate.
See `localization/README.md` for the UTF-8 authoring and deterministic compiler
contract. Classic never opens a language pack.
New Classic also owns the Replay Patch's versioned per-character HiScore file.
Its viewer opens on `ALL CHARACTERS`, cycles through the unlocked character
pages with Left/Right, appends the derived TOTAL row, and shows play time, 1CC,
and accepted-continue statistics. A first launch can import a valid Classic
board by value, but New Classic then writes only `YUME.NEW.NEM`; Classic's
824-byte `YUME.NEM` remains unchanged. Live Story time is checkpointed on
registration, Continue, menu return, restart, and application exit, while
Demo, Practice, and replay playback remain excluded.
Validated cached Story leaderboards add the four-row `Local HiScores`, `Online
Leaderboards`, `Personal Bests`, and `Return` New Classic HiScore hub. Personal
Bests reads the already-validated expanded score document once at entry and
summarizes every unlocked character's active-rank best score/stage plus local
playtime and 1CC totals without storage work on a source tick. The official
build uses `https://azinn.dev` for bounded background refresh and Online Match.
`TH03_ONLINE_ORIGIN=https://...` overrides that endpoint for staging, while an
explicitly empty value keeps the product wholly offline. Cache and network
work never run on a source tick, captures disable refresh, and complete results
appear only after the
player leaves and re-enters HiScore. A replay-backed detail can also download
the exact V13 payload on a separate bounded worker. Digest, format, cached
metadata, and atomic-publication checks must all pass before the UI reports it
as downloaded; local downloads are revalidated on later launches. Confirm on
`Downloaded` validates the local file once more outside source time, then
enters the ordinary New Classic replay player through a black/silent OP
boundary. When an active-rank cache contains replay records, the title Replay
route opens a source-styled hub for Local Replays, the Online Replay Library,
Downloaded Replays, and Return. An explicitly configured Windows online origin
also adds `Upload Replay`; its numbered-slot browser transfers one parsed
finalized V13 document to a bounded worker, which loads or enrolls the protected
opaque identity and posts canonical bytes directly from memory. No working
upload file is created, and accepted/rejected/failure is latched back onto the
retained UI at the next source boundary. An existing enrolled identity also
refreshes all of its accepted active-rank submissions into memory and adds
`My Records` only when nonempty; it never enrolls merely because the app
started. My Records and the two public libraries reuse the same Download and
Watch transactions. Without any replay-backed cache, accepted history, or
configured upload capability, Replay still enters the authenticated local
browser directly. Production Cloudflare verifier resources remain unavailable.
The private `tools\th03-private-editor` utility provides direct migration:
it accepts an original 824-byte PC-98 scorefile or Replay Patch 0.4.12's exact
7,812-byte expanded file, validates it, backs up any current native score, and
atomically installs `YUME.NEW.NEM`. Expanded imports and all exports remain
byte-for-byte exact. Close the game before importing or exporting so the live
process cannot later overwrite the migrated profile. The private tool is not
bundled with the game executable.

For presentation regression work, setting `TH03_PIXEL_ORACLE_LOG` writes one
`T3PXH2` row per successful complete host publication, with logical source-tick,
host-overlay, reduced-flash, guest Raster/TRAM, and page/palette selector
identity. Capture mode publishes every surface-changing logical tick separately
so host catch-up cannot hide a source publication. The strict
`tools\compare_pixel_oracle.ps1` comparator reports the first differing field
and emits a structured JSON verdict. See `docs\PIXEL_ORACLE.md` for commands,
the proven two-run/cross-compiler gate, and the implemented devtools-only raw
PC-98 planar producer. Silent/Turbo guest execution and final FONT/gaiji TRAM
composition remain external acceptance work; the lightweight hash stream is
not represented as collision-proof pixel evidence.
Setting `TH03_PIXEL_ORACLE_RGB_DIR` additionally commits the exact RGB bytes of
every successful 640x400 host publication as a `T3RGB1` manifest plus lossless
PPMs. `tools\compare_pixel_frames.py` compares the bytes, localizes the first
differing pixel and bounding box, and accepts losslessly imported DOSBox-X
`T3EMU1` video streams. This closes repeat-run/cross-compiler pixel equality and
provides the external comparator; source-boundary alignment with the planned
devtools-only PC-98 producer remains required before claiming guest parity.
Unless overridden with `--config`, `--score`, and `--replay-root`, Classic
stores its exact `YUME.CFG` and `YUME.NEM`. New Classic stores its versioned
`PODD.CFG`, derives `YUME.NEW.NEM` beside the selected `--score` path, and
stores `TH3KEY.CFG`, `TH3JOY.CFG`, and `TH3DEV.CFG` plus the `REPLAY`
directory under the selected `--data` root. An absent `PODD.CFG` imports valid
Classic option values once by value;
later New Classic changes never rewrite `YUME.CFG`. This avoids dependence on
the launcher working directory; use explicit writable paths when the extracted
content root is read-only.

`Online Match -> Network Settings` is also part of `PODD.CFG`. Its selected
Room Code Service or Direct UDP connection, Automatic or Relay Only policy,
Direct UDP host port, and manual 0..6-frame gameplay input delay survive
returning to the title and later launches. Transport and port fields are local
product preferences only. The host's delay is explicitly negotiated and shown
to both peers, but remains outside rollback state, replays, and Classic.

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

Render a finalized V13 replay to a full-length Matroska video without opening
a game window:

```powershell
out\build\msvc-x64-sdl\Release\th03-replay-video.exe `
  --replay X:\replays\TH3R00.RPY `
  --data X:\podd-content `
  --output X:\video\TH3R00.mkv
```

To inherit the installed standalone's New Classic language, flash, and audio
levels while explicitly requesting 85% SFX, use:

```powershell
out\build\msvc-x64-sdl\Release\th03-replay-video.exe `
  --replay X:\replays\TH3R00.RPY `
  --data X:\podd-content `
  --output X:\video\TH3R00.mkv `
  --use-installed-settings --sfx-volume 85
```

`--settings X:\path\PODD.CFG` reads another validated profile. Explicit
`--language`, volume, `--reduced-flash`, and `--full-flash` flags override the
profile field by field. Replay-affecting state remains replay-owned, and output
resolution/codec remain export policy rather than being copied from the game
window.

The default is nearest-neighbor 1920x1200 4:4:4 output at TH03's exact nominal
`526315/9328` Hz source cadence, with 48 kHz stereo FLAC audio. The exporter
runs the native replay and PMD/OPNA engine unthrottled, shows progress and ETA
for both deterministic passes, selects `h264_nvenc` when FFmpeg exposes it,
does nearest-neighbor NVENC scaling on CUDA, and overlaps bounded palette
expansion/frame delivery with the next exact simulation tick. It retries with
`libx264` if automatic NVENC encoding fails. `--resolution`
accepts any 8:5 size; `--encoder` also accepts `hevc_nvenc`, `av1_nvenc`, and
`libx264`. NVENC defaults to the balanced `p4`; `--encoder-preset p1` trades
compression efficiency for maximum throughput, while `p5` through `p7` spend
progressively more encoder time. On the primary RTX 4090 measurement,
`hevc_nvenc`/`p1` reached 10.4x during the video pass versus 4.6x for the old
hard-coded H.264 `p6` profile. `--help` lists checkpoint, language, flash,
volume, settings, rhythm-source, and diagnostic partial-render options.
FFmpeg must be on `PATH` (or supplied through `--ffmpeg`). The only
intermediate is a temporary lossless FLAC beside the requested output, removed
on success or failure; raw video and PCM are streamed directly to FFmpeg.
Use the Release executable for export; Debug intentionally retains expensive
state validation and is not representative of rendering throughput.

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
- `th03_app_runtime` owns dependency-free host policy: startup ownership,
  caller-selected rational logical deadlines, timestamped two-player input
  accumulation, exact
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
  preserves the exact eight-byte Classic/Replay Patch layout while native
  writes zero DOS-only resident/debug metadata.
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
  recipes are preserved, selected, and simulated behind the source-black
  transition. The source masks an already-running PMD song during that hidden
  preroll; native playback instead re-arms the correct cue at the selected
  frame, so exact chip phase remains part of the external register/waveform
  oracle rather than a claimed local equivalence.

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
`docs/NEW_CLASSIC_PRODUCT_DESIGN.md` records the accepted working product
hierarchy, sealed Classic boundary, and ordered New Classic
menu/settings/services plan.
`docs/UPDATE_MANIFEST.md` freezes the authenticated, monotonic release-metadata
boundary, offline signing tool, provider-backed pinned-key verification, inert
exact-artifact staging, and the manifest-first bounded acquisition transaction.
Windows supplies the first strict HTTPS adapter; production keys,
Linux/macOS adapters, polling/UI, and self-replacement remain disabled.
`docs/research/README.md` indexes the presentation, asset, and audio research
contracts prepared for those later layers.
