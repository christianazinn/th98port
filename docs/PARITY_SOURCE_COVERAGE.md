# TH03 deterministic source-coverage audit

Audit date: 2026-08-09

Native snapshot: `69da429ccbe696d15a40f89349fc26f2b0a44d77`

Anniversary authority: `3e2381d4275be7287f5525de5c95389d492b0a3c` in
`.scratch/port-refs/oracle_anniversary_th3r02`

The three public-input contract closures recorded below are the audited working
tree changes on top of native snapshot `69da429`.

This is the final whole-source coverage ledger for the headless deterministic
TH03 backend at the snapshots above. It was produced by reading the complete
Anniversary definitions and call chains, not by inferring completeness from a
green replay corpus or from the older parity documents. The replay corpus and
focused tests remain independent integration evidence.

## Status vocabulary and boundary

- **Complete** means source-equivalent gameplay state, control flow, collision,
  score, shared RNG consumption/order, fixed-width arithmetic, and source-used
  stale-state behavior for every stock-reachable state and stock data record.
- **Presentation external** means the native core deliberately omits pixels,
  palette, sound, VRAM/page state, or animation-only state with no later
  gameplay/RNG/control outflow. A graphical client still has to implement it.
- **Effective-stock** means the native representation is equivalent for every
  value created by stock code and stock data, but does not reproduce undefined
  memory access, hardware corruption, or malformed-resource behavior.
- **Application boundary** means behavior belongs above `Game`'s deterministic
  frame engine rather than being silently approximated in the core.
- **Resolved contract closure** means a formerly accepted artificial state was
  either rejected at the public boundary or initialized exactly like the
  source. Such states were never emitted by the stock application or corpus.

The audit covers MAIN gameplay, its MAINL round/stage/continue lifecycle, all
nine characters, the stock ENEDAT program, gameplay-adjacent assembly and BSS,
and renderer-named callbacks that mutate deterministic state or consume RNG.
It does not claim pixel, audio, menu, cutscene, score-file, or PC-98 hardware
parity.

## Exact `sub_9778()` frame schedule

Anniversary `th03/main/rndloop.cpp:79-310` is the authoritative scheduler.
Native divides the same frame across `src/core/frame.cpp`'s
`gameplay_frame_prefix()`, `gameplay_input_latch()`, warning-interstitial
functions, and the two gameplay suffix functions.

| Order | Anniversary operation | Native disposition |
|---:|---|---|
| 1 | `collmap_reset()`, `bullet_template_reset_stuff()` | Complete. Collision state and the source-used persistent template angle reset first. |
| 2 | P1 Extra update, Boss update, Bomb update; then P2 | Complete, including character-private state and RNG. |
| 3 | `hitcircles_update()`, `shots_update()` | Shot gameplay complete; hit-circle age/cursors are presentation-only. |
| 4 | P1 Charge update, then P2 Charge update | Complete. This remains after ordinary-shot movement and before entities. |
| 5 | `sub_BB12()`, formations, bullets, enemies, fireballs | Complete. During the HUD start only formations self-suppress; bullets, existing enemies, fireballs, and later gauges continue. |
| 6 | P1 pellet Gauge then bullet Gauge; then P2 | Complete. Native's dispatch is equivalent to the two source callbacks and preserves template/RNG side effects. |
| 7 | input mode, T3CASE I/O, P1 update, P2 update | Complete for replay, CPU, and ordinary live gameplay inputs. Input is latched at the same boundary. |
| 8 | human Cancel pause, then warning wait and portraits, then background callback | Recorded-pause and warning behavior complete. Direct live-human pause/Q is an application boundary described below. |
| 9 | P1/P2 Bomb render callbacks, then P1/P2 Boss render callbacks | Complete for every gameplay mutation and RNG read; graphics-only work is external. |
| 10 | shots, enemies, fireball hittest/render, hit circles | Complete. Fireball collision is deliberately in this late phase. Shot/enemy sprite work is presentation external. |
| 11 | P1 Extra/Charge render callbacks, then P2 | Complete for render-fed state, collision, and RNG; graphics-only work is external. |
| 12 | players/overlays, result banner logic and round route | Player/result gameplay complete; drawing is external. |
| 13 | gauge overlays, extends, warning state, HUD, combos, result callback, demo callback | Complete for deterministic state, score, control, and RNG. |
| 14 | presentation/page flip, frame counters, speed growth, 1024-frame RNG refill/damage step | Presentation omitted; all deterministic counters, clamps, refill order, and damage increments complete. |

