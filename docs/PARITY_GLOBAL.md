# Global gameplay parity audit

Snapshot: **2026-08-08T20:56:10-07:00**

This is the consolidated static comparison of the native deterministic core
against the Anniversary ReC98 source after the 2026-08-08 character,
lifecycle, renderer, RNG, and numeric closure parcels. It starts at the
complete round loop and follows every gameplay subsystem reachable from that
loop. It does not claim pixel parity. Dynamic claims are limited to the frozen
replay-oracle results listed below.

The exact revisions and working-tree state examined were:

| Tree | Revision | State at snapshot |
|---|---|---|
| Native port, `th03-port` | `33ad4189695236f72ca9bbe9cc20dc15d56de84e` | Dirty: integrated Phase 3 gameplay, lifecycle, session, replay-adapter, tests, and documentation |
| Anniversary donor, `oracle_anniversary` | `3e2381d4275be7287f5525de5c95389d492b0a3c` | Dirty: build/instrumentation artifacts were present |

The port was edited concurrently during the audit. This final refresh was
checked against the current dirty tree after those parcels converged. Function
names are more authoritative than volatile line numbers. The character,
lifecycle, renderer, RNG, and numeric ledgers remain the detailed authorities.

## Executive conclusion

The static port is now source-complete for stock-reachable `MAIN.EXE` character
callbacks, the shared gameplay pipeline, deterministic render-side effects,
RNG consumers, and the valid-stock round/match/session lifecycle. Replays are
now serving as integration validation rather than discovery of absent callback
families.

Frozen Anniversary oracles are fully green for TH3R00 through TH3R05. TH3R01's
former two-byte shared-ring delta was caused by an incorrect Rikako Boss switch:
source state 6 enters the four-spread pattern, but native had grouped it with
states 2-5. Restoring the complete state grouping makes all 964 TH3R01 rows and
all 135,215 samples exact.

Remaining fidelity limitations are narrower and explicit: the intentionally
compact schema-3 checkpoint hash, live human pause/UI ownership, and optional
terminal resident-byte normalization. Numeric cases N1 and N6 now have explicit,
tested policies at the architecture boundary. A green replay remains path-scoped
evidence, not a proof of every presentation path.

Status terms used below:

- **Complete**: direct comparison found the relevant gameplay state,
  deterministic side effects, ordering, and RNG consumption equivalent for
  valid stock inputs. This is still subject to dynamic validation.
- **Partial**: a real counterpart exists, but some source behavior or proof is
  absent.
- **Missing**: no counterpart exists, or the dispatch cannot execute one.
- **Effective stock parity**: omitted source paths are proven unused by stock
  data or unreachable in the stock control flow.
- **Presentation-only**: drawing/sound work was checked not to mutate gameplay
  state and not to consume a deterministic RNG stream.

## Authoritative per-frame order

The source of truth is Anniversary `th03/main/rndloop.cpp:sub_9778()`. The
native core splits one source frame into prefix, input latch, optional warning
interstitial, and suffix. That API split is acceptable only while preserving
the following order.

