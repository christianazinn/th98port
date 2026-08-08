# Phase 3 Story handoff

Phase 2 models one `MAIN` gameplay process. Phase 3 retains that boundary:
`Game` does not emulate DOS process launching, MAINL rendering, file loading,
music timing, or replay cursor storage.

For Story validation, a client performs this typed sequence:

1. step `Game` until its natural route;
2. obtain `next_story_stage_init()` after a route-2 P1 win;
3. validate and consume the recorded `MAIN_END` boundary;
4. consume MAINL's recorded phase-1 samples without advancing gameplay;
5. validate `MAINL_END`, assign its global sample index to the next init; and
6. create the next `Game` with the same immutable content.

The next init carries the source resident semantics: prior `round_frame` as
the RNG seed, incremented Story stage, current lives and skill, winner score
only, and the next Story opponent. A fresh MAIN then regenerates the random,
CPU-charge, and formation rings in donor order. Expected split rows never
populate this state.

MAINL sample counts are presentation-path observations, not hardcoded gameplay
constants. T3CASE process controls delimit them. Phase 3's first TH3R03 MAINL
span happens to contain five samples (6,773 through 6,777), while later spans
contain two. This client behavior is validation scaffolding, not application
design.

## Current validation boundary

The typed coordinator and the progressively expanded Classic gameplay core
match the frozen Anniversary TH3R03 oracle through global sample 14,464. This
includes the complete first and second gameplay processes, both intervening
MAINL boundaries, and the first 1,136 logical frames of Stage 3. The next first
failure is the interstitial input boundary at sample 14,573, which the native
core has not yet scheduled; expected oracle values remain comparison-only and
never initialize or steer simulation.
