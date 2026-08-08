# Deterministic state inventory

Every ported field must be placed deliberately before implementation.

| category | current representation | rule |
|---|---|---|
| startup/resident | `GameInit` | explicit fixed-width fields, never a memory image |
| RNG | `RuntimeSnapshot` rings, seed, and cursors | preserve every bit and exact call order |
| player stage start | `PlayerStart` | positions are signed Q12.4 raw values; other widths remain exact |
| replay cursor/input | `ReplaySession` records | owned outside `Game`; V11/T3CASE layouts do not enter core |
| process handoff | semantic `ProcessBoundary` | no DOS executable spawning or resident-memory pointer transfer |
| canonical gameplay | `CanonicalState` plus private `Game` state | expands mechanically from the donor; no padding or pointer hashing |
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

## TH3R03 first-round inventory

| state family | category | Phase 2 rule |
|---|---|---|
| scenario, scores, stage/lives/credits, characters, CPU flags, rank/skill | startup/resident | copied field-wise from `GameInit`; no resident-memory image |
| live seed, 256-byte random ring/cursor, formation rings/cursors, CPU charge rings/cursors | gameplay RNG | exact widths, exact consumption order; V11 snapshot restore occurs after ordinary round initialization |
| round ID, frame/result counters and modulo counters, round/bullet/enemy difficulty | gameplay | exact increment/reset order from `sub_9778()` and `sub_9EBF()` |
| both `player_stuff_t` semantic fields | gameplay | fixed-width field model; Q12.4/Q4.4 values remain raw fixed point; callback pointers become stable character behavior IDs |
| bullets, templates, trails, enemies, formations, shots, charge shots, Extra/Boss Attacks, fireballs, explosions, hit circles, bombs | gameplay | private deterministic entity state; port when it feeds collision, score, gauge, chain, player, RNG, or result behavior |
| collision bitmap | gameplay | deterministic logical collision state, never PC-98 VRAM addresses |
| combos, chain rings/arrays, gauge attack flags and counters | gameplay | exact arrays and update order; only the documented display-hit clamp belongs to the oracle view |
| input latches, Autofire shot gates, warning/pause continuation | gameplay/input boundary | `Game` consumes one logical sample at a time and exposes the next expected input context; no host polling |
| sample cursor, T3CASE phase/control records | replay adapter | outside gameplay state/hash; validator checks them against the core's requested input context and route |
| normalized background/result callback phases | gameplay phase identity | stable enum, replacing callback pointer identity |
| sprites, VRAM pages, palettes, text, sound, VSync | presentation/platform | never stored in deterministic state; preserve ordering only where a donor routine also mutates gameplay |
| far/near pointers, segments, DOS file handles, executable names | platform | absent from core; typed IDs/indices and client-owned files replace them |

The complete schema-3 hash order and first-round evidence are frozen in
`TH3R03_ROUND_DOSSIER.md`.

## Phase 2 implementation status

- Implemented and oracle-exercised: startup/restore, random sources, round
  counters, human and stage-0 CPU movement, HUD start, initial background RNG,
  ENEDAT formation scripts and motion, ordinary P1/P2 shot pairs, enemy HP and
  explosions, early chains/combos, Reimu/Kana charge-shot trails, and the CPU
  collision map/avoidance route.
- The public input boundary is deliberately after the donor's input latch and
  before player update. `StepResult::input_boundary_state` exposes that state
  for comparison without importing expected split data into the core.
- Exact canonical hashes are verified at start, sample 128, and sample 256;
  both scores are exact at sample 384. Transfer pellets, their bullet/cloud
  lifecycle, and full warning/interstitial control remain open.
- Presentation state remains excluded unless it consumes gameplay RNG. The
  type-B initial background's 40 IRAND calls per frame and HUD particle draws
  are therefore deterministic core effects even though no renderer is present.
