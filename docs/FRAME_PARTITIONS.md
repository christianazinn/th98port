# Gameplay frame implementation partitions

Snapshot: **2026-08-09**

The source-complete Classic frame backend was mechanically split after the
whole-source audit and the 19-file V11/V12 corpus gate turned green. This is a
source-organization change, not a module-boundary or gameplay change.

`src/core/frame.cpp` now contains the private constants/types, ordered includes,
and the staged frame entry points used by `Game`. Eight implementation
partitions are included inside the same anonymous namespace and translation
unit, in their original order:

| partition | pre-split `frame.cpp` lines | responsibility |
|---|---:|---|
| `frame/bullets.inl` | 185-1595 | score/gauge/combo helpers, bullet and fireball allocation, transfers, explosions, bullet update |
| `frame/shots_charge.inl` | 1596-2300 | ordinary shots, Charge state update and hittest |
| `frame/player.inl` | 2301-4116 | movement, Charge release, Bomb/Hyper, Gauge attacks, damage, human and CPU players |
| `frame/enemies_motion.inl` | 4117-4635 | ENEDAT enemy motion and collision-map writers |
| `frame/extra_attacks.inl` | 4636-5605 | all nine Extra Attack families and direct allocators |
| `frame/bosses.inl` | 5606-8265 | shared Boss lifecycle and all nine Boss pattern families |
| `frame/world_collision.inl` | 8266-8936 | fireball/enemy update, hittest, scoring, chains, and explosions |
| `frame/round.inl` | 8937-9151 | HUD/background/result/demo late-frame state |

Historical parity documents that cite `frame.cpp:<line>` refer to the frozen
pre-split layout summarized by this table. Function names remain unchanged.

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

Expanding the ordered includes and normalizing line endings reconstructs the
pre-split implementation exactly before the explanatory partition comment.
V11, V12, future V13, replay coordination, and every client remain outside this
source split.

## Verification gate

After the split:

- GCC build and CTest: pass;
- MSVC x64 build and CTest: pass;
- locked GCC `th03rv` gate: 19/19 replay files, zero failures;
- locked MSVC `th03rv` gate: 19/19 replay files, zero failures; and
- `git diff --check`: pass.

Any future move from an included partition to a separately compiled unit must
be its own behavior-free parcel and rerun these same gates.
