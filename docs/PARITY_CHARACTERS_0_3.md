# Character parity audit: Reimu, Mima, Marisa, and Ellen

Static-audit status: **2026-08-08, corrective renderer pass complete against
the current dirty native tree**

This is a source-backed audit of character indices 0 through 3 against the
frozen Anniversary ReC98 source. The present revision concentrates on the
complete callback chains around Charge Shot, Extra Attack, Gauge Attack, Bomb,
and Boss rendering. In particular, it does not assume that a callback is safe
to omit merely because it does not draw from the shared random ring. Every
persistent write, random draw, collision/score/control effect, and value read by
a later gameplay callback was traced.

This document reports static evidence only. No emulator run is claimed here.
“Materialized” means that the headless core preserves the deterministic effect
or state transition. “Drawing-only” means that the omitted work changes only
VRAM, palettes, sound, sprite/GRCG/EGC configuration, or a scratch cursor that
every later user reinitializes before reading.

Source authority: the detached Anniversary reference pinned in `SOURCES.lock`.
Native authority: the current `th03-port` working tree.

## Result

All stock-used deterministic side effects found in the audited character
renderer chains are now materialized. The corrective pass found one persistent
state machine that the native core did not model: Mima's active Boss renderer
updates `byte_1F353` and `byte_1F354`. This is now represented by
`BossState::render_state` and `BossState::render_frame` in
`boss_render_phase_update()`.

No remaining RNG, collision, score, lifecycle, player-control, or later-gameplay
input discrepancy was found in the scoped render callbacks. This is not a
whole-game parity sign-off; global systems and characters 4 through 8 have
separate ledgers.

The older missing-system inventory in the first revision of this file is no
longer current. Mima and Marisa Charge/Extra/Boss implementations, Marisa's
Gauge implementation, and the Mima/Marisa/Ellen Hyper routes are present in the
current tree. Their mechanical parity remains covered by source review and the
other parity ledgers; this corrective pass specifically closes renderer-side
effects.

## Anniversary callback order

`th03/main/rndloop.cpp:78-163` establishes the relevant order:

1. For P1, then P2: Extra update, Boss update, Bomb update.
2. Shot update, then P1 and P2 Charge updates.
3. Formation, Bullet, Enemy, and Fireball updates.
4. For P1, then P2: Pellet Gauge callback, then Bullet Gauge callback.
5. Input and player updates.
6. For P1, then P2: Bomb render.
7. For P1, then P2: Boss render.
8. Shot and Enemy render, then `fireballs_hittest_and_render()`.
9. For P1, then P2: Extra render, then Charge render.

The native core preserves every gameplay-bearing portion of this sequence. Its
`gameplay_frame_suffix_after_interstitial()` calls
`bombs_render_phase_update()`, `boss_render_phase_update()`,
`fireballs_hittest()`, and `extra_attacks_render_phase_update()` in that order
(`src/core/frame.cpp:9255-9258`). Charge rendering is absent because all four
audited Charge renderers are drawing/scratch-only. A single native Boss-render
call is equivalent to the two source callback calls because every source Boss
renderer immediately returns unless `pid_current == gba_boss_launched_by`.

Callback installation for both players is explicit in
`th03/hitcb.cpp:199-369`; the native character dispatch uses
`(paletted_character - 1) / 2` at the corresponding phase boundaries.

## Charge Shot render callbacks

| Character | Anniversary callback | Persistent non-drawing effects | RNG / collision / control | Native disposition |
|---|---|---|---|---|
| Reimu | `chargeshot_render_reimu()`, `th03/main/player/ch_shot.cpp:733-788` | Advances the renderer scratch cursor `word_205CA`; changes sprite clip/size and mono-color state. It does not write any Charge node. | None. | Drawing and scratch cursor omitted. No deterministic materialization required. |
| Mima | `chargeshot_render_mima()`, `ch_shot.cpp:1255-1298` | Advances scratch cursor `word_20E22`; changes sprite clip/size. It only reads node activity/position and the loop index used for sprite selection. | None. | Drawing and scratch cursor omitted. No deterministic materialization required. |
| Marisa | `chargeshot_render_marisa()`, `ch_shot.cpp:176-254` | Assigns scratch pointer `word_1FE4E`; changes GRCG/EGC state. It only reads active/position arrays. | None. | Drawing and scratch pointer omitted. No deterministic materialization required. |
| Ellen | `chargeshot_render_ellen()`, `th03/main_08.cpp:338-377` | Advances scratch cursor `word_1F868`; changes sprite clip/size. It only reads node activity, phase, and position. | None. | Drawing and scratch cursor omitted. No deterministic materialization required. |