This order has several non-obvious consequences that the native code retains:

- Bosses test the previous-frame positions of ordinary and Charge shots, while
  enemies test Charge shots after those shots have moved.
- Gauge-created bullets first update on the following frame.
- Fireball hittest occurs after enemies and all Boss render callbacks.
- Character Bomb presentation RNG occurs in the render callback, not the
  earlier Bomb update callback.
- Result/warning/HUD callbacks remain late even though their names suggest
  presentation.
- `th03/main/rstart.cpp:81` initializes `byte_23AFA` to zero and no stock code
  writes it. The alternate every-other-frame branch at `rndloop.cpp:133-137,
  246-257` is therefore source-unused and is not part of injected-state parity.

## Shared C++ source-to-native matrix

| Source modules and whole definitions | Native counterpart | Status and stock disposition |
|---|---|---|
| `main/rndloop.cpp::sub_9778()` | `frame.cpp:9167-9355`, `game.cpp::finish_gameplay_frame()` | **Complete** schedule and deterministic late callbacks. |
| `main/entry.cpp`, `main/rstart.cpp`, `main/resscore.cpp` (`sub_9EBF`, `sub_A21F`) | `round_start.cpp:423-718`, `game.cpp` | **Complete** for valid Story/Demo/VS starts and route-1 repeats. Exact targeted resets, carried bytes, callback setup, rings, formation state, background initialization, and resident handoff are retained. The three artificial-init contract edges found by this audit are closed below. |
| `main/roundcb.cpp`, `main/player/c2f9.cpp`, `main/player/shot.cpp::sub_E3F2()` | `frame.cpp::round_mode_update()`, result control/update helpers | **Complete** for stock Demo and round results. |
| `main/bullet/bullet.cpp` (reset/add/groups/transfer/trails/update/render) | `frame.cpp:360-1594` | **Complete** for every stock group and flag. Slot order, template-angle persistence, group counts, angle/speed wrapping, transfer, fall/cloud/accel, collision, scoring, and RNG agree. `bullets_render()` is presentation external. Zero-count generic rings and invalid flags are impossible internal inputs. |
| `main/enemy/enemy.cpp` | `enedat.cpp`, `frame.cpp::enemy_speedtune()`, `enemy_move_and_clip()`, `enemy_run()`, `add_formation()`, `enemies_update()` | **Complete** for the entire stock ENEDAT program. Forty-slot order, script timing, formation rings, mirror flag, clipping, hittest, HP-before-decrement, explosion/chain/score/gauge/fireball effects agree. Malformed programs are rejected rather than executed out of bounds. |
| `main/enemy/expl.cpp` | `frame.cpp::for_each_explosion()` and entity overlay state | **Complete** persistent explosion slot order, frame cadence, score/chain behavior, and collision-map contribution. |
| `main/enemy/fireball.cpp` | `frame.cpp` fireball add/update/hittest paths | **Complete** 24-slot overlay, target/background shifts, transfer/fall, RNG, generation, HP-before-decrement, scoring, and charge effects. Source `fireball_collision_in_previous_hittest` is written but never read. |
| `main/hitbox.cpp`, `main/colvline.cpp` | `frame.cpp` generic hitbox, explosion, Extra/Boss/enemy/fireball collision helpers | **Complete** for stock writers and callers. Every relevant generic hittest retains the `DF_BANNER` suppression; specialized Extra bounds use its source-local guard. |
| `main/player/shot.cpp` | `frame.cpp:1596-1665` | **Complete** pool/order, Q12.4 movement, modes, clipping, Hyper amulets, and result-owned effects. `so_anim` only selects sprites and is omitted. |
| `main/player/bomb.cpp`, `c433.cpp`, `c54a.cpp`, `c568.cpp`, `move.cpp`, `stuff.cpp`, `defeat.cpp`, `c7a5.cpp` | `frame.cpp:2301-4115`, `game.cpp` replay pause adapter | **Complete** common movement, CPU, shot gates, Charge release fallthrough, Bomb/Hyper activation, damage, knockback, defeat, winner, and CPU RNG. Human pause/Q remains an application boundary. |
| `main/player/gauge.cpp`, `gaugeovl.cpp` | `frame.cpp` Gauge dispatch/update and late state | **Complete** gameplay and level/state transitions; overlay graphics external. |
| `main/player/combo.cpp`, `main/player/score.cpp` | `frame.cpp` combo/score helpers, `state_hash.cpp` display cap | **Complete** chain timing, 99-hit Anniversary display cap, raw player maximum, BCD score, bonus ownership, and extends. |
| `main/player/bb12.cpp`, `bdc2.cpp`, `be2a.cpp`, `hudstart.cpp`, `warning.cpp`, `d3f9.cpp` | `round_start.cpp::initialize_background_rng()`, `frame.cpp` HUD/warning/background/result helpers | **Complete** deterministic state and RNG. HUD start retains 256 randring and 256 IRAND reads; background type A/B retain 48/40 IRAND reads per initial frame; warning retains its 27-sample transition and 30-frame activation. Particle geometry, palettes, circles, and steady background vectors are presentation external. |
| `main/hitcirc.cpp`, `hitc_mrs.cpp`, `hitc_pal.cpp` | hit-circle RNG retained at creation sites; no persistent native visual object | **Presentation external** after creation. Source update/render state has no later gameplay outflow. |
| `main/hud/static.cpp`, `main/playfld.cpp`, `main/sprite16.cpp`, `main/player/scoreblt.cpp` | application renderer boundary | **Presentation external**. Coordinate conversions used by gameplay are independently represented with Q12.4 and playfield-local coordinates. |
| `main_03.cpp` common Boss helpers (`sub_F356`, `sub_F3A9`, `sub_F402`, `sub_F4B4`, `sub_F512`, `sub_F52D`, `sub_F58C`, `sub_F1FA`) | `frame.cpp:5606-5776`, `boss_render_phase_update()` | **Complete** move/exit/launch/reverse/defeat/target/pattern/materialization, prior-pattern carry, signed HP, score, and RNG. |
| `hitcb.cpp`, `main_03[p-x].cpp`, `main_05.cpp` through `main_11.cpp`, `main/player/ch_shot.cpp`, `chybomb.cpp`, `exatt.cpp`, `yumemi_bomb.cpp`, `main/bullet/mbomb.cpp` | character dispatch and helpers throughout `frame.cpp`, character init/reset in `round_start.cpp` | **Complete** for characters 0-8. See the character closure section and the two dedicated character matrices. |
| `math/polar.cpp`, `math/vector.cpp`, master-library `iatan2.asm` | `trig.cpp`, `numeric.hpp` | **Complete** over every source-defined stock input. Exactly-one-`INT16_MIN` `iatan2` inputs are a source DIV landmine and are deterministically normalized by policy. |
| `main/rand2.cpp`, `main/randfill.cpp` | `random.cpp` | **Complete** IRAND LCG, descending ring fill, cursor carry, and the 16-bit ring read's source alias at index 255. `randring_fill()` does not reset the cursor. |
| `main/t3case.cpp` | `state_hash.cpp`, format/replay layers | **Complete** for the oracle's canonical fields and capped Anniversary combo display. This hash is intentionally compact, not an exhaustive serialization of live gameplay. |

