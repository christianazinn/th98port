# Round, match, retry, and Story lifecycle dossier

Snapshot: **2026-08-08T16:53:11-07:00**

This is an implementation-ready static dossier for the complete Anniversary
round lifecycle surrounding `main/rndloop.cpp:sub_9778()`. It covers result
branching, route 0/1/2, in-process round repetition, Story loss and game over,
VS first-to-two matches, result bonuses, the final Yumemi round, the MAIN to
MAINL handoff, and Continue mutations that must cross an application/core
boundary.

The exact trees examined were:

| Tree | Revision | Notes |
|---|---|---|
| Native port | `33ad4189695236f72ca9bbe9cc20dc15d56de84e` plus dirty Phase 3 work | `frame.cpp` was modified at 2026-08-08 16:52:22 local time. No source file was changed for this dossier. |
| Anniversary donor | `3e2381d4275be7287f5525de5c95389d492b0a3c` plus instrumentation/build dirt | All conclusions below come from whole source definitions and their call chains, not an emulator run. |

Authoritative source definitions read in full:

- `th03/main/entry.cpp:main_entry()`
- `th03/main/rndloop.cpp:sub_9778()`
- `th03/main/rstart.cpp:round_startup()`
- `th03/main/resscore.cpp:sub_9EBF()`, `sub_A21F()`, and
  `resident_score_last_update()`
- `th03/main/player/defeat.cpp:sub_C0D8()`
- `th03/main/player/shot.cpp:sub_E313()` and `sub_E3F2()`
- `th03/main/hitcirc.cpp:sub_B4A3()`, `sub_B4A8()`, and `sub_B60A()`
- `th03/main/roundcb.cpp:demo_round_update()` and
  `round_mode_update_none()`
- reset helpers in `player/ch_shot.cpp`, `player/exatt.cpp`, `main_08.cpp`,
  `player/d3f9.cpp`, `bullet/bullet.cpp`, and `enemy/enemy.cpp`
- `th03/mainl/mlentry.cpp`, `mainl/screens.cpp`,
  `cutscene/continue.cpp`, and `mainl/ending.cpp`

Anniversary defines Story, Demo/attract, and VS modes. It does **not** define a
separate Practice mode in `th03/resident.hpp`; Practice would be a native
extension and must not be attributed to this source lifecycle.

## Core conclusion

Route 1 is an in-place repeat, not construction of a fresh game or stage. A
native implementation that assigns `DeterministicState{}` and restores a few
obvious fields will be wrong. ZUN's `sub_9EBF()` clears a precise list of
objects and deliberately or incidentally carries every other byte. Several of
those carried values are gameplay-visible in the next round.

Routes 0, 1, and 2 are completed-round/process results, while zero is also the
numeric value stored before a route completes. Those concepts cannot be
distinguished by the route integer alone. The native port now pairs the exact
source route with `round_complete`, so active play and completed route 0 are
unambiguous.

Recommended shape:

```text
LifecyclePhase::active_round
LifecyclePhase::repeat_transition      source route 1
LifecyclePhase::stage_or_match_exit    source route 2
LifecyclePhase::session_exit           source route 0
```

An `optional<SourceRoute>` would also have been valid. Current code uses the
separate flag: consumers must test `round_complete` rather than interpreting
`state.route == 0` by itself.

## Current dirty-tree status after lifecycle integration

Static recheck: **2026-08-08**, still based on port HEAD
`33ad4189695236f72ca9bbe9cc20dc15d56de84e` plus the concurrent dirty Phase 3
integration.

The dirty tree now represents the ordinary valid-stock in-process lifecycle:

- explicit `round_complete` distinguishes active play from source route 0;
- Story retry/game-over decisions, VS first-to-two repeat/completion, final
  bonus/gate, and exact route frames are present;
- route-1 performs a targeted in-place reset, carries `randring_cursor`,
  interleaves formation type/side draws, preserves live score/match fields,
  and runs the closing transition in the new round;
- Demo timing/exit and CPU-vs-CPU Cancel route 0 are present; and
- `last_fatal_player` is now separate from resident winner for the
  simultaneous-fatal bonus bug.

No remaining static discrepancy was found in the normal valid-stock route-1
reset or typed MAIN/MAINL session path after that integration:

- `finish_main_process()` persists source `resident->rand = round_frame` once
  for every terminal route, including route 0, terminal VS, game over, and the
  final ending;
- `build_session_transition()` represents route 0, VS winner-score
  persistence, ordinary Story stage advance, game over/high-score, Continue
  request/accept/decline/no-credit mutations, and the final ending/stage-99
  handoff; and
- focused contract tests cover Story and VS result routing, simultaneous
  fatal ordering, route-1 targeted reset/RNG order, Demo and CPU-vs-CPU route
  0, and MAIN/MAINL session transitions.

Frozen-oracle integration is complete for TH3R00 through TH3R05. TH3R01's
former two-byte shared-ring delta was traced to Rikako Boss dispatch rather
than lifecycle/session behavior; correcting the source state grouping makes
all 964 rows and 135,215 samples exact. Static closure does not replace these
replay-oracle gates.

The V11/T3CASE pause-menu gap from the original audit is now closed. The
headless replay coordinator and core deferred-frame API implement the semantic
adapter specified in `PARITY_RENDER_PHASE.md`; pause exit becomes route 0 only
after the already-started outer frame, and route 1/2 retains precedence.

Remaining P1/source-policy edges:

- Closed in the 2026-08-08 late-render audit: selecting `sub_B4A8` now
  preserves the independent transition phase byte, and a focused contract test
  covers a nonzero carried phase. Ordinary transitions still enter at phase
  zero; the correction matters for callback-overlap states such as a Demo timer
  and result gate coinciding.
- Live human-mode pause/Q interaction is implemented through the staged Game
  and interactive-driver protocol. Separately, recorded V11/T3CASE pause input
  is mandatory replay-coordinator behavior and remains covered by focused
  deferred-frame/route-precedence tests.
- Final `story_opponents[9]`/`unused_2` overwrite is not represented, which is
  acceptable only under an explicit terminal-state normalization policy.
- The lower clamp in native Story safety and the native requirement that Demo
  mode accompany nonzero `demo_number` differ for malformed state. Validate
  inputs or specify those as supported-input restrictions.
- Eight-digit score addition now preserves the source's raw most-significant
  byte addition (including its top-digit overflow quirk), with focused test
  coverage.

## End-of-defeat origin

`player/defeat.cpp:sub_C0D8()` owns the transition from active play to result
time.

On the final defeat-animation update, in player update order:

1. `lose_anim_time` changes from 1 to 0.
2. The losing playfield shift and defeat visual scratch are reset.
3. `lose_anim_time` becomes `0xFF`.
4. `defeat_flag` becomes `DF_BANNER`.
5. `players[1 - loser].rounds_won` increments as an 8-bit field.
6. `resident->pid_winner` becomes `1 - loser`.
7. `round_or_result_frame` becomes 0.
8. The same source frame continues into the render and result branch with
   result frame 0. Its final counter block increments result frame to 1.

`round_frame` is **not** reset at defeat. It continues through the complete
result interval and is written to `resident->rand` at the eventual route
boundary.

### Simultaneous-fatal ordering bug

The source maintains two different winner origins. On the frame where a
player's half-hearts reach zero, `player_update()` writes that PID to
`byte_20E3D`. Forty-eight animation updates later, `sub_C0D8()` independently
sets `resident->pid_winner` from whichever defeat animation completes first.
They agree for an ordinary single defeat.

If both players become fatal on the same frame, P1's fatal write occurs first
and P2's occurs last, leaving `byte_20E3D == 1`. On the shared completion frame,
P1's animation is processed first; it sets the banner, awards the round to P2,
and makes P2 the resident winner. P2's update then sees the banner and skips
its own completion. The resulting split state is therefore:

```text
resident winner / round win: P2
last fatal PID (byte_20E3D): P2
sub_E3F2() derived winner:   P1 = 1 - byte_20E3D
```

This is a gameplay-affecting ZUN bug for a completed VS match: winner bonus
statistics and live bonus score are applied to P1, while route-2 score
persistence copies P2's live score and zeros the other resident score. The
defeat banner is also drawn from `byte_20E3D`, but that part is presentation.
Story loss routing uses the resident winner and does not call the winner bonus,
so the split does not change its retry/game-over branch. No RNG is consumed.

The native state now stores `last_fatal_player` separately from resident
`winner`. `result_bonus_update()` derives its target as
`1 - last_fatal_player`, and focused simultaneous-fatal tests preserve the
source-order split rather than “fixing” it implicitly.

## Exact result decision table

The following table is the branch in `sub_9778()` whenever
`defeat_flag == DF_BANNER`.

| Mode/outcome | Entry condition | Before transition gate | Gate begins | Returned route |
|---|---|---|---|---|
| VS, match not complete | Neither player's `rounds_won >= 2` | No winner bonus | Result frame 160 | Route 1 at frame 176 |
| VS, match complete | Either player's `rounds_won >= 2` | `sub_E3F2()` every result frame | Result frame `word_1E6E8`, normally 320 | Route 2 at frame 336 |
| Story, P1 wins | `resident->pid_winner == 0` | `sub_E3F2()` every result frame | 320 for stages 0-7; 460 for stage 8 | Route 2 at 336 or 476 |
| Story, P1 loses with reserve life | Winner is P2 and `story_lives != 0` after transition | No winner bonus | Result frame 160 | Decrement `story_lives`, then route 1 at frame 176 |
| Story, P1 loses without reserve life | Winner is P2 and `story_lives == 0` | No winner bonus | Result frame 160 | Save P1 score, then route 2 at frame 176 |
| Demo/attract | Demo callback or any physical input requests exit | Defeat branch is not required | Demo callback opens the same trapezoid | Route 0 after transition completion |
| Pause/quit | Direct CPU-vs-CPU Cancel, stock human Q, or a non-Resume T3CASE pause selection sets the loop-exit byte | None | None required | Route 0 at loop test |

The final-stage gate is chosen by `story_stage < STAGE_YUMEMI`, without an
explicit mode check. Valid VS startup normally has Story stage 0, so its match
gate is 320.

### Result time does not freeze gameplay

`DF_BANNER` only changes the late result branch and causes each
`player_update()` to skip normal movement/attack work. Every frame up to and
including the route-boundary frame still executes the ordinary prefix in exact
source order: Extra Attacks, Boss updates, character callbacks, Charge Shots,
formations, bullets, enemies, fireballs, gauge patterns, input latch, player
bookkeeping, and deterministic render-side callbacks. Residual objects can
therefore move, collide, score, update combos, and consume either RNG stream
during the complete 160/320/460-frame wait.

On route-boundary frame 176/336/476, that full prefix has already run before
the result branch returns. Only the late after-result suffix and frame counters
are skipped. Route-1 reset must begin from the post-prefix RNG/entity state,
not from a snapshot taken when the defeat banner first appeared.

