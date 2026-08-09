# Deterministic RNG closure audit

Snapshot: **2026-08-08T20:41:45-07:00**

This ledger is a source-complete static audit of deterministic random-number
consumption reachable from TH03's Anniversary `MAIN.EXE`. It follows direct
`IRAND`, `randring*`, `SUB_CE5B()`, and random-centering hit-circle calls, as
well as indirect random draws inside bullet-group expansion. It covers fresh
rounds, active gameplay, render-named callbacks, Demo/attract, Story and VS
result paths, final-stage handoff, and route-1 retries/repeated VS rounds.

This document does not claim a new emulator result. “Closed” below means that
the source call site, its guards, its order, and its native counterpart agree
by static comparison at this snapshot.

| Tree | Revision | State examined |
|---|---|---|
| Native `th03-port` | `33ad4189695236f72ca9bbe9cc20dc15d56de84e` | Dirty Phase 3 snapshot: gameplay, lifecycle, tests, and audit documents modified |
| Anniversary donor `oracle_anniversary` | `3e2381d4275be7287f5525de5c95389d492b0a3c` | Dirty only for harness/instrumentation work |

Function names are authoritative; line numbers describe this snapshot and can
move as the port is completed.

## Executive result

The source-complete static inventory maps every known Anniversary consumer
reachable after entry to `sub_9778()` to a native counterpart, including
otherwise-presentational background, HUD, bomb, Boss Attack, and hit-circle
calls. Dynamic closure is still provisional for one corpus segment: TH3R01 is
exact through sample 19200, but its sample-19328 checkpoint has every canonical
field exact except that Anniversary's shared-ring cursor is two bytes ahead.
No missing static call site has yet been identified for that delta.

The implemented `MAIN.EXE` lifecycle also has static RNG closure: route-1 Story
retries and repeated VS rounds expose an explicit native transition whose draw
order matches `sub_A21F()` / `sub_9EBF()`, and fresh Demo startup applies the
source speed/level overrides after the ordinary startup draws. Demo's timer and
exit callback consume zero RNG and are implemented by `round_mode_update()`.

Score-file encoding and staff-roll flakes remain outside the current core.
They are not `MAIN.EXE` active gameplay, but they are listed in the scope
appendix so a later application layer does not accidentally share the wrong
`IRAND` state.

## Two independent streams

Anniversary has two deterministic streams whose call orders must be preserved
separately.

### master.lib `IRAND`

- Anniversary source: master.lib `IRAND`, called through
  `th03/main/randfill.cpp`, `main/enemy/enemy.cpp`, `main/player/bb12.cpp`, and
  `main/player/d3f9.cpp`.
- Native: `src/core/random.cpp:irand()`.
- Transition: `seed = seed * 0x015A4E35 + 1` with 32-bit wrap; result is
  `(seed >> 16) & 0x7FFF`.
- `round_startup()` seeds it once from `resident->rand`. A route-1 repeat does
  **not** reseed it.

### The 256-byte shared random ring

- Anniversary source: `th03/main/randfill.cpp`, `main/rand2.cpp`, and
  `math/randring_fill.asm`.
- Native: `src/core/random.cpp:randring_fill()`, `randring_next8()`, and
  `randring_next16()`.
- `randring_fill()` writes indices 255 down through 0 and therefore consumes
  exactly 256 `IRAND` values.
- TH03's private `randring_fill()` does **not** write the cursor. Process BSS
  supplies cursor 0 for the first fill; both the 1024-frame refill and every
  route-1 refill retain the cursor value reached by preceding gameplay.
- All source spellings (`randring1_*`, `randring2_*`, and `randring_far_*`)
  share one byte ring and one cursor. They are code-location variants, not
  independent streams. The single native cursor is correct.
- A 16-bit read advances the cursor by one byte, not two. At cursor 255, the
  high byte is the already-incremented wrapped cursor byte stored immediately
  after the ring in the original data segment. Native explicitly preserves
  this landmine.
- The range helpers do not add draws. Each expands to one 16-bit ring read.

### Host argument-evaluation order

The source-complete call-expression audit found four Anniversary calls that
place two shared-ring consumers in separate arguments of the same
`sub_1A1ED()` call. Turbo C++ evaluates the first textual random coordinate
before the second; native C++ must not rely on the host compiler choosing that
same otherwise-unspecified argument order.

