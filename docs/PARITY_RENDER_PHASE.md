# TH03 shared late-frame and render-phase parity audit

Snapshot: **2026-08-08T20:41:45-07:00**

This is a static closure audit of every shared, non-character callback after
`player_update()` in Anniversary `th03/main/rndloop.cpp:sub_9778()`. It also
checks the late callbacks' ordering against the current native dirty tree.
Character Bomb, Boss, Charge, Extra, and Gauge callback formulas remain owned
by the character parity dossiers, but their exact positions in the shared
pipeline are recorded here.

Revisions examined:

| Tree | Revision | Notes |
|---|---|---|
| Native `th03-port` | `33ad4189695236f72ca9bbe9cc20dc15d56de84e` | Dirty concurrent Phase 3 tree; this audit added two isolated corrections and tests |
| Anniversary donor | `3e2381d4275be7287f5525de5c95389d492b0a3c` | Read-only source donor |

No emulator was run. “Closed” below means source-proven static parity plus a
focused native contract where noted, not replay-oracle proof.

## Result

The shared native late pipeline now preserves all stock-reachable gameplay,
RNG, scoring, collision, and route side effects found in the audited shared
callbacks once a gameplay sample has reached the core. Three isolated
discrepancies were corrected:

1. `SUB_CA3C()` invokes `sub_C7A5()` after every one of its 27 warning-wait
   input samples. CPU-vs-CPU Cancel now sets a persistent native exit request
   from any interstitial sample, even if later samples release Cancel.
2. Assigning `fp_1FBC0 = sub_B4A8` changes only the source callback pointer.
   It does not reset `byte_1FBC2`, the independent transition phase byte.
   Native result-callback selection now preserves `result_phase_frame`.
3. On-wire phase 1 is also used by the Replay Patch pause menu. The native
   core now exposes a deferred-frame boundary, and the headless replay
   coordinator implements the complete four-choice pause parser without
   treating its variable auxiliary stream as a fixed warning wait. It retains
   the raw phase-0 checkpoint, accounts every auxiliary logical sample, and
   completes input-exhaustion exits after the already-started frame.

Exact character-callback parity remains a separate dependency: this audit
proves where those callbacks run, not the contents of each implementation.

## Exact source order and native mapping

The order below starts immediately after P1 then P2 `player_update()`.
“Terminal return” means source route 1 or 2 returns directly from `sub_9778()`;
the remainder of the row and frame-counter suffix is skipped.