## Character source coverage

The final character audits were performed against the same two snapshots and
are durable in `PARITY_CHARACTERS_0_3.md` and `PARITY_CHARACTERS_4_8.md`.

| Characters | Source authority | Disposition |
|---|---|---|
| Reimu, Mima, Marisa, Ellen (0-3) | `hitcb.cpp:199-376`; `shot.cpp`, `bomb.cpp`, `ch_shot.cpp`; `main_06.cpp`, `main_08.cpp`, `player/exatt.cpp`; Boss families in `main_03.cpp`, `main_03p.cpp` through `main_03t.cpp`; shared reset in `bullet.cpp` | **Complete** Ordinary/Hyper, Charge, Extra, Gauge, Bomb, and Boss states 2-17, including update/render-fed mutations, shared-template persistence, reset/reuse, collision/damage/score/control, and source-width live ranges. |
| Kotohime, Kana, Rikako, Chiyuri, Yumemi (4-8) | `main_05.cpp` through `main_11.cpp`; `main_03q.cpp` through `main_03x.cpp`; `player/exatt.cpp`, `ch_shot.cpp`, `chybomb.cpp`, `yumemi_bomb.cpp`; `bullet.cpp` | **Complete** Ordinary/Hyper, Charge, Extra, Gauge, Bomb, Boss states 0/1/2-17/`0x80`/`0xFF`/default tails, P2-selected background RNG, prefix/render/repeat paths. |