On every non-boundary result frame, score/extend processing also runs after the
result decision and bonus update. Residual scoring or an ordinary winner bonus
can cross a Story extend threshold during result time. In particular, a P1
loss that entered the banner with zero reserve lives can still gain a life
before frame 176; the frame-176 branch then sees one life, decrements it to
zero, and returns route 1 instead of game over. Yumemi's frame-1 bonus first
sets `extends_gained` to disabled, so her remaining-life bonus cannot do this.

### Transition timing

At the gate frame, `sub_9778()` assigns `fp_1FBC0 = sub_B4A8` and later calls
that callback in the same frame. `sub_B4A8()` increments its phase byte once
per frame. Calls occur on:

- 160 through 175 for retry/incomplete-match transitions;
- 320 through 335 for ordinary win/match transitions;
- 460 through 475 for Yumemi's final transition.

The sixteenth call sets `byte_1FBC3 = 1`, resets the phase byte, and changes
the callback pointer to `sub_B60A`. On the next frame, the result branch sees
the completion byte and returns before calling the closing callback or the
normal end-of-frame counter block. Therefore route boundary states retain
result frames 176, 336, and 476 exactly. A winning route-2 frame does call
`sub_E3F2()` once more before testing completion; “return before the suffix”
does not mean “return before the final bonus update.”

For route 1, `sub_A21F()` then resets the trapezoid phase and completion bytes
but deliberately does not replace the callback pointer. Consequently,
`sub_B60A()` runs in the repeated round at `round_or_result_frame` 0 through
15, after combo processing and before the frame counters. Its sixteenth call
restores the no-op `sub_B4A3()` pointer. Route 2/0 leaves the process, so no
closing callback runs there.

The triangles drawn by `sub_B4A8()` and `sub_B60A()` are presentation-only.
The opening callback's 16-step phase, completion flag, and route gate are
gameplay/process control and must remain in the headless core. The carried
closing callback only mutates transition-render scratch, its own phase, and
its own pointer; a headless core may collapse it once the route-1 boundary has
been represented, unless presentation state itself is included in a
checkpoint. Neither callback consumes RNG.

## Winner bonus state machine

`player/shot.cpp:sub_E3F2()` runs only for a Story victory or a completed VS
match. It derives the winner as `1 - byte_20E3D`, where `byte_20E3D` is the
losing PID recorded on fatal damage.

At result frame 1 it snapshots the winner's accumulated statistics and
calculates an unsigned 32-bit total:

```text
combo_hits_max          *  1,000
gauge_attacks_fired     * 10,000
boss_attacks_fired      * 15,000
boss_attacks_reversed   * 20,000
boss_panics_fired       * 30,000
final-stage lives       *100,000    only when story_stage == 8
```

For the final stage, `story_lives` is copied before multiplication and
`extends_gained` is set to `0xFF` before the first bonus score addition. This
prevents the final bonus itself from granting an extend.

The per-frame addition is:

```text
bonus_per_frame = min(total / 192, 0xFFFF)
if bonus_per_frame < remaining:
    score_add(bonus_per_frame)
    remaining -= bonus_per_frame
else if remaining > 0:
    score_add(remaining)  // remaining is then <= 0xFFFF
    remaining = 0
```

The source uses a signed `long` comparison, but all stock-produced totals are
positive and below `INT32_MAX`. The displayed remainder/high portions are
presentation-only. The snapshotted statistics, 32-bit arithmetic, quotient
cap, remaining total, score additions, and final extend disable are gameplay
state.

`sub_E3F2()` is also called on result frame 0. Initialization is guarded by
frame 1, and process BSS or the preceding path leaves remaining bonus zero.
Do not move initialization to frame 0 merely because it looks cleaner.

## Route semantics in `main_entry()`

Every returned route first executes:

```text
t3case_route(route)
resident->rand = round_frame
```

The `resident->rand` write is critical for cross-stage determinism. A new MAIN
process seeds IRAND from this persisted **round-frame count**, not from the
outgoing IRAND seed. Route 1 does not reseed the still-running process; its
write only persists state for a later process boundary.

| Route | `main_entry()` behavior | Persistent gameplay mutation |
|---:|---|---|
| 0 | Finish session, free formations, stop BGM, execute OP | `resident->rand = round_frame`; no Story-stage increment |
| 1 | Call `sub_A21F()`, announce a new replay round, loop into `sub_9778()` again | `resident->rand = round_frame`, then in-place repeat reset and `round_id++` |
| 2 | Finish session, free formations, stop BGM, increment `resident->story_stage`, execute MAINL | Winner/score persistence has already occurred in `sub_9778()`; stage increments even for VS |

The unconditional route-2 Story-stage increment is source behavior. It is
normally meaningful only in Story; MAINL immediately takes VS to its terminal
win/game-over presentation.

### Exact route-0 timing

The loop-exit byte is tested only at `sub_9778()`'s loop header. A callback or
pause action that sets it does not abort the current frame: the remaining
suffix and frame counters execute, and route 0 is returned at the following
loop test.

For an uninterrupted no-input Demo, `demo_frame` advances from 0 to
`DEMO_N == 7000`. At round/result frame 7000 the Demo callback merely installs
the opening trapezoid callback, after that frame's no-op transition call.
Opening calls therefore run at frames 7001..7016. On the sixteenth call, the
Demo callback installs the opening callback again, observes the completion
byte, and sets loop exit; counters advance to 7017, and the next loop test
returns route 0. `demo_frame` remains 7000. Any physical input requests the
same route-0 exit after the current frame finishes.

