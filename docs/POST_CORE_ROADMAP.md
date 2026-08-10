# Post-core roadmap

Status: decision and sequencing record after the V11/V12 gameplay-oracle
parcel. This document does not authorize application, renderer, audio, or
network implementation. Those layers begin only after their product choices
are accepted.

## Stable foundation

The port begins with one deterministic gameplay implementation rather than
separate Anniversary and Replay gameplay forks. `th03_core` models Classic
Anniversary gameplay. Historical Replay Patch features are clients and format
adapters around that core unless a feature genuinely changes gameplay and is
therefore offered as an explicit ruleset.

The current dependency boundary remains:

```text
              V13 reader
                 |
        version-neutral replay model
                 |
        deterministic replay runner
                 |
              th03_core
                 |
      optional neutral observation seam
                 |
        presentation/audio adapters
```

In particular:

- `th03_core` owns fixed-width, fixed-point gameplay and deterministic process
  transitions. It does not open files, render, play audio, inspect clocks, or
  know a replay version. It owns no framebuffer/page/palette/TRAM state, asset
  ID, sprite/cel state, draw/audio command, or persistent presentation queue.
- An optional synchronous observer can inspect immutable gameplay facts at
  semantic phase boundaries and raw primitive results from shared-RNG reads the
  core has already consumed. Compatibility reads carry only a minimal
  source-site/phase tag and ordinal; visual grouping and interpretation happen
  outside core. Observation never changes stepping and is retained by the
  client, not the core. `th03rv` supplies no observer and links no presentation
  or asset library.
- `th03_formats` owns byte layouts and adapts them to neutral replay/session
  state. Parsers consume byte spans rather than paths.
- `th03_replay` owns replay-driven process, warning, and pause coordination.
- gameplay-affecting late-phase mutations and the exact shared-RNG
  compatibility schedule remain in core even when their source callback was
  named as rendering. This is deterministic gameplay machinery, not renderer
  state.
- filesystem, platform, presentation, assets, audio, and network code remain
  clients.
- Anniversary and Replay presentation modes can share the same core. An
  eventual Arrange mode is a separate rules/content selection, not a gradual
  contamination of Classic behavior.

## Replay Patch and V13 integration

### Branch and worktree layout

Active development continues from the existing tips instead of accumulating
single-use branches:

1. Replay/harness work continues on `emdash/replays-and-validation-17ptz`,
   with the ReC98 submodule advancing its `replays` branch. Short-lived agent
   work may use an isolated worktree, but accepted work is folded promptly into
   those tips so there is one branch strictly ahead of obsolete development
   heads.
2. Native work continues from the current libth03 branch. V13 belongs in
   `th03_formats` and the replay clients; neither `th03_core` nor the neutral
   runner changes merely because the container version changed.
3. The Replay/harness and native histories remain separate repositories and
   are advanced independently. Never rebase either accepted history underneath
   the other or publish the enclosing harness as the standalone port.

The canonical V13 schema is `tools/replay/FORMAT.md`. Exact packed
structures, constants, and accepted game-side combinations come from ReC98's
`th03/replay_format.hpp` and its MAIN/MAINL/OP readers. The authority snapshot,
native ownership, arithmetic correction, and retirement gates are frozen in
`V13_INTEGRATION.md`.

### Format contract

V13 is the finalized player-facing replay format. Its 622-byte header retains
the portable V12 compact checkpoint and RLE-input contract while adding
14-byte round rows, stage-clear bonus records, real-time/Slowdown telemetry,
and the complete validated summary capability set. Optional data after the
logical input end remains an ignorable accelerator cache.

TH03 Data Manager 0.4.2 now owns the one-way V11/V12-to-V13 converter in the
Replay/harness work. It converted all 19 locked historical inputs, whose V13
identities are frozen in `REPLAY_V13_CORPUS_LOCK.tsv`; all 173 summaries, all
152 stored checkpoints, and terminal results reproduce through V13 on GCC and
MSVC. Native V11/V12 parsers are therefore retired. Historical inputs remain
external regression sources for the converter rather than formats accepted by
libth03. V13 tests cover pause/process transitions, Story, Practice, ordinary
VS, checkpoint boundaries, pending streams, and corruption. T3CASE/T3SPLIT
remain independent oracle formats.