The scratch globals are not latent gameplay state. Each update, hit-test, and
render function that uses one assigns it to the appropriate array base before
dereferencing it. Therefore its post-render value cannot affect the following
frame.

## Extra Attack render callbacks

| Character | Anniversary callback | Persistent non-drawing effects | RNG / collision / control | Native disposition |
|---|---|---|---|---|
| Reimu | `exatt_render_reimu()`, `th03/main_06.cpp:442-468` | For each active one of the first 8 slots, increments record byte `+1` after drawing. This is the Extra Attack frame/age used by the next update. | No RNG or collision in the renderer, but the age write is later gameplay input. | Materialized by `extra_attacks_render_phase_update()` (`src/core/frame.cpp:5414-5432`), which increments `ExtraAttackState::frame` for every active regular Reimu slot. |
| Mima | `exatt_render_mima()`, `main_06.cpp:749-774`, via `mima_1A684()` at 509-574 | Scratch-record cursor and sprite globals only; attack records are read-only. | None. | Drawing/scratch-only; omitted. |
| Marisa | `exatt_render_marisa()`, `th03/main/player/exatt.cpp:1249-1273`, via `marisa_19B4F()` at 1119-1170 | Scratch-record cursor and sprite/GRCG state only; attack records are read-only. | None. | Drawing/scratch-only; omitted. |
| Ellen | `exatt_render_ellen()`, `exatt.cpp:787-813`, via `ellen_19510()` at 474-562 | Scratch-record/reference cursors and sprite state only. The stored trail/reference arrays are read, not mutated. | None. | Drawing/scratch-only; omitted. |

No Extra renderer calls `SUB_CDBD`, `SUB_CE0C`, `SUB_CE5B`, a hit-test, or a
random function. Collision-map insertion and Shot/Charge collision occur in the
update callbacks and are already before this phase in both implementations.

## Gauge callbacks and their visual helpers

Gauge callbacks are update callbacks rather than late render callbacks, but
they interleave bullet creation with visual-effect helper calls and therefore
must be audited as a whole. Their gameplay state, bullet allocation, RNG, and
termination are implemented in `gauge_pattern_reimu()`
(`src/core/frame.cpp:2890`), `gauge_pattern_marisa()` (2975),
`gauge_pattern_mima()` (3177), and `gauge_pattern_ellen()` (3244), dispatched by
`gauge_patterns_update()` (3536).

| Character | Anniversary callback | Non-drawing effects and random order | Status |
|---|---|---|---|
| Reimu | `gauge_pattern_reimu()`, `th03/main/player/ch_shot.cpp:790-866` | Initializes frame/count/cadence fields, advances the Gauge flag, emits the aimed spreads and paired rings, increments the frame byte, and terminates the Gauge. `SUB_CE0C()` writes a visual ring only; `sub_A3A8()` is palette cleanup. No RNG. | Materialized; visual ring/palette storage omitted. |
| Mima | `gauge_pattern_mima()`, `ch_shot.cpp:1300-1392` | Initialization consumes exactly one shared-ring draw to select signed angle delta `-7` or `+7`, then stores angle/frame/count/speed. Active frames emit the narrow spread and alternating ring, advance the byte angle/frame, and terminate. `SUB_CE0C()` and palette cleanup add no RNG. | Materialized, including the initialization draw and byte arithmetic. |
| Marisa | `gauge_pattern_marisa()`, `ch_shot.cpp:260-338` | Initialization consumes one shared-ring draw for the negative starting Y and stores Y increment/frame. Alternating edge groups create five bullets, advance Y, and eventually terminate. `SUB_CE0C()` and palette cleanup add no RNG. | Materialized. |
| Ellen | `gauge_pattern_ellen()`, `th03/main_08.cpp:379-483` | Initializes frame/X/Y/count/speed. The lower phase consumes one angle draw before the left ring and an explicit second draw after the right ring. The second result is not fired, but Anniversary stores it in the shared bullet-template angle after the final add. X/Y/frame/flag state advances and terminates normally. | Materialized, including both the second draw and its persistent template write. |

The whole-source shared-template audit also found that Ellen Boss helper
`th03/main_03s.cpp:ellen_11439()` increments its template angle after the last
of its eight ring adds. `boss_pattern_ellen_spreads()` now preserves that final
wrapped angle (`0x60` for the stock `0x20` start), allowing later same-prefix
callbacks to inherit the source value. Reimu and Mima were explicitly checked:
their stock Boss and Gauge bullet producers either assign angle before firing
or correctly leave the last fired angle, and their Extra callbacks add no
bullets. No further fix was required.

