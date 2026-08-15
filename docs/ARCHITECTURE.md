# Architecture

The only current product is a deterministic, unbounded-speed gameplay library
and its validation clients. No playable application is designed or scaffolded
in Phase 1.

```text
                         V13 reader
                         |
                 version-neutral ReplayDocument
                                |
                         ReplaySession runner
                                |
T3CASE validation cursor ----> CoreInput + semantic process boundaries
                                |
                             libth03::Game
                                |
                 canonical state / optional neutral
                       gameplay observations
                                |
                  headless and verification clients
```

Dependency direction is one way:

```text
th03_core <- th03_replay <- command-line clients
     ^             ^
     +-- th03_formats --+
```

`th03_core` cannot open files, inspect a clock, sleep, poll host input, render,
play audio, use networking, serialize pointers, or own renderer-shaped state.
In particular it stores no framebuffer/page/palette/TRAM state, asset IDs,
sprite/cel state, draw commands, or persistent presentation event queue.
`th03_formats` receives
`std::span<const std::byte>` and likewise has no filesystem dependency. Clients
own paths and process exit codes. V13 compact snapshots
become format-neutral reconstruction/round-start contracts; replay layouts
never enter the core.

## Optional observation boundary

The playable client may attach a synchronous, non-owning gameplay
observer while stepping the same core. Observations report only immutable raw
gameplay facts at semantic phase boundaries and primitive results from
shared-RNG reads the core has already consumed, carrying only a minimal
source-site/phase tag and, only where the source contract itself requires it,
a semantic sequence ordinal. Repeated synchronous facts otherwise preserve
their order without exposing renderer-private slot identities. The core does
not group or decode those
results into visual payloads. Observations are not draw instructions, asset or
sound IDs, visual entity state, or replay data. The core retains no observation
after the callback returns, and an observer cannot alter control flow.

The implemented MAIN seam retains those three source-ordered ordinary-entity
views—shots/enemies, fireballs after their collision pass, and final
players/bullets—inside a complete direct-VS callback schedule. Successful shot and Bullet16 construction plus player
pose/defeat facts close source presentation lifetimes that cannot be inferred
from late gameplay state. These are typed gameplay facts rather than renderer
commands. `th03_present` consumers privately own shot animation, the shared
player-pose and defeat records, Bullet16 construction style, both original
hit-circle/common-effect pools, and their presentation-only Kana/Kotohime Bomb
state while drawing those views without storing anything in core. Explicit
hit-circle and common-effect passes occupy the source slots after fireballs and
between player overlays and bullets, respectively.

Additional borrowed views and semantic facts preserve background, Boss,
Extra/Charge, Warning/Gauge, HUD-start, score/extend, combo, result,
transition, and footer boundaries. Result is observed before its control/bonus
mutation; terminal routes emit no later Gauge/HUD/footer facts. No callback
contains a color, glyph, asset, page operation, or draw command.

Some original callbacks named as rendering perform gameplay mutations or
consume the RNG stream later used by gameplay. Those mutations and that exact
compatibility-read schedule stay in `th03_core`, regardless of whether an
observer exists. The retained counters are deterministic schedule state, not
particle, cel, palette, or other renderer state. Omitting those reads in a
headless run would change later gameplay, so `th03rv` executes them but supplies
no observer and links neither presentation nor assets.

The post-core targets remain in this repository and depend inward:

```text
th03rv ------------------------------> th03_replay / th03_formats / th03_core

th03-app --> th03_app_runtime -------> th03_present
    |                                  |     |
    +--> th03_app_resources            |     +--> th03_assets
                                       +--------> th03_core
```

`th03_assets` and `th03_present` are separate sibling library targets, not
separate repositories. After agreeing on a small decoded-model interface,
asset decoding and synthetic raster/palette/page primitives proceed in
parallel. The asset library now supplies RGB/PI, CDG/CD2, BFNT, and MRS models;
presentation supplies indexed pages, clipping, palette, planar/TRAM, cel,
Sprite16, text, staff-flake, and source-exact GRCG line/circle primitives. The
static MAIN transaction and observer-backed ordinary-entity/effect consumers
establish their initial join.
`th03_present` owns all presentation records,
sprite/cel lifetimes, asset identities, page/palette/TRAM state, and private
draw ordering.

`th03_app_runtime` is a third, platform-neutral client boundary. It owns the
rational tick/input accumulator, final RGBA/letterbox policy, typed product
lifecycle, and one-time source-derived startup sequence. The latter authenticates
the original `TOUHOU.DAT` logo BFNT, reproduces the shared TH01-TH03 ZUN Soft
opening before the direct New Classic title, and retains a following zero-duration slot
for a future PoDD Port development-team credit animation. The caller provides
the clock rate, timestamps, physical-event mapping, and output memory; the
runtime neither sleeps nor creates a window. Archive validation/caching stays
in the orthogonal `th03_app_resources` target so neither dependency is forced
into core, assets, or presentation.