| Order | Anniversary operation | Native counterpart | Status and significance |
|---:|---|---|---|
| 1 | `collmap_reset()`, `bullet_template_reset_stuff()` | Collision-map clear at start of `gameplay_frame_prefix()`; group creation supplies native templates directly | Complete for translated paths. No stale collision marks survive a frame. |
| 2 | P1 `exatt.update`, `gba_boss_update`, `callback_205CE`; then P2 | `extra_attacks_update()`, `bosses_update()`, `bomb_character_update()` for P1 then P2 | Complete. All nine stock character families are dispatched and source-audited. |
| 3 | `hitcircles_update()`, `shots_update()` | Visual hitcircle records omitted; `shots_update()` present | Hitcircle ageing is presentation-only. Hitcircle **creation RNG** is separately retained. |
| 4 | P1 then P2 `chargeshot_update` | `chargeshots_update()` iterates P1 then P2 | Complete; individual mechanics are closed in the character ledgers. |
| 5 | `sub_BB12()` round-start HUD effect | `hud_start_update_prefix()` | Gameplay gate and all deterministic presentation RNG retained. Drawing omitted. |
| 6 | formations, bullets, enemies, fireballs | Formation calls are conditionally suppressed by the HUD flag; `bullets_update()`, `enemies_update()`, and `fireballs_update()` always continue | Complete. **Only formations self-suppress during the round-start HUD.** Carried bullets, enemies, and fireballs continue; enemy hittest remains before fireballs. |
| 7 | P1 pellet and bullet gauge patterns; then P2 | `gauge_patterns_update()` P1 then P2 | Complete for all nine characters. Gauge callbacks also continue during the round-start HUD. |
| 8 | `SUB_CEB2` visual-record update | Omitted | Presentation-only: ages a 12-record effect pool, no RNG or gameplay mutation. |
| 9 | `input_mode()`, replay I/O | `gameplay_input_latch()` | Native input becomes visible after all prefix simulation, matching the source. |
| 10 | P1 `player_update`, then P2 | `gameplay_frame_suffix_before_interstitial()` iterates P1 then P2 and selects human/CPU per player | Complete shared dispatch at this snapshot. Both CPU players are supported. |
| 11 | sound update and pause | Sound omitted; live pause is application-owned; recorded T3CASE pause uses the core deferred-frame API and the headless four-choice coordinator | Complete for the replay-validation contract. Live human pause/release/Q remains an application boundary. |
| 12 | `SUB_CA3C`, P1/P2 `SUB_CB81`, background callback | `warning_interstitial_begin()`, 27 interstitial input latches in `game.cpp`, `warning_frame_update()`, `background_update()` | Required ordering is represented. The interstitial consumes input/global frames without advancing round counters. |
| 13 | Bomb render P1/P2, Boss render P1/P2, then ordinary entity render callbacks | `bombs_render_phase_update()`, `boss_render_phase_update()`, `fireballs_hittest()`, `extra_attacks_render_phase_update()` | Complete deterministic materialization for all nine characters. Drawing-only work is omitted. |
| 14 | defeat banner, bonus, route decisions | `result_control_update()`, `story_extend_update()`, `result_transition_update()` | Complete for valid-stock Story, VS, Demo, route 0/1/2, retry/game-over, and final-Yumemi paths. |
| 15 | `sub_C830`, `sub_C8C4`, `sub_D52E`, `SUB_C9FE`, HUD, combo, result callback, round callback | Defeat/result state, warning portrait update, `combos_update()`, result transition and round-mode update | Complete deterministic behavior; presentation-only HUD/sound work is omitted. |
| 16 | round/result counters, speed, 1024-frame ring refill, damage increments, CPU-frame reset | End of `gameplay_frame_suffix_after_interstitial()` | Complete, including source early-return/route precedence. |

Anniversary has an every-other-render branch controlled by `byte_23AFA`.
`round_startup()` initializes it to zero and no stock writer was found, so
omitting that branch is effective stock parity. It must not be confused with
the warning interstitial, which is stock-used and deterministic.

## Source-to-port parity matrix

