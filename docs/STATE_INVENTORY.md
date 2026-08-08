# Deterministic state inventory

Every ported field must be placed deliberately before implementation.

| category | current representation | rule |
|---|---|---|
| startup/resident | `GameInit` | explicit fixed-width fields, never a memory image |
| RNG | `RuntimeSnapshot` rings, seed, and cursors | preserve every bit and exact call order |
| player stage start | `PlayerStart` | positions are signed Q12.4 raw values; other widths remain exact |
| replay cursor/input | `ReplaySession` records | owned outside `Game`; V11/T3CASE layouts do not enter core |
| process handoff | semantic `ProcessBoundary` | no DOS executable spawning or resident-memory pointer transfer |
| canonical gameplay | `CanonicalState` stub | expands field-wise during Phase 2; no padding or pointer hashing |
| presentation | semantic events | excluded from gameplay unless evidence proves feedback or RNG use |
| platform | clients only | paths, clocks, devices, windows, and networking never enter deterministic state |

Two Phase 0 landmines are fixed requirements for the future core:

- `fireballs_hittest()` uses Anniversary's corrected deterministic chain slot;
- formation randomization starts with an impossible previous ID initially and
  previous ID 1 on repeat rounds, explicitly modeling the observed PC-98 stack
  residue without depending on a host register or stack.

Before Phase 2 code is added, expand this file with the complete state touched by
the selected TH3R03 round and mark every field as gameplay, replay-adapter,
presentation, or platform state.
