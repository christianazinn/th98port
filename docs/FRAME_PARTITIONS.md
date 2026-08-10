# Gameplay frame implementation partitions

Snapshot: **2026-08-09**

The source-complete Classic frame backend was mechanically split after the
whole-source audit and the 19-file V11/V12 corpus gate turned green. This is a
source-organization change, not a module-boundary or gameplay change.

`src/core/frame.cpp` now contains the private constants/types, ordered includes,
and the staged frame entry points used by `Game`. Generic mechanics live
directly under `src/core/frame/`; code that implements one or more of the nine
playable characters lives under `src/core/frame/characters/`. No partition is
longer than 700 lines.

Generic partitions:

| partition | pre-split `frame.cpp` lines | responsibility |
|---|---:|---|
| `frame/bullets_core.inl` | 185-698 | score, gauge, combo, generic bullet allocation, and trails |
| `frame/bullets_transfer.inl` | 735-878 | coordinate transforms, fireball allocation, and transfer initialization |
| `frame/bullets_world.inl` | 1177-1595 | chain fire, transfer pellets/clouds, explosions, collision-map insertion, and bullet update |
| `frame/shots.inl` | 1596-1735 | ordinary shot pairs and CPU target selection |
| `frame/player_movement.inl` | 2301-2396 | movement velocity, clipping, and human movement |
| `frame/player_actions.inl` | 2620-2731 | Gauge release and Bomb activation |
| `frame/player_collision.inl` | 3581-3874 | common player hit, Hyper, damage, knockback, and update stages |
| `frame/player_cpu.inl` | 3875-4116 | CPU targeting, avoidance, and movement |
| `frame/enemies_motion.inl` | 4117-4635 | ENEDAT enemy motion and collision-map writers |
| `frame/extra_common.inl` | 4636-4719 | shared Extra Attack movement and hit testing |
| `frame/boss_common.inl` | 5606-5777 | shared Boss movement, launch, damage, and defeat lifecycle |
| `frame/world_collision.inl` | 8266-8936 | fireball/enemy update, hit testing, scoring, chains, and explosions |
| `frame/round.inl` | 8937-9151 | HUD, background, result, and Demo late-frame state |

Character-owned partitions:

| partition | pre-split `frame.cpp` lines | responsibility |
|---|---:|---|
| `frame/characters/kana_extra_bullet.inl` | 699-734 | Kana's accelerated Extra Attack bullet allocator |
| `frame/characters/extra_spawn.inl` | 879-1176 | all nine Extra Attack transfer/spawn callbacks |
| `frame/characters/charge_update.inl` | 1736-2094 | all nine Charge Shot update callbacks |
| `frame/characters/charge_collision.inl` | 2095-2300 | all nine Charge Shot collision callbacks |
| `frame/characters/charge_add.inl` | 2397-2619 | all nine Charge Shot and Hyper add callbacks |
| `frame/characters/bombs.inl` | 2732-2904 | character Bomb update and renderer-owned deterministic effects |
| `frame/characters/gauge.inl` | 2905-3580 | all nine Gauge Attack patterns and dispatch |
| `frame/characters/extra_update.inl` | 4720-5417 | all nine Extra Attack update callbacks |
| `frame/characters/extra_dispatch.inl` | 5418-5605 | Extra Attack dispatch and direct allocators |
| `frame/characters/bosses_reimu_mima.inl` | 5778-6323 | Reimu and Mima Boss Attack families |
| `frame/characters/bosses_marisa_ellen.inl` | 6324-6757 | Marisa and Ellen Boss Attack families |
| `frame/characters/bosses_kotohime_kana.inl` | 6758-7305 | Kotohime and Kana Boss Attack families |
| `frame/characters/bosses_rikako_chiyuri.inl` | 7306-7852 | Rikako and Chiyuri Boss Attack families |
| `frame/characters/bosses_yumemi.inl` | 7853-8184 | Yumemi's Boss Attack family |
| `frame/characters/boss_dispatch.inl` | 8185-8265 | character Boss dispatch and renderer-owned deterministic effects |

Historical parity documents that cite `frame.cpp:<line>` refer to the frozen
pre-split layout mapped above. Function names remain unchanged. Character
selection uses the named `character_*` constants and
`character_index_from_paletted()` rather than repeating numeric roster IDs.

## Why one translation unit remains intentional

The private implementation contains many source-order-sensitive helpers,
overloads, shared constants, and callbacks. Turning the partitions into
separately compiled units would require new declarations, linkage, and helper
ownership. That can be reviewed later, but it is an architectural refactor and
was deliberately excluded from this mechanical parcel.

Keeping one translation unit preserves:

- anonymous-namespace linkage and helper visibility;
- definition and lookup order;
- shared PC-98 compatibility constants;
- compiler diagnostics and warning policy; and
- the existing `th03_core` public and private API.

The first coarse split reconstructed every function body from the committed
pre-split source. The refined ownership split retains those function bodies and
replaces only roster, shot-mode, and terminal Story literals with named
constants. V11, V12, V13, replay coordination, and every client remain outside
this source split.

## Verification gate

After the split:

- GCC build and CTest: pass;
- MSVC x64 build and CTest: pass;
- locked GCC `th03rv` gate: 19/19 replay files, zero failures;
- locked MSVC `th03rv` gate: 19/19 replay files, zero failures; and
- `git diff --check`: pass.

Any future move from an included partition to a separately compiled unit must
be its own behavior-free parcel and rerun these same gates.
