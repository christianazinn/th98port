# Static gameplay parity audit: characters 4-8

Audit date: 2026-08-08

This is a source-complete static audit of Kotohime, Kana, Rikako, Chiyuri,
and Yumemi against the Anniversary ReC98 source. It covers ordinary shots,
charge shots, extra attacks, gauge attacks, Boss Attacks, bombs and hypers,
including non-drawing work hidden in functions named as render callbacks.
It deliberately covers code paths beyond the currently exercised TH3R03
route.

The Anniversary source was read from
`.scratch/port-refs/oracle_anniversary` at commit `3e2381d4`. The port was
based on `33ad4189695236f72ca9bbe9cc20dc15d56de84e` plus the dirty gameplay
work in progress on 2026-08-08. Because the port was being edited in
parallel, function names below are the authoritative locations; line
numbers record the snapshot examined during the audit and can drift.

No emulator was run for this audit. Conclusions below come from complete
source/caller comparison and focused native unit tests. The one production
correction made during this audit is identified explicitly.

## Classification

- **Complete** means that gameplay state, collision, fixed-point arithmetic,
  callback timing, and shared-RNG consumption were found equivalent. It does
  not imply pixel-complete rendering.
- **Partial** means a usable counterpart exists but some source behavior is
  absent or semantically different.
- **Missing** means there is no character-specific counterpart or dispatch.
- **Presentation-only** means omitted drawing or animation was checked and
  does not consume the shared RNG ring or affect gameplay state.
- **Presentation-RNG** means visually motivated code consumes the shared RNG
  ring and therefore must remain in the deterministic simulation even in a
  headless build.

## Executive inventory

This table reports the post-fix snapshot at the end of the audit. The fixes
found while auditing are listed separately below.

| Character | Ordinary shots / hyper | Charge | Extra attack | Gauge attack | Boss Attack | Bomb RNG |
|---|---|---|---|---|---|---|
| Kotohime (4) | Complete | Complete | Complete | Complete | Complete | Complete; omitted effects are presentation-only |
| Kana (5) | Complete | Complete | Complete | Complete | Complete | Complete; omitted effects are presentation-only |
| Rikako (6) | Complete, including special hyper | Complete | Complete | Complete | Complete | Complete |
| Chiyuri (7) | Complete | Complete | Complete | Complete | Complete | Complete, including presentation-RNG |
| Yumemi (8) | Complete | Complete | Complete | Complete | Complete | Complete, including presentation-RNG |

All five character families are dispatched in the current dirty snapshot.
The corrective audit found one P0 gameplay discrepancy: Kotohime's state-0
Boss Attack renderer advances the shared orbit angle and radius that later
attack patterns consume. That renderer-owned mutation was absent and is now
implemented and regression-tested.

## Severity-ranked discrepancies

### Resolved P0: Kotohime materialization renderer mutated gameplay state

Anniversary `th03/main_03u.cpp::gba_boss_render_kotohime()` calls
`kotohime_12103()` in state 0. On every even `round_frame`, and on every
materialization frame from `0x40` onward, that helper:

1. adds 2 to `byte_1F355` with byte wrap; and
2. writes `word_1F356 = (200 - pattern_frame * 2) << 4`.

The subsequent Kotohime attack update in `th03/main_03t.cpp` consumes those
globals as its orbit angle and radius. This is a source landmine: the
apparently presentational callback seeds later gameplay. The current port's
`src/core/frame.cpp::boss_render_phase_update()` now performs the same writes
to `boss.pattern_angle` and `boss.anchor_x` at the same suffix-phase point.
`tests/test_main.cpp::test_kotohime_boss_materialization_renderer_state()`
checks the full state-0 materialization sequence, the exact accumulated
angle of 134, the final radius, and the odd-frame pre-`0x40` skip.

### No remaining P0 discrepancy found in characters 4-8 render callbacks

The audit below distinguishes gameplay-affecting mutations from persistent
presentation state and scratch pointers. The latter are intentionally not
added to the architecture-neutral headless state if no later gameplay or
RNG consumer reads them.

### Lower priority: stale-field presentation differences

- Kana's direct extra-attack helper is
  `th03/main/player/exatt.cpp:859` `KANA_19896()`. Anniversary initializes
  selected fields without clearing the whole slot; the port's direct helper
  path clears/normalizes more state and explicitly records the other player.
  The active update does not read the stale source fields for gameplay, so
  this is presently a low-risk presentation-fidelity difference.
- Rikako's direct helper `th03/main_06.cpp:1254` `RIKAKO_1B006()` similarly
  leaves unused bytes that the port normalizes. No gameplay reader of those
  bytes was found.

These should not be “fixed” blindly: the Anniversary behavior is a source
landmine/stale-state property, and a future pixel-perfect renderer may make
the old values visible.

## Complete renderer-side-effect ledger

This section records every non-drawing side effect reachable through the
Charge, Extra, Gauge, Bomb, and Boss presentation callbacks for characters
4-8. Hardware clip rectangles, GRCG/EGC modes, palette writes, sound calls,
and sprite scratch offsets are listed when they persist, but are classified
separately from simulation state.

The three common effect helpers have persistent presentation side effects:
`SUB_CDBD()`, `SUB_CE0C()`, and `SUB_CE5B()` each advance the 12-slot
`byte_20EA6` visual-record ring cursor `byte_20F1E` and overwrite one record's
type, age, position, radius, and radius delta. `SUB_CE5B()` additionally
writes a random angle and therefore consumes one shared-ring value. The
later `sub_CEB2()`/`sub_CEE0()` callbacks mutate and draw those records but
never feed gameplay or gate future RNG consumption. The native core can
therefore omit the records/cursor while retaining every `SUB_CE5B()` draw.