### Round setup, formations, and enemies

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| `main/rstart.cpp:round_startup`, `main/entry.cpp`, `main/resscore.cpp:sub_9EBF/sub_A21F` | `round_start.cpp:initialize_first_round()`, `initialize_repeat_round()` | Every round | Complete for valid stock state | Fresh and targeted in-place repeat initialization, rank/safety/gauge/Boss updates, callback carry, entity resets, and route-1 state carry are explicit. |
| `main/rstart.cpp` formation randomization | formation rings in first/repeat initialization | Every round | Complete | Fresh startup begins with `previous = formation_count`; repeat uses `previous = 1`. Both generate 256 non-repeating type indices and interleave one side draw per accepted type. |
| `main/enemy/enemy.cpp:enemy_formations_update`, formation add | `add_formation()` | Every active round | Complete for stock data | Both playfields update before bullets. Cursor, direction/type rings, delay, and alive-count gates are represented. |
| `main/enemy/enemy.cpp:enemy_run` | native enemy VM inside `enemies_update()` | Every formation | Effective stock parity | All stock-used operations are implemented. `EO_CLIP_BOTTOM`, broken `EO_LOOP_ABS`, and broken `EO_LOOP_REL` are source bloat explicitly unused by original ENEDAT; native has clip-bottom support but omits both loops. |
| `main/enemy/enemy.cpp:enemies_update`, `enemy_hittest` | `enemies_update()` | Every active round | Complete for valid stock inputs | Pool order, movement, reverse hittest, HP-before-decrement behavior, explosion conversion, formation alive counts, and speed tuning are structurally equivalent; `PARITY_NUMERIC.md` records the stock ENEDAT range proof. |
| `main/enemy/expl.cpp`, EFE ordering in `enemy.cpp`/`fireball.cpp` | `for_each_explosion()`, explosion/chain helpers | Every explosion chain | Complete | The native traversal preserves physical enemy explosions before fireball explosions and the Anniversary slot-reuse quirks used by chains. |

### Bullets, trails, fireballs, and shots

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| `main/bullet/bullet.cpp` group enum and add engine | bullet group constants and `bullets_add_group()` | Enemies, gauge attacks, bosses | Complete for valid groups | All declared group IDs 0-29 and 32-44 are dispatched, including horizontal symmetric group 34. A zero-count ring is malformed source input; source divides by zero while native rejects it. |
| `main/bullet/bullet.cpp:bullets_update` | `bullets_update()` | Every active round | Complete | Velocity/acceleration, pellet transfer, clipping, collision-map insertion, and per-slot order compare structurally. |
| `main/bullet/bullet.cpp` six-point shared trail ring | `BulletTrailState`, `bullet_trail_update_and_clip()` | Mima and Chiyuri Boss Attacks | Complete | Six samples, 48 shared slots, shared-ring overwrite behavior, X clamp, last-point clipping, and both stock producers are present. |
| `main/enemy/fireball.cpp:fireball_add`, update, hittest/render | `fireball_add()`, `fireballs_update()`, `fireballs_hittest()` | Every active round | Complete | Three add-time RNG draws, transfer/fall targeting, red-generation quirk, reverse hittest, explosion-chain slot reuse, score/gauge/chain changes, and red descendants are represented. |
| `main/player/shot.cpp:shots_add`, `shots_update` | `shots_add()`, `shots_update()` | Human and CPU fire | Complete shared engine | 32-slot pair pool, modes, bounds, velocity, and collision processing compare. Sprite animation and sound are presentation-only. |
| `main/player/ch_shot.cpp` and character callbacks | `chargeshots_update()` plus nine `chargeshot_update_*()` functions | Charge attacks and some hypers | Complete | All nine callbacks and deterministic renderer dispositions are source-audited in the character ledgers. |

### Collision and player control

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| `main/collmap.asm`, `collmap_reset.asm`, `colvline.cpp` | map clear and `collmap_set_enemy`, `_fireball`, `_rect_striped`, `_bullet` | Every active round | Complete for translated writers | Tile math, vertical stride four, rectangle/vline clipping, and bottom boundary compare. |
| `main/collmap_slope.asm` | `collmap_set_slope_striped()` | Chiyuri Extra Attack | Complete | The slope writer and Chiyuri producer are both present. |
| `main/player/move.cpp:player_hittest` and ASM dependencies | native `player_hittest()` | Every vulnerable player update | Complete Anniversary-compatibility behavior | Packed scan, clipping, first-column mask, and the negative-column register-alias behavior are intentionally reproduced. The isolated `anniversary_borland_runtime_collision_alias_profile` models the exact initialized DGROUP region a wrapped probe can bit-mask; `PROVENANCE.md` records its linker-map ownership and compatibility scope. |
| `main/player/move.cpp:player_update`, movement and charge logic | `player_update_begin/finish`, `player_update_human()` | Human-controlled players | Complete shared logic | Input timing, movement fallback, clamps, charge/release, bomb gate, hyper lifetime, knockback, damage, and shot gate are closed. TH3R00 specifically verifies source fallthrough that releases a pending Ellen charge in the same update after CPU movement activates Hyper. |
| `main/player/c433.cpp`, `c54a.cpp`, `c568.cpp`, CPU parts of `bomb.cpp`; target helper in `main/bullet/bullet.cpp:sub_16983` | `cpu_safe_move()`, `cpu_target_movement()`, `cpu_target_point()`, `player_update_cpu()` | Story P2 and VS CPU | Complete shared logic at snapshot | Both P1 and P2 select CPU update by `is_cpu`. VS safety frames are initialized for both. Target priority, boss/no-target sentinels, bounds fallback, safety phase, discarded `%3` ring draw, dodge, charge, and bomb thresholds compare. |
| same CPU source, retry/random-strategy fragments | no distinct native retry branch | Not effectively reachable | Effective stock parity, document as quirk | Counter/control flow makes the apparent retry/random dodge-strategy path unreachable. Do not “restore” it without proving reachability. |

