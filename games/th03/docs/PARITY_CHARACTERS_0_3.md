# Character parity audit: Reimu, Mima, Marisa, and Ellen

Static-audit status: **2026-08-09, complete against native HEAD `69da429`**

This ledger covers the complete gameplay-bearing character implementation for
character indices 0 through 3, including ordinary shots, Charge Shots, Extra
Attacks, Gauge Attacks, Bombs, Boss Attacks, and Hyper behavior. It includes
update callbacks, render-fed mutations, cross-callback shared bullet-template
state, first-round initialization, repeat-round reset/reuse, collision and
damage, source-width arithmetic, and paths not selected by the current replay
corpus.

This is a static audit. No emulator result is claimed here.

## Authorities and result

- Source: clean frozen Anniversary Git object
  `3e2381d4275be7287f5525de5c95389d492b0a3c`.
- Native: `th03-port` at
  `69da429ccbe696d15a40f89349fc26f2b0a44d77`.
- Dispatch authority: Anniversary `th03/hitcb.cpp:199-376`.
- Frame-order authority: Anniversary `th03/main/rndloop.cpp:83-174` and native
  `src/core/frame.cpp:9167-9198,9276-9355`.
- Round-lifecycle authority: Anniversary `th03/main/resscore.cpp:55-80` and
  native `src/core/round_start.cpp:423-564,567-718`.

**Result:** no remaining gameplay, collision, score, control, shared-RNG, or
gameplay-reachable shared-template discrepancy was found for characters 0-3.
All stock-valid states and every stock Boss pattern state 2 through 17 were
audited, not merely the states exercised by TH3R00-TH3R03.

The previous revision incorrectly said that native code advanced Ellen's Boss
animation byte. Anniversary increments shared `byte_1F354` once on every Ellen
Boss update (`th03/main_03s.cpp:346`) and by another 3 in `ellen_11547()`
(`main_03s.cpp:260`). Native intentionally does not carry this counter: its only
Ellen use is sprite animation in `ellen_11885()`
(`th03/main_03t.cpp:89-100`). No collision, RNG, allocation, score, control, or
later gameplay callback reads it. This is a presentation-only omission, not a
gameplay mismatch.

## Complete coverage matrix

“Complete” means that all gameplay effects and replay-visible RNG consumption
are materialized. “Presentation omitted” means that source-owned VRAM, sprite,
palette, sound, or visual-only records are deliberately outside the headless
state; any RNG or later gameplay input produced by such code is still retained.

| System | Reimu | Mima | Marisa | Ellen | Native counterpart | Disposition |
|---|---|---|---|---|---|---|
| Ordinary shots | `main/player/shot.cpp:272-345` | Same generic path | Same generic path | Same generic path | `frame.cpp:1603-1665` | Complete. Spawn scan order, clipping, signed Q12.4 positions, velocity, and Hyper shot modes match. `shots_render()`'s sprite-animation byte is presentation omitted. |
| Hyper | `main/player/bomb.cpp:48-67` | `bomb.cpp:48-54,69-81` | `bomb.cpp:48-54,83-95`; `ch_shot.cpp:85-106` | `bomb.cpp:48-54,97-107`; `main_08.cpp:93-131` | `frame.cpp:1596-1653,2317-2333,3613-3675,3750-3827` | Complete. One-frame standby transition, shot modes, movement deltas, character effects, per-frame gauge drain, exit frame, and shot blocking match. |
| Charge add/update/hittest | `ch_shot.cpp:439-732` | `ch_shot.cpp:1051-1254` | `ch_shot.cpp:58-175` | `main_08.cpp:43-337` | `frame.cpp:1736-1836,1968-2023,2095-2193,2397-2520,2577-2594` | Complete. See character details below. |
| Charge render | `ch_shot.cpp:733-788` | `ch_shot.cpp:1255-1298` | `ch_shot.cpp:176-254` | `main_08.cpp:338-377` | None required | Presentation omitted. All four renderers only read Charge records and mutate drawing state or a scratch cursor that every later user reinitializes. |
| Extra add/update/render | `main_06.cpp:114-468` | `main_06.cpp:470-774` | `main/player/exatt.cpp:1051-1273` | `main/player/exatt.cpp:369-813` | `frame.cpp:879-1175,4720-5444` | Complete. Allocator scan limits, pre-allocation RNG, transfer, direct Boss allocation, collision-map writes, state timing, byte/word wrap, and arena-full behavior match. Reimu's render-owned age increment is retained at `frame.cpp:5446-5463`; other renderers are presentation-only. |
| Gauge | `ch_shot.cpp:790-868` | `ch_shot.cpp:1300-1394` | `ch_shot.cpp:260-340` | `main_08.cpp:379-483` | `frame.cpp:2905-3343,3553-3579` | Complete. Both pellet and bullet routes, init-to-active flags, params, cadence, groups, termination, RNG, and final shared-template angle match. |
| Bomb | `ch_shot.cpp:889-1049` | `main/bullet/mbomb.cpp:21-107`; `main/bullet/bullet.cpp:795-839` | common updater `ch_shot.cpp:870-887`; renderer `ch_shot.cpp:345-436` | `main/player/chybomb.cpp:143-393` | `frame.cpp:2704-2892` | Complete. Update/render split and P1-before-P2 order match. Visual payloads are omitted, while all visual RNG and renderer-fed gameplay state are retained. |
| Boss | update `main_03r.cpp:212-503`; render `main_03s.cpp:51-159` | update `main_03p.cpp:159-421`; render `main_03q.cpp:57-192` | update/common `main_03.cpp:49-252,254-544`; render `main_03p.cpp:52-157` | update `main_03s.cpp:161-394`; render `main_03t.cpp:52-178` | common `frame.cpp:5606-5776`; Reimu `5778-6030`; Mima `6031-6322`; Marisa `6324-6559`; Ellen `6561-6756`; render phase `8208-8263` | Complete for gameplay. Initial records, launch/reversal, previous-pattern persistence, all states, direct Extras, collision/damage/defeat, score, and renderer-fed Mima state match. Ellen animation counter is presentation omitted as described above. |