| Order | Anniversary operation | Gameplay/RNG/control side effects | Native operation | Status |
|---:|---|---|---|---|
| 1 | `snd_se_update()` | Audio only | Omitted | Presentation-only |
| 2 | `if(input_sp & INPUT_CANCEL) sub_C7A5()` | CPU-vs-CPU sets `byte_23B00`; human mode enters pause and can Q-exit. T3CASE playback clears Cancel before this call after consuming its own phase-1 pause stream | `gameplay_frame_suffix_before_interstitial()` sets `session_exit_requested` for direct CPU-vs-CPU Cancel | Closed for direct stock control; T3CASE pause interception still belongs to the replay driver |
| 3 | `SUB_CA3C()` | If either warning is white, changes all white warnings to red, performs exactly 27 held-input samples without advancing round counters, and invokes the Cancel handler after each sample | `warning_interstitial_begin()`, 27 `interstitial_input_latch()` calls | Closed, including Cancel on any of the 27 samples |
| 4 | `SUB_CB81(0)`, then `(1)` | Advances red-warning state; on completion clears the warning and copies `gba_flag_next[pid]` to active. Palette oscillators are visual | `warning_frame_update(0)`, then `(1)` | Closed deterministic state and order |
| 5 | `farfp_20F24()` background callback | Initial A consumes 48 IRAND calls per outer frame through source frame 129; initial B consumes 40 through frame 145. Steady callbacks consume no RNG | `background_update()` | Closed gameplay RNG and callback phase |
| 6 | `byte_23AFA` alternate-render gate | If nonzero and result frame even, skips orders 7–28 and enters counters | Not represented | Effective stock parity: startup writes zero and no stock writer exists |
| 7 | P1 then P2 Bomb render callbacks | Character-dependent random-ring reads and foreground shifts; Ellen also mutates its visual-vector pool | `bombs_render_phase_update()` in P1/P2 order | Shared order closed; formulas owned by character/RNG dossiers |
| 8 | P1 then P2 Boss render callbacks | Character collision/hittest/damage and the shared defeated-boss 2,560-point award/release | Character Boss state plus `boss_render_phase_update()` | Shared award/order closed; character callbacks remain a parity dependency |
| 9 | `shots_render()` | Advances each live shot pair's sprite animation offset; no gameplay, collision, score, or RNG | Omitted | Presentation-state only |
| 10 | `enemies_render()` | Drawing only; enemy hittest already ran in `enemies_update()` | Omitted | Presentation-only |
| 11 | `fireballs_hittest_and_render()` | Reverse-slot collision pass; shots/Charge/Bomb/explosion hits; chain, combo, gauge and score mutations; Extra/Fireball spawning; hit-circle RNG | `fireballs_hittest()` | Closed shared gameplay path and position in order; drawing omitted |
| 12 | `hitcircles_render()` | Drawing only; creation-time random centering occurs at callers | Omitted | Presentation-only; caller RNG retained |
| 13 | P1 Extra render, P1 Charge render, P2 Extra render, P2 Charge render | Character-dependent frame/collision/RNG state | `extra_attacks_render_phase_update()` plus translated Charge collision paths | Exact shared position recorded; character dossiers remain authoritative |
| 14 | P1 then P2 `player_render()` | Defeat-ring drawing only. Defeat countdown, winner, rounds won, damage-all, and foreground shift already changed in the earlier `player_update()` | Omitted | Presentation-only at this call site |
| 15 | P1 then P2 `player_overlay_render()` | Glow/charge drawing only | Omitted | Presentation-only |
| 16 | `SUB_CEE0()` | Draws the 12-record visual-effect pool and advances type-2 visual angles | Omitted | Presentation-state only; no RNG/collision/gameplay reader |
| 17 | `bullets_render()` | Drawing only | Omitted | Presentation-only |
| 18 | If defeat banner: `sub_C2F9()` and result/route branch | Banner itself is visual. Branch invokes result scoring, starts transition callbacks, consumes Story lives, persists score, and may terminal-return route 1/2 | `result_control_update()` | Closed for in-core route behavior; `finish_main_process()` and `session_transition()` expose the following MAIN/MAINL handoff |
| 19 | `sub_C830()`, `sub_C8C4()` | Gauge/Hyper drawing only | Omitted | Presentation-only |
| 20 | `sub_D52E()` | Draws score and awards Story extends from score digit 6; can turn a zero-life result into one final retry before its later route gate | `story_extend_update()` | Closed and correctly after result control |
| 21 | `SUB_C9FE(0)`, then `(1)` | Changes each portrait warning from 1 to white 2 after drawing it | `warning_portrait_update()` P1/P2 | Closed |
| 22 | `sub_BE5D()` | Round-start HUD drawing only | Omitted | Presentation-only; its update/RNG half is earlier `sub_BB12()` |
| 23 | `combos_update_and_render()` | P1 then P2 timer decrement; on zero clears highest hits, adds bonus to score, clears bonus | `combos_update()` | Closed |
| 24 | `fp_1FBC0()` | No-op/opening/closing transition callback; opening sets completion after its 16th call, closing installs no-op after 16 calls | `result_transition_update()` | Closed, including phase-byte preservation on callback selection |
| 25 | `fp_1E6EA()` | Demo timer; selects opening callback at 7000 and requests route 0 on completion or nonzero system input | `round_mode_update()` | Closed core behavior |
| 26 | Pacing, palette, page swap, playfield-bottom clears | Presentation/timing only; page parity only selects later visual cels | Omitted | Presentation-only |
| 27 | `round_frame++`, `round_or_result_frame++`, mod counters | Gameplay timing | Native suffix counters | Closed |
| 28 | Every 64 result frames: speed increase; every 1024: refill ring and raise pending damage; below 2000 reset both CPU frames | RNG/gameplay state | Native counter cadence | Closed |
| 29 | Loop test of `byte_23B00` | Route 0 after completing this frame | Native applies persistent exit after counters | Closed; route 1/2 terminal return takes precedence, matching source |