### Charge render callbacks

| Character | Anniversary callback and helper | Non-drawing writes | Native handling | Classification |
|---|---|---|---|---|
| Kotohime | `th03/main_10.cpp::chargeshot_render_kotohime()`, `kotohime_chargeshot_1C1E9()` | Reassigns scratch pointer `word_1FE6A`; clip state | No simulation write | Presentation-only; update and hittest each establish their own pointer |
| Kana | `th03/main_09.cpp::chargeshot_render_kana()`, `kana_chargeshot_1BDF8()` | Reassigns and advances scratch pointer `word_1FD8C`; overlap/clip state | No simulation write | Presentation-only |
| Rikako | `th03/main_11.cpp::chargeshot_render_rikako()`, `rikako_chargeshot_1C62A()` | Reassigns and advances scratch pointer `word_20E86`; clip state | No simulation write | Presentation-only |
| Chiyuri | `th03/main_07.cpp::chargeshot_render_chiyuri()`, `chiyuri_chargeshot_1B35F()` | Reassigns and walks scratch pointer `word_1F51A`; clip state | No simulation write | Presentation-only |
| Yumemi | `th03/main/bullet/bullet.cpp::chargeshot_render_yumemi()`, `yumemi_chargeshot_16BB5()` | Clip/drawing state only | No simulation write | Presentation-only |

None of these functions calls `IRAND`, `randring*`, `SUB_CE5B()`, hit tests,
collision-map functions, score/control helpers, or any helper that mutates a
field later read by gameplay.

### Extra-attack render callbacks

| Character | Anniversary callback and helper | Non-drawing writes | Native handling | Classification |
|---|---|---|---|---|
| Kotohime | `th03/main/player/exatt.cpp::exatt_render_kotohime()`, `kotohime_19E2A()` | Reassigns/walks `word_2028A`; clip state | No simulation write | Presentation-only |
| Kana | `th03/main/player/exatt.cpp::exatt_render_kana()`, `kana_198DD()` | Reassigns/walks `word_2028A`; clip state | No simulation write | Presentation-only |
| Rikako | `th03/main_06.cpp::exatt_render_rikako()`, `rikako_1B05A()` | Reassigns/walks `word_2028A`; clip state | No simulation write | Presentation-only |
| Chiyuri | `th03/main/player/exatt.cpp::exatt_render_chiyuri()`, `chiyuri_1905A()` | Reassigns/walks `word_2028A`; GRCG state | No simulation write | Presentation-only |
| Yumemi | `th03/main_06.cpp::exatt_render_yumemi()`, `yumemi_1A9B0()` | Reassigns/walks `word_2028A`; clip/GRCG/EGC state | No simulation write | Presentation-only |

All entity state increments, collision, bullet creation, and random reads for
these five characters occur in their update callbacks. In particular, their
renderers do **not** share Reimu's renderer-owned extra-attack frame increment;
`src/core/frame.cpp::extra_attacks_render_phase_update()` correctly acts only
on character 0.

### Gauge callbacks

Gauge callbacks combine simulation and visual calls in the original and run
in the gameplay prefix rather than the later render pass.

| Character | Anniversary source | Simulation side effects | Presentation side effects | Native counterpart/status |
|---|---|---|---|---|
| Kotohime | `th03/main_10.cpp::gauge_pattern_kotohime()` | Init frame/count/direction/type; one direction RNG read; frame-0 target-X RNG; direct extra attack; frame increment/clear | Palette restore `sub_A3A8()` on clear | `gauge_pattern_kotohime()` complete |
| Kana | `th03/main_09.cpp::gauge_pattern_kana()` | Init period/speed/X; symmetric bullets; X motion; frame increment/clear | Two `SUB_CE0C()` records per firing; `sub_A3A8()` on clear; no RNG | `gauge_pattern_kana()` complete |
| Rikako | `th03/main_11.cpp::gauge_pattern_rikako()` | Init period/speed; four edge bullets; frame increment/clear | Two `SUB_CE0C()` records per firing; `sub_A3A8()` on clear; no RNG | `gauge_pattern_rikako()` complete |
| Chiyuri | `th03/main_07.cpp::gauge_pattern_chiyuri()` | Init duration/X; X step; odd-frame aimed pairs; post-increment clear | Two `SUB_CE0C()` records per 16-frame X step; `sub_A3A8()` on clear; temporary `bullet_template.is_animated=false` is visual; no RNG | `gauge_pattern_chiyuri()` complete |
| Yumemi | `th03/main/bullet/bullet.cpp::gauge_pattern_yumemi()` | Init count/speed; staged aimed rings at frames 16-56; frame increment/clear | One `SUB_CE5B()` record/shared-ring read at frames 0, 8, 16, 24, 32, and 40; `sub_A3A8()` on clear | `gauge_pattern_yumemi()` complete, including all six reads |

`sub_A3A8()` is not a gauge refill: its complete definition in
`th03/main/hitc_pal.cpp` copies one player's three palette bytes and marks
the palette changed. Omitting it from the headless state is intentional.

### Bomb render callbacks