| Anniversary call | Required order | Native status |
|---|---|---|
| `th03/main_06.cpp:132`, Reimu Extra | target X, then target Y | Explicit `target_x` / `target_y` locals in `extra_attack_add()` |
| `th03/main_06.cpp:488`, Mima Extra | target X, then target Y | Explicit locals |
| `th03/main_06.cpp:794`, Yumemi Extra | target X, then target Y | **Corrected in this parcel:** explicit locals prevent GCC's observed right-to-left argument evaluation |
| `th03/main/player/exatt.cpp:1293`, Kotohime Extra | transfer target X, then transfer target Y; its later Y target and speed remain subsequent statements | Explicit locals, followed by the two later reads |

The Yumemi defect was observed directly in TH3R04. With ring bytes
`E3 1A FE`, Anniversary assigned the first overlapping 16-bit value to X
(`2275` after modulo), while GCC evaluated the inline Y expression first and
assigned X from the next value (`538`). The focused
`test_yumemi_extra_attack_rng_argument_order()` locks the source order.

After stripping comments and walking balanced call expressions, no native
`frame.cpp` call expression retains two direct `randring_next16()` / `irand()`
descendants. The indirect consumers were also checked:

- `fireball_add()`, `extra_attack_add()`, transfer-pellet helpers, and random
  bullet-group expansion are invoked with already-evaluated or non-RNG
  arguments. Their internal draws begin only after argument evaluation.
- Calls that pass one random angle to `bullets_add_group()` are safe even for
  a random-expanding group: the angle argument is evaluated before entering
  the callee, then the callee performs its per-bullet draws in source order.
- `boss_yumemi_random_bullet_type()` is always assigned to a local before the
  following speed/group/angle draws; it is never nested beside another RNG
  consumer in one host call expression.

`SUB_CE5B()` (`th03/main/player/d3f9.cpp:277-295`) creates a visual record and
uses exactly one 16-bit shared-ring read for its angle. `SUB_CDBD()` and
`SUB_CE0C()` create related records but consume zero RNG.

`hitcircles_enemy_add()` (`th03/main/hitcirc.cpp:374-401`) uses two 16-bit
shared-ring reads, X first and Y second, unless
`hitcircles_enemy_add_do_not_randomly_center_within_hitbox` is set. Ordinary
Shot collision sets that flag and consumes zero draws. Charge Shot collision
does not set it and consumes two values for every overlap that calls the
helper. `hitcircles_player_add()` consumes zero RNG.

## Authoritative call order

Anniversary `th03/main/rndloop.cpp:sub_9778()` establishes this order for one
simulation frame. Rows without consumers are retained where they determine the
relative position of later consumers.

| Order | Anniversary operation | RNG-bearing cases | Native counterpart | Status |
|---:|---|---|---|---|
| 1 | Collision/template reset | None | Start of `gameplay_frame_prefix()` | Closed |
| 2 | P0 Extra, Boss, bomb-prefix; then P1 | Extra/Boss calls; Mima and Ellen bomb-prefix | `extra_attacks_update()`, `bosses_update()`, `bomb_character_update()` in P0/P1 order | Closed |
| 3 | Hit-circle age, ordinary Shots | None; ordinary hit effects are guarded to zero draws | `shots_update()`; visual ageing omitted | Closed |
| 4 | P0 then P1 Charge update | Ellen Hyper additions occur later in player update, not here | `chargeshots_update()` in player order | Closed |
| 5 | Round-start HUD | One interleaved ring/`IRAND` burst | `hud_start_update_prefix()` | Closed |
| 6 | Formations, bullets, enemies, fireballs | While the HUD is active, only formation creation is suppressed; carried bullets, enemies, fireballs, and later Gauge callbacks continue updating | `add_formation()`, `bullets_update()`, `enemies_update()`, `fireballs_update()` | Closed; the earlier whole-prefix early return was corrected after TH3R04/TH3R05 repeat-round evidence |
| 7 | P0 pellet Gauge then bullet Gauge; then P1 | Character Gauge calls below | `gauge_patterns_update()` P0 then P1 | Closed; combined dispatch is equivalent because only the active source wrapper can match |
| 8 | Input/demo I/O | None | `gameplay_input_latch()` | Closed for RNG |
| 9 | P0 then P1 player update | CPU AI, knockback, Ellen Hyper additions | `gameplay_frame_suffix_before_interstitial()` | Closed |
| 10 | Warning callbacks, then background callback | Background `IRAND` | `warning_frame_update()` then `background_update()` | Closed |
| 11 | Render: P0/P1 bombs, P0/P1 bosses, ordinary entities, P0/P1 Extra and Charge | Character bomb presentation RNG; no Boss-render RNG | `bombs_render_phase_update()`, `boss_render_phase_update()`, `fireballs_hittest()`, `extra_attacks_render_phase_update()` | Closed |
| 12 | Results, HUD, combos, round callback | None | Result/combo functions | Closed for implemented routes |
| 13 | Increment counters | Every 1024th `round_or_result_frame`: ring refill = 256 `IRAND` | End of `gameplay_frame_suffix_after_interstitial()` | Closed |