## Callback side-effect inventory

### Warning and interstitial control

`SUB_CA3C()` does more than a palette flash. Its one outer-frame invocation
contains 27 replay-visible input samples. It changes every white warning to red
before those samples, but the per-player red-frame increment does not occur
until `SUB_CB81()` afterward. Simultaneous white warnings share the same single
27-sample interstitial.

Every interstitial sample calls `sub_C7A5()` when Cancel is held. For two CPU
players that handler sets the loop-exit byte immediately and returns; the
remaining samples and the rest of the outer frame still execute. The native
exit request must therefore be sticky across later released samples and must
only become route 0 after the counter suffix. This audit added that behavior.

For live human-controlled players, `sub_C7A5()` performs an interactive
release / pause / Q-exit loop. That future application behavior is distinct
from the V11/T3CASE pause stream described next; the two must not be conflated
by treating every phase-1 sample as warning input or every human Cancel as an
immediate core exit.

`SUB_CB81()` has two state machines:

- warning red frames: flag 3 becomes 4 on the first post-interstitial outer
  frame; flag 32 becomes 33; the following callback at 33 clears the flag and
  activates `gba_flag_next`;
- gauge palette oscillation in `byte_20E98/byte_20E9A`, which affects only
  palette output and does not feed attack timing, collisions, score, or RNG.

The native core retains the first and intentionally omits the second.

### Background callbacks

The background callback executes before every deterministic render-side
collision and before the defeat route branch. This placement is mandatory
because both initial callbacks consume the shared master.lib IRAND stream.

| Callback | Source behavior | Native retained state |
|---|---|---|
| `sub_D135()` initial A | 24 GRCG plus 24 sprite particles, one IRAND each; after all 48 calls, changes callback when `round_frame > 128` | 48 IRAND calls and transition to A-steady |
| `sub_D340()` initial B | 24 GRCG particles consume IRAND; only sprite indices 24–39 consume IRAND, total 40; after all calls changes callback when `round_frame > 144` | 40 IRAND calls and transition to B-steady |
| `sub_D1E7()` A steady | Moves 48 particles and sometimes changes shared visual angle/velocities | Callback phase only; zero RNG |
| `sub_D3F9()` B steady | Moves 50 particles and sometimes changes shared visual angle/velocities | Callback phase only; zero RNG |

Particle coordinates, velocities, and `angle_2142C` have no reader outside
the background renderer. Omitting them cannot alter later RNG call counts:
the initial callback counts are fixed by loop bounds, not particle state.

### Shared visual-effect pool

`sub_CEB2()` runs before input and ages all 12 `cee0_rec_t` records, updates
their radius, and frees age-over-16 entries. `SUB_CEE0()` later draws them and
advances only the type-2 angle based on the physical slot parity. Neither
function consumes RNG or participates in collision.

Creation remains relevant:

- `SUB_CDBD()` and `SUB_CE0C()` consume no RNG;
- `SUB_CE5B()` consumes exactly one random-ring value for its visual angle.

The port correctly retains each `SUB_CE5B()` draw at its translated caller.
The omitted pool update/render state is safe for the gameplay core, but a
future pixel renderer must own the pool and preserve its source slot cursor.

### Entity render calls

- `shots_render()` changes only `shotpair->so_anim`. Shot movement, lifetime,
  allocation, and collision use no animation field.
- `enemies_render()`, `hitcircles_render()`, and `bullets_render()` do not
  mutate source entity state or consume RNG.
- `player_render()` invokes `sub_C248()` during defeat, but that function only
  draws the already-updated defeat rings. `sub_C0D8()` in the earlier
  `player_update()` owns the countdown, foreground shift, damage-all, winner,
  rounds-won, and result-frame reset.
- `player_overlay_render()` is visual only.
- `fireballs_hittest_and_render()` is not presentation-only. Its hittest half
  remains at the source position after Boss render and before Extra/Charge
  render callbacks. The native port retains this as `fireballs_hittest()`.