| Character | Anniversary source | Gameplay/deterministic side effects | Omitted persistent presentation state | Native counterpart/status |
|---|---|---|---|---|
| Kotohime | `th03/main/player/chybomb.cpp::kotohime_bomb()` | Foreground shift at frames 64-127, reset from 128 | Palette/tone and `word_1FE56`, which only positions later `SUB_CDBD()` effects | `bombs_render_phase_update()` complete; no shared RNG |
| Kana | `th03/main/player/chybomb.cpp::kana_bomb()` | Foreground shift at frames 64-127, reset from 128 | Palette and `angle_1FBD4`, used only by later `SUB_CDBD()` effects | Complete; no shared RNG |
| Rikako | `th03/main_05.cpp::rikako_bomb()` | Foreground shift at frames 64-127; one `randring` read at each frame divisible by 8 in that interval; reset from 128 | Palette and `word_220EC` line radius | Complete, including eight shared-ring reads |
| Chiyuri | `th03/main/player/chybomb.cpp::chiyuri_bomb()` | Foreground shift at frames 64-143; four `SUB_CE5B()` reads at frames 64, 80, 96, 112, 128; reset from 144 | Palette/tone | Complete, including 20 shared-ring reads |
| Yumemi | `th03/main/player/yumemi_bomb.cpp::yumemi_bomb()` | Foreground shift for frames 65-143; one `SUB_CE5B()` read at frames 72 through 136 in steps of 8; reset from 144 | Palette/tone | Complete, including nine shared-ring reads |

The native call remains after background presentation RNG and before Boss
render callbacks, preserving cross-callback RNG order. `SUB_CDBD()` and
`SUB_CE0C()` consume no shared RNG; `SUB_CE5B()` consumes exactly one ring
value.

### Boss render callbacks

All five callbacks first gate on `pid_current == gba_boss_launched_by` and
select the opponent playfield clip. Their common defeated-state call is
`th03/main_03.cpp::sub_F58C()` -> `sub_F1FA()`: it multiplies
`pattern_frame` by 8, adds 2,560 score at signed length >=200, changes state
from `0xFF` to 0, and clears `gba_boss_launched_by`. Native
`boss_render_phase_update()` performs this control/score work once at the
same render-phase position.

| Character | Anniversary renderer | Active/materialization non-drawing writes | Native handling | Classification |
|---|---|---|---|---|
| Kotohime | `th03/main_03u.cpp::gba_boss_render_kotohime()`, `kotohime_120A0()`, `kotohime_12103()` | State 0 transiently sets `byte_1F353=1` around the orbit draw, adds 2 to `byte_1F355`, writes `word_1F356`, then restores `byte_1F353=0` | Angle/radius in `boss_render_phase_update()`; transient draw mode omitted | Gameplay-affecting and complete after this audit |
| Kana | `th03/main_03w.cpp::gba_boss_render_kana()`, `kana_13174()`, `kana_13223()` | None beyond drawing/clip state | None required | Presentation-only |
| Rikako | `th03/main_03x.cpp::gba_boss_render_rikako()`, `rikako_137CF()`, `rikako_138B3()` | None beyond drawing/clip state | None required | Presentation-only |
| Chiyuri | `th03/main_03v.cpp::gba_boss_render_chiyuri()`, `chiyuri_12A10()`, `chiyuri_12ADD()` | State 0 conditionally writes `word_1F356` and adds/subtracts 4 from `byte_1F355`; defeated state restores palette | Omitted | Presentation-only: state 0 values are consumed only by `chiyuri_1295E()` in that renderer and are overwritten at the state-0 transition before gameplay uses the shared bytes |
| Yumemi | `th03/main_03r.cpp::gba_boss_render_yumemi()`, `yumemi_108CA()`, `yumemi_10A17()` | None beyond drawing/clip/overlap state | None required | Presentation-only |

No character-4-through-8 Boss renderer directly consumes RNG. Their
presentation-RNG calls occur in the attack update functions and are retained
there. Chiyuri's state-0-only visual sequence would end with a wrapped angle
of 124 and radius 8 after render frame 95, but the frame-96 update writes the
shared bytes to 16 before entering active state and then applies its common
tail. Consequently those materialization values cannot affect bullets,
collision, score, control, or later RNG order.

## Cross-cutting ordinary shots and hypers

### Ordinary shots

- Anniversary: `th03/main/player/shot.cpp:272` `shots_add()` and line 334
  `shots_update()`.
- Port: `src/core/frame.cpp:1246` `shots_add()` and line 1298
  `shots_update()` in the initial snapshot.

The shared implementation is complete for characters 4-8: shot-pair mode,
global slot allocation, fixed-point velocity, and the `-16` raw-unit expiry
threshold agree.

### Hyper callbacks

- Anniversary: `th03/main/player/bomb.cpp:109` `hyper_kotohime()`, line 122
  `hyper_chiyuri()`, line 135 `hyper_yumemi()`, line 148 `hyper_kana()`, and
  line 161 `hyper_rikako()`.
- Rikako helpers: `th03/main_11.cpp:79` `rikako_1C497()` and line 87
  `rikako_hyper_1C4B4()`.
- Port: `src/core/frame.cpp::player_update_begin()` (about line 2979) and
  `movement_velocity()` (about line 1888), with
  `add_rikako_chargeshot()` at about line 2128.

Post-fix status is complete:

- Kotohime, Kana, Chiyuri, and Yumemi use four shot pairs while the hyper
  callback is active and add `TO_SP(2)` to aligned player velocity and
  `TO_SP(1) + 8` to diagonal player velocity.