Two honest lower-level qualifications remain:

1. The pinned tree exposes declarations/callers/procdesc but not C++ bodies for
   the common Extra transfer helpers `sub_1A1ED()` and `sub_1A1A7()`. Native
   semantics are closed by every caller, state layout, and replay path rather
   than by a body-to-body comparison.
2. `main_03u.cpp::chiyuri_12498()` restores shared bullet-template speed to
   base after its final lattice add. Native's last explicit add uses base+2,
   but the complete stock call-chain audit finds no later inherited-speed
   consumer. This is an unconsumed internal-state difference, not gameplay or
   RNG divergence. No dormant speed field was added.

The native core also clears some modeled auxiliary Extra/Boss members that the
source leaves stale. Every stock read is initialized first; Ellen trail data
and all gameplay-bearing character-private carry are explicitly preserved.
The normalization is observable only through artificial mid-pattern state or
raw-memory hashing.

## Stock ENEDAT closure

`originals/th03/dat/ENEDAT.DAT` was independently parsed during this audit,
not merely exercised through the corpus:

- 18 formations, 184 scripts, exactly 5,072 bytes consumed from a 5,072-byte
  file.
- Opcode counts: `00=184`, `01=176`, `02=106`, `03=162`, `04=16`, `05=12`,
  `06=110`, `07=8`, `08=14`, `09=6`, `0A=8`, `10=184`, `82=8`.
- No `80`/`81` broken loop operators, no `83` bottom-clip operator, no unknown
  opcode, truncation, or overrun occurs in stock data.

Native implements every stock-used operator and also implements bottom clip.
Broken loops and malformed/truncated programs return
`simulation_not_implemented`/format errors instead of reproducing source
out-of-bounds execution. This is the intentional effective-stock boundary.

## Assembly implementation coverage