This is distinct from route 1/2, whose defeat-result completion check returns
from inside the result branch before the suffix counters. No route-0
presentation callback consumes gameplay RNG.

## First-round startup versus route-1 repeat

The distinction must remain explicit.

| Operation | `round_startup()` | `sub_A21F()` repeat |
|---|---:|---:|
| Seed IRAND from `resident->rand` | Yes | **No** |
| Shared random-ring cursor | Fresh MAIN BSS starts at 0; refill itself does not write it | Carry outgoing cursor through refill |
| Load ENEDAT and allocate formation rings | Yes | No |
| Set `round_id` | Set to 0 | Increment existing 8-bit value |
| Run `sub_9EBF()` targeted round reset | Yes | Yes |
| Initialize/copy live score from resident | Story copies only P1; fresh-process P2 remains BSS zero | No; both live scores carry |
| Reset `rounds_won` | Yes, both to 0 | No; match wins carry |
| Set both gauges available to 64.0 | Yes | No; availability carries |
| Reset match-wide combo maxima | Yes | No; maxima carry |
| Set extends policy | Yes | No; carries |
| Choose input mode | Yes | No |
| Register character callbacks and character initial state | Yes | No |
| Result-transition callback | Set to no-op before play | Carries `sub_B60A`; closes during new-round frames 0..15 |
| Select result gate 320/460 | Yes | No; same stage gate carries |
| Initialize randomized background | `farfp_20F20()`, after startup | No |
| Recompute existing background velocities | No | `farfp_20F28()` (`sub_D05D`) |
| Restart BGM | Yes | No; song continues |
| Clear pages/HUD/sound effects | Yes | Yes, with different presentation sequence |

Calling `initialize_first_round()` for route 1 is therefore semantically
wrong even if selected fields are copied back afterward.

## Exact `sub_9EBF()` order

Order matters because this function changes both the master.lib IRAND stream
and the random ring.

1. Clear both per-player special-target override words
   (`word_1F32A[0..1]`), `ef_onehit`, trapezoid phase and completion, and
   `enemy_speed`.
2. `sub_E313()` frees all shot pairs and EFE slots and sets:
   `round_frame = 0`, `round_or_result_frame = 0`, frame-wait byte = 1, loop
   exit byte = 0.
3. `randring_fill()` calls IRAND for indices 255 down to 0. TH03's local
   definition does **not** reset the shared `randring_p`; its outgoing cursor
   carries across the refill and into the repeated round.
4. `bullets_reset()` frees every bullet and resets the shared bullet-trail
   cursor to 0.
5. Free every EFE slot again.
6. Clear the active byte at the start of all 16 Extra Attack slots on both
   playfields. The remaining bytes of each slot are not cleared.
7. Free every shot pair again.
8. Reset character Charge Shot storage through, in this exact order:
   `sub_14A76()` (Reimu), `sub_153BB()` (Mima), `sub_142D0()` (Marisa),
   `sub_1B653()` (Ellen), then `sub_193BC()` (Ellen Extra Attack reference
   pointers). These helpers are targeted; they do not zero every byte of every
   character buffer.
9. In Story only, recalculate P2 CPU safety.
10. Set round speed, bullet base speed, Boss level, and non-Story gauge-level
    increments for the selected rank; clamp each maximum.
11. Clear launched Boss owner, defeat, and HUD-start flags; set Boss threshold
    to 5,120; clear round-frame modulo bytes.
12. Reset each player in P1-then-P2 order. Each player consumes 64 IRAND calls
    to rebuild its CPU charge threshold ring.
13. Randomize all 256 formation entries. Every entry consumes at least one
    IRAND for type and exactly one for side; repeated type draws consume more.
    Reset both formation cursors and alive counts.

The five targeted helper calls in step 8 have these exact writes:

| Helper | Exact writes |
|---|---|
| `sub_14A76()` | Clear byte 0 of 24 Reimu Charge Shot records, stride 32 bytes (12 records per player). |
| `sub_153BB()` | Clear byte 0 of 30 Mima Charge Shot records, stride 10 bytes (15 records per player). It does not clear Mima's separate per-player active/frame bytes. |
| `sub_142D0()` | Clear the active byte of each of the two Marisa Charge Shot buffers. |
| `sub_1B653()` | Clear byte 0 of 64 Ellen Charge Shot nodes, stride 12 bytes (32 nodes per player). |
| `sub_193BC()` | Rewrite 12 Ellen Extra Attack record pointers per player. It does not clear their payloads; the generic Extra Attack loop already cleared each 32-byte slot's active byte. |

No equivalent character-specific reset is dispatched for Kotohime, Kana,
Rikako, Chiyuri, or Yumemi in this route. Their active work is stopped only by
the generic entity/shot/Extra Attack clears and by the per-player fields that
`sub_9EBF()` actually writes.

Route 1 then calls `farfp_20F28()` to recompute background velocities. It
consumes no RNG. It sets both player hyper callbacks to standby and performs
only presentation/sound/HUD work afterward.

### Repeat RNG contract

The repeat RNG sequence is therefore:

```text
256 IRAND calls: random ring fill, descending destination order
0 cursor writes: preserve the outgoing shared random-ring cursor
64 IRAND calls: P1 CPU charge ring
64 IRAND calls: P2 CPU charge ring
for formation entries 0..255, interleaved:
    >=1 IRAND calls: type, including immediate rejection retries
    1 IRAND call: side
0 calls: background velocity reset and repeat HUD/presentation
```