- Rikako creates a state-2 charge following the player while
  `gauge_avail > TO_SP(250)`, subtracts `TO_SP(1)` from aligned velocity and
  8 raw units from diagonal velocity, and clears the charge when hyper ends.
- The standby transition remains one callback/frame step, matching the
  source function-pointer transition rather than applying the character
  effect immediately on the activation frame.

These behaviors were absent or incomplete when the audit started and were
added by the main implementation agent while the audit was running.

## Kotohime (character 4)

### Charge shot: complete

- Anniversary: `th03/main_10.cpp:44` add, line 60 update, line 109 hittest,
  and line 150 render.
- Port: `src/core/frame.cpp:2183` `add_kotohime_chargeshot()`, line 1668
  `chargeshot_update_kotohime()`, and the character-4 branch of
  `chargeshot_hittest()` around line 1812.

The port preserves activation and position, initial `vy = -0x10`, upward
movement, clearing at `y <= -0x100`, acceleration by `-2`, gauge reset, the
40-pixel X and inside-Y hit test, damage 3, and one-hit semantics. A
successful hit consumes the two shared-ring values used by
`hitcircles_enemy_add()` (`th03/main/hitcirc.cpp:374-401`). The omitted
charge renderer consumes no RNG.

### Extra attack: complete

- Anniversary: `th03/main/player/exatt.cpp:1275` `exatt_add_kotohime()`,
  line 1319 `kotohime_19DD3()`, helpers at lines 1354, 1400, 1418, and
  1441, update at line 1464, render at line 1574.
- Port: character-4 branch of `src/core/frame.cpp::extra_attack_add()`
  (initially around line 762), `extra_attacks_update_kotohime()` (about line
  4156), and the Kotohime branch of `extra_attack_add_direct()` (about line
  4466).

Preserved behavior includes:

- Ordinary allocation from slots 0-7.
- Random target X followed by conditional random target Y, base frame
  `0x5A`, target-Y range beginning at 96 pixels, upward velocity 16-47, and
  behavior 0.
- Gauge/direct allocation using slot 8 if free and otherwise overwriting
  slot 9, starting from the player center with base frame `0x78`, target Y
  `0x0C00`, rise `0x20`, and behavior 1.
- All ten slots updating, 16-pixel hit radius, 12-pixel collision-map
  footprint, transfer/delay state, and both attack milestones.
- Behavior 0's ten-bullet ring at milestone 16, speed 24, and one random
  angle.
- Behavior 1's milestone-4 speeds 16, 18, 20, and 22 and milestone-16 speeds
  16 through 34. The source helper overwrites the misleading speed assigned
  before the milestone-4 helper call.

The `SUB_CDBD()` milestone effects consume no RNG. During this audit the
port's slot selector was corrected from a non-rebinding C++ reference to a
real selected pointer, and the milestone-4 effective speeds were corrected.

### Gauge attack: complete

- Anniversary: `th03/main_10.cpp:173` `gauge_pattern_kotohime()` and wrappers
  at lines 219-231.
- Port: `src/core/frame.cpp::gauge_pattern_kotohime()` (about line 2584) and
  its character-4 dispatch in `gauge_patterns_update()`.

Initialization sets frame 0 and `count = level / 2 + 4`, consumes one random
direction, and records pellet versus bullet type. Each frame-0 emission
consumes one random target-X in the 64-224-pixel interval and invokes the
direct extra attack. The callback clears at frame `0x80`. The source's
`sub_A3A8()` presentation call consumes no RNG.

### Boss Attack: complete

- Anniversary attack update:
  - `th03/main_03t.cpp:180` `kotohime_11A6D()`
  - line 201 `kotohime_11AC1()`
  - line 222 `kotohime_11B51()`
  - line 254 `kotohime_11BC6()`
  - line 295 `kotohime_11C5F()`
  - line 333 `kotohime_11D1A()`
  - line 395 `gba_boss_update_kotohime()`
- Anniversary rendering: `th03/main_03u.cpp:57` `kotohime_11FE4()`, line 86
  `kotohime_120A0()`, line 106 `kotohime_12103()`, and line 121
  `gba_boss_render_kotohime()`.
- Port:
  - `src/core/frame.cpp::boss_kotohime_add_from_orbit()` (about line 5174)
  - `boss_pattern_kotohime_random_rings()` (about line 5207)
  - `boss_pattern_kotohime_aimed_spreads()` (about line 5245)
  - `boss_pattern_kotohime_pellet_rings()` (about line 5294)
  - `boss_pattern_kotohime_bursts()` (about line 5330)
  - `boss_update_kotohime()` (about line 5396)
  - dispatch in `bosses_update()`
  - HP initialization in `src/core/round_start.cpp:259-269`

The active-state renderer is presentation-only and consumes no shared RNG.
The state-0 helper `kotohime_12103()` is not presentation-only: its orbit
angle/radius writes are implemented in `boss_render_phase_update()`.

#### Kotohime Boss Attack translation checklist

- [x] Initialize center `(144, 80)`, velocity `(-0x20, 0)`, HP `0x6E`, state
  0, motion angle 0, render state 0, and pattern index 0.
- [x] Derive the seven level parameters exactly:
  `L/2+0x10`, `L/3+6`, `L/2+0x10`, `L/2+0x20`, `L/2+0x18`,
  `0x20-L/2`, and `L/2+8`.
- [x] Run the shared launch/reversal helper before character-specific logic.
- [x] Recalculate level parameters only on a fresh launch.
- [x] Gate the active callback by the launching player and target the other
  player.