The warning interstitial samples input 27 times without running this frame
sequence or advancing the round counters. It consumes no RNG. The source's
optional every-other-render branch is never enabled by stock initialization.

## Round and lifecycle consumers

### Fresh `MAIN.EXE` round

The source order is `round_startup()` -> `sub_9EBF()` -> both
`set_callbacks_*()` calls -> `main_entry()`'s `farfp_20F20()`.

| Source | Exact consumption | Native | Status |
|---|---|---|---|
| `round_startup(): random_seed = resident->rand` | Seed only, zero draws | `initialize_first_round(): state.random_seed = init.resident_rand` | Closed |
| `sub_9EBF(): randring_fill()` | 256 `IRAND`, descending ring indices | `initialize_runtime_rings()` -> `randring_fill()` | Closed |
| `sub_9EBF()` player loop | P0 64 `IRAND`, then P1 64; each stores `((r & 3) << 6) + 0x3F` | `initialize_runtime_rings()`, player-major then slot-major | Closed |
| `enemy_formations_randomize()` | For each of 256 entries: one or more `IRAND % formation_count` until not equal to predecessor, then one `IRAND & 1`; first predecessor is `formation_count` | Same function's native loops | Closed for a nonempty valid formation table |
| `sub_D092()` for Reimu/Mima/Rikako P2 background | 48 records x 4 `IRAND` = 192 | `initialize_background_rng(type_a)` | Closed |
| `sub_D2E8()` for all other P2 characters | 50 records x 2 `IRAND` = 100 | `initialize_background_rng(type_b)` | Closed |

Both callback setters overwrite the shared background function pointer, so P2's
character determines which background initializer and per-frame consumer runs.
The native selection has the same last-writer result.

A `RuntimeSnapshot` intentionally bypasses all fresh-round draws. This is
correct only for a snapshot taken after Anniversary's startup initialization,
as V11/T3CASE snapshots are. The snapshot's restored `random_seed`, ring,
cursor, CPU rings, and formation rings then define the boundary. This
precondition should remain explicit in any V13 format API.

### Route-1 repeat

`main_entry()` stores `resident->rand = round_frame`, but `sub_A21F()` does not
copy it back to the live `random_seed`. It increments `round_id`, calls
`sub_9EBF()`, and continues from the existing live `IRAND` state. The repeat
sequence is therefore:

1. 256 `IRAND` calls for `randring_fill()`.
2. 64 CPU-ring calls for P0, then 64 for P1.
3. Formation entries 0..255, each with one or more type draws followed by one
   side draw. The first predecessor is 1 on every repeat.
4. Zero background-initialization draws; `farfp_20F20()` is not called again.

Native `initialize_repeat_round()` preserves this sequence: it carries the
live seed and ring cursor, calls `randring_fill()`, fills P0's CPU ring and then
P1's, and finally randomizes the formation rings with predecessor 1. It does
not call `initialize_background_rng()`. `Game::start_next_round()` exposes the
source's automatic route-1 transition as an explicit client boundary after a
`StepStatus::round_transition`; its RNG behavior is closed when that boundary
is taken.

### Stage handoff and final stage

For a non-repeat Story win, the next `MAIN.EXE` process seeds its new stream
from the outgoing `round_frame` stored in `resident->rand`. Native
`next_story_stage_init()` supplies that value as `resident_rand`, so its next
fresh startup sequence is correct. The final Yumemi result path changes bonus,
extend, and result timing but adds no RNG call. Its broader lifecycle behavior
is tracked in `PARITY_LIFECYCLE.md`.

## Shared gameplay consumers

### Bullet creation and transfer

Anniversary source is `th03/main/bullet/bullet.cpp:group_velocity_set()`,
`bullets_add()`, `bullets_add_next_from_p()`, and `bullets_update()`. Native is
`src/core/frame.cpp:bullets_add_group()`, `bullet_add_transfer_pellet()`,
`bullet_add_cloud_from_transfer()`, and `bullets_update()`.

