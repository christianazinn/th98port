# Architecture

The only current product is a deterministic, unbounded-speed gameplay library
and its validation clients. No playable application is designed or scaffolded
in Phase 1.

```text
V11 / V12 historical readers          future V13 reader
                \                       /
                 -> version-neutral ReplayDocument
                                |
                         ReplaySession runner
                                |
T3CASE validation cursor ----> CoreInput + semantic process boundaries
                                |
                             libth03::Game
                                |
                     canonical state / semantic events
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
play audio, use networking, or serialize pointers. `th03_formats` receives
`std::span<const std::byte>` and likewise has no filesystem dependency. Clients
own paths and process exit codes. Version-specific V12 compact snapshots become
format-neutral reconstruction/round-start contracts; V12 layouts never enter
the core.

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
integration corpus now covers 19 V11/V12 files and 173 embedded round
summaries across Story, Practice, and ordinary VS; all match. The whole-source
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

The remaining live-human pause/Q loop is deliberately an application protocol
boundary. Recorded pauses are fully coordinated by `th03_replay`; the future
interactive client must expose the same staged frame boundary without placing
UI or device polling inside `th03_core`.

## Gameplay frame implementation partitions

`src/core/frame.cpp` owns the private constants/types and exported staged-frame
entry points. Its source-complete implementation is mechanically partitioned by
subsystem under `src/core/frame/`. The partitions remain included, in source
order, into one anonymous-namespace translation unit. This keeps private helper
linkage and lookup identical while allowing subsystem ownership without a
9,000-line file. See `FRAME_PARTITIONS.md` for the exact mapping and gate.