- [x] Increment `pattern_frame` before the state switch.
- [x] In state 0, return until exactly frame 100; at frame 100 reset the frame
  and enter state 1, but do not execute the newly selected pattern in that
  same switch dispatch.
- [x] Preserve all chooser mappings: A states 2-6, B states 7-9, C states
  12-14, D states 10, 11, and 15-17, exit state `0x80`, and return state
  `0xFF`.
- [x] Pattern A: frame-1 render/radius initialization; angle `+4` during
  expansion; move and add `0x20` radius while radius is at most 64; afterward
  switch render state, advance render frame, change angle by `+2`, then use
  one random angle per orbit point and emit `BG_16` rings of count 10 at
  speed 24 before reset.
- [x] Pattern B: initialize render/radius/angle; angle `-2`; expand through
  radius 64; afterward add 8 radius and `+2` angle, yielding the source's net
  angle behavior; emit two narrow aimed bullets at parameter 3 speed and five
  medium aimed bullets at parameter 2 speed from each orbit point.
- [x] Pattern C: move the boss; on `frame % p5 == 1`, emit five pellet rings
  of count 4 and speed `p4`, using the exact offsets
  `(-48,0), (-24,+12), (0,+24), (+24,+12), (+48,0)` and low byte of frame as
  angle; reset at `0x60`.
- [x] Pattern D: initialize render/radius without resetting the pattern
  angle; angle `-4` while expanding; afterward add 8 radius and subtract 2
  angle; for every outer orbit point and inner index, emit a bullet with
  `angle = inner*0x80/p6 + outer_angle + 0x40` and
  `speed = inner*0x30/p6 + 0x10`.
- [x] After non-returning states, write the 64x48-pixel boss collision map,
  use a 32-pixel damage radius, ignore explosion shots, subtract damage with
  16-bit wrap semantics, and run defeat handling.
- [x] Preserve shared-RNG count and order. Computing a pure polar coordinate
  before requesting a random angle is harmless only because that coordinate
  calculation itself has no state or RNG side effect.
- [x] Preserve `kotohime_12103()` in the suffix render phase: on even global
  round frames or materialization frames at least `0x40`, add 2 to the shared
  byte angle and write `(200 - frame*2) << 4` as the shared radius.
- [x] Keep only the remaining graphics helpers out of the headless state
  after confirming that they consume no RNG and write no gameplay state.

### Hyper and bomb: complete for deterministic gameplay

The hyper behavior is covered in the cross-cutting section. Kotohime's bomb
is `th03/main/player/chybomb.cpp:481-558`; its omitted `SUB_CDBD()` effects
consume no RNG.

## Kana (character 5)

### Charge shot: complete

- Anniversary: `th03/main_09.cpp:42`, 87, 210, and 290.
- Port: `src/core/frame.cpp:2109` `add_kana_chargeshot()`, line 1481
  `chargeshot_update_kana()`, and the Kana branch of
  `chargeshot_hittest()`.

The four 13-element trails, starting angles `0x90`, `0xB8`, `0xC8`, and
`0xF0`, initial speed 30, deceleration/bending, frame-`0x28` transition to
speed 80, tail-based clearing, collision samples 0/4/8/12, first-hit exit,
and two hit-circle RNG reads are preserved. Rendering consumes no RNG.

### Extra attack: complete for gameplay

- Anniversary: `th03/main/player/exatt.cpp:815` add, line 859 direct helper,
  line 942 update, line 1025 render.
- Port: character-5 branch of `extra_attack_add()`,
  `extra_attacks_update_kana()` (about line 4114), and
  `bullet_add_kana_extra_attack()` (about line 507).

Random target-X consumption occurs even when the buffer is full, while the
target-Y draw occurs only after a slot is found. Random angle offset, side
flip, acceleration, 16-frame firing cadence, clipping, and delay agree. The
stale-field difference is documented above.

### Gauge attack: complete

- Anniversary: `th03/main_09.cpp:333` and wrappers at lines 406-418.
- Port: `gauge_pattern_kana()` (about line 2645).

The period `8 - level/4`, speed `level + 0x30`, symmetric aimed singles,
`x = +/-0x80`, and clear at frame `0x80` agree. Its `SUB_CE0C()` calls do not
consume RNG.

### Boss Attack: complete

- Anniversary attack: `th03/main_03v.cpp:209` `kana_12BFB()`, lines 230,
  266, 332, and 359 for its patterns, and line 408
  `gba_boss_update_kana()`.
- Anniversary rendering: `th03/main_03w.cpp:56`, 94, and 128.
- Port: `boss_pattern_kana_accel_ring()` (about line 5473),
  `boss_pattern_kana_rings()` (about line 5507),
  `boss_pattern_kana_clouds()` (about line 5566),
  `boss_pattern_kana_spiral()` (about line 5596), and
  `boss_update_kana()` (about line 5640).
- HP: `0x64`, initialized in `src/core/round_start.cpp:264-265`.

Pattern parameters, state dispatch, direct Kana extra attacks, accelerating
rings, collision, and all gameplay RNG agree. Omitted `SUB_CE0C()` and
`SUB_CDBD()` effects are presentation-only.

### Hyper and bomb: complete for deterministic gameplay

Kana's hyper is covered above. Her bomb is
`th03/main/player/chybomb.cpp:396-479`; its omitted helpers consume no shared
RNG.

## Rikako (character 6)

### Charge shot and special hyper: complete

- Anniversary: `th03/main_11.cpp:47` add, line 79 special hyper add, line 87
  hyper clear, line 92 update, line 186 hittest, line 251 render.