The helper definitions close the presentation-call chain:

- `SUB_CDBD()` (`th03/main/player/d3f9.cpp:239-256`) advances the 12-slot visual
  allocator and writes effect position/type/age. It consumes no RNG.
- `SUB_CE0C()` (`d3f9.cpp:258-275`) does the same for a different effect type
  and consumes no RNG.
- `SUB_CE5B()` (`d3f9.cpp:277-295`) additionally consumes one shared-ring draw
  for visual angle. None of the four audited Gauge callbacks calls it.
- `SUB_CEB2()` (`d3f9.cpp:297-319`) later updates those visual records without
  feeding collision, score, player control, or shared RNG.
- `sub_A3A8()` (`th03/main/hitc_pal.cpp:48-66`) only resets palette state.

Consequently the visual allocator records themselves need not be in
`DeterministicState`; the draws of RNG-bearing visual helpers must be, wherever
such helpers are called.

## Bomb update and render chains

Bomb callback mapping is in `th03/hitcb.cpp:168-175, 216-369`. The port splits
their gameplay-bearing behavior between `bomb_character_update()`
(`src/core/frame.cpp:2717`) and `bombs_render_phase_update()` (2774).

### Reimu

Sources: updater `reimu_1508C()` and renderer `reimu_bomb()` in
`th03/main/player/ch_shot.cpp:889-1049`.

- Preparing initializes 32 falling visual records; update moves/recycles them
  and advances/terminates the bomb. Those record coordinates never feed
  gameplay.
- Render frames 0-31 create one record per frame. Velocity selection consumes
  one shared-ring draw, then X selection consumes a second. The native core
  preserves exactly these two draws even though it omits the record.
- Render frames 64-111 write playfield foreground shift `+4/-4`; frame 112 and
  later reset it to 0. The native core materializes those writes.
- Remaining palette, EGC/GRCG, sound, image-copy, and sprite work is drawing or
  audio only.

Status: **complete for deterministic headless behavior**.

### Mima

Sources: updater `mima_17043()` in `th03/main/bullet/mbomb.cpp:21-107` and
renderer `mima_bomb()` in `th03/main/bullet/bullet.cpp:920-996`.

- During frames 8, 16, ..., 64, the updater consumes one shared-ring draw to
  choose each visual column. Column positions are not later gameplay inputs.
  The port preserves the draw cadence.
- Renderer frames 81-143 call nonrandom `SUB_CDBD()` every eighth frame and
  write foreground shift `+4/-4`; frame 144 and later reset the shift. These
  shift writes are materialized.
- Palette, column drawing, sound, and image-copy state is presentation-only.

Status: **complete for deterministic headless behavior**.

### Marisa

Sources: common updater `sub_1501E()` and renderer `marisa_bomb()` in
`th03/main/player/ch_shot.cpp:868-887, 345-436`.

- The updater advances and terminates the bomb with no RNG.
- Renderer frames 64-127 write foreground shift `+4/-4`. Every eighth frame in
  this interval consumes X first and Y second from the shared ring before
  calling nonrandom `SUB_CDBD()`. The native core preserves both draws in that
  order and resets the shift at frame 128.
- The retained point is only used to draw the flash until the next allocation;
  no collision or gameplay reads it.

Status: **complete for deterministic headless behavior**.

### Ellen

Sources: updater `ellen_185AB()` and renderer `ellen_bomb()` in
`th03/main/player/chybomb.cpp:143-214, 269-393`.

- Preparing writes all eight vector X values to `19999` (inactive). Update
  moves active vectors and respawns a vector whose X is `9999` (free) or which
  clips. Each respawn consumes one angle draw and writes the resulting Q12.4
  velocity.
- Every render before frame 64 resets the shared cursor `word_1FB3C` to 0.
  Every fourth frame from 64 through 127 marks the next vector X as `9999` and
  increments that cursor. The following frame's updater sees this sentinel and
  consumes the respawn draw. This is a direct renderer-to-gameplay/RNG dataflow,
  not presentation-only behavior.
- The cursor is global rather than per-player. Because source renders P1 before
  P2, simultaneous Ellen bombs have the same inter-player coupling in the
  native loop. `DeterministicState::ellen_bomb_vector_cursor` intentionally is
  shared, while the vector arrays are per-player.
- Frames 64-127 write foreground shift, but the renderer temporarily clears it
  only around vector drawing and restores it afterward. The headless core keeps
  the persistent post-render shift and does not materialize the temporary draw
  transform. Frame 128 and later reset the persistent shift.

The required vector/cursor state is present at
`src/core/game_internal.hpp:399-402`, and both release and respawn behavior are
materialized. Status: **complete**.