| Anniversary assembly | Gameplay role | Native disposition |
|---|---|---|
| `main/collmap.asm`, `collmap_slope.asm`, `collmap_reset.asm`, `main/colvline.cpp` | Packed 80-column collision map; rectangle, two-column slope, vertical line, reset | **Complete effective-stock.** Semantic cells reproduce all stock writers/readers. The legacy player reader separately reproduces DGROUP wrap, negative byte-column aliasing, stride/CL/mask quirks. Arbitrary packed-memory corruption outside stock writers is not emulated. |
| `main/player/combo.asm` | Chain slot allocation/update support | **Complete** in native chain/combo state and fixed slot order. |
| `main/player/score_add.asm`, `score_rn.asm` | BCD addition, score/extend/result number state | **Complete**, including the source's raw most-significant-byte add and owner selection. Drawing routines remain external. |
| `math/randring_fill.asm`, `main/main_04_randring.asm` | Descending fill and ring access helpers | **Complete**, including cursor carry and index-255 adjacent-byte behavior. |
| `libs/master.lib/iatan2.asm` | 8-bit-angle vector direction | **Complete on the source-defined stock domain**; the unreachable signed-compare/DIV landmine is normalized as documented above. |
| `main/main_03_{reimu,mima,kotohime,kana,rikako,chiyuri,ellen,yumemi}_update.asm` | Character callback entry/update fragments | **Complete** through the character callback implementations and dispatch audit. |
| `main/main_03_prefix.asm`, `main_03_dispatch.asm`, `main_04_hitbox_prefix.asm`, `main_06_anchor.asm`, `hitc_mid.asm`, `hitc_prf.asm`, `player/warning_cacb.asm` | Link/procdesc anchors, dispatch stubs, or continuation labels | No independent gameplay body to port; their target definitions/call order are covered. |
| `main/hitc_sfx.asm`, hardware input/page/palette/sprite/CDG/MRS assembly | VRAM, sound, input device, and presentation implementation | **Presentation/application external.** No hidden RNG or gameplay state was found. |

## BSS and data disposition

The native core does not reproduce DGROUP byte-for-byte. This table disposes
all gameplay-adjacent `th03/main` assembly state/data modules by dataflow.

| Modules | Source state represented | Disposition |
|---|---|---|
| `bomb_player_pid_bss.asm`, `chargeshot_gba_bss.asm` | current PID/scratch, Bomb/Hyper flags, player velocities/speed, CPU damage, Charge callbacks/state, GBA active/levels | **Complete** as typed deterministic state or explicit call context. |
| `collmap_bss.asm` | collision map and hit coordinates | **Complete effective-stock**, with legacy player alias state explicitly modeled. |
| `demo[bss].asm`, `round_gate_bss.asm`, `round_frame_bss.asm`, `frame_mod[bss].asm` | demo/result flags, round counters and modulo bytes | **Complete**. Modulo values are derived from the wrapped source counter. |
| `enemy_explosion_score_bss.asm`, `enemy_formation_bss.asm` | enemy/explosion/fireball arena, score/chain globals, formation rings/cursors | **Complete** typed arenas and rings. Source overlay/alias behavior used by gameplay is retained. |
| `exatt_bss.asm`, `shot_kotohime_bss.asm`, `kana_bss.asm`, `ellen_bss.asm`, `chiyuri_bss.asm`, `yumemi_rikako_small_bss.asm` | character attack arenas/private words/bytes | **Complete for every source-read gameplay/RNG field**; renderer-only stale payload is external. |
| `gba_boss_bss.asm`, `gba_exatt_bomb_bss.asm`, `gba_combo_playerm_bss.asm`, `gba_hitc_enemy[data].asm`, `hitbox_defeat_rikako_bss.asm`, `hud_player_gba_bss.asm`, `palette_gba_bss.asm` | Boss/Gauge/Extra/Bomb callbacks and shared state, defeat/hitbox state, initialized `gba_boss_launched_by`, HUD/player state | **Complete** for gameplay and renderer-fed state. Palette/render-only members are external. |
| `player_shot_result_tail_bss.asm`, `player/players[bss].asm`, `shots[bss].asm`, `combo[bss].asm`, `score[bss].asm` | players, shots, result state, combos/chains, BCD scores | **Complete** for deterministic gameplay. Shot animation and drawing cursors are external. |
| `randring_bss.asm` | 256-byte ring and cursor | **Complete**, including the source alias byte used by 16-bit reads. |
| `soch_data.asm`, `gba_hitc_enemy_data.asm` | chain cursor initializer and sprite/coordinate tables | Chain initializer represented; remaining entries are presentation/static lookup data. |
| `chars/speeds[data].asm`, `pbpat[data].asm`, character initial Boss records in C++ | movement/charge speeds, bullet patterns, initial states | **Complete** as native constants and dispatch parameters. |
| `playfield_fg_x[bss].asm`, `warn[data].asm`, `warn_data.asm` | foreground shake, warning state/tables | Gameplay-visible foreground shifts and warning timing complete; palette/graphics tables external. |
| `hitcircle_bss.asm`, `ccorddat.asm`, `cmb_data.asm`, `peldat.asm`, `pbpat_data.asm`, `sfntdat.asm`, `rtextdat.asm`, `5_powers_of_10[data].asm` | circles, coordinates, sprites/fonts/text/decimal-render tables | **Presentation external**. All RNG at gameplay creation sites is retained. |
| `story_startup_data.asm`, `story_startup[data].asm`, `support_format*.asm`, `support_resident_bss.asm` | MAIN/MAINL startup, support resident records, format strings | Process/format boundary; the deterministic values entering `GameInit` and leaving the resident handoff are represented. |