| Source bullet group/path | Draw contract | Native status |
|---|---|---|
| `BG_1_RANDOM_ANGLE` | One ring draw if a free bullet slot is reached | Closed; stock-unused but implemented |
| `BG_RANDOM_ANGLE` | One draw per allocated bullet; source's `i >= count` test produces `count + 1` bullets | Closed |
| `BG_RANDOM_ANGLE_AND_SPEED` | Angle then speed-offset draw per allocated bullet; also `count + 1` bullets | Closed |
| `BG_RANDOM_CONSTRAINED_ANGLE_AIMED` | One angle-offset draw per allocated bullet; also `count + 1` | Closed |
| Initial `BF_PELLET_TRANSFER` add | After center validation and finding a free slot: target X, then target Y = two draws | Closed |
| Transfer reaches target | One constrained-angle draw if another free slot exists | Closed |
| Pellet destroyed by an explosion and transferred | Two initial-transfer draws if a free slot exists; later one arrival draw | Closed |

Pool exhaustion is part of the contract: source checks/fills a free physical
slot before each group draw. Native returns before consuming a value at the
same boundary. Mima's Boss pattern is the only stock producer of
`BG_RANDOM_ANGLE_AND_SPEED`; ordinary chain transfers are the stock producer of
the constrained aimed group.

### Fireballs

`th03/main/enemy/fireball.cpp:fireballs_add()` consumes two shared-ring values
after finding a free slot: fall speed, then target X (either full-playfield
modulo or player-relative masked offset depending on
`round_or_result_frame % 512`). The travel vector is deterministic and consumes
no random value. Native `fireball_add()` preserves that order and does not draw
when the pool is full. Red descendant creation follows the same function and
call contract.

### Charge-hit visuals and ordinary Shots

`th03/main/hitbox.cpp:hitbox_hittest()` disables random hit-circle centering
only around ordinary Shot Pair scanning. Native omits those zero-RNG visual
records. It then dispatches the owning character's Charge hittest. Native
`chargeshot_hittest()` consumes the two hit-circle values at every source call
site and in the same scan order; see the character ledger below.

### CPU and player state

| Source | Guard and order | Native | Status |
|---|---|---|---|
| `main/player/c433.cpp:sub_C433()` | After safety expires, always consume discarded `% 3`; after a small hit and gauge <= 80 pixels, consume `& 3` before possible bomb | `player_update_cpu()` | Closed |
| `main/player/c568.cpp:sub_C568()` | Apparent `% 3` retry draw is unreachable because `target_flags` has already been incremented before the `target_flags == 0` test | No draw in `cpu_safe_move()` | Closed; omission preserves reachable source behavior |
| `main/player/bomb.cpp:player_knockback_update()` | One `& 0x3F` draw exactly when `knockback_time == KNOCKBACK_FRAMES` | `player_knockback_update()` | Closed |
| `main/player/bb12.cpp:sub_BB12()` state 2 | For each of 64 left then 64 right particles: ring Y, ring angle, `IRAND % 3`, `IRAND % 3` | `hud_start_update_prefix()` | Closed, including interleaving |

CPU updates run P0 then P1. If both players are CPU, their unconditional and
conditional draws consequently share the same ring in that player order.

### Background presentation RNG

The background function is called after player updates and before render-phase
bombs.

| P2-selected kind | Source | Per active call | Transition | Native |
|---|---|---:|---|---|
| A: Reimu, Mima, Rikako | `sub_D135()` | 48 `IRAND` (24 GRCG records, then 24 sprite records) | Draws first, then switches after `round_frame > 128` | `background_update(type_a)` closed |
| B: all other characters | `sub_D340()` | 40 `IRAND` (24 GRCG records, then first 16 of 26 sprite records) | Draws first, then switches after `round_frame > 144` | `background_update(type_b)` closed |

The steady callbacks `sub_D1E7()` and `sub_D3F9()` consume zero RNG. Native
retains only the presentation-owned draws and phase transition.

## Character closure ledger

All character IDs below are unpaletted IDs 0 through 8. Shared Boss launch and
chooser draws apply in addition to each character's entries:

- `main_03.cpp:sub_F402()`: one `& 7` draw on a fresh launch.
- `main_03.cpp:sub_F52D()`: one or more `& 0x0F` draws after 80 chooser frames,
  rejecting candidates within the source's previous-pattern window.
- Native: `boss_launch_or_reverse()` then `boss_choose_pattern()`.