### Replay Patch maintenance reminder

Apply Anniversary's deterministic `enemy_formations_randomize()` previous-
formation initialization to the Replay Patch in this change set. It was not
the cause of the resolved V12 divergences, but leaving an uninitialized local
as a gameplay input is a replay landmine.

### Host tools and Data Manager

For the accepted V13 package:

- keep V13 in the version-neutral parser and `th03rv` dispatch;
- keep the V11/V12-to-V13 converter in the Replay/harness Data Manager, never
  core logic;
- update `tools/replay/FORMAT.md`, exporters, validators, frozen fixtures, and
  SHA/size locks together;
- update `tools/th03-data-manager/update-patch.ps1` and regenerate
  `src/th03_data_manager/patch_manifest.json` through the repository's normal
  packaging script;
- update `src/th03_data_manager/formats.py` and the Data Manager replay labels
  for V13 when the converter/package lands;
- refresh patch README/package hashes and verify both fresh installation and
  upgrade from the last accepted replay package.

## Broader native-port architecture

The next implementation boundary should be the deterministic output of the
gameplay core, not a monolithic Windows executable. A practical target graph
is:

```text
th03rv ------------------------------> th03_replay / th03_formats / th03_core

th03-app --> th03_present -----------> th03_core (neutral observations only)
    |          |
    |          +---------------------> th03_assets (decoded models)
    +--------------------------------> th03_audio_pmd
    `--------------------------------> th03_platform_sdl