### Gauge attacks, warnings, combos, and score

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| `main/player/gauge.cpp`, `bomb.cpp` | gauge availability/charge, release, attack flags | Every active round | Complete shared state | Attack, reversal, Boss, and panic thresholds and counters are represented. |
| `main/player/warning.cpp:SUB_CA3C`, `SUB_C9FE`; `d3f9.cpp:SUB_CB81` | warning begin/frame/portrait and `game.cpp` interstitial | Gauge/Boss attacks | Complete deterministic state | Portrait-to-white-to-27-input-sample interstitial-to-red-frame sequence is preserved. Palette and text are presentation-only. |
| character gauge callbacks | nine `gauge_pattern_*()` functions | Every gauge attack | Complete | All nine formulas, guards, state transitions, bullet/pellet groups, and RNG order are source-audited in the character/RNG ledgers. |
| `main/player/combo.asm`, `combo.cpp`, `enemy/expl.cpp`, chain fields | `combo_add()`, `combos_update()`, chain helpers | Every explosion chain | Complete | Saturation, settlement timing, displayed/total score movement, chain cursor, hit count, charge slots, fixed-round-speed thresholds, and known slot-reuse quirks are represented. |
| `main/player/score_add.asm:score_add` | `frame.cpp:score_add()` | Every scoring event | Complete | Source AAA-normalizes only the first seven digits and raw-adds the most-significant byte; native preserves that top-digit overflow quirk. |
| `main/player/score.cpp` extend logic | `story_extend_update()` and result lifecycle | Story | Complete | Ordinary thresholds and final-Yumemi ordering are present; final bonus disables extends before adding remaining-life score. |