The formation type count is data-dependent. Test the resulting IRAND seed,
entire random ring, entire formation rings, and cursors rather than only an
assumed count.

The first round differs only in the formation predecessor and later background
initialization: ENEDAT load sets `formation_prev = formation_count`, while
every completed randomization sets it to 1 for the next repeat. First-round
background initialization consumes IRAND; repeat background reset does not.

## Rank and retry formulas

Let `r` be the current 8-bit `round_id` after the repeat increment and `s` the
resident Story-stage byte.

| Rank | `round_speed` before clamp | `bullet_base_speed` | `gba_boss_level` before clamp | Non-Story gauge increment per repeat |
|---|---:|---:|---:|---:|
| Easy | `r << 4` in 8-bit AL | 0 | `(s / 2) + r` | +1 |
| Normal | `r << 5` in 8-bit AL | 0 | `s + (2 * r)` | +2 |
| Hard | `(r << 5) + 0x20` in 8-bit AL | 8 | `s + (2 * r) + 2` | +2 |
| Lunatic | `0x60` | `0x18` | `s + (2 * r) + 8` | +4 |

Round speed clamps to `0x7F`; Boss and both gauge levels clamp to 16. Every
round-speed/Boss-level calculation and non-Story gauge increment is stored or
performed in an 8-bit register before the clamp. Overflow can therefore wrap
below the clamp threshold. Very large `round_id` values and malformed carried
gauge levels must use explicit byte arithmetic. Story does not increment gauge
levels on repeat; the stage-derived first-round values carry.

Story P2 safety uses:

```text
i = (round_id - remaining_credits) + 3
if i > 5: i = 5
base = story_cpu_safety_frames[story_stage][i]
Easy:    base / 3
Normal:  base
Hard:    base * 2
Lunatic: base * 3 + 500
clamp to 0xFFFF
```

There is no source lower clamp for `i`. Valid stock credits are at most 3, so
the normal first-round/retry cases stay nonnegative. The native port currently
lower-clamps malformed values; retain that only as an explicitly documented
input-contract decision.

Non-Story CPU safety is assigned once during first startup and carries across
route-1 repeats: 1000, 1900, 4000, or `0xFFFF` by rank, with `0xFFFF` for
CPU-vs-CPU regardless of rank.

## Reset/carry matrix for route 1

The safest implementation is a named in-place `repeat_round_reset()` that
spells out every reset. Do not zero the containing state. In the tables below,
“carry” means no writer was found in the complete route-1 call chain; it is
not a recommendation to clean the value.

### Persistent/session and match fields

| State | Route-1 behavior |
|---|---|
| `game_mode`, rank, Story stage, remaining credits, skill | Carry |
| `story_lives` | Carry after the loss branch has already decremented it once |
| live P1/P2 scores | Carry |
| `extends_gained` | Carry |
| `resident->pid_winner` / native winner | Carry until the next defeat overwrites it |
| `resident->rand` | Set to outgoing `round_frame`; does not reseed live IRAND |
| `round_id` | Increment before reset |
| player `rounds_won` | Carry |
| player `combo_hits_max`, `combo_bonus_max` | Carry across the match |
| `gba_gauge_level` | Story carries unchanged; VS increments by rank and clamps |
| `cpu_hit_damage_additional`, extend policy, result gate | Carry |
| live IRAND seed | Carry after the reset's refill/ring generation advances it; never reseed from `resident->rand` |
| Demo frame and round-mode callback | Carry; a Demo defeat/repeat does not restart its 7,000-frame timer |
| input mode and most character callback bindings | Carry; the active `hyper` callbacks are the exception below |

### Per-round shared fields

| State | Route-1 behavior |
|---|---|
| `round_frame`, `round_or_result_frame`, modulo bytes | Reset to 0 |
| loop-exit flag | Reset to 0 |
| result trapezoid phase/completion | Reset to 0; route 1 carries the `sub_B60A` binding, which runs during new-round frames 0..15 and then installs the no-op callback |
| `round_speed`, `bullet_base_speed`, `gba_boss_level` | Recomputed from rank/round and clamped |
| `combo_points_for_boss_attack` | Reset to 5,120 |
| current launched Boss owner | Reset to none |
| per-player special-target override flags (`word_1F32A[0..1]`) | Reset both |
| inactive Boss object bytes and character Boss scratch | Not blanket-cleared; future launch logic initializes what it needs |
| defeat flag | Reset to none |
| HUD-start flag | Reset to init |
| enemy speed | Reset to 0 |
| one-hit EFE latch (`ef_onehit`) | Reset to false |
| frame-wait byte / loop-exit byte | Set to 1 / reset to 0 |
| foreground shifts, damage-all, warning flags, active gauge flags | Reset per player |
| next gauge flags | Not explicitly reset; inactive warning/active state makes stale values dormant until overwritten |
| bomb flags and bomb frames | Not explicitly reset; the long result interval normally lets active bombs expire |
| autofire/shot-gate scratch (`byte_220FC`) | Carry |
| losing PID and winner-bonus scratch/remaining total | Carry; a later fatal defeat/frame-1 bonus overwrites the live fields before normal use |
| bullet template and collision scratch | Carry across the reset; the first new gameplay frame resets its normal per-frame portions before entity callbacks |
| CPU target scratch and other transient register-derived helpers | No blanket source reset; reset only where their owning callback does so |

### Entity, combo, and chain fields

