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
V11 / V12 / future V13 readers
                 |
        version-neutral replay model
                 |
        deterministic replay runner
                 |
              th03_core
                 |
       semantic output/event boundary
                 |
      renderer, audio, UI, and tools
```

In particular:

- `th03_core` owns fixed-width, fixed-point gameplay and deterministic process
  transitions. It does not open files, render, play audio, inspect clocks, or
  know a replay version.
- `th03_formats` owns byte layouts and adapts them to neutral replay/session
  state. Parsers consume byte spans rather than paths.
- `th03_replay` owns replay-driven process, warning, and pause coordination.
- filesystem, platform, presentation, audio, and network code remain clients.
- Anniversary and Replay presentation modes can share the same core. An
  eventual Arrange mode is a separate rules/content selection, not a gradual
  contamination of Classic behavior.

## Next Replay Patch and V13 change set

### Branch and worktree layout

Continue the established development lines instead of creating another pair of
short-lived topics:

1. Use outer `emdash/replays-and-validation-17ptz` as the active integration
   tip. It contains the completed libth03 and oracle-planning histories and is
   the branch checked out in the corpus-bearing harness worktree.
2. Develop the PC-98 format and patch directly on ReC98 `replays`, beginning at
   `f4a080c5`. Land the deterministic formation-previous repair before V13
   changes expand or perturb the executable layout. Push coherent accepted
   Replay Patch commits, then advance the outer ReC98 gitlink.
3. Finalize the V13 PC-98 specification, reader/writer, conversion tool, and
   emulator evidence on those active lines. Keep the frozen
   `harness/TH03-ORACLE-*` branches unchanged.
4. After the byte and semantic contract is frozen, continue native adapter work
   at the tip of the existing `emdash/th03-v12-normalizer` branch/worktree; do
   not create another port topic. Add V13 to `th03_formats` and version dispatch
   to replay clients. The core and neutral runner should not change merely
   because V13 exists.
5. Merge the resulting native-port commits back into
   `emdash/replays-and-validation-17ptz`, keeping that outer integration branch
   strictly ahead. Publish a fresh port-only subtree checkpoint to the
   standalone libth03 remote.

This retains distinct PC-98 product, validation-harness, and native-port commit
ownership without accumulating dead topic branches. Temporary divergence is
allowed while a parcel is being developed, but the accepted outer integration
tip must merge every completed line before the next product layer begins.

### Format contract

V13 should be the one finalized player-facing replay format. V11 and V12
remain historical import formats and regression fixtures; conversion is one
way, `V11/V12 -> V13`. The old unreleased V13/V14 experiments in repository
history are not a compatibility promise and should not be revived by magic
number alone.

The V13 contract should prefer semantic, architecture-neutral fields:

- explicit ruleset/content/schema identifiers;
- logical input samples and typed process boundaries;
- explicit warning-wait and pause-menu semantics rather than an ambiguous
  auxiliary phase where practical;
- complete deterministic round/process anchor state;
- embedded round summaries and integrity metadata;
- optional retry accelerators that are validated caches, never authoritative
  state and never required for sequential playback;
- no far pointers, DGROUP images, executable addresses, host pointers, or
  opaque process-memory snapshots.

The converter should preserve old files as input fixtures. It should reject
malformed or unsupported combinations explicitly instead of guessing. V13
tests should include conversion equivalence, sequential versus accelerated
playback equivalence, pause and process transitions, Story, Practice, ordinary
VS, CPU-vs-CPU where recordable, and boundary corruption.

### Replay Patch maintenance reminder

Apply Anniversary's deterministic `enemy_formations_randomize()` previous-
formation initialization to the Replay Patch in this change set. It was not
the cause of the resolved V12 divergences, but leaving an uninitialized local
as a gameplay input is a replay landmine.

### Host tools and Data Manager

Once the PC-98 V13 package is accepted:

- add V13 to the version-neutral parser and `th03rv` dispatch;
- add the V11/V12-to-V13 converter as a separate format tool, not core logic;
- update `tools/replay/FORMAT.md`, exporters, validators, frozen fixtures, and
  SHA/size locks together;
- update `tools/th03-data-manager/update-patch.ps1` and regenerate
  `src/th03_data_manager/patch_manifest.json` through the repository's normal
  packaging script;
- update `src/th03_data_manager/formats.py`, which currently documents V11 and
  V12, only after V13 is actually final;
- refresh patch README/package hashes and verify both fresh installation and
  upgrade from the last accepted replay package.

## Broader native-port architecture

The next implementation boundary should be the deterministic output of the
gameplay core, not a monolithic Windows executable. A practical target graph
is:

```text
th03_core
   |-- th03_present ------ canonical indexed frame + presentation events
   |-- th03_replay ------- recording/playback/session coordination
   |-- th03_assets ------- original-format decode/import and cached models
   |-- th03_audio_pmd ---- PMD sequence + YM2608 synthesis
   `-- th03_platform_sdl - window, input, clocks, texture upload, audio device
                             |
                           th03-app
```

The boundaries should permit a headless verifier, the faithful playable game,
debugging/inspection clients, replay tools, and a later Arrange client to use
the same core independently.

### Presentation contract before renderer selection

Define and test a presentation/event contract before building the playable
application:

- canonical 640x400 indexed frame or equivalent ordered draw-command stream;
- palette writes and frame-boundary timing;
- text/TRAM composition needed for exact Classic output;
- semantic music and sound-effect requests;
- UI/process events that do not belong to gameplay state;
- explicit Classic versus future Arrange content/presentation selection.

Gameplay hashes must not depend on rendered pixels, host timing, audio device
state, or renderer-owned interpolation. Renderer-named original callbacks that
mutate gameplay or consume shared RNG remain in the core; purely visual work
belongs in `th03_present`.

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
the port repository. Use `thdat` or the existing Data Manager to obtain the
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

Keep audio outside `th03_core`. The core/presentation boundary should emit
semantic music/SFX events. Validate the audio layer first through PMD event or
YM2608 register traces, then through deterministic rendered-PCM comparisons.
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

1. Seal the source-wide gameplay parity audit and keep expanding replay corpus
   coverage. Split the large `frame.cpp` only as a mechanical, no-behavior
   refactor after this gate, rerunning both compilers and the full corpus before
   and after each subsystem move.
2. Complete the Replay Patch topic work and freeze V13. Add V13 import and
   conversion to the native format layer afterward.
3. Specify `th03_present` events and the canonical indexed framebuffer. Build
   original-format asset decoders and reference-frame tests.
4. Implement the faithful SDL client renderer and host input mapping.
5. Select and integrate the PMD/YM2608 audio stack after the licensing and
   rhythm-data questions are resolved.
6. Add configuration, persistence, replay browser/recording, and packaging.
7. Add enhanced presentation or Arrange features as separate profiles.
8. Design and deploy leaderboard verification only after V13 and reproducible
   native builds are stable.

The first three unresolved product choices are therefore the Windows support
floor/toolchain matrix, the PMD engine and rhythm-data provenance, and whether
the first presentation contract is a canonical framebuffer or an ordered
draw-command stream. None requires changing the deterministic gameplay core.