Additional root-level and MAINL data (`opsf*`, opening/selection BSS,
`mainl/*[data].asm`, cutscene/staff/ending resources, CDG/PI/sprite/palette/MRS
tables, sound state) belongs to menus, presentation, or external file/device
I/O. It neither executes inside `sub_9778()` nor consumes the shared gameplay
randring. It is intentionally outside the headless core.

## RNG, widths, and source-used stale state

- Decimal gameplay coordinates and velocities are signed 16-bit Q12.4, not
  floating point. Native uses explicit wrapping subpixel types.
- Source byte/word narrowing, signed byte angle deltas, wrapped counters,
  signed Boss HP, CPU halfheart comparisons, and 16-bit coordinate differences
  are explicit in native code.
- IRAND and randring are separate streams. The complete audit includes
  presentation-fed consumers: HUD particles, initial backgrounds, hit-circle
  creation, character Bomb visuals, and Boss/Extra render callbacks.
- Fresh start fills the ring and CPU/formation rings in source order. Route-1
  repeat refills the randring without resetting its cursor, retains resident
  random seed/carry state, performs 128 CPU IRAND draws per player, and uses
  the source repeat formation previous value of 1.
- The native core preserves source-used stale state such as previous Boss
  pattern, selected character-private counters, the shared Ellen Bomb release
  cursor, and relevant bullet-template angle. It does not preserve stale bytes
  proven to feed only sprites or never to be read before overwrite.

## Process, modes, and lifecycle

| Route | Source authority | Native disposition |
|---|---|---|
| Story round win/loss | `rndloop.cpp:175-234`, `entry.cpp`, MAINL `mlentry.cpp` | **Complete** winner route, lives decrement, game over, score owner, stage increment, ordinary 320-frame and final-Yumemi 460-frame result gate. |
| VS 1P/CPU, 1P/2P, CPU/CPU | `rndloop.cpp`, `entry.cpp`, resident mode values `0x80/0x81/0x82` | **Complete** first-to-two route-1 repeat and route-2 match exit; direct CPU/CPU Cancel exit retained. |
| Demo `0x7F` | `rstart.cpp:156-170`, `roundcb.cpp::demo_round_update()` | **Complete** for stock Demo records, including timer/result exit and attract input. A nonzero Demo number outside this mode is now rejected. |
| Continue and no-credit game over | `mainl/mlentry.cpp`, `mainl/screens.cpp`, `mainl/ending.cpp`, `cutscene/continue.cpp` | **Complete** deterministic resident mutations: score clear timing, credit check/decrement, accepted/declined stage/lives changes, Chiyuri/Yumemi progression and ending route. Presentation/cutscenes external. |

Native directly terminates after the final stage instead of performing the
source's terminal out-of-bounds `story_opponents[9]` read. No later gameplay
can observe that source landmine.

## Remaining discrepancies and explicit limitations

### P1 application boundary: live human pause and Q