### Result and late HUD callbacks

The defeat branch occurs before score extends, warning portrait conversion,
combo settlement, transition advancement, demo exit, and counters. Therefore
a route-1/2 terminal return skips all of those later operations and skips the
counter increment. Native `result_control_update()` returns early at the same
point.

On a continuing result frame, the exact gameplay suffix is:

1. `sub_D52E()` awards a Story extend if score digit 6 advanced;
2. P1 then P2 portrait warnings become white;
3. P1 then P2 combos decrement and possibly settle into score;
4. the selected transition callback advances once;
5. the Demo/normal round-mode callback runs;
6. counters and periodic difficulty/RNG state advance.

This explains the source-observable “earned retry” case: result scoring or
combo settlement can raise score digit 6, `sub_D52E()` grants a life, and a
later Story-loss route gate consumes it.

Selecting an opening transition at the defeat gate merely replaces a function
pointer. The phase byte is shared by opening and closing callbacks and is not
reset by assignment. The native selection now does the same; the callback then
advances that carried phase later in the same outer frame.

## Early-return and precedence contracts

| Condition | Source consequence | Native status |
|---|---|---|
| `byte_23AFA != 0` and even result frame | Warning/background ran; deterministic render/result/HUD/mode work skipped; counters still run | Stock-unused branch intentionally omitted |
| Defeat route 1 or 2 becomes ready | Background and all preceding render-side gameplay ran; later HUD/extend/combo/transition/mode/counters are skipped | Closed |
| CPU Cancel requested earlier in frame | Entire nonterminal frame and counters still run, then route 0 | Closed |
| CPU Cancel and result route become ready together | Direct result route 1/2 return wins before loop-test route 0 | Closed by result-control early return |
| Cancel occurs in warning sample 1–27 | Exit remains requested through all later warning samples and the outer suffix | Closed by this audit |
| Demo input occurs in an interstitial | Demo callback observes the last interstitial system sample, as source `input_sp` does | Closed by input latch overwrite |

## Phase-1 is a boundary tag, not a warning tag

V11 and T3CASE encode only two input phases. Phase 0 is the ordinary MAIN
input boundary. Phase 1 means an auxiliary input boundary and has two distinct
consumers:

1. `SUB_CA3C()` consumes exactly 27 phase-1 samples for a gauge/Boss warning;
2. `t3case_pause_playback()` consumes a variable number of phase-1 samples for
   the Replay Patch pause menu after a phase-0 sample carrying Cancel.

The neutral `replay::InputPhase::interstitial` name preserves the wire value
but not this semantic distinction. The headless `ReplayPauseCoordinator`
classifies phase 1 contextually after a phase-0 Cancel. `Game::step()` accepts
warning samples only while a warning is pending, while a recorded pause defers
the player/late half of the phase-0 frame until `finish_deferred_frame()`.
This keeps menu policy outside the gameplay core without conflating the two
phase-1 consumers.

### Complete T3CASE pause adapter

The Anniversary validation donor's `th03/main/t3case.cpp` contains the compact
Classic adapter used to play V11 input on the unmodified gameplay branch:

1. `t3case_frame_io()` consumes and commits the phase-0 gameplay sample.
2. If playback mode is active and that sample's `input_sp` has Cancel, it calls
   `t3case_pause_playback()` before the source `player_update()` calls.
3. The adapter saves only phase-0 P1/P2 gameplay input. It initializes menu
   selection 0: Resume, Restart, Save and Exit, Exit Without Saving.
4. `t3case_pause_wait_release()` tests all three current input words. While any
   of P1, P2, or system input is nonzero, it consumes another phase-1 sample.
   Therefore the first all-zero release sample is itself consumed by the
   release wait.
5. In menu wait, each iteration consumes one phase-1 sample. System Q exits;
   Cancel resumes; Up and Down wrap selection in 0..3 and then run a release
   wait; OK/Shot resumes only at selection 0 and exits at selections 1..3.