### Bosses, damage, defeat, and round results

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| `main_03.cpp:sub_F402`, `sub_F4B4`, `sub_F512`, `sub_F52D`, `sub_F58C`, `sub_F1FA`, `sub_F356`, `sub_F3A9` | `boss_launch_or_reverse()`, target/pattern/hittest/defeat/exit helpers | Every Boss Attack | Complete shared helpers | Launch RNG, pattern selection, reversal, target flag, collision, damage, defeat threshold, level/threshold reset, exit, and the render-phase 2,560-point award compare structurally. |
| per-character Boss callbacks in `main_03*.cpp` | nine `boss_update_*()` implementations and `bosses_update()` dispatch | Boss Attack | Complete | All nine update/collision/exit families and deterministic render-side state are present. |
| `main/player/exatt.cpp` and per-character callbacks | nine `extra_attacks_update_*()` implementations | Explosion chains and gauge behavior | Complete | All nine attack families, including Chiyuri slope collision and Yumemi transfer/RNG order, are present. |
| character bomb prefix/render callbacks, including `main/bullet/mbomb.cpp:mima_17043` and `main/bullet/bullet.cpp:mima_bomb` | `bomb_character_update()`, `bombs_render_phase_update()` | Bombs | Complete deterministic side effects | Mima consumes one shared-ring value at frames 8..64 and shakes frames 81..143. Reimu/Marisa/Rikako/Chiyuri/Yumemi presentation RNG and all known playfield shifts are retained. Drawing is omitted. |
| `main/player/bomb.cpp` fatal PID write; `main/player/defeat.cpp:sub_C0D8` | `player_defeat_update()`, `last_fatal_player`, and `winner` | Every round loss | Complete | The 48-frame countdown, +/-6 losing-playfield shift and reset, damage-all, winner, rounds-won, and simultaneous-fatal split between bonus owner and persisted winner are explicit and tested. |
| `main/rndloop.cpp` defeat/result branch; `main/player/shot.cpp:sub_E3F2`; `main/resscore.cpp` | result bonus/control/transition plus session handoff | Every round end | Complete for valid stock lifecycle | Story retry/life decrement/game-over/Continue handoff, VS first-to-two continuation and completed-match bonus, Demo/CPU-vs-CPU route 0, and route 1/2 precedence are represented. Anniversary has no separate Practice mode. |
| `main/rstart.cpp:round_startup`, `main/resscore.cpp:sub_9EBF`, `sub_A21F`, `main/entry.cpp` | `initialize_repeat_round()`, `Game::start_next_round()` | Every retry or later VS round | Complete | Targeted resets/carries, unchanged randring cursor across refill, round/rank/safety/gauge/Boss updates, formation predecessor/order, and closing-transition carry are explicit and tested. |
| final Yumemi result path | final result/session path | Story stage 9 | Complete | Adds `story_lives * 100000`, disables further extends first, uses gate 460/route frame 476, and emits the ending/stage-99 handoff. Optional unused resident-byte overwrite remains a documented normalization choice. |

### RNG, fixed-point arithmetic, and verification state

| Anniversary source and functions | Native counterpart | Stock use | Status | Precise discrepancy or evidence |
|---|---|---|---|---|
| master.lib IRAND and random ring implementations | `random.cpp:irand`, `randring_fill`, `randring_next16` | Global | Complete primitive | LCG, descending fill 255..0, one shared cursor, adjacent-byte 16-bit read, and index-255 high-byte-after-wrap landmine are preserved. ReC98 `randring1`/`randring2` code instances share one ring/cursor; the single native cursor is correct. |
| `rndloop.cpp` 1024-frame refill | end-of-frame counter block | Long rounds | Complete | Refill occurs after simulation and result callbacks on the same cadence as source. |
| all source call sites | distributed native consumers | Global | Source-complete static and corpus closure | The source-derived `PARITY_RNG.md` ledger maps every stock-reachable active-round, startup, route-1, HUD/background, bomb/Boss, hit-circle, bullet-group, and transfer consumer. All frozen TH3R00-05 oracles are green. |
| `th03/math/vector.cpp`, integer trig tables, Turbo 16-bit types | `numeric.hpp`, `trig.cpp`, `vector2()` and call sites | Global | Complete with explicit N1/N6 boundary policies | Explicit wrapping helpers and source-range proofs close N2-N5. N1 preserves deterministic unsigned-magnitude normalization for the source's unreachable faulting edge; N6 rejects invalid Story construction while retaining source-valid non-Story resident bytes. See `PARITY_NUMERIC.md`. |
| signed subpixel conversion | `screen_x_to_playfield_subpixel()` | Cross-playfield fireballs/bullets | Complete | Uses `exact::wrap_shift_left()` and never left-shifts a negative signed host value. |
| `main/t3case.cpp:t3case_state_hash()` schema 3 | `state_hash.cpp` canonical hash | Replay checkpoints | Deliberately compact oracle | The frozen schema includes scores, players, counters, selected globals, combo/chain state, and RNG seed/cursor but not every transient pool. Green full-run replay results gain strength from butterfly effects and terminal equality; semantic subsystem diagnostics remain the first-divergence tool, not part of the stable schema. |

## Deterministic side effects hidden in render callbacks

