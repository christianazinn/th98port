# TH01 boss gameplay MVP

This headless layer follows the boss switch in `th01/main_01.cpp` and keeps
each source owner in a named state record rather than a shared pattern engine.
It covers gameplay state, shared RNG consumption, projectile spawning,
collision, and the stage/modal boundaries. Palette, VRAM and sound calls stay
with presentation.

| Stage | Stored route | Source owner | Native owner |
|---|---:|---|---|
| 5 | either | `main/boss/b05.cpp` SinGyoku | `boss.cpp` |
| 10 | 0 Makai | `main/boss/b10m.cpp` YuugenMagan | `boss_yuugenmagan.cpp` |
| 10 | 1 Jigoku | `main/boss/b10j.cpp` Mima | `boss_mima.cpp` |
| 15 | 0 Makai | `main/boss/b15m.cpp` Elis | `boss_elis.cpp` |
| 15 | 1 Jigoku | `main/boss/b15j.cpp` Kikuri | `boss_kikuri.cpp` |
| 20 | 0 Makai | `main/boss/b20m.cpp` Sariel | `boss_sariel.cpp` |
| 20 | 1 Jigoku | `main/boss/b20j.cpp` Konngara | `boss_konngara.cpp` |

`route_t` is `ROUTE_MAKAI = 0`, `ROUTE_JIGOKU = 1`; the route-selection
palette colors 1 and 3 are not route values. SinGyoku's pure 200-frame
entrance is collapsed at its source endpoint: it consumes no RNG and does not
advance the outer gameplay loop, while retaining its cel and phase writes.
Its defeat clears non-cloud pellets, then requires an explicit route-selection
submission before the stage boundary.

The common `boss_hit_update()` is the gameplay subset of
`main/boss/hit.cpp::boss_hit_update_and_render()`: test damage, shot test,
Orb/bomb damage, bounce, score and invincibility timing. Owner-specific
callbacks and all visual work remain at their direct source call sites.

## Sariel (`b20m.cpp`)

`boss_sariel.cpp` keeps the source phase ownership rather than flattening the
fight into a common loop:

- Phase 1 selects random purple lasers, vortices, and the egg/bird ellipse.
- Phases 3, 5, and 7 retain the snowflake, A2 rings, sling clusters, aimed
  lasers, rain, radial stacks, bird, semicircle, and bottom-column patterns.
- Phases 2, 4, 6, and 8 preserve the background-transition's 100 `irand()`
  draws per gust frame; these are presentation-driven calls with a shared
  gameplay RNG consequence.
- Phase 99 performs the form-one reset, Phase 100 owns all five-to-eight
  curved-spray/seal-rain/leaf variants, and Phase 101 adds 50,000 score and
  sets `game_cleared` without inventing a normal stage-clear flag.

The named Sariel record carries the three independent 2x2 allocation pools,
bird and egg pool, leaves in source decimal-subpixel tenths, vortex/laser
locals, snowflake state, and both curved sprays. This preserves pool-dependent
RNG consumption and the bird/leaf collision checks. `test_boss_sariel.cpp`
covers the entrance seed boundary, purple-laser draw count, a background gust,
the cloud-ring allocation, and both form-defeat boundaries. Synthetic tests
enter phase-three patterns through frames 1–9 so source initializers such as
the snowflake interval are established; arbitrary later-frame injection is
not a supported construction boundary.

The current MVP deliberately excludes VRAM unblitting, sprite cels, palette
updates, sound, and blocking presentation delays. Pure presentation loops are
collapsed only at their source endpoints without advancing an extra outer
gameplay frame. This is source-shaped native coverage, not emulator replay
parity or a renderer claim.