6. Resume performs a final release wait. Exit sets `byte_23B00` and does not
   wait for release. Reaching T3CASE input-end while inside the adapter also
   disables playback and requests loop exit.
7. On every completion path, the adapter restores the saved phase-0 P1/P2
   values and clears `input_sp`. Thus the later stock Cancel handler is
   suppressed, while an adapter exit remains sticky in `byte_23B00`.
8. The source then performs both `player_update()` calls and the complete late
   frame. Unless a result route preempts it, the loop test observes the exit
   only after counters, producing route 0.

The adapter intentionally omits menu graphics, sound pausing, font work, and
save behavior. Any nonzero selection accepted with OK/Shot has the same
gameplay-oracle result: exit. The menu state and input consumption are not
presentation-only because they decide whether MAIN exits and how many logical
samples the replay consumes.

### Canonical end-of-replay example

The current corpus endpoint demonstrates this path exactly:

| Samples | Input | Adapter consequence |
|---|---|---|
| 25128, phase 0 | Cancel | Enter pause adapter; save P1/P2 gameplay input |
| 25129–25132, phase 1 | Cancel held | Initial release wait continues |
| 25133–25192 | Idle | 25133 completes release; later idle samples remain in menu wait |
| 25193–25196 | Down held | Selection becomes 1; release wait consumes through the hold |
| 25197–25200 | Idle | 25197 completes release; remaining samples wait |
| 25201–25204 | Down held | Selection becomes 2; release wait consumes through the hold |
| 25205–25209 | Idle | 25205 completes release; remaining samples wait |
| 25210 | OK | Selection 2 exits immediately, without another release wait |
| following control | MAIN boundary | Restored gameplay P1/P2 ran once; cleared system input suppressed stock pause; sticky exit returned route 0 after the frame |

This is 82 phase-1 samples and is not a 27-sample warning wait.

### Implemented ownership and API boundary

Do not teach the gameplay core the Replay Patch's four-choice menu. The replay
coordinator should own pause selection/release parsing, while the gameplay core
owns the partially executed MAIN frame and final route precedence.

The implemented API is a staged frame boundary:

1. `Game::step(raw_phase0_input, InputContext::gameplay)` executes the source
   prefix, latches raw input, increments the logical sample cursor, and returns
   the canonical phase-0 input-boundary snapshot. Under
   `InputProtocol::recorded_replay`, Cancel returns a deferred
   `StepStatus::phase_transition` before player update.
2. The headless replay coordinator consumes the semantically typed pause
   sequence through `Game::step(..., InputContext::interstitial)`. These calls
   advance the logical sample cursor but no round, entity, RNG, collision,
   score, or warning state.
3. `Game::finish_deferred_frame(action)` restores the saved P1/P2 gameplay
   input, supplies zero system input after a recorded pause, runs both players
   and the late pipeline, and applies `DeferredFrameAction::exit_session` only
   at the loop-test position. A route-1/2 result return retains precedence.
4. A warning request returned while finishing the frame then accepts exactly
   27 warning-wait samples through the same interstitial context with the
   replay-pause deferral no longer active.

This split is useful beyond this one corpus endpoint: it represents the real
source boundary where T3CASE is already hooked and lets live, replay, and test
clients remain independent of gameplay implementation.

The staged API is required because a monolithic replay step would otherwise
have to carry three distinct facts: raw phase-0 system input for the returned
input-boundary hash, effective post-pause system input for the deferred frame,
and the sticky exit decision plus its auxiliary-sample count. Merely skipping
phase-1 records loses the global sample cursor and menu exit. Feeding them to
warning input is also incorrect because that path requires exactly 27 samples
and runs warning state afterward.

The source checkpoint hook deliberately emits periodic checkpoints only for
phase 0. Mid-pause state differs between the original Replay Patch and the
Classic adapter because their pause hooks surround `player_update()`
differently. Excluding phase-1 checkpoints is what makes their gameplay
boundaries comparable. The native coordinator must retain that rule.

For V13, use semantically distinct input kinds such as `gameplay`,
`warning_wait`, and `pause_menu` rather than carrying forward ambiguous phase
1. A V11/V12-to-V13 converter can parse pause sequences after a phase-0 Cancel;
warning samples remain driven by the core's explicit warning request. Raw
T3CASE/V11 parsing should continue to preserve the original phase bit until
that contextual classification pass.

