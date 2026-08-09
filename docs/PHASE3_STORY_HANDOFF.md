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

The typed coordinator and Classic gameplay core match the frozen Anniversary
oracles for the complete TH3R00, TH3R02, TH3R03, TH3R04, and TH3R05 documents:

- TH3R00: 715/715 canonical rows and 100,642 logical samples;
- TH3R02: 626/626 canonical rows and 91,939 logical samples;
- TH3R03: 186/186 canonical rows and 25,211 logical samples;
- TH3R04: 627/627 canonical rows and 86,437 logical samples; and
- TH3R05: 783/783 canonical rows and 111,747 logical samples.

This includes all MAIN/MAINL transitions, route-1 repeat rounds, recorded
pause and warning interstitials, final terminal MAINL boundaries, and both
Story-victory and Continue-request handoffs represented by the typed session
transition API. A source-backed pipeline correction was required for repeats:
`sub_9778()` continues bullet, enemy, fireball, and gauge-pattern updates while
the round-start HUD suppresses only formation spawning. The native prefix had
previously frozen that entire suffix until the HUD completed.

TH3R01 remains the sole open corpus case. Expected oracle values remain
comparison-only and never initialize, repair, or steer simulation.