Source `main/player/c7a5.cpp::sub_C7A5()` runs after both players and before
warning/background/late callbacks. It waits for Cancel release, permits Q to
exit, and resumes the same gameplay frame. Native reproduces CPU-vs-CPU Cancel,
and the recorded-replay protocol reproduces the stock pause through the
coordinator in `game.cpp` and `replay/run.cpp`. The direct live-human `Game`
input protocol does not expose the blocking menu/Q interaction yet. This must
be owned by a future application client or an explicit pause protocol; it is
not a replay-oracle gameplay mismatch.

### Resolved contract closure: non-Demo mode with nonzero demo number

Source `rstart.cpp:156-170` selects `demo_round_update` and attract input solely
from `resident->demo_num != 0`. Native applies the start overrides for any
nonzero `GameInit::demo_number`, but `frame.cpp:9135-9149` additionally requires
`game_mode == GM_DEMO` before advancing/exiting the demo. Stock OP code never
constructs Story/VS plus a nonzero demo number. `Game::create()` now rejects
that inconsistent combination while continuing to accept `GM_DEMO` with
either zero or nonzero demo number. The latter preserves the source's
artificial no-demo callback path when the number is zero. Valid Demo and all
replay records are unaffected.

### Resolved contract closure: Story P2 resident score

Source `rstart.cpp:89-117` loads only P1 `resident->score_last[0]` in Story.
The stock MAIN/MAINL lifecycle always keeps the non-owner score zero. Native
`round_start.cpp` now clears both scores and copies only
`GameInit::last_score[0]` in Story, matching the source's behavior even if an
artificial caller supplies a nonzero P2 carry. VS continues to clear both.

### Resolved contract closure: rank outside 0-3

The source contains rank switches whose out-of-range behavior depends on
prior/BSS state rather than defining a fifth difficulty. Stock CFG and replay
inputs use only ranks 0-3. `Game::create()` now rejects rank greater than 3
instead of applying native default values or normalizing it to a difficulty.

### Effective-stock and observability limitations

- Invalid bullet flags, zero-count generic rings, invalid shot modes, malformed
  ENEDAT, impossible player/runtime-snapshot bytes, and arbitrary DGROUP alias
  corruption are not promised public gameplay states. Some receive explicit
  errors; injected full runtime snapshots remain a trusted oracle/development
  interface rather than a complete hostile-input validator.
- Native `iatan2()` gives deterministic results for the source's unreachable
  exactly-one-`INT16_MIN` DIV landmine.
- Source-unused `byte_23AFA` pacing and the terminal Story opponent OOB read are
  not emulated.
- `t3case_state_hash()`/native `state_hash()` intentionally omit many live
  entities, Boss/Gauge flags, and other state. Hash equality is a strong compact
  oracle, not a proof that two complete machine states are identical.
- The graphical renderer, audio, menus, live pause UI, cutscenes, persistent
  score/config files, and PC-98 device behavior remain future client/platform
  work. The headless core's boundaries are kept independent of those clients.

## Verification after contract closure

The working tree on top of `69da429` was rebuilt with the GCC preset. CTest and
the direct unit binary pass with 264 checks and zero failures. The locked
external replay gate was then run with the GCC `th03rv` against the harness
evidence root and stock `ENEDAT.DAT`: all 19 V11/V12 replay files passed, with
zero failures. This gate checks the frozen size/SHA-256 lock before executing
the replays.

## Final disposition

No stock-reachable shared or character gameplay/RNG discrepancy was found at
native `69da429` against Anniversary `3e2381d4`. All nine character families
and the complete stock ENEDAT backend have a source-level disposition, and all
renderer-named deterministic side effects have been either retained or proven
presentation-only/unconsumed.

The all-green replay gate (19/19 after the contract closures, including all
locked V11 and V12 cases) corroborates this result over long executions and
branch-derived inputs. It does not replace the static coverage argument, and
the compact hash's omissions remain explicit. Future work should treat new
replay divergence as evidence of either an uncovered source path or a genuine
Anniversary/Replay branch difference rather than patching to the recording.