Headless does not mean that every source render callback may be deleted. This
classification is required at each application/core boundary.

| Source behavior | Classification | Headless requirement |
|---|---|---|
| Ordinary enemy, shot, bullet, and fireball sprite drawing/animation | Presentation-only after update/hittest has run | Omit safely. |
| Hitcircle record ageing/rendering | Presentation-only | Omit safely. |
| `hitcircles_enemy_add()` randomized visual center | Presentation-RNG | Preserve two random-ring draws unless the source caller explicitly requests no random centering. |
| `SUB_CEB2`/`SUB_CEE0` 12-record effect update/draw | Presentation-only | Omit safely. |
| HUD palette, text, gauge overlays, sound | Presentation-only | Omit safely. |
| HUD-start particle initialization and background effects | Presentation-RNG / IRAND | Preserve exact call count and order even when no pixels are produced. |
| `SUB_CE5B` visual spawn | Presentation-RNG | Preserve one random-ring draw per call. |
| Bomb render callbacks | Deterministic gameplay side effect | Preserve all RNG draws and `playfield_fg_shift_x` schedules. The shift changes cross-playfield transforms. |
| Defeat foreground shake | Deterministic cross-playfield side effect | Preserved through the 48-frame sequence and reset at defeat completion. |
| Reimu Extra Attack renderer | Gameplay state mutation | Preserve attack-frame advancement. |
| Character Charge/Boss render callbacks | Must inspect individually | Preserve any state, RNG, collision, award, or route mutations. |
| Boss defeat renderer | Gameplay score mutation | Preserve the 2,560-point award and boss release. |
| Result trapezoid callback | Gameplay control flow | Preserve route-completion gate even without rendering. |
| Warning interstitial | Input/timing mutation | Preserve all 27 samples/global frames; do not advance active-round counters. |

## Remaining exact limitations at this snapshot

No stock-reachable Anniversary character callback, active-round RNG consumer,
or valid-stock lifecycle route remains statically missing. The open items are:

1. Schema 3 intentionally omits full transient pools; it is a stable sparse
   oracle rather than a complete serialization of deterministic state.
2. Live human pause/release/Q input belongs to the future application client.
   Recorded T3CASE pause semantics and boundary exhaustion are implemented.
3. The final source write to unused terminal resident bytes is normalized away
   unless resident-byte parity is later selected as a product requirement.

Mima's prefix RNG at frames 8, 16, ..., 64 and frame-81..143 foreground shake
are both retained. The same whole-definition rule was applied to every other
renderer-named callback.

## Stock-used versus source-unused paths

| Path | Classification | Port policy |
|---|---|---|
| `EO_LOOP_ABS`, `EO_LOOP_REL` | ZUN bloat/bugs unused by original ENEDAT | May remain omitted for stock-game parity; document if source-completeness becomes a product requirement. |
| `EO_CLIP_BOTTOM` | Unused by original ENEDAT | Native already implements it; it is not a current blocker. |
| `byte_23AFA` every-other-render path | No stock writer found after round startup clears it | Omit as effective stock parity unless supporting modified data/runtime snapshots that can set it. |
| CPU retry/random-strategy-looking branch | Effectively unreachable under source counters/control flow | Preserve effective behavior, not an imagined repair. |
| Bullet group zero-count rings and invalid IDs | Malformed input, not valid stock data | Native defensive behavior may differ, but the supported-input contract must say so. |
| Demo/attract callback and round-start overrides | **Stock-used mode**, not source-unused | Core timing, startup overrides, and route-0 exit are implemented; pixels/client presentation remain outside headless scope. |
| VS later-round lifecycle | **Stock-used mode** | In-place route-1 repeat and first-to-two completion are implemented and covered by TH3R04/TH3R05. Anniversary has no separate Practice mode. |

## PC-98 width and wrap audit

The native implementation correctly avoids floating point for gameplay. Most
coordinates and speeds retain their PC-98 fixed-point scales (primarily Q12.4
and Q4.4), and `numeric.hpp` supplies explicit 16-bit bitcasts, wrapping
addition/subtraction/negation, and arithmetic right shift.