MAIN's palette boundary retains master.lib's full 16-entry RGB8 logical
`Palette8`, a separate applied RGB4 DAC result, raw `PaletteTone`, one
process-scoped baseline snapshot, and an explicit dirty latch. Core reports
only neutral Bomb/Warning/defeat/footer lifecycle facts; character presenters
own the corresponding RGB/tone formulas. Immediate shows do not clear the
dirty latch, and the normal footer flushes it before the page transaction.

V11/T3CASE phase 1 is an auxiliary boundary, not a semantic warning tag. It can
carry either the fixed 27-sample warning wait or the Replay Patch's variable
pause-menu stream. The replay coordinator owns pause selection/release parsing;
the core must expose a staged MAIN input boundary so the coordinator can clear
post-pause system input and request loop exit without moving gameplay during
the auxiliary samples. See `PARITY_RENDER_PHASE.md` for the exact contract.

## Numeric contract

The PC-98 game's apparent decimal coordinates and velocities are overwhelmingly
fixed-point. Deterministic Classic code therefore does not use floating point
unless a donor source proves that the original operation did. Phase 1 provides:

- signed 16-bit Q12.4 subpixels;
- signed and unsigned 8-bit Q4.4 values;
- signed 16-bit decimal-tenths values;
- explicit wrapping add/subtract/negation; and
- explicit arithmetic signed right shift.

Fixed wrappers expose raw construction and conversion but deliberately do not
offer broad arithmetic operators. Phase 2 ports each expression mechanically,
including the original width, promotion, wrap, shift, truncation, and comparison
order. This prevents a convenient abstraction from silently changing Turbo C++
semantics.

## Phase boundary

Phase 1 is complete when both local compiler builds pass, all strict synthetic
format tests pass, all six external V11/T3CASE pairs match the frozen hashes and
sample counts, all twelve frozen split comparisons agree, and `th03-headless`
reaches the typed `simulation-not-implemented` result.

Phase 2 replaces that one deliberate gap with the smallest complete gameplay
round, using TH3R03 as the first oracle. Phase 3 expands the same core through
complete Classic gameplay and process phases. Application work remains later.

Phase 2 is complete: the TH3R03 first-round trace matches all 46 canonical
Anniversary rows through its natural route-2 terminal. Phase 3 preserves that
gate and has completed the source-backed Classic gameplay backend. The locked
integration corpus now covers 19 frozen V13 conversions and 173 embedded round
summaries across Story, Practice, and ordinary VS; all match. Historical
V11/V12 inputs are converted externally and are no longer native parser
formats. The whole-source
disposition is recorded in `PARITY_SOURCE_COVERAGE.md`.

## Completeness contract

Corpus replays are validation coverage, not implementation scope. Phase 3 is
not complete merely because TH3R03, TH3R04, or even the current corpus happens
to pass. The Classic core must faithfully implement every gameplay-relevant
path in the Anniversary donor, including every character's shots, charge shots,
Extra Attacks, gauge attacks, Boss Attacks, bombs, and hypers, plus all shared
round, collision, damage, scoring, CPU, enemy, projectile, and result logic.

Every donor callback reachable during gameplay must be inventoried as one of:

- mechanically ported, with its call order and PC-98 numeric semantics retained;
- deliberately presentation-only, with evidence that it mutates no gameplay
  state and consumes no gameplay RNG; or
- still missing, which blocks Phase 3 completion even if all available replays
  are green.

Replay oracles then prove integration, ordering, and deterministic equivalence.
They must never be used to choose which gameplay paths deserve implementation.

The live-human pause/Q loop is a staged core/application protocol boundary.
`th03_core` exposes the source release/choice/release states and freezes the
late frame; the interactive driver supplies device samples while retaining its
last published surface. Recorded pauses remain fully and separately coordinated
by `th03_replay`.

## Gameplay frame implementation partitions

`src/core/frame.cpp` owns the private constants/types and exported staged-frame
entry points. Its source-complete implementation is mechanically partitioned by
subsystem under `src/core/frame/`; character callbacks are separated under
`src/core/frame/characters/`. The partitions remain included, in source order,
into one anonymous-namespace translation unit. This keeps private helper
linkage and lookup identical while allowing generic and character ownership
without oversized source files. See `FRAME_PARTITIONS.md` for the exact mapping
and gate.