```

The boundaries should permit a headless verifier, the faithful playable game,
debugging/inspection clients, replay tools, and a later Arrange client to use
the same core independently. These are sibling library targets in the existing
port repository; they do not require separate repositories.

### Presentation contract before renderer selection

Define and test a presentation contract before building the playable
application:

- optional immutable gameplay observations at semantic phase boundaries;
- raw, minimally tagged observation of shared-RNG results already consumed by
  core, interpreted only by presentation;
- presenter-private ordered draw commands terminating in a canonical 640x400
  indexed display state;
- palette writes and frame-boundary timing;
- text/TRAM composition needed for exact Classic output;
- presenter/audio-owned semantic music and sound-effect requests derived from
  gameplay observations or application actions;
- UI/process events that do not belong to gameplay state;
- explicit Classic versus future Arrange content/presentation selection.

Gameplay hashes must not depend on rendered pixels, host timing, audio device
state, observer presence, or renderer-owned interpolation. Renderer-named
original callbacks that mutate gameplay or consume shared RNG remain in the
core, but only as gameplay mutations and deterministic compatibility reads.
Purely visual state and interpretation belong in `th03_present`; the core owns
no presentation event queue.

### Faithful-first implementation and localization

Port observable Classic behavior directly before generalizing it: source draw
order, coordinates, plane/mask operations, palette timing, TRAM cells, CP932
code-unit boundaries, and fixed text metrics define the Authentic profile.
This does not require preserving segmented heaps, unchecked reads, hardware
ports, three process binaries, or other DOS memory-management constraints that
have no observable output.

Localization and other Replay-style quality-of-life work belong in an enhanced
client/presentation profile. The faithful parser retains source byte spans and
semantic labels outside `th03_core`; a later adapter may map those labels to
Unicode/localized resources without changing gameplay, replay verification, or
the Authentic renderer. Do not preemptively introduce a scene graph, ECS,
generic plugin system, or per-frame heap-owned event bus merely to anticipate
future languages. Add abstractions only at a proven content/presentation seam.

### Graphics and platform layer

SDL3 is the leading recommendation over SFML for the faithful client because
it offers a lower-level C interface across windowing, input, rendering, and
audio without imposing a scene model. Its renderer supports logical
presentation, integer scaling, nearest texture sampling, and streaming texture
updates. The official platform notes list Windows desktop support back to XP,
although the selected compiler and C/C++ runtime impose their own, separate
minimum Windows version.

The faithful renderer should initially be deliberately simple:

1. reproduce the PC-98 frame in a software-owned indexed/palette surface;
2. convert or palette-expand once per presented frame;
3. upload one streaming RGBA texture;
4. use nearest-neighbor integer scaling and letterboxing by default;
5. keep fullscreen, window size, aspect policy, and optional non-integer
   scaling in platform configuration rather than gameplay.

This is pixel-faithful without emulating the PC-98 CPU or spending meaningful
modern resources. An enhanced renderer can later consume the same semantic
presentation stream. It must not be the first proof that Classic draw order is
correct.

Do not equate an SDL support claim with a binary support claim. The current
C++23/MSVC build uses the Universal CRT and MSVC redistributable. Before the
application layer, choose an explicit support floor and toolchain matrix. A
reasonable first gate is x64 Windows 10/11 plus Wine, followed by an x86 build
and Windows 7 testing if desired. Windows XP support should be a separate,
conscious toolchain/product decision rather than an accidental constraint on
the architecture-neutral core.

References:

- SDL Windows notes: <https://wiki.libsdl.org/SDL3/README-windows>
- SDL platform list: <https://wiki.libsdl.org/SDL3/README-platforms>
- SDL logical presentation: <https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation>
- SDL logical scaling modes: <https://wiki.libsdl.org/SDL3/SDL_RendererLogicalPresentation>
- SDL texture scale mode: <https://wiki.libsdl.org/SDL3/SDL_SetTextureScaleMode>
- SDL streaming texture updates: <https://wiki.libsdl.org/SDL3/SDL_UpdateTexture>
- SDL source and zlib license: <https://github.com/libsdl-org/SDL>
- SFML 3 Windows packages: <https://www.sfml-dev.org/download/sfml/3.0.0/>

### Assets

Original assets remain user-owned external inputs and must not be committed to
the port repository. `th03_assets` is a separate library target inside this
same repository, not a new repository or a dependency of `th03_core`. Use
`thdat` or the existing Data Manager to obtain the
contents of the original DAT archives, then decode original PI/BFT/CDG/font and
related formats into neutral models:

- indexed images and palettes;
- sprite/cel geometry and transparency semantics;
- fonts/glyph tables and text metrics;
- stage/background data and animation metadata;
- music, effect-bank, and auxiliary files in their original formats.

Prefer deterministic runtime or install-time decoding with a versioned cache.
Do not flatten every asset to PNG/WAV during development: doing so can discard
palette, indexing, timing, and format metadata needed for exact reproduction.
The asset library should accept byte spans, while a Data Manager/import client
owns discovery, extraction, legal ownership checks, and cache paths.

Asset and presentation foundations can proceed in parallel once they agree on
a small immutable decoded-model interface. `th03_assets` can implement and test
RGB/palette and PI decoders against synthetic spans while `th03_present`
implements indexed pages, palette/TRAM state, clipping, and raster primitives
against hand-authored models. Their first mandatory join is a real static MAIN
frame; neither library should wait for the other's full format/scene coverage.

References:

- `thdat` manual: <https://thtk.thpatch.net/master/thdat.1.html>
- THTK overview: <https://www.thpatch.net/wiki/Touhou_Patch_Center%3ATHTK>

### Music and sound effects

Do not replace TH03's `.M` music and `.EFC` effects with captured WAV files.
The original code drives PMD and YM2608-class synthesis, so the native audio
stack needs two separable pieces:

1. a PMD-compatible sequence/effect engine that interprets the original files
   and produces chip-register/timing events; and
2. a YM2608 implementation plus host-rate conversion and mixing.

`ymfm` is a strong chip-core candidate: it is BSD-3-Clause, C++14, includes
YM2608, and is designed for clock-independent embedding. It is not itself a
PMD sequencer. `foo_input_pmd` is useful implementation research because its
MIT-licensed project contains a heavily modified PMDWin-derived engine and
uses ymfm without requiring PMDWin.dll; importing or adapting it still needs a
focused licensing, provenance, and architecture review.

Keep audio outside `th03_core`. An audio adapter should derive semantic
music/SFX cues from neutral gameplay observations and application scene
actions; sound IDs and playback state never enter core. Validate the audio
layer first through PMD event or YM2608 register traces, then through
deterministic rendered-PCM comparisons.
Authentic YM2608 rhythm data may require a legally obtained ROM/sample source;
do not silently bundle copyrighted ROM or WAV data.

References:

- ymfm source/license/chip list: <https://github.com/aaronsgiles/ymfm>
- `foo_input_pmd` source: <https://github.com/stuerp/foo_input_pmd>
- published component description: <https://www.foobar2000.org/components/view/foo_input_pmd>

### Input, persistence, and application shell

After exact presentation and audio foundations:

- map host inputs into the existing logical input contract;
- add deterministic recording at that boundary, targeting V13;
- implement window/fullscreen/scaling/audio/key configuration outside core;
- keep save data, scores, replay files, and configuration versioned and
  atomically written;
- preserve Anniversary and Replay experiences as selectable clients/presets
  over the same Classic gameplay implementation.

The playable executable should not own hidden gameplay rules. Configuration
must select explicit rules/content/presentation profiles that can also be
constructed by tests and headless clients.

### Online leaderboards

Leaderboards are a later service/client feature. Define the trust model before
writing network code. A useful submission should include the V13 replay,
rules/content/build identity, final summary, and verifier result; the server
should re-simulate accepted submissions with a pinned verifier rather than
trust client scores. Local gameplay must never wait on or depend on the
network. Authentication, privacy, moderation, retention, and hosting cost are
product decisions requiring user approval.

### Arrange mode

Keep extension points narrow and explicit:

- ruleset selects mechanics and deterministic data;
- content pack selects stages, characters, sprites, and scripts;
- presentation profile selects renderer/UI behavior;
- audio profile selects music/effect content and synthesis;
- replay header identifies the exact combination.

Do not introduce a generic plugin/ECS framework preemptively. First prove that
Classic can be expressed cleanly through these boundaries; add abstractions
only when an actual Arrange requirement needs them.

## Recommended execution order

1. Preserve the sealed source-wide gameplay parity audit while expanding replay
   coverage. The mechanical `frame.cpp` partition is complete; every later
   core change still reruns both compilers and the full corpus.
2. Keep the structurally green V13 adapter while the external converter lands.
   Convert and lock the complete historical corpus, then retire V11/V12 native
   parsers only after the V13-only dual-compiler gate passes.
3. Continue `th03_assets` decoders and `th03_present` PC-98 raster primitives
   in parallel from their RGB/PI and indexed-page foundations. Complete the
   exact planar/TRAM decisions, then join them at the first static MAIN frame.
4. Implement the minimal optional gameplay observer only when that static-frame
   integration names the facts it actually needs. Prove null/discard/recording
   observer invariance and keep `th03rv` independent of both sibling libraries.
5. Select a minimal Windows/platform backend from an explicit support-floor
   matrix, then implement the host compositor and input mapping.
6. Select and integrate the PMD/YM2608 audio stack after the licensing and
   rhythm-data questions are resolved.
7. Add configuration, persistence, replay browser/recording, and packaging.
8. Add enhanced presentation or Arrange features as separate profiles.
9. Design and deploy leaderboard verification only after V13 and reproducible
   native builds are stable.

The first two unresolved product choices are therefore the Windows support
floor/toolchain matrix and the PMD engine/rhythm-data provenance. Presentation
uses both a private ordered draw stream and a canonical indexed display state;
that is no longer an either/or product choice. None requires adding
presentation state to the deterministic gameplay core.