## Character-specific mechanics

### Reimu

- Charge clear scans all 24 source groups; ordinary Charge initializes groups
  0-3 while Hyper searches groups 0-11. The shared primary-fire throttle is
  set by ordinary Charge and cleared by Hyper even when no slot is free.
  Group state/frame, six-node Q12.4 motion, deceleration, release threshold,
  clipping, first-six hittest limit, odd-frame throttle, damage, and two
  hit-circle RNG draws per hit match.
- Extra Attack uses the first 8 slots. The free-slot scan precedes the two
  target-coordinate draws, so a full arena consumes no RNG; after a slot is
  found, the X and Y draws precede generic transfer initialization. Transfer
  speed is `90 + round/4`; the direct Boss allocator,
  gravity/bounce states 3-21, collision-map rectangles, hittest bounds, and the
  post-render frame increment all match.
- Gauge params are `level + 0x20` and `0x20 - level`; aimed spreads, paired
  rings at frame 16 modulo 32, and the frame-64 end match.
- Bomb render frames 0-31 consume velocity RNG then X RNG. Foreground shift at
  frames 64-111 and reset from 112 are materialized.
- Boss params are `[5-level/5, level+0x18, level*2+0x28, level/8+4,
  level+0x10, level+0x10]`. States 2-6 use orbiting pellet clouds, 8/9/10/13
  use moving spreads, 15-17 use the six-orbit direct Extra pattern, and
  7/11/12/14 use paired four-spreads. Initial-entry writes, RNG order, orbit
  arrays, final shared-template angles, and exact end comparisons match.

### Mima

- Charge uses 15 nodes and the source angle table
  `{192,182,202,192,176,208}`. Node 0 starts immediately, later nodes activate
  every four frames, and the last node clears the global active flag even if an
  earlier node remains. Point-Y hittest, byte damage accumulation, and two RNG
  draws per hit match.
- Extra Attack uses 6 slots. The free-slot scan precedes its two draws, and its
  target Y deliberately takes modulo playfield **width**. Initial frame 3,
  timer, bounce count, speed ramp, state changes,
  transfer-completion angle draw, hittest, and collision-map behavior match.
- Gauge consumes one draw to select signed angle delta `-7` or `+7`, fires on
  the exact eight-frame cadence, alternates the ring, wraps the byte angle, and
  ends after frame 64.
- Bomb update consumes one draw on frames 8,16,...,64 for the omitted column
  positions. Render shift at frames 81-143 and reset from 144 are retained.
- Boss params are `[level+0x20, level/4+2, level+0x20,
  0x40-level*2, level+0x28, level*4+0x40, level+0x36]`. States 2-5 are
  spreads, 6-10 random bullet/pellet bursts, 11-13 aimed trail spreads, and
  14-17 orbiting pellet clouds. The source switch dispatches the state captured
  before state-0 transition; native does the same. Renderer states 2 and 5,
  hit-path early return, pre-increment threshold, and frame reset are retained
  because they are persistent source state even though Mima mechanics do not
  read them back.

### Marisa