### 0: Reimu

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge, `main/player/ch_shot.cpp:chargeshot_hittest_reimu()` | Two draws for the first overlapping active group; odd-frame primary-charge gate can suppress the entire scan | `chargeshot_hittest()` character 0 | Closed |
| Extra add, `main_06.cpp:exatt_add_reimu()` | After a free slot: target X, then target Y = two | `extra_attack_add()` character 0 | Closed |
| Extra arrival, `main_06.cpp:exatt_update_reimu()` | On successful state-2 transfer: angle `& 0x1F`, then side `& 1` = two | `extra_attacks_update_reimu()` | Closed |
| Gauge, `ch_shot.cpp:gauge_pattern_reimu()` | Zero | `gauge_pattern_reimu()` | Closed |
| Boss orbit, `main_03r.cpp:reimu_10C4D()` | One direction draw when pattern frame is 0 | `boss_pattern_reimu_orbit_pellets()` | Closed |
| Boss orbit/exatt, `reimu_10E16()` | One initial orbit angle at frame 0; from frames 24 through 79, one ring angle whenever global `round_frame % 16 == 0` | `boss_pattern_reimu_orbit_exatt()` | Closed |
| Bomb renderer, `ch_shot.cpp:reimu_bomb()` | Two draws per rendered bomb frame `< 32`; prefix increments first, so stock frames 1..31 consume 62 total | `bombs_render_phase_update()` character 0 | Closed |

Reimu normal Shots and Hyper add no independent RNG beyond Charge overlaps.

### 1: Mima

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge, `ch_shot.cpp:chargeshot_hittest_mima()` | Two per overlapping active node, scanning all 15 nodes | `chargeshot_hittest()` character 1 | Closed |
| Extra add, `main_06.cpp:exatt_add_mima()` | After a free first-six slot: target X then target Y = two | `extra_attack_add()` character 1 | Closed |
| Extra arrival, `main_06.cpp:exatt_update_mima()` | One raw angle on successful transfer | `extra_attacks_update_mima()` | Closed |
| Gauge, `ch_shot.cpp:gauge_pattern_mima()` | One direction draw during init | `gauge_pattern_mima()` | Closed |
| Boss random burst, `main_03p.cpp:mima_FC6B()` | On every frame >48 divisible by 4, Bullet then Pellet `BG_RANDOM_ANGLE_AND_SPEED`: angle then speed per allocated bullet, `count + 1` each group. At frame 64, one additional explicit ring angle after both random groups | `boss_pattern_mima_random_bursts()` through `bullets_add_group()` | Closed |
| Boss orbit, `mima_FE2B()` | At frame 1: direction draw then initial angle draw | `boss_pattern_mima_orbit_clouds()` | Closed |
| Bomb prefix, `main/bullet/mbomb.cpp:mima_17043()` | One column-X draw at bomb frames 8,16,...,64 = eight total | `bomb_character_update()` character 1 | Closed |

Mima's Boss renderer and Hyper add zero RNG.

### 2: Marisa

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge, `ch_shot.cpp:chargeshot_hittest_marisa()` | Two for the first overlap among nodes 0,4,8 | `chargeshot_hittest()` character 2 | Closed |
| Extra add, `main/player/exatt.cpp:exatt_add_marisa()` | After a free slot: one target-X draw | `extra_attack_add()` character 2 | Closed |
| Gauge, `ch_shot.cpp:gauge_pattern_marisa()` | One `& 0x1F` initial-Y draw | `gauge_pattern_marisa()` | Closed |
| Boss rings, `main_03.cpp:marisa_F72D()` | One raw ring angle whenever pattern frame `& 0x1F` is zero | `boss_pattern_marisa_rings()` | Closed |
| Bomb renderer, `ch_shot.cpp:marisa_bomb()` | At frames 64,72,...,120: X then Y = two per event, 16 total | `bombs_render_phase_update()` character 2 | Closed |

Direct Marisa Boss beams and Hyper add zero RNG.

### 3: Ellen

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge add, `main_08.cpp:chargeshot_add_ellen()` | 32 raw angle draws in node order | `add_ellen_chargeshot()` | Closed |
| Hyper add, `ellen_hyper_1B6CA()` | One raw angle only after finding the first free node | `add_ellen_hyper_node()` | Closed |
| Charge hittest | Two per overlapping active node; every hit node is cleared | `chargeshot_hittest()` character 3 | Closed |
| Extra add, `player/exatt.cpp:exatt_add_ellen()` | Before scanning the pool: target X, target Y, direction bit, base angle = four even if no pair can spawn | `extra_attack_add()` character 3 | Closed |
| Gauge, `main_08.cpp:gauge_pattern_ellen()` | During each vertical paired-ring event: used angle draw, then a second overwritten/dead angle draw | `gauge_pattern_ellen()` | Closed |
| Boss orbit spawn, `main_03s.cpp:ellen_11547()` | One base angle at frame 48 and one at frame 96 | `boss_pattern_ellen_orbits()` | Closed |
| Bomb prefix, `player/chybomb.cpp:ellen_185AB()` | One raw angle for each vector whose state is FREE or clips this update; scan order 0..7 | `bomb_character_update()` character 3 | Closed |