| State | Route-1 behavior |
|---|---|
| random ring and shared cursor | Refill all 256 bytes; carry the cursor unchanged |
| bullets | All flags free; trail ring cursor 0; stale payload bytes carry |
| enemies and fireballs (EFE pool) | All flags free; stale payload bytes carry |
| enemy alive counts | Both 0 |
| shot pairs | All inactive; stale payload bytes carry |
| Extra Attacks | Active byte of all 16 slots per player cleared; other slot bytes carry |
| Reimu Charge Shot storage | Clear byte 0 of every record; collision parity byte `byte_205CC` and all payload bytes carry |
| Mima Charge Shot storage | Clear byte 0 of every record; separate per-player active/frame bytes `byte_20E24[]`/`byte_20E26[]` and payload bytes carry |
| Marisa Charge Shot storage | Clear the two per-player active bytes; payload bytes carry |
| Ellen Charge Shot storage | Clear byte 0 of every node; payload bytes carry |
| Ellen Extra Attack reference table | Rebuilt; referenced generic Extra Attack active bytes are already clear, payload bytes carry |
| combo display records | Not explicitly reset; the 176+ result frames normally settle active combos to time/hits/bonus zero |
| chain cursor and chain arrays | **Carry.** This affects which physical chain slot the next round allocates. |
| `boss_panic_fired_in_current_combo` | Carry unless ordinary combo logic cleared it before transition |
| formation type/side rings | Fully regenerated |
| formation cursors | Both 0 |
| `formation_prev` | 1 for every repeat randomization |

### Player fields

| Reset for each player | Value |
|---|---:|
| character and CPU flags | Refreshed from resident |
| `is_hit` | false |
| `unused_1` | 0 |
| invincibility | 50 |
| shot mode | one pair |
| half-hearts | 10 |
| knockback time, move-lock time, knockback-active | 0, 0, false |
| center | `(144.0, 336.0)` in Q12.4 |
| gauge charged | 0 |
| shot active | enabled |
| next hit damage | 1 |
| CPU charge ring/cursor | 64 new IRAND-derived bytes; cursor 0 |
| bombs | 2 |
| lose animation, hyper-active, CPU frame | 0, 0, 0 |
| gauge/Boss attack counters | all 0 |
| active `hyper` callback, after `sub_9EBF()` | Set to generic `hyper_standby()` by `sub_A21F()` |

| Carried player byte/field | Consequence |
|---|---|
| gauge available | A player starts the next VS/retry round with the previous gauge amount, not 64.0 |
| rounds won | Required for first-to-two match flow |
| combo maxima | Required for the eventual completed-match bonus |
| base speed and gauge charge speed | Character is unchanged; startup callbacks are not rerun |
| human last movement, pattern numbers | Byte-exact carry; may affect the first ambiguous input/render frame |
| knockback angle/length, CPU dodge strategy, spell-ready scratch | Byte-exact carry, generally dormant after related active flags reset |
| CPU safety | Story P2 is recomputed; VS values and Story P1 carry |
| callback bindings | Character Extra Attack, Charge Shot, gauge-pattern, Boss, bomb, and stored `hyper_func` bindings remain installed; only active `hyper` is replaced by `hyper_standby()` |

## Story route-2 and MAINL mutations

Route 2 is a process/application boundary. Headless gameplay should not embed
cutscene rendering, but it must expose enough state for an application adapter
to apply the same resident mutations.

### Story victory

Before route 2, `resident_score_last_update(0)` copies live P1 score to
`score_last[0]` and zeros all eight P2 resident digits. `main_entry()` stores
the outgoing `round_frame`, increments Story stage, and executes MAINL.

For ordinary stages, MAINL selects the next opponent from
`story_opponents[story_stage]`, presents the win/cutscene/splash sequence, and
executes a fresh MAIN process. The new process seeds IRAND from the persisted
outgoing round-frame value and copies resident P1 score back into the live
score. Presentation polling does not alter gameplay RNG.

Current `Game::session_transition()` implements this no-loss handoff: outgoing
`round_frame` has become `resident_rand`, stage increments, Story lives/skill
carry, P1 score carries and P2 score clears, and the new P2 opponent is
selected. `next_story_stage_init()` remains a compatibility convenience over
that typed seam, separate from route-1 repeat.

### Story game over

When P1 loses with `story_lives == 0`, `resident_score_last_update(0)` runs,
route 2 stores `round_frame`, and MAIN increments Story stage before MAINL.
MAINL observes P2 as winner and reaches the Continue path.

Before opening Continue, MAINL calls `regist_menu()` while the saved P1 score
and already-incremented Story stage are still resident. High-score persistence
is application/service work, but its event must precede the score clear.
`continue_menu()` then first zeros **both** resident score arrays, even before
it checks whether credits remain.

| Continue condition/choice | Credits | Story stage | Story lives | Score |
|---|---:|---:|---:|---|
| No credits on entry | unchanged 0 | Remains incremented; early return skips the undo | Remains 0 | Both resident scores zero |
| Credits available, choose Continue | Decrement once | Decrement once, undoing MAIN's prior increment | Set to `CREDIT_LIVES` = 2 | Both resident scores zero |
| Credits available, choose No or Cancel | Unchanged | Decrement once, undoing MAIN's prior increment | Set to 2 despite quitting | Both resident scores zero |

Only the Continue choice returns to the stage splash and starts fresh MAIN on
the same stage. “No” proceeds to game over/OP after applying the stage/lives
mutations above.

These menu inputs belong to an application adapter. The deterministic seam
represents them as `ContinueChoice::accept`/`decline`; the focused session tests
exercise both choices without presentation code.

### Final Yumemi victory

