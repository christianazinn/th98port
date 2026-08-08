# Architecture

The only current product is a deterministic, unbounded-speed gameplay library
and its validation clients. No playable application is designed or scaffolded
in Phase 1.

```text
V11 oracle decoder now                 future V13 decoder
            \                           /
             -> version-neutral ReplaySession
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
th03_core <- th03_formats <- command-line clients
```

`th03_core` cannot open files, inspect a clock, sleep, poll host input, render,
play audio, use networking, or serialize pointers. `th03_formats` receives
`std::span<const std::byte>` and likewise has no filesystem dependency. Clients
own paths and process exit codes.

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