Ellen's Boss renderer consumes zero RNG. Its bomb renderer only marks vector
slots FREE; the following frame's prefix consumes the corresponding respawn
draws.

### 4: Kotohime

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge hittest, `main_10.cpp` | Two for the successful overlap | `chargeshot_hittest()` character 4 | Closed |
| Extra add, `player/exatt.cpp:exatt_add_kotohime()` | After a free slot: target X, target Y, later Y target, vertical speed = four | `extra_attack_add()` character 4 | Closed |
| Extra firing, `exatt_update_kotohime()` | One raw angle at milestone 4 only for behavior 1; one at milestone 16 for either behavior | `extra_attacks_update_kotohime()` | Closed |
| Gauge, `main_10.cpp:gauge_pattern_kotohime()` | Init direction bit, then frame-0 target X = two | `gauge_pattern_kotohime()` | Closed |
| Boss pattern A, `main_03t.cpp:kotohime_11B51()` through `kotohime_11AC1(1)` | At the single final burst, one raw angle for each of `boss.params[1]` orbit points | `boss_pattern_kotohime_random_rings()` and `boss_kotohime_add_from_orbit()` | Closed |

Kotohime's other Boss patterns, Hyper, and bomb consume zero RNG.

### 5: Kana

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge hittest, `main_09.cpp` | Two for the first overlapping sampled trail point | `chargeshot_hittest()` character 5 | Closed |
| Extra add, `player/exatt.cpp:exatt_add_kana()` | Target X is drawn before pool scan and therefore always consumed; after a free slot, target Y then angle offset = two more | `extra_attack_add()` character 5 | Closed |
| Gauge, `main_09.cpp:gauge_pattern_kana()` | Zero | `gauge_pattern_kana()` | Closed |
| Boss cloud, `main_03v.cpp:kana_12E78()` | At each `pattern_frame % params[3] == 0`: angle offset, then side bit = two | `boss_pattern_kana_clouds()` | Closed |
| Boss spiral, `kana_12F06()` | One `& 3` direction-class draw at frame 16 | `boss_pattern_kana_spiral()` | Closed |

Kana's direct Extra helper, Hyper, and bomb consume zero RNG.

### 6: Rikako

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge hittest, `main_11.cpp` | Two per overlapping one of four nodes | `chargeshot_hittest()` character 6 | Closed |
| Extra add, `main_06.cpp:exatt_add_rikako()` | After a free slot: target X, behavior bit, speed = three | `extra_attack_add()` character 6 | Closed |
| Gauge, `main_11.cpp:gauge_pattern_rikako()` | Zero | `gauge_pattern_rikako()` | Closed |
| Boss spreads, `main_03w.cpp:rikako_133A5()` | One direction bit at frame 40 | `boss_pattern_rikako_spreads()` | Closed |
| Boss exatt/rings, `rikako_134AA()` | Frame 16 `SUB_CE5B`: one; frame 40 overwritten raw draw: one; frame 64 four raw ring angles | `boss_pattern_rikako_exatt_and_rings()` | Closed, including overwritten draw |
| Boss four spreads, `rikako_135A4()` | Whenever `phase_mod == 1`: angle first, then type bit | `boss_pattern_rikako_four_spreads()` | Closed |
| Bomb renderer, `main_05.cpp:rikako_bomb()` | One draw at frames 64,72,...,120 = eight total | `bombs_render_phase_update()` character 6 | Closed |

Rikako's Hyper and Boss renderer consume zero RNG.

### 7: Chiyuri

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge hittest, `main_07.cpp` | Reverse node scan; two per accepted overlap until a state-0 node stops the scan | `chargeshot_hittest()` character 7 | Closed |
| Extra add, `player/exatt.cpp:exatt_add_chiyuri()` | After finding a free even slot: top target X, then bottom target X = two | `extra_attack_add()` character 7 | Closed |
| Gauge, `main_07.cpp:gauge_pattern_chiyuri()` | Zero | `gauge_pattern_chiyuri()` | Closed |
| Boss state chooser, raw switch body in `main_03u.cpp:gba_boss_update_chiyuri()` | On entry to state 1 with render state 16: target-point `% 5`; after 48 frames: next-state `& 0x0F` | `boss_update_chiyuri()` | Closed |
| `chiyuri_121F5()` | Four `SUB_CE5B` draws at its first scheduled frame | `boss_pattern_chiyuri_cardinals()` via `boss_chiyuri_random_effect()` | Closed |
| `chiyuri_12355()` | Each scheduled event: `SUB_CE5B` then raw bullet angle = two | `boss_pattern_chiyuri_rings()` via `boss_chiyuri_random_effect()`, then its explicit draw | Closed |
| `chiyuri_12425()` | One `SUB_CE5B` whenever frame `& 7 == 0`; at frame 48, then one raw trail-ring angle | `boss_pattern_chiyuri_trail_ring()` via `boss_chiyuri_random_effect()`, then its explicit draw | Closed |
| `chiyuri_12498()` | Two `SUB_CE5B` draws at frame 32 and two at frame 64 | `boss_pattern_chiyuri_lattices()` via `boss_chiyuri_random_effect()` | Closed |
| Bomb renderer, `player/chybomb.cpp:chiyuri_bomb()` | Four `SUB_CE5B` draws at frames 64,80,96,112,128 = 20 total | `bombs_render_phase_update()` character 7 | Closed |