The played stage is index 8. Its differences are all deterministic:

1. Result gate is 460.
2. Frame-1 bonus includes `story_lives * 100000`.
3. `extends_gained` becomes disabled before scoring.
4. Opening callback runs on frames 460..475.
5. Route 2 is returned at frame 476.
6. P1 score persists, outgoing `round_frame` persists, Story stage increments
   from 8 to 9.
7. MAINL recognizes `STAGE_COUNT`, runs ending/staff/high-score flow, sets
   Story stage to `STAGE_ALL` (99), and exits to OP rather than starting
   another gameplay stage.

Within step 7, `ending_staff_and_regist()` runs ending/staff presentation,
then sets stage 99, then invokes high-score registration with the persisted P1
score, and only afterward may run the credit/character-gated extra ending.
Those are application events; their ordering and pre-clear score payload are
still part of the handoff contract.

There is one terminal ZUN landmine before step 7. `mainl/screens.cpp:sub_9887()`
copies `story_opponents[story_stage]` into P2 before checking
`story_stage == STAGE_COUNT`. At stage 9 this reads one byte past the nine-byte
array—resident `unused_2`—and stores it as P2's paletted character, then returns
the ending result. The ending path does not use P2 for gameplay and consumes no
gameplay RNG, so this is terminal/presentation state rather than a gameplay
rule. Preserve or explicitly normalize it at the application boundary if
resident-state byte parity is a product requirement.

The ending presentation is outside libth03 gameplay. The stage-9 terminal
handoff and final score/lives/extend state are not.

## VS lifecycle

VS uses one MAIN process for all rounds of a first-to-two match.

- Each defeat increments the winner's `rounds_won` before result branching.
- With scores 1-0, 0-1, or 1-1, result frame 160 starts the transition and
  route 1 occurs at 176.
- `sub_A21F()` increments `round_id`, performs the targeted reset, preserves
  both match-win counters and match-wide combo maxima, and continues the same
  BGM/process.
- At 2 wins, `sub_E3F2()` awards the winner using maxima and counters carried
  across the match. The gate is normally 320 and route 2 occurs at 336.
- The winner's live score is copied to resident storage and the loser's
  resident score is zeroed.
- MAINL presents the winner, then non-Story control goes directly to terminal
  game-over/OP flow. No further gameplay round is started.

Human-vs-human, human-vs-CPU, CPU-vs-human, and CPU-vs-CPU share this lifecycle.
Only input and first-start CPU-safety selection differ.

## Presentation and RNG classification

| Callback/work | Classification | Headless behavior |
|---|---|---|
| Defeat banner sprites | Presentation-only | Omit after preserving winner, rounds won, and result-frame reset. |
| Result text/stat rendering in `sub_E3F2()` | Presentation-only | Omit. |
| Result stat snapshot, total, quotient, score drain, final extend disable | Gameplay | Preserve exactly. |
| Opening trapezoid triangles | Presentation-only | Omit drawing. |
| Opening trapezoid phase/completion and route gate | Gameplay/process control | Preserve the exact 16 calls and completion boundary. |
| Closing trapezoid in repeated-round frames 0..15 | Presentation-only, zero RNG | Omit in headless unless presentation callback state is checkpointed; it must not delay gameplay. |
| Route-1 page fills, HUD wipe/static redraw, SE reset | Presentation-only | Omit from headless core. |
| Route-1 `sub_D05D()` background velocity recomputation | Presentation-only and zero RNG | May be omitted by headless; required for future pixel renderer. |
| `sub_9EBF()` random-ring refill, CPU charge rings, formation rings | Gameplay RNG | Preserve exact order and widths. |
| MAINL win/cutscene/stage splash rendering and audio waits | Application presentation | Keep outside deterministic gameplay. |
| MAINL Continue input loop | Application input with resident mutations | Keep UI outside; expose deterministic choice mutation. |
| Continue score/credit/stage/life writes | Game/session state | Preserve in transition adapter. |

No result, trapezoid, route-1 presentation, or MAINL presentation callback in
this audited chain consumes the gameplay random ring or master.lib IRAND.
`sub_9EBF()` is the sole lifecycle RNG boundary and must run before the next
round starts. This statement is limited to lifecycle-specific callbacks: the
ordinary gameplay prefix continues throughout result time and can consume both
streams as described above.

## Native implementation status

The lifecycle/session dependencies above are present in the current tree:

1. `round_complete` separates active play from exact source routes 0/1/2.
2. `SessionTransition` carries resident/session state across the core boundary,
   including credits, scores, Story lives/stage, winner, skill, and
   `resident_rand`.
3. `result_bonus_update()` implements the frame-1 snapshot, final-life bonus,
   extend disable, 320/460 gate, completed-VS eligibility, and exact numeric
   behavior.
4. `result_control_update()` implements Story win/loss, reserve-life retry,
   game over, incomplete/completed VS, and exact route-boundary frames.
5. `initialize_repeat_round()` performs the source's targeted in-place reset
   and exact RNG refill/order; `start_next_round()` applies it without
   reconstructing the `Game`.
6. `finish_main_process()` and `build_session_transition()` preserve the
   terminal route-0/route-2 MAIN and MAINL mutations described above.
7. Optional presentation remains outside the headless gameplay/session core.

The transition implementation should not depend on any character callback
work. Its only character-sensitive carry is existing callback/state storage;
route 1 intentionally does not redispatch character setup.

## Contract tests