- Charge initializes all 12 trail points, blocks ordinary firing, advances the
  byte age, releases after `0x24`, and hittest samples nodes 0/4/8 with the
  source's lower-Y-only bound. Hyper only initializes an inactive beam, sets
  age `0x10`, and still blocks firing. Hit visuals consume two RNG draws.
- Extra Attack scans 14 slots before consuming the random target X, so a full
  arena consumes no RNG. Transfer targets Y=368 with speed `0x6E`; the direct
  Boss allocator selects the opposing buffer from target PID
  and partially overwrites only state/frame/position/PID. Beam state timing,
  collmap, collision, and arena-full behavior match.
- Gauge consumes one draw for the negative initial Y, fires the five-bullet
  edge groups only in the two firing phases, advances signed Q12.4 Y, and ends
  at the source frame. The omitted `SUB_CE0C` phases have no RNG or gameplay
  output.
- Bomb uses the common updater. Renderer frames 64-127 consume X then Y every
  eighth frame, retain the alternating foreground shift, and reset from 128.
- Boss params are `[level*2+0x32, level*2+0x18, level+0x20,
  level+0x0A, level+0x16]`. States 2-5 use paired-speed spreads; 6/7/17 use
  three direct Extra pairs; 8-10 use four pairs; 11-16 use rings. Both the
  level-under-8 and level-at-least-8 ring branches, count/speed selection,
  angle offset, raw direct coordinates, switch table, and exact terminal frames
  match.

### Ellen

- Charge clear scans 64 nodes. Ordinary Charge consumes 32 angle draws and
  stores even countdown states 0,2,...,62; node 0 therefore begins inactive.
  Hyper activates the first state-0 node with one angle draw. Update ordering,
  CPU target selection, target Y offset, countdown, deceleration, homing speed,
  clipping, gauge zeroing, point hittest, freeing, damage 2, and two hit-circle
  RNG draws match.
- Extra Attack always consumes four target/angle/delta RNG draws before the
  free-slot search, then may allocate at most two of 12 slots. Opposite angles,
  shared signed delta, seven-point trail/reference shifts, expanding orbit,
  derived velocity, both-endpoint clipping, transfer, hittest, collision map,
  and direct Boss allocator match.
- Gauge's first phase emits inward aimed rings. Its second phase uses one angle
  draw for both 16-rings and then writes a second RNG result to the shared
  template after firing. Native retains that otherwise-unused second draw and
  write.
- Bomb's eight vector arrays are per-player, but the release cursor is a single
  shared word. The renderer resets that cursor before frame 64 and releases one
  vector every fourth frame through 127; the following update respawns it with
  one angle draw. Native retains this renderer-to-update/RNG dataflow and source
  P1-before-P2 coupling.
- Boss params are `[level*2+0x32, level/10+4, level+0x30,
  10-level/3]`. States 2-5 use the spread sequence, 6-10 allocate Ellen Extras
  at frames `0x30` and `0x60` with deltas +2 and -2, and 11-17 use moving
  pellet spreads. Direct-allocation failure still advances the loop angle.
  The eight-add spread writes the post-loop template angle; native explicitly
  retains it. `byte_1F354` animation is presentation-only omitted.

## Call order and shared bullet template

Anniversary performs, in order:

1. P1 then P2: Extra update, Boss update, Bomb update.
2. Generic shot update, then P1 and P2 Charge update.
3. Formation, Bullet, Enemy, and Fireball update.
4. P1 then P2: pellet Gauge callback, then bullet Gauge callback.
5. Input/player update.
6. P1 then P2 Bomb render, then P1/P2 Boss render.
7. Shot/Enemy render and Fireball hittest/render.
8. P1 then P2 Extra render and Charge render.

Native preserves every gameplay-bearing phase at
`frame.cpp:9167-9198,9276-9355`. One native Boss-render invocation is equivalent
to the source's two calls because the non-owner callback returns immediately.

`bullet_template_reset_stuff()` (`main/bullet/bullet.cpp:1160-1167`) resets
angle, count, animation, collision, and trail flags at the frame start. Native
models the only field that can remain an input to a later abstracted producer,
`DeterministicState::bullet_template_angle`. Count and the three booleans are
explicit arguments/defaults on every native add; speed, PID, group, and type are
overwritten before every character 0-3 add.

The complete producer audit found these important final-angle cases:

- Bullet-transfer update copies `angle_next` into the shared template before
  the later Gauge callbacks; native does so at `frame.cpp:1287`.
- Ellen Gauge preserves the second post-fire RNG angle
  (`frame.cpp:3329-3333`).
- Ellen Boss's eight-add loop writes the incremented post-loop angle; native
  preserves it at `frame.cpp:6598`.
- Reimu, Mima, and Marisa producers either overwrite angle before every add or
  correctly leave the last fired angle. Their separate private angle counters
  do not implicitly mutate the source template.