Chiyuri's Extra renderer, Charge renderer, and Boss renderer consume zero RNG.

### 8: Yumemi

| Family and source | Exact source consumption | Native counterpart | Status |
|---|---|---|---|
| Charge hittest, `main/bullet/bullet.cpp:chargeshot_hittest_yumemi()` | State 1 trigger consumes zero; every state-2 overlap consumes two | `chargeshot_hittest()` character 8 | Closed |
| Extra add, `main_06.cpp:exatt_add_yumemi()` | After a free regular slot: target X then target Y = two; this is one multi-RNG source call expression | `extra_attack_add()` character 8 | Closed with explicit sequencing; focused regression test |
| Direct Extra, `yumemi_1A95F()` | Zero | `extra_attack_add_direct_yumemi()` | Closed |
| Gauge, `bullet.cpp:gauge_pattern_yumemi()` | One `SUB_CE5B` at frames 0,8,16,24,32,40 = six total; firing adds no direct draw | `gauge_pattern_yumemi()` | Closed |
| Boss accel rings, `main_03q.cpp:yumemi_10324()` | Frame 1: initial angle then direction; frames 1,2,3: one `SUB_CE5B` each | `boss_pattern_yumemi_accel_rings()`; effects use `boss_yumemi_random_effect()` | Closed |
| Boss random groups, `yumemi_10405()` | `SUB_CE5B` at frames 1,5,9,13; every even emission from frame 32: speed, type, group = three | `boss_pattern_yumemi_random_groups()`; effects use `boss_yumemi_random_effect()` | Closed |
| Boss four rings, `yumemi_1050F()` | Each scheduled event: type first, then four ring angles = five | `boss_pattern_yumemi_four_rings()` | Closed |
| Boss crosses, `yumemi_105B5()` | Each late frame with low nibble 15: one `SUB_CE5B`, then zero-RNG direct Extra | `boss_pattern_yumemi_crosses()` via `boss_yumemi_random_effect()` | Closed |
| Boss rain, `yumemi_10669()` | Each scheduled event at frame >=32: 12 repetitions of X then angle = 24; early spread groups add zero | `boss_pattern_yumemi_rain()` | Closed |
| Bomb renderer, `player/yumemi_bomb.cpp:yumemi_bomb()` | One `SUB_CE5B` at frames 72,80,...,136 = nine total | `bombs_render_phase_update()` character 8 | Closed |

Yumemi's Charge, Extra, and Boss renderers consume zero RNG.

## Demo, VS, retry, and final-path classification

| Path | Anniversary RNG behavior | Native status |
|---|---|---|
| Demo/attract input and `demo_round_update()` | Zero direct draws. It feeds recorded inputs into the same active gameplay order. Fresh Demo startup runs the ordinary startup draws before applying speed/level overrides. | Closed for both fresh and snapshot-backed startup; `initialize_first_round()` applies the overrides and `round_mode_update()` implements the timer/exit callback. |
| 1P vs CPU | Same active-frame consumers; only P1/P2 control selection changes | Closed for an active first round |
| CPU vs CPU | Same, with CPU RNG in P0 then P1 order | Closed for an active first round |
| 1P vs 2P | Same with no CPU AI draws | Closed for an active first round |
| VS round 1 result | Result callbacks add zero RNG; periodic 1024-frame refill remains live until route return | Closed up to route reporting |
| VS round 2/3 after route 1 | `sub_A21F()` repeat sequence described above | Closed by `Game::start_next_round()` / `initialize_repeat_round()` |
| Story retry after loss | Same route-1 repeat sequence | Closed by the same explicit transition |
| Story next-stage process | New seed from outgoing `round_frame`, then fresh startup | Closed by `next_story_stage_init()` for supported ordinary wins |
| Final Yumemi result | No new RNG consumer | RNG-closed; broader result/bonus behavior is a lifecycle concern |