These tests define the deterministic unit/component contract. The current
suite covers the principal routing, simultaneous-fatal, repeat-reset/RNG,
Demo/route-0, and MAIN/MAINL cases; the remaining items retain value as
boundary-coverage requirements alongside long replay-oracle runs.

### Branch and timing tests

1. Fatal P1/P2 damage: verify losing PID, winner, exactly one rounds-won
   increment, banner, and result frame 0 in the completion frame.
2. Simultaneous fatal damage: P2 remains the last-fatal PID, P2 receives the
   round win because P1's animation completes first, and a completed-VS bonus
   targets P1 while route-2 persistence targets P2.
3. VS incomplete match: gate starts at 160, transition calls cover 160..175,
   route 1 appears at 176, and no bonus/stat snapshot occurs.
4. VS completed match: bonus starts at frame 1, gate starts at 320, route 2 at
   336, and the correct winner score persists while the loser clears.
5. Story loss with lives 2, 1, and 0: routes are respectively retry with 1,
   retry with 0, then game over without underflow.
6. Story loss at zero lives with a result-time threshold crossing: extend
   processing raises lives to 1 before frame 176, which consumes it and returns
   route 1 with zero lives rather than route 2.
7. Ordinary Story win: bonus/gate/route at frames 1/320/336.
8. Yumemi win: lives bonus and extend disable at frame 1, gate 460, route 476,
   stage 8 to terminal stage 9.
9. Route frame test: no after-result suffix work (warning/HUD/combo,
   trapezoid/round-mode callback, or round counter) executes on the frame that
   returns route 1/2. Verify that winning route 2 still performs its final
   `sub_E3F2()` call before the completion check.
10. Route 0 test: active and completed-route-zero are distinguishable.
11. Route-1 callback carry test: the closing callback is selected at route 1,
   its phase/completion bytes reset, and it executes alongside gameplay on
   repeated-round frames 0..15 before restoring the no-op pointer.
12. Uninterrupted Demo test: opening calls occur at 7001..7016, counters reach
   7017 before route 0, and `demo_frame` remains 7000. A physical-input exit
   also completes its current suffix/counter pass.

### Bonus arithmetic tests

1. Each statistic individually contributes its exact multiplier.
2. Combined total uses unsigned 32-bit arithmetic.
3. Quotient is floor division by 192 and caps at `0xFFFF`.
4. A typical total drains in the exact source number of result frames and the
   last remainder is added once.
5. Total zero adds nothing; retain the source frame-1 state transition.
6. Final life count is snapshotted once and score threshold crossing cannot
   grant an extend after disable.
7. Source eight-digit score behavior is used by every bonus addition,
   including top-digit overflow quirks.

### Reset/carry tests

1. Seed every deterministic field with a distinct canary, call the route-1
   reset, and assert every row of the reset/carry matrix.
2. Verify score, gauge availability, rounds won, combo maxima, chain cursor,
   autofire gate, and callback/character bindings carry.
3. Verify half-hearts, center, bombs, invincibility, gauge charged, attacks
   fired, entities, active Extra/Charge flags, warning/defeat/Boss ownership,
   and counters reset.
4. Verify inactive payload bytes in pools are not globally zeroed where source
   only clears flags.
5. Verify Story P2 safety is recomputed and VS safety carries.
6. Verify gauge-level increments and round/Boss/bullet speeds for every rank
   and several `round_id` values, including byte-wrap boundaries.

### RNG and formation tests

1. From a known outgoing IRAND seed and nonzero shared-ring cursor, compare the
   entire refilled random ring, unchanged shared cursor, both 64-byte CPU
   rings, entire formation type/side rings, formation cursors, and final seed
   with an Anniversary oracle.
2. Verify random-ring fill destination order 255 down to 0.
3. Verify P1 CPU ring is generated before P2.
4. Verify first-round predecessor is `formation_count`; every repeat
   predecessor is 1.
5. Force formation rejection draws and verify the final seed/order.
6. Verify repeat performs zero background IRAND calls.
7. Spawn a residual RNG-consuming entity before the defeat banner and verify
   that it consumes in every result prefix, including the route-boundary
   prefix, before route-1 refill begins from the resulting seed/cursor.

### Process/application handoff tests

1. On every route, persisted random value equals outgoing `round_frame`, not
   outgoing IRAND seed or global replay frame.
2. Ordinary Story win persists P1 score, zeros P2, increments stage, and seeds
   the new stage from the prior round frame.
3. Story game over with no credits zeros both resident scores but leaves the
   already incremented stage and zero lives; emit high-score registration with
   the pre-clear score first.
4. Continue Yes decrements credits, undoes stage increment, resets lives to 2,
   clears score, and restarts the same stage.
5. Continue No/Cancel leaves credits, undoes stage increment, resets lives to
   2, clears score, and terminates.
6. Final win increments stage to 9 and produces an ending terminal handoff,
   never `next_story_stage_init()`.
7. VS route 2 persists only the winner score and terminates after presentation.
8. If resident-byte parity is enabled, final-stage handoff reproduces the
   `story_opponents[9]`/`unused_2` terminal P2 overwrite; otherwise test the
   documented normalization policy.

## Lifecycle completion gate

This subsystem is complete only when a single native `Game` can execute a
multi-round VS match and a Story retry without reconstruction, while a session
adapter can execute ordinary next-stage, game-over/Continue, final ending, and
route-0 exits with exact persisted state. A full replay should then validate
the implementation; it should not be needed to discover which state survives
route 1.

The current definitions and focused tests satisfy this static contract.
Frozen-oracle validation is green for every TH3R00-TH3R05 corpus case.