- Port: `add_rikako_chargeshot()` (about line 2128),
  `chargeshot_update_rikako()` (about line 1523), Rikako branch of
  `chargeshot_hittest()`, and hyper handling in `player_update_begin()`.

The ordinary charge starts at radius `0x80`, uses angles `0x20 + 0x40*i`,
raises its origin, follows the player in state 2, alternates the angle by
`+/-8`, expands, clears state 1 after frame 144, pins non-state-1 frame to
`0x20`, tests all four 12-pixel nodes, and consumes two RNG reads per hit.
The special hyper lifecycle and movement penalties are also preserved.

### Extra attack: complete for gameplay

- Anniversary: `th03/main_06.cpp:1214` add, line 1254 direct helper, line
  1340 update, line 1445 render.
- Port: character-6 branch of `extra_attack_add()`,
  `extra_attacks_update_rikako()` (about line 4346), and
  `extra_attack_add_direct_rikako()` (about line 4495).

The first-8 ordinary allocator, random target X, random behavior bit, initial
angle `0x40`, speed 32-47, 14-slot update, behavior-0 homing window from
frames 64 through 80, speed increment, signed delta divided by 8, collision,
and map writes agree. The stale-field normalization difference is noted
above.

### Gauge attack: complete

- Anniversary: `th03/main_11.cpp:286` and wrappers at lines 344-356.
- Port: `gauge_pattern_rikako()` (about line 2866).

Period `0x10 - level/2`, speed `level + 0x30`, four aimed bullets from the
edges, frame-`0x80` termination, and non-RNG `SUB_CE0C()` effects agree.

### Boss Attack: complete

- Anniversary attack: `th03/main_03w.cpp:153` `rikako_1334D()`, line 175
  `rikako_133A5()`, line 230 `rikako_134AA()`, line 288
  `rikako_135A4()`, and line 343 `gba_boss_update_rikako()`.
- Anniversary rendering: `th03/main_03x.cpp:25`, 69, and 106.
- Port: `boss_pattern_rikako_spreads()` (about line 5715),
  `boss_pattern_rikako_exatt_and_rings()` (about line 5749),
  `boss_pattern_rikako_four_spreads()` (about line 5795), and
  `boss_update_rikako()` (about line 5836).
- HP: `0x6E` in `src/core/round_start.cpp:259-263`.

The port preserves the shared-RNG draw hidden in the presentation call at
pattern frame `0x10`, the apparently overwritten random draw at frame
`0x28`, four random angles at frame `0x40`, and the later phase angle/type
draws. The complete source jump table dispatches states 2-5 to spreads,
states 7-12 to Extra Attacks/rings, and state 6 plus states 13-17 to the
four-spread pattern. A former native off-by-one grouping at states 6, 7, and
13 was corrected after a bounded TH3R01 trace showed that source state 6
consumed two draws and added eight bullets at global frame 19,326. The full
TH3R01 oracle is now exact for 964/964 rows and 135,215 samples. Omitted
animation-ramp state does not feed gameplay or shared RNG.

### Bomb RNG: complete

Anniversary `th03/main_05.cpp:23-124` consumes one random value every eight
frames from 64 through 120 before drawing a repeated visual pattern. The
character-6 branch of `bombs_render_phase_update()` preserves that read and
the foreground shift.

## Chiyuri (character 7)

### Charge shot: complete

- Anniversary: `th03/main_07.cpp:55`, 83, 171, and 241.
- Port: `add_chiyuri_chargeshot()` (about line 2149),
  `chargeshot_update_chiyuri()` (about line 1576), and the Chiyuri branch of
  `chargeshot_hittest()`.

The port preserves eight nodes staggered by `i*4+2`, node-0 initialization,
regular-shot disabling, reverse 7-to-0 update order, capturing the player's
position on activation, upward movement by 14 pixels, re-enabling regular
shots only when node 7 leaves above `-0x300`, gauge reset, reverse collision
order, stopping at the first state-0 node, the source's asymmetric hitbox,
and two RNG reads per hit. Rendering consumes no RNG.

### Extra attack: complete

Source locations are `th03/main/player/exatt.cpp:48-338`; the port is
`src/core/frame.cpp::extra_attack_add()` and
`extra_attacks_update_chiyuri()`. The implementation:

1. Allocate an even/odd pair from slots 0/1 through 14/15, testing only the
   first slot of each pair as the source does.
2. Consume two random target-X values in raw range
   `rand % ((PLAYFIELD_W-12)<<4) + 0x60`; use target Y 0 for one member and
   376 pixels for the other; use base frame `0x50`.
3. Transfer the two members independently but synchronize their transition
   to the active phase.
4. Increment the state-1 frame in source order; execute the frame-4
   `SUB_CE0C()` effects without adding RNG; play the frame-16 sound.
5. Apply the sloped beam collision and collision-map writes only after frame
   `0x28` and before frame `0x70`.
6. Clear both pair members after frame `0x88`.

There is no presentation RNG or gameplay mutation in
`exatt_render_chiyuri()` / `chiyuri_1905A()`.

### Gauge attack: complete

Source is `th03/main_07.cpp:284-357`; the port is
`src/core/frame.cpp::gauge_pattern_chiyuri()`. It preserves:

1. Set duration to `((level/2)<<4) + 0x20` and initial X to -24 pixels.
2. Every 16 frames, advance X by 24 pixels and issue the two non-RNG
   `SUB_CE0C()` effects.