There is no Anniversary Practice mode. It must not be treated as a missing RNG
branch.

## Renderer-named callbacks checked

The following presentation-facing functions were read in full because omission
of their drawing alone is not evidence that their RNG may be omitted:

- `sub_BB12()` round-start HUD: both streams, retained.
- `sub_D092()`, `sub_D135()`, `sub_D2E8()`, `sub_D340()` backgrounds:
  `IRAND`, retained.
- Reimu, Marisa, Rikako, Chiyuri, and Yumemi bomb render callbacks: ring draws,
  retained.
- Mima and Ellen bomb prefix callbacks: ring draws, retained.
- `SUB_CE5B()` effects in Rikako, Chiyuri, and Yumemi Boss/Gauge/Bomb paths:
  retained.
- All nine Charge renderers and all nine Extra renderers: no RNG. Reimu's Extra
  renderer has a non-RNG frame mutation retained separately.
- All nine Boss renderers: no RNG. Boss defeat rendering has non-RNG score and
  lifecycle mutations retained separately.
- `SUB_CDBD()`, `SUB_CE0C()`, `hitcircles_player_add()`, and ordinary Shot hit
  circles: zero RNG.

## Discrepancy register

### Missing consumers

No source-level missing consumer has been found in Anniversary gameplay paths
represented by the native core. The unresolved TH3R01 sample-19328 cursor delta
is nevertheless dynamic evidence that this statement is not yet corpus-closed;
its first differing frame and consumer remain to be identified.

### Extra consumers

None found in the native active-round or fresh-round paths.

The unused native `randring_next8()` primitive is not a consumer until called.
`GameInit::random_seed` is likewise not an extra draw; source-correct fresh
initialization uses `resident_rand`, while runtime snapshots restore the live
seed explicitly.

### Order-ambiguous boundaries

1. `RuntimeSnapshot` must continue to mean “after all source startup RNG,” not
   a partial pre-background snapshot. Otherwise skipping both runtime-ring and
   background initialization would be ambiguous.
2. The combined native Gauge dispatcher is closed only while exactly one of
   the pellet or bullet source flags is active. Adding a client-side call that
   invokes both logical variants in one frame would duplicate consumers.
3. Bullet-pool exhaustion is RNG-visible. Refactors must retain “allocate slot,
   then draw,” including each individual member of a random group.
4. Ellen bomb FREE marking occurs in the render phase; its respawn draw belongs
   to the following frame's prefix. Moving it into one callback would shift the
   shared cursor relative to every intervening consumer.

## Outside `MAIN.EXE` active gameplay

These TH03 source matches are intentionally excluded from the native headless
gameplay closure, but are recorded for later application work:

| Source | Consumption | Classification |
|---|---|---|
| `th03/op_01.cpp` Story opponent selection | Rejection-loop `IRAND % RANDOM_OPPONENT_COUNT` after `irand_init(resident->rand)` | `OP.EXE`, pre-game selection; not a `MAIN.EXE` active-round consumer |
| `th03/formats/score_es.cpp` | Three `IRAND` calls for score-file keys/unknown field | Persistent score encoding, application layer |
| `th03/mainl/stf_bclr.cpp` | Each new staff-roll flake consumes position, angle, length, and cel = four `IRAND` calls | `MAINL.EXE` staff roll, presentation/application layer |

The staff-roll position branch chooses either left or top but consumes exactly
one position draw in either branch, followed by the other three draws.

## Implementation and verification checklist

- [x] Preserve master.lib LCG and 32-bit wrap.
- [x] Preserve descending 256-byte fill and shared one-byte cursor.
- [x] Preserve cursor-255 adjacent-byte landmine.
- [x] Close every active-round direct `randring*` call.
- [x] Close indirect random bullet groups and pool-exhaustion guards.
- [x] Close every gameplay-reachable `SUB_CE5B()` call.
- [x] Close every randomly centered enemy hit-circle call and prove zero-RNG
  ordinary Shot calls.
- [x] Preserve P0/P1 callback and CPU order.
- [x] Preserve renderer-owned HUD/background/bomb RNG.
- [x] Preserve fresh-round startup order and counts.
- [x] Implement route-1 repeat RNG without reseeding or background reset.
- [x] Add focused repeat tests that compare the full post-reset random seed,
  ring bytes/cursor, CPU rings/cursors, and formation rings/cursors.
- [x] Preserve fresh Demo startup overrides and its zero-RNG timer/exit callback.
- [ ] Re-run this static search after every new character/application callback;
  replay matching validates the ledger but must not be used to discover omitted
  source consumers.