Turbo C++ uses a 16-bit `int`; host C++ normally promotes narrow values to a
32-bit `int`. `PARITY_NUMERIC.md` audits those expression boundaries and the
stock operand ranges. Named wrap/multiply/divide/shift helpers and focused
tests close the stock-used N2-N5 cases, including negative cross-playfield
conversion and the raw most-significant score byte.

N1 is closed by an explicit deterministic normalization for the source's
unreachable/faulting `iatan2()` magnitude edge, with a literal instruction-model
test documenting the source behavior. N6 is closed by narrow construction-time
validation of Story stage and credit ranges plus internal assertions for generic
ring counts. Keep these range assertions at parsers and initializers so later
content cannot silently invalidate the valid-stock proofs.

## Prioritized implementation plan and dependencies

### Closed P0: Classic corpus divergence

TH3R01's first differing frame was global frame 19,326. Anniversary entered
Rikako Boss state 6's four-spread callback, consumed angle then type, and added
eight bullets; native incorrectly dispatched state 6 to the spreads callback.
The corrected full grouping is spreads 2-5, Extra Attacks/rings 7-12, and
four-spreads 6 plus 13-17. A focused test locks boundary states 6, 7, and 13,
and the complete frozen oracle is now green.

### P1: harden deterministic validation

1. Keep reusable semantic subsystem hashes/dumps available for first-
   divergence work without expanding the frozen T3SPLIT schema.
2. Add or retain schedule tests for the exact `sub_9778()` order, especially
   HUD-start behavior: only formations suppress themselves; bullets, enemies,
   fireballs, and Gauge callbacks continue.
3. Keep character callback-completeness and route-1 reset/carry tests so future
   refactors cannot silently lose a source path.
4. Continue treating score as an excellent end-to-end butterfly oracle while
   using semantic state only to localize a mismatch.

### P1: retain architecture contracts

1. Retain the documented N1 unsigned-magnitude normalization and its literal
   source-model tests; do not invent host-dependent behavior for the source's
   faulting edge.
2. Retain N6's narrow Story construction validation and generic-ring invariants
   without rejecting source-valid stale resident bytes in non-Story modes.
3. Keep the Anniversary DGROUP collision-alias profile isolated, documented,
   and covered by its compatibility tests.
4. Decide whether final unused resident bytes require byte parity or remain a
   documented terminal-state normalization.

### P2: future application boundary

1. Keep rendering, audio, device input, menus, replay UI, and online services
   outside the deterministic gameplay core.
2. Implement live human pause/release/Q behavior in the client; do not conflate
   it with the already-complete recorded replay-pause adapter.
3. Validate Demo/attract, Story, VS CPU, VS human, and CPU-vs-CPU with source
   recordings as corpus coverage expands. Any future Practice mode is a native
   extension, not Anniversary behavior.

## Completion gates

Current gate status:

- [x] Every stock-reachable character callback has a native implementation.
- [x] The source-derived RNG ledger has no unmapped stock-reachable call site.
- [x] Story win/loss/retry/game-over/final and VS repeat/match transitions are
  represented, including separate simultaneous-fatal bonus/winner ownership.
- [x] Route-1 targeted reset/carry, RNG, formation, and callback ordering have
  focused tests and full TH3R04/TH3R05 replay coverage.
- [x] TH3R00 and TH3R02-05 complete their frozen oracles through natural
  terminal boundaries.
- [x] TH3R01 matches all 964 frozen rows and 135,215 samples.
- [x] N1/N6 are explicit, tested architecture/input contracts rather than
  host-dependent undefined behavior or silent malformed-input normalization.
- [ ] Broader mode corpus coverage and a future client are needed before a
  playable-application claim; they are not prerequisites for the headless
  gameplay architecture.

Replay equality after static closure is strong evidence that the source-backed
implementation is correct for the exercised path. It remains path-scoped and
must never justify patching over a true cross-branch inconsistency.