No other source template field has an outgoing gameplay dataflow in the scoped
call chain.

## Initialization, reset, and stale-state semantics

- First-round character speeds match `th03/main/chars/speeds[data].asm`:
  Reimu `64/64/48/48/32`, Mima `72/72/54/54/20`, Marisa
  `64/64/48/48/28`, Ellen `52/52/46/46/14`.
- Boss initial HP is `0x006E` for Reimu/Mima/Marisa and `0x0064` for Ellen.
  Position `(144,80)`, X velocity `-32`, state/motion/pattern/render bytes, and
  P2 sprite selection's gameplay-neutral disposition match. Native omits sprite
  offsets.
- A new Boss launch copies the source 32-byte initial record but must retain
  the previous-pattern byte outside that record. Native preserves only that
  byte across `BossState{}` reconstruction (`frame.cpp:5646-5655`). Other
  zeroed native fields are either source initial-record zeroes or are fully
  initialized before their first stock read.
- Repeat reset clears only Extra state bytes and the exact targeted Charge
  state/active bytes in source order. It does **not** clear character-private
  angle/timer storage. Native `round_start.cpp:599-634` matches this. In
  particular, Mima's global Charge-active flag is intentionally not cleared
  when the node actives are cleared, preserving the source landmine.
- Ellen's pellet Boss angle and other character-private bytes persist across
  Boss launches/rounds exactly as in BSS. Patterns that require a fresh value
  initialize it on their entry frame.

Two native initializers normalize more memory than Anniversary without changing
gameplay:

- `extra_attack_transfer_init()` and Reimu's generic direct initializer clear
  modeled auxiliary members that source `sub_1A1ED()` leaves stale. Every
  character 0-3 field read by the selected state is overwritten first; Ellen's
  gameplay-bearing trail arrays are explicitly preserved around transfer.
- `boss_launch_or_reverse()` clears modeled Boss auxiliaries beyond the copied
  source record. Reimu orbit arrays and anchors are fully written on state entry
  before update or render reads them; Mima/Marisa/Ellen use separate persistent
  character-private fields where the source does.

These are deterministic inert-memory normalizations, not semantic fixes. They
would matter only to raw-memory hashing or an externally injected mid-pattern
state, neither of which is a stock gameplay route.

## Fixed-width and stock-range closure

- Positions, velocities, radii, and trail coordinates are signed 16-bit Q12.4
  and use explicit wrapping helpers. Speeds, angles, frames, states, counts,
  damage accumulators, and character params retain their source byte width;
  Boss frames/HP and Mima Extra timers retain word width.
- Boss level is clamped to 16. Therefore Mima's modulo divisor
  `0x40-level*2` is 32-64, Ellen's divisor `10-level/3` is 5-10, and all ring
  counts are nonzero. Native unsigned division produces the same result over
  these source-live positive ranges.
- Signed angle deltas (`-7`, `+7`, `-2`, `+2`) are stored as `int8_t` and added
  with byte wrap. All source narrowings after arithmetic are explicit.
- Ordinary shot `pairs_fired` is a native byte rather than source `int`, but its
  live range is 0-4. Invalid `shot_mode` values would use an uninitialized
  source local; all installed callbacks produce only modes 0-4, so this is an
  unreachable stock landmine rather than a port contract.

## Presentation-only source dependencies

These source modules/data remain deliberately unmaterialized, with no
unrecorded gameplay outflow:

- Charge and Extra renderer scratch pointers/cursors and sprite state.
- Generic shot `so_anim` and character/palette sprite offsets.
- Reimu Bomb falling-sprite records, Mima Bomb column records, and Marisa Bomb
  flash coordinates. Their shared-ring consumption is retained.
- `SUB_CDBD()` and `SUB_CE0C()` effect slots in
  `th03/main/player/d3f9.cpp:239-275`; neither consumes RNG. `SUB_CE5B()` at
  `d3f9.cpp:277-295` consumes one draw, but no character 0-3 Gauge/Boss/Bomb
  callback invokes it.
- Ellen Boss `byte_1F354` animation and all four characters' Boss sprite/VRAM
  render payloads.
- Palette, GRCG/EGC, page-copy, sound, and MRS resources.

If a future graphical client needs pixel parity, these are renderer contracts
that still require implementation. They are not missing deterministic gameplay
state in the headless core.

## Verification disposition

This final gate was a read-only static comparison of whole definitions, call
sites, switch tables, BSS layout, and data tables. No emulator or build was run
for this audit. The existing corpus and focused tests are separate evidence;
this document does not infer unreviewed-path completeness from them.