## RNG closure for this slice

The shared late pipeline has no unrepresented stock RNG consumer after this
audit. Its source RNG sites are:

| Site | Stream | Calls | Native |
|---|---|---:|---|
| Initial A background `sub_D135()` | IRAND | 48 per callback | `background_update()` |
| Initial B background `sub_D340()` | IRAND | 40 per callback | `background_update()` |
| `SUB_CE5B()` callers | random ring | 1 per spawn | Retained at translated caller |
| randomized hit-circle creation reached by fireball hittest | random ring | 2 per randomized circle | Retained in collision helpers |
| character Bomb render callbacks | random ring | Character/frame dependent | `bombs_render_phase_update()`; see `PARITY_RNG.md` |
| character Boss/Charge/Extra render callbacks | varies by complete definition | Character dependent | See character and RNG dossiers |
| 1024-frame periodic `randring_fill()` | IRAND | 256 per refill | Native counter suffix; cursor preserved |

Warning, result, combo, score/extend, transition, Demo, shots, enemies,
fireball rendering itself, hitcircle drawing, player drawing, bullets, HUD,
and gauge overlays consume zero RNG. Fireball **hittest** can reach
randomized-hitcircle callers and is therefore not classified as zero-RNG.

## Presentation state deliberately outside the gameplay core

These omissions are safe only while deterministic checkpoints and clients do
not claim pixel-renderer state:

- warning palette ramp bytes and palette contents;
- background particle coordinates, velocities, sprite cels, and visual angle;
- the 12-record `cee0_rec_t` pool, apart from preserving creation RNG;
- shot sprite animation offsets;
- hitcircle ages/cels after preserving creation RNG;
- player movement/glow cels and page-back alternation;
- palette tone, page indices, clipping, text/HUD erasure, and pacing counters
  that have no gameplay reader.

A future faithful renderer should receive or own these fields behind the
presentation boundary. It must not move their source RNG consumption out of
the deterministic core.

## Remaining dependencies and gaps

There is no remaining shared late-pipeline or recorded-pause implementation
gap. The staged core boundary and the headless four-choice coordinator are
implemented, and the typed session adapter now covers MAIN completion,
route-2 persistence, Continue decisions, ordinary next stages, VS exit, and the
final-stage ending handoff.

Outstanding dependencies outside this shared slice:

1. Character/corpus work must close the remaining replay-oracle divergences;
   the character dossiers remain authoritative for every
   Bomb/Boss/Charge/Extra/Gauge callback and its render-phase collision/RNG
   behavior.
2. Live human pause/release/Q interaction needs an application input contract
   if a playable client is expected to reproduce it; do not reuse the replay
   coordinator's compact four-choice parser as the UI implementation.
3. `byte_23AFA` support is unnecessary for stock gameplay, but a future raw
   runtime-snapshot format must reject nonzero values or model its alternate-
   render branch explicitly.
4. Presentation state above is required for pixel parity, not for the current
   gameplay oracle.

## Contract coverage added

`tests/test_main.cpp:test_late_pipeline_control_order()` now proves:

- CPU-vs-CPU Cancel in the first warning-interstitial sample remains sticky
  across 26 released samples and produces route 0 only after the outer frame
  counter advances;
- selecting the opening transition callback with a nonzero carried phase does
  not reset the phase and advances it once later in that same outer frame.

`tests/test_main.cpp:test_recorded_pause_frame_boundary()` proves the deferred
phase-0 frame, auxiliary-sample accounting, Resume path, single-completion
guard, and delayed route-0 Exit path. `test_main_and_mainl_session_transitions()`
proves ordinary Story continuation, Continue accept/decline/no-credit paths,
the final ending handoff, VS persistence/exit, and route-0 abort semantics.

The latest pre-publication verification reported warning-clean GCC and MSVC
builds with both test presets passing. The earlier transient red-build notes
for the `GameOptions` constructor integration and `bullet_count` shadow warning
are superseded.