## Boss render callbacks

### Common defeat renderer

All four callbacks reach `sub_F58C()` when Boss state is `0xFF`.
`sub_F1FA()`/`sub_F58C()` in `th03/main_03.cpp:49-107, 240-252`:

- play sound 16 at defeat length 1;
- compute the drawing radius as 16-bit `length * 8`;
- once its signed value is at least 200, award 2560 points to the opposing
  player, set Boss state to 0, and clear `gba_boss_launched_by`.

Sound and explosion drawing are omitted. Score/lifecycle/control writes are
materialized by `boss_render_phase_update()` (`src/core/frame.cpp:8168`). This
phase occurs before fireball hit testing, matching Anniversary.

### Reimu

Source: `gba_boss_render_reimu()` and helpers in
`th03/main_03s.cpp:51-159`.

Launch and active paths only change sprite clipping, sprite configuration, and
VRAM. They read Boss position, hit, render state, frame, and orbit arrays but do
not write them and consume no RNG. Defeat uses the common helper above.

Status: **drawing-only except for the materialized common defeat path**.

### Mima

Source: `mima_10053()` and `gba_boss_render_mima()` in
`th03/main_03q.cpp:57-192`.

The launch path is drawing-only. The active path returns immediately after its
hit overlay when `byte_1F34E != 0`. Otherwise it has these exact persistent
writes:

- Render state 2 checks `byte_1F354 >= 0x20` before the increment; if true it
  writes render state 3, then increments the frame byte.
- Render state 5 writes render frame 0 and jumps into the state-2 body. It does
  **not** change state 5 to state 2. Therefore each later state-5 render resets
  the byte again and leaves it at 1 after incrementing.
- States 3 and 4 do not mutate persistent state.

There is no RNG, collision, score, or player-control action in the active
helper. The update functions assign these animation fields but do not read the
renderer-produced values for Mima's attack mechanics. Even so, they are genuine
persistent source state and are now materialized exactly in
`boss_render_phase_update()`. `BossState::render_state` and `render_frame` are
the required architecture fields (`src/core/game_internal.hpp:348, 361`).

Focused coverage in `tests/test_main.cpp:552` verifies the pre-increment
threshold, state-2 transition, state-5 reset-without-state-change, and hit-path
early return.

Status: **complete after the 2026-08-08 corrective patch**.

### Marisa

Source: `marisa_F9A6()`, `marisa_FA71()`, and
`gba_boss_render_marisa()` in `th03/main_03p.cpp:52-157`.

Launch and active paths only read Boss state/position/hit and
`round_or_result_frame` to choose sprites; no persistent game record, RNG,
collision, score, or control state is changed. Defeat uses the common helper.

Status: **drawing-only except for the materialized common defeat path**.

### Ellen

Source: `ellen_11814()`, `ellen_11885()`, `ellen_1190A()`, and
`gba_boss_render_ellen()` in `th03/main_03t.cpp:52-178`.

All launch/active work is drawing-only. In particular, the animation frame
`byte_1F354` is only read by `ellen_11885()`; its increment occurs earlier in
`gba_boss_update_ellen()` (`th03/main_03s.cpp:328-389`), where the native core
already advances it. The renderer consumes no RNG and changes no collision,
score, or control state. Defeat uses the common helper.

Status: **drawing-only except for the materialized common defeat path**.

## State and dataflow closure

The renderer audit requires these deterministic fields, all present:

- `ExtraAttackState::frame` for Reimu's render-owned age increment.
- Per-player `playfield_fg_shift_x` for Bomb render writes.
- Per-player Ellen bomb vectors plus the intentionally shared Ellen vector
  cursor, because render release controls next-update RNG.
- Boss `render_state` and `render_frame` for Mima's persistent renderer state.
- Boss state, pattern frame, launcher owner, and score for common defeat.
- The shared random-ring cursor for Reimu/Mima/Marisa Bomb visual draws and all
  RNG-bearing update effects.

Explicitly omitted visual records (Reimu falling Bomb sprites, Mima Bomb
columns, Marisa Bomb flash point, and `SUB_CDBD`/`SUB_CE0C` effect slots) have no
outgoing dataflow to collision, damage, score, player/CPU decisions, lifecycle,
or a later random-call condition. Their RNG consumption is materialized without
carrying their pixels-only payload.

## Verification

Focused native test added:

- `test_mima_boss_renderer_state()` in `tests/test_main.cpp:552`.

Verification on 2026-08-08:

```text
cmake --build --preset gcc
ctest --preset gcc
```

Result: GCC build succeeded; `th03-tests` passed (1/1). No emulator was run.