3. On odd frames, emit symmetric aimed pellet/bullet shots with
   `speed = ((frame % 16) << 2) + 24`, non-animated.
4. Increment and clear at the source's post-increment duration boundary.

### Boss Attack: complete

Source is `th03/main_03u.cpp:146-500`, with rendering in
`th03/main_03v.cpp:57-204`. The port is
`src/core/frame.cpp::boss_pattern_chiyuri_cardinals()`,
`boss_pattern_chiyuri_rings()`, `boss_pattern_chiyuri_trail_ring()`,
`boss_pattern_chiyuri_lattices()`, and `boss_update_chiyuri()`. It preserves:

- HP `0x8C` (already initialized in `src/core/round_start.cpp:266-267`).
- Parameters `L+0x30`, `6-L/4`, `L+0x34`, `0x20-L`, `2L+0x38`,
  `L+0x10`, and `L/2+0x0C`.
- Initializer `chiyuri_1219D()` semantics and all four patterns.
- The custom raw switch chooser and motion, including `rand % 5` and
  `rand & 0x0F` in their exact source order.
- `chiyuri_121F5()`: four `SUB_CE5B()` reads on its first event.
- `chiyuri_12355()`: one `SUB_CE5B()` read and one random bullet angle per
  scheduled event.
- `chiyuri_12425()`: `SUB_CE5B()` every eight frames and a random ring angle
  at frame `0x30`.
- `chiyuri_12498()`: two `SUB_CE5B()` calls at frame `0x20` and two at frame
  `0x40`.
- Source switch-entry timing, state-0 collision exceptions, collision-map
  footprint, damage radius, HP subtraction, and defeat handling.

The renderer's materialization-only angle/radius writes are documented in
the ledger above. They are overwritten before active gameplay and are not
part of the headless state.

### Bomb RNG: complete

`src/core/frame.cpp::bombs_render_phase_update()` preserves all 20
`SUB_CE5B()` shared-ring reads at the source frames and the foreground shift.

## Yumemi (character 8)

### Charge shot: complete

Source is `th03/main/bullet/bullet.cpp:425-590`; the port is
`src/core/frame.cpp::add_yumemi_chargeshot()`,
`chargeshot_update_yumemi()`, and the Yumemi branch of
`chargeshot_hittest()`. It preserves:

1. Add with state 1, frame 0, and the supplied center.
2. Increment the frame **before** processing the state.
3. In state 1, move upward by `0x40` raw fixed-point units and transition to
   state 2 with frame reset at frame `0x40` or `y <= 0`.
4. In state 2, clear at frame `0x40`.
5. Keep `gauge_charged = 0` while the object is active.
6. In hittest state 1, use the 8-pixel overlap, transition to state 2 and
   frame 0 on a hit, but return 0 damage for that call.
7. In hittest state 2, use the 56-pixel square, return 1 damage, and consume
   the two shared RNG values used by `hitcircles_enemy_add()`.
8. Treat state 0 as a miss. The renderer itself consumes no RNG.

### Extra attack: complete

Source is `th03/main_06.cpp:776-1212`; the port is the Yumemi branch of
`src/core/frame.cpp::extra_attack_add()`,
`extra_attack_add_direct_yumemi()`, and `extra_attacks_update_yumemi()`. It
preserves:

1. For normal attacks, allocate among slots 0-7, consume random X and random
   Y in the 96-352-pixel interval, use base frame `0x46`, arm extent 0, and
   duration `0x60`. The two values are evaluated into explicit locals in
   source order before calling the transfer initializer; this is required
   because GCC was observed evaluating the original inline call arguments
   right-to-left, unlike Turbo C++.
2. For `yumemi_1A95F()`, allocate the first free slot among 8-15, initialize
   state 3/frame 0 at the requested target, target the other player, use
   extent 0 and duration `0x20`, then preserve the delay-state timing through
   frame `0x28` before state 1.
3. Update all 16 slots. During state 1, grow extent by 6 through frame 8,
   shrink by 6 from `duration` through `duration+7`, and at
   `duration+8` set state 0 **but still execute that frame's active collision
   and collision-map work**, matching the source control flow.
4. Implement both crossing beam hitboxes and reproduce the source's unusual
   vertical collision-map geometry exactly.

The renderer consumes no shared RNG and writes no later gameplay input.

### Gauge attack: complete

Source is `th03/main/bullet/bullet.cpp:731-836`; the port is
`src/core/frame.cpp::gauge_pattern_yumemi()`. It preserves the staged centers:

| Frame | Center | Action |
|---:|---|---|
| 0 | `(144,32)` | stage center, consume one `SUB_CE5B()` draw |
| 8 | `(96,80)` | stage center, consume one draw |
| 16 | `(192,80)` | fire prior center, stage center, consume one draw |
| 24 | `(96,56)` | stage center, consume one draw |
| 32 | `(192,56)` | stage center, consume one draw |
| 40 | `(144,104)` | stage center, consume one draw |
| 48 | prior center | fire |
| 56 | last center | fire and clear |

Each fired pattern is an aimed ring with count `level + 0x18` and speed
`level + 0x20`. Frames 0, 8, 16, 24, 32, and 40 therefore consume six shared
RNG values even before bullet-angle RNG elsewhere is considered.

### Boss Attack: complete

Source is `th03/main_03q.cpp:194-534`, with rendering in
`th03/main_03r.cpp:55-211`. The port is
`src/core/frame.cpp::boss_pattern_yumemi_accel_rings()`,
`boss_pattern_yumemi_random_groups()`, `boss_pattern_yumemi_four_rings()`,
`boss_pattern_yumemi_crosses()`, `boss_pattern_yumemi_rain()`, and
`boss_update_yumemi()`. It preserves:

- HP `0x82` (already initialized in `src/core/round_start.cpp:268-269`).
- Parameters `L/2+0x10`, `L/2+0x10`, `L+0x40`, `0x12-L`,
  `4L+0x50`, and `0x10-L/2`.
- State 0's 96-frame materialization and the fact that boss collision does
  not begin before frame 96. The sounds at frames 24 and 72 are
  presentation-only.
- `yumemi_10324()`: random initial angle and direction (two draws) plus one
  `SUB_CE5B()` draw on each of frames 1-3.
- `yumemi_10405()`: `SUB_CE5B()` at frames 1, 5, 9, and 13; from frame 32,
  each even-frame emission consumes three RNG values for speed, type, and
  group.
- `yumemi_1050F()`: one type draw and four angle draws per scheduled event.
- `yumemi_105B5()`: one periodic `SUB_CE5B()` draw plus a direct Yumemi extra
  attack.
- `yumemi_10669()`: each scheduled late 12-shot event consumes an X and an
  angle draw per shot, 24 reads total; its early spread groups add no RNG.
- The 56x56-pixel collision map, 32-pixel hit radius, damage/defeat handling,
  and the source's radius/render-state tail updates.

### Bomb RNG: complete

`src/core/frame.cpp::bombs_render_phase_update()` preserves all nine
`SUB_CE5B()` reads in the render phase, including their order relative to
the later Boss and extra-attack callbacks.

## Callback dispatch and per-frame ordering

Anniversary's authoritative order is `th03/main/rndloop.cpp:79-166`:

1. Reset collision and bullet-template state.
2. Player 0 extra attacks, Boss Attack, and character bomb callback.
3. Player 1 extra attacks, Boss Attack, and character bomb callback.
4. Hit-circle update, then ordinary shots.
5. Player 0 charge update, then player 1 charge update.
6. HUD/start state, formations, bullets, enemies, and fireballs.
7. Player 0 gauge pellet wrapper then bullet wrapper; then the same pair for
   player 1.
8. Input and player 0/player 1 updates.
9. Render phase: both bomb callbacks; both boss render callbacks; shared
   renderers and fireball hittest; hit circles; then player 0 extra/charge
   render callbacks followed by player 1 extra/charge render callbacks.

The port splits this between
`src/core/frame.cpp::gameplay_frame_prefix()`,
`gameplay_frame_suffix_before_interstitial()`, and
`gameplay_frame_suffix_after_interstitial()`. The gameplay order is
equivalent for the completed callbacks:

- Player 0 extra/boss/bomb state, then player 1.
- Shots before charge updates.
- Formations/bullets/enemies/fireballs before gauges.
- Player 0 gauge before player 1 gauge.
- Player updates after gauges.
- Bomb and boss render-phase deterministic callbacks before fireball hittest
  and extra-attack render-phase callbacks.

Consequences that must remain invariant:

- Bosses test the previous-frame positions of ordinary and charge shots.
- Enemies test charge shots after those charge shots have moved.
- Gauge-spawned bullets first update on the following frame.
- Character bomb presentation RNG occurs in the render-phase callback, not in
  the earlier bomb-state update.
- Anniversary calls distinct pellet and bullet gauge wrappers. The port's
  combined dispatcher is equivalent only while it runs solely for the
  matching init/active flag and does not accidentally execute both logical
  callbacks.
- Source switch-entry semantics matter: changing a state in one switch case
  does not fall through into the newly selected attack unless the source
  explicitly does so.
- During the round-start HUD, source suppresses formation creation only.
  Bullets, existing enemies/fireballs, and Gauge callbacks still execute.
  Returning from the entire native prefix here incorrectly froze carried
  route-1 state and was the repeat-round cause exposed by TH3R04/TH3R05.

## Audit-caught corrections already incorporated

These were discrepancies at the start of this audit and are recorded to
prevent regressions:

1. Kotohime's direct gauge extra-attack allocator used a C++ reference that
   did not rebind when slot 8 was occupied. It now selects slot 8 or the
   overwrite slot 9 by pointer.
2. Kotohime behavior-1 milestone-4 bullet speed now reflects the helper's
   actual overwrite: 16, 18, 20, and 22 rather than 28-34.
3. Kotohime Boss Attack HP is initialized to `0x6E`.
4. Chiyuri and Yumemi boss HP values are initialized to `0x8C` and `0x82`.
5. Hyper shot-pair and movement-speed behavior is present for Kotohime,
   Kana, Chiyuri, and Yumemi.
6. Rikako's state-2 following charge, hyper movement penalty, and end cleanup
   are present.
7. Chiyuri and Yumemi bomb presentation-RNG, charge/extra/gauge dispatch, and
   complete Boss Attack families are present.
8. Kotohime state-0 Boss rendering now seeds the later gameplay orbit angle
   and radius exactly, with a focused materialization regression test.
9. Yumemi Extra target X/Y RNG is explicitly sequenced before its transfer
   initializer, with a focused test for the Turbo-C++-versus-GCC argument
   evaluation-order difference.

## Closeout

No character-4-through-8 renderer-owned gameplay/RNG consumer remains open
in this snapshot. Replay first-divergence comparison is still required as an
integration oracle, but it should validate this source-complete inventory
rather than serve as the mechanism for discovering unported callback
families.
