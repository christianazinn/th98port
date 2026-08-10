# TH03 MAIN ordinary-entity gameplay observer schema

Status: source-grounded and implemented. All three phase views, the original
four ordinary-entity lifecycle families, and the hit-circle/common-effect
lifecycle facts are present. `th03_present` has source-ordered consumers for
all three views and for both effect pools.

## Scope and authority

This parcel closes the observation contract for the ordinary live entities in
`MAIN.EXE`: the two players, ordinary shot pairs, enemies and their explosions,
fireballs and their explosions, and bullets. Bosses, Extra Attacks, Charge
Attacks, HUD, results, backgrounds, warnings, and audio remain separate
parcels. Hit circles and common effects began outside this ordinary-entity
schema but are now implemented as a narrow lifecycle extension because their
exact source positions constrain the fireball and final-view draw order.

The behavioral authority is the clean Anniversary source tree at commit
`3e2381d4275be7287f5525de5c95389d492b0a3c`. All `th03/...`, `th01/...`, and
`libs/...` citations below refer to that commit. Native citations refer to the
`th03-port` tree at `e1d0805e71583d86dba216f54b3858c48aa4a68d`.

This schema refines the neutral synchronous seam proposed by
`docs/research/PRESENTATION_CONTRACT.md:134-218` and the entity row at
`docs/research/PRESENTATION_CONTRACT.md:881-915`. It preserves the governing
source schedule in `th03/main/rndloop.cpp:79-310` without giving `th03_core` a
framebuffer, page selector, asset ID, sprite cel, palette, clip rectangle, draw
operation, or presenter clock.

The governing rules are:

- Core remains the sole owner of gameplay mutation and shared RNG.
- Observations are immutable, borrowed, synchronous, and optional. The core
  queues nothing and stores no observer pointer in deterministic state.
- The presenter must finish the source-ordered work for a phase before its
  callback returns. It may copy facts into storage it owns, but it may not
  retain a borrowed view.
- A null observer, a discard observer, and a recording observer must produce
  identical gameplay state, RNG state, hashes, and `StepResult`s.
- Presentation owns every cel cursor and animation record that has no later
  gameplay or shared-RNG reader. `th03rv` uses the null path and links neither
  `th03_present` nor `th03_assets`.

## The minimum phase partition

Three synchronous phase views are sufficient. A fourth bullet-only core phase
would merely name a source draw slot: no gameplay mutation occurs between the
player/overlay/effect slots and `bullets_render()`. The presenter can preserve
that private order inside one final callback. Conversely, fewer than three
views lose a real before/after distinction around fireball hittest or the late
Extra/Charge mutations.

| Phase view | Exact source boundary | Facts available | Work the Authentic presenter completes before returning |
|---|---|---|---|
| `ShotsEnemiesPhaseView` | After both Bomb and Boss render-phase callbacks, before `shots_render()` and before `fireballs_hittest()` | Shot pairs and enemies | Finish preceding Bomb/Boss presentation; draw shot pairs in ascending slot order, then enemies in ascending slot order |
| `FireballsPhaseView` | Immediately after `fireballs_hittest()`, at the render half of `fireballs_hittest_and_render()` | Post-hittest fireballs | Draw fireballs/explosions in ascending slot order, then finish the out-of-scope hit-circle draw before returning |
| `PlayersBulletsPhaseView` | After P1 Extra/Charge and P2 Extra/Charge render-phase gameplay mutations, before player rendering and before result routing | Players and final late-frame bullets | Finish Extra/Charge presentation; draw P1/P2 player bases, P1/P2 overlays, the private common-effect pool, and then all bullet passes |

The source order proving those boundaries is
`th03/main/rndloop.cpp:139-185`. The current native late suffix has the same
gameplay mutation order—Bomb, Boss, fireball hittest, Extra, result control—at
`src/core/frame.cpp:380-395`; the phase calls belong between those existing
operations, not at the end of `Game::step()`.

Expanded with the out-of-scope slots left visible, one ordinary drawn frame is:

1. P1 Bomb, P2 Bomb; P1 Boss, P2 Boss.
2. `ShotsEnemiesPhaseView`: shot slots 0..31, then enemy slots 0..39.
3. Core performs the reverse-slot fireball collision pass.
4. `FireballsPhaseView`: fireball slots 0..23, then the presenter-owned hit
   circles before the callback returns.
5. Core/presenter jointly pass through P1 Extra, P1 Charge, P2 Extra, and P2
   Charge at their already-audited seams.
6. `PlayersBulletsPhaseView`: player base pass P1 then P2; disable EGC; overlay
   pass P1 then P2; common-effect draw; bullet16 pass, cloud pass, then pellet
   pass.
7. Core performs result control. A route-1/2 return omits later HUD/counter
   work, but it does not omit any of the three ordinary-entity phase views.

This is deliberately not a generic render event stream. Each callback denotes
a gameplay-coherent instant. The per-slot ordering and the multiple bullet and
player draw passes remain presenter algorithms.

### Deferred and early-return behavior

The native split frame must retain source-equivalent observation timing:

- The gameplay prefix can create bullets before input is latched
  (`src/core/frame.cpp:232-263`). Lifecycle facts from successful prefix
  allocations are therefore allowed before a recorded-pause deferral, but no
  ordinary-entity phase view occurs until `finish_deferred_frame()` completes
  the player and late halves (`src/core/game.cpp:315-353`). The same observer
  instance must consume both calls if it is maintaining Authentic state.
- A warning starts after both player updates. Shot-spawn and player-lifecycle
  facts from those updates have already occurred, but the three phase views are
  deferred. They occur exactly once after the 27th interstitial sample invokes
  the late suffix (`src/core/game.cpp:273-304`; `src/core/frame.cpp:341-358`).
- A result route that becomes terminal still reports all three phase views,
  because source result control follows `bullets_render()`
  (`th03/main/rndloop.cpp:174-185`).
- A call made after `round_complete` reports nothing. A
  `simulation_not_implemented` result reports no phase view; it may already
  have produced lifecycle facts for successful work before the native content
  gate failed, just as the current prefix may already have mutated state.
- The stock-unused `byte_23AFA` alternate draw cadence is not a reason to add a
  phase today. If it is ever modeled, its skipped draw frames must suppress all
  three phase views and must not advance presenter-owned shot animation
  (`th03/main/rndloop.cpp:129-139`).

## Coordinate contract

### Raw facts first

Every entity coordinate crossing the seam remains a signed 16-bit Q12.4 raw
value. The presenter must perform the source's signed arithmetic shifts; it
must not use host floating point or signed division, whose negative rounding
would differ. `SUBPIXEL_BITS` is 4 and `TO_PIXEL` is a right shift in
`th01/math/subpixel.hpp:12-32,99-113`.

For a raw playfield X coordinate `x`, raw Y coordinate `y`, and player ID `p`:

```text
pixel_q12_4(v) = arithmetic_shift_right(v, 4)
foreground_x(x, p) = pixel_q12_4(x) + 16 + (p * 320) + fg_shift_x[p]
foreground_y(y)    = pixel_q12_4(y) + 16
```

The constants are the 288-pixel playfield, 16-pixel border, and 320-pixel
bordered playfield in `th03/main/playfld.hpp:7-38`; the exact foreground-X
addition is in `th03/main/playfld.cpp:3-24`. `fg_shift_x[2]` is a gameplay fact,
not presenter animation: source also feeds it into cross-playfield coordinate
conversion, and native correctly retains it in
`src/core/game_internal.hpp:403-440`.

The observer never precomputes screen or page coordinates. In MAIN's zoomed
200-line mode, a `sprite16_put()` caller supplies a logical 400-line `top`; the
SPRITE16 wrapper arithmetic-shifts it right once before writing VRAM
(`th03/main/sprite16.cpp:32-52,73-93`). Sprite height is likewise stored as half
the logical height (`th03/main/sprite16.hpp:11-18`). Direct GRCG pellet writes,
superimpose player overlays, and other raw-page operations do not pass through
that conversion. This distinction belongs entirely in `th03_present`.

### Exact ordinary-entity placement

| Entity/draw | Source placement from observed raw facts |
|---|---|
| Shot pair | `left = foreground_x(topleft_x, p)`; logical `top = pixel_q12_4(topleft_y) + 16`; draw 16x16 at `left` and `left + 16` |
| Living enemy | `left = foreground_x(center_x, p) - size_pixels/2`; logical `top = foreground_y(center_y) - size_pixels/2` |
| Enemy explosion | Same center formula with logical size `size_pixels + 16` |
| Transferring fireball | Common center formula with logical size 16x16 |
| Falling fireball | Common center formula with logical size 32x32 |
| Fireball explosion | Common center formula with logical size 64x64 |
| Player base / defeat SPRITE16 path | Base `left = foreground_x(center_x, p) - 16`, logical `top = pixel_q12_4(center_y) - 16`; defeat has the separate captured anchor below |
| Player glow superimpose | `left` as player base; raw page row `arithmetic_shift_right(center_y, 5) - 8` |
| Charged-gauge superimpose | `left` as player base; raw page row `arithmetic_shift_right(center_y, 5) - 15` |
| Bullet16 | Common center formula with logical size 16x16, applied independently to each trail point and the main point |
| Delay cloud | Common center formula with logical size 32x32 |
| Pellet / transfer pellet | `left = foreground_x(center_x, p) - 4`; raw page row `arithmetic_shift_right(center_y, 5) + 6` |

The shot formula is direct from `th03/main/player/shot.cpp:347-369`; enemy and
explosion placement is in `th03/main/enemy/enemy.cpp:175-240`; fireball
placement is in `th03/main/enemy/fireball.cpp:200-287`; the deliberately
different player formulas are in `th03/main/player/bomb.cpp:487-560`; and the
three bullet passes and pellet raw-row expression are in
`th03/main/bullet/bullet.cpp:1588-1731`.

The presenter must also select the same source primitive/clip family:

- shots and fireballs use reset/full MAIN horizontal SPRITE16 clipping;
- enemies select the clip for their own bordered playfield;
- player base SPRITE16 drawing uses the full MAIN range, while player overlays
  use superimpose drawing;
- bullet16 main sprites and trails use `sprite16_put_noclip()`, clouds use the
  reset/full clip, and pellets use direct GRCG drawing.

Those are raster semantics, not additional core facts.

## Immutable fact inventory and presenter derivations

An indexed view exposes fixed capacities rather than allocating a live-entity
vector: 2 players, 32 shot pairs, 40 enemies, 24 fireballs, and 320 bullets.
Every accessor returns a checked `std::optional` containing a compact fact
value; an out-of-range typed slot returns `std::nullopt` rather than indexing a
pool. It must not expose a reference to `DeterministicState` or any internal
enum whose layout would make the public ABI depend on `game_internal.hpp`.

The shared phase context is only:

| Field | Width | Reason |
|---|---:|---|
| `round_id` | `uint8_t` | Scene/lifecycle identity and stale-observer guard |
| `round_or_result_frame` | `uint16_t` | Selects the charged-player overlay cel |
| `fg_shift_x[2]` | signed `int16_t[2]` | Exact gameplay-owned foreground transform at this phase |

Graphics page parity is intentionally absent. The presenter owns `page_back`
and `page_front` and uses its current `page_back` when reproducing the source's
page-dependent glow choice.

### Players

`PlayerFact`, returned for player IDs P1 and P2 in the final phase, contains:

| Field | Width | Draw use |
|---|---:|---|
| `player_id` | typed `uint8_t` domain 0..1 | Stable order and playfield transform |
| `center_x`, `center_y` | signed Q12.4 `int16_t` | Base, glow, charged overlay, and defeat origin |
| `paletted_character` | signed `int8_t` | Character and palette-variant selection; never compare it to an unnamed literal |
| `lose_anim_time` | `uint8_t` | 0 = ordinary sprite, 1..0x30 = defeat rings, 0xFF = defeated/no player draw |
| `invincibility_time` | `uint8_t` | Page-dependent invincibility glow gate |
| `hyper_active` | `uint8_t` | Page-dependent hyper glow gate |
| `gauge_charged` | Q12.4 `uint16_t` | Charged overlay visibility and one of four gauge ranges |

These are already gameplay state in `src/core/game_internal.hpp:112-157`.
Source player storage, including the omitted visual pattern bytes, is defined at
`th03/main/player/stuff.hpp:24-82`.

Presenter-private player state is:

- the three-way horizontal pose (neutral/left/right);
- `page_back != 0` glow selection: hyper glow wins over invincibility glow;
- movement-pose-to-character-cel mapping and all asset handles;
- charged-overlay range and cel. It is hidden below raw gauge `0x0100`, chooses
  four source ranges below 64.0, below 128.0, below maximum, and at maximum,
  and advances with `(round_or_result_frame >> 2) & 3`;
- the defeat anchor/radius and ring drawing state.

The ordinary player renderer reads no gameplay RNG. Base and overlay gates and
cels are exactly `th03/main/player/bomb.cpp:469-560`.

Defeat rendering needs one lifecycle fact on every defeat-update tick. The
source has one shared anchor/radius record, not one record per player
(`th03/main/hitbox_defeat_rikako_bss.asm:34-40`). At pre-update time 0x30 it
copies the then-current foreground-transformed X and raw-center-derived Y and
sets the radius to zero *before* replacing the playfield shift with the +/-6
shake. Every later tick first adds 9; a tick that decrements the timer to zero
then clears the shared record (`th03/main/player/defeat.cpp:28-67`). The event
therefore carries gameplay inputs, not a render-space anchor or radius:

```cpp
struct PlayerDefeatStep {
    PlayerId player;
    std::uint8_t lose_anim_time_before;
    RawQ12_4 center_x;
    RawQ12_4 center_y;
    std::int16_t fg_shift_x_before_defeat_shake;
};
```

The presenter applies those operations in P1/P2 event order to one shared
private record and draws three 16-sprite rings in the source angle order
(`th03/main/player/defeat.cpp:70-151`). On the fatal-hit frame, before this
first defeat step, source renders from the prior private values; the Authentic
presenter must preserve the Anniversary BSS/previous-completion initialization
rather than inventing an anchor from the new `lose_anim_time`. A lone defeated
player happens to imply radius `9 * (47 - L)` for post-update timer `L` in
1..47, but the presenter must apply events: simultaneous defeat updates share,
overwrite, and increment the same source record. Value 0xFF draws no rings.
The captured anchors are
`pixel_q12_4(center_x) + 16 + player*320 + pre_shake_shift - 24` and
`pixel_q12_4(center_y) - 8`. Each ring point then adds the *current* foreground
shift to the already-shifted captured X, exactly preserving source's second
shift application at draw time.

### Ordinary shot pairs

`ShotPairFact` at `ShotsEnemiesPhaseView` contains only:

| Field | Width | Draw use |
|---|---:|---|
| `slot` | typed 0..31 | Exact ascending source order and private animation identity |
| `alive` | `bool` | Draw gate |
| `topleft_x`, `topleft_y` | signed Q12.4 `int16_t` | Exact pair placement |
| `player_id` | typed 0..1 | Sheet/playfield selection |

Velocity is not read by `shots_render()` and stays out of the view. Native
already retains all listed gameplay fields in
`src/core/game_internal.hpp:221-227`.

Source `so_anim` is presenter-private. A successful allocation resets it to cel
0; every observed live draw advances it once through four cels, even if clipping
produced no pixels (`th03/main/player/shot.cpp:272-369`). Consecutive phase views
with the same live slot cannot identify a lifecycle: a shot can be drawn, be
destroyed by a later fireball/Extra collision, and have its slot reused before
the next view. Core must therefore emit one `ShotPairSpawned { slot,
player_id }` fact after every successful source-equivalent slot initialization.
Failed allocation emits nothing. No animation counter is added to core state.

### Enemies and enemy explosions

`EnemyFact` at `ShotsEnemiesPhaseView` contains:

| Field | Width | Draw use |
|---|---:|---|
| `slot` | typed 0..39 | Ascending draw order |
| `flag` | exact `EfeFlag` byte wrapper | Free/unspawned/living/explosion classification without pretending values above 10 cannot occur |
| `frame` | `uint8_t` | Explosion cel |
| `center_x`, `center_y` | signed Q12.4 `int16_t` | Placement |
| `player_id` | typed 0..1 | Playfield and clip |
| `size_pixels` | `uint8_t` | Living geometry and explosion size (`size + 16`) |
| `hp` | `uint8_t` | Living damage cel offset |
| `explosion_max_enemy_hits_half` | `uint8_t` | Source `size_words()` sprite/explosion family and HP stride |

The source structure and the non-enum explosion flag rule are in
`th03/main/enemy/enemy.cpp:40-100` and `th03/main/enemy/efe.hpp:16-53`;
native fields are in `src/core/game_internal.hpp:190-219`.

Free (0) and running-unspawned (3) slots do not draw. A running-spawned (1)
enemy chooses family `size_words - 1` and subtracts
`hp * explosion_max_enemy_hits_half * 2` source bytes from that family's base.
An explosion chooses cel 0 for frames below 10, alternates cels 1/2 using
`1 + ((frame / 4) & 1)` for frames 10..31, and chooses cel 3 for frames 32..35.
The update frees it at 36 before this phase view. These rules and the ascending
render pass are `th03/main/enemy/enemy.cpp:175-240,915-981`.

There is no presenter-owned per-enemy animation cursor and no enemy lifecycle
event is needed. Script pointers, velocities, angles, formation indices, chain
slots, and hit-test fields are gameplay-private but irrelevant to authentic
ordinary enemy drawing.

### Fireballs and fireball explosions

`FireballFact` at `FireballsPhaseView` contains:

| Field | Width | Draw use |
|---|---:|---|
| `slot` | typed 0..23 | Ascending draw order after the reverse hittest pass |
| `flag` | exact `EfeFlag` byte wrapper | Free/fall/transfer/explosion classification |
| `frame` | `uint8_t` | Living two-cel cycle or explosion cel |
| `center_x`, `center_y` | signed Q12.4 `int16_t` | Placement after hittest mutation |
| `player_id` | typed 0..1 | Current playfield and transform |
| `variant` | typed blue/red value backed by the source EHA value | Living sprite family |

The source record is `th03/main/enemy/fireball.cpp:17-82`; native retains these
facts in `src/core/game_internal.hpp:324-348`. HP, velocities, target positions,
chain slot, size, and generation remain gameplay-private because the renderer
does not read them.

For a living fireball, `frame % 8 <= 3` selects the second of two cels and the
other half-cycle selects the first. Fall uses 32x32 art; every other living
flag uses 16x16 transfer art. Explosion rendering is fixed at 64x64: cel 0 for
frames below 12, alternating cels 1/2 through frame 31, and cel 3 for frames
32..35. The update frees it at 36. These rules are
`th03/main/enemy/fireball.cpp:200-287,289-374`.

No lifecycle or private animation record is required. The post-hittest phase is
non-negotiable: the reverse pass can free a fireball, turn it into an explosion,
reset its frame, decrement HP, change playfield/variant consequences, and spawn
new fireballs that source draws at frame 0 in the same frame
(`th03/main/enemy/fireball.cpp:388-495`).

### Bullets

`BulletFact` at `PlayersBulletsPhaseView` contains:

| Field | Width | Draw use |
|---|---:|---|
| `slot` | typed 0..319 | Ascending order within each of three passes and private style identity |
| `flag` | exact bullet flag byte/wrapper | Bullet16, cloud, and odd-flag pellet pass gates |
| `age` | `uint8_t` | Bullet16 animation and cloud cel |
| `center_x`, `center_y` | signed Q12.4 `int16_t` | Main sprite placement |
| `player_id` | typed 0..1 | Current target playfield/transform |
| `has_trail` | `bool` | Enables source trail subpass |

For a live trail bullet, the typed view also supplies
`trail_point(slot, index)` for exactly six signed Q12.4 points. It returns a
point by value and does not expose `trail_index`, a pointer, or ring ownership.
This preserves the current coordinates even when two bullets share a source
trail slot. Native storage is `src/core/game_internal.hpp:292-322,506-510`;
source layout and the 48-slot aliasing behavior are
`th03/main/bullet/bullet.hpp:4-29` and
`th03/main/bullet/bullet.cpp:54-136`.

The presenter performs these source passes:

1. For each flag-2 bullet16 slot, draw trail points 5, 3, and 1 with trail cels
   3, 2, and 1, then the main cel. The main cel is `age % 4` for a cycling
   bullet and cel 0 for a static bullet. All four puts use the no-clip path.
2. If any flag-4 cloud exists, draw clouds in ascending order as 32x32 white
   mono sprites. While the flag remains cloud, its cel is `age / 4` (0..2);
   the update converts it to a pellet instead of producing cel 3.
3. In ascending order, draw every odd-valued flag as an 8x8 white GRCG pellet;
   exact flag 1 uses the ordinary shape/color token and other odd flags use the
   transfer token. This retains source behavior if a non-stock invalid flag is
   ever modeled rather than prematurely closing the flag domain.

The pass implementation is `th03/main/bullet/bullet.cpp:1588-1731`; cloud
advancement is `th03/main/bullet/bullet.cpp:1289-1336`.

Bullet16 style cannot be recovered from current late state. Source copies a
template `sprite_offset` and `is_animated` into every allocated bullet
(`th03/main/bullet/bullet.cpp:991-1152`). Default bullet16 art is relative to
the currently selected attack sheet, not the target playfield; multiple Boss,
Gauge, and Charge call sites temporarily select static cels. Kana Extra alone
uses the otherwise-unused custom-with-acceleration path and an attack-sheet cel
at +72 rows (`th03/main/player/exatt.cpp:982-995`). Current native
`BulletState` omits both fields, the generic add helper has no style/animation
parameter, and the Kana helper retains only gameplay mechanics
(`src/core/game_internal.hpp:292-317`;
`src/core/frame/bullets_core.inl:178-496`;
`src/core/frame/characters/kana_extra_bullet.inl:3-37`).

The smallest closure is one successful-allocation fact for bullet16 only:

```cpp
enum class Bullet16ConstructionKind : std::uint8_t {
    default_attack,
    kana_extra_attack,
};

struct Bullet16Spawned {
    BulletSlot slot;
    PlayerId source_player; // Selects the source attack/character sheet.
    Bullet16ConstructionKind construction;
    bool source_template_animation_enabled;
};
```

The presenter maps this semantic construction fact to an asset and stores the
style per slot. The event occurs only after a bullet instance is successfully
initialized; failed/clipped/full-arena attempts emit nothing. Pellets and
clouds need no style event because their flag and age completely determine
their drawing. Raw sprite offsets and asset handles must not cross the core
boundary.

## Late gameplay mutations by entity class

The phase placement follows dataflow, not visual convenience:

| Entity | Earlier work | Last source mutation relevant to this frame's draw | Later mutation that must not retroactively alter the draw |
|---|---|---|---|
| Shot pairs | Prefix update; player updates can allocate new pairs | Both Boss render-phase callbacks can collide with/free shots before `shots_render()` | Fireball hittest and Extra/Charge render callbacks can free shots after source already drew them |
| Enemies/explosions | Formation/enemy update and reverse enemy hittest in the prefix | Prefix enemy hittest can reset frame 0 and enter explosion state | Later Extra/Charge collision can alter enemies only after `enemies_render()` |
| Fireballs/explosions | Prefix fireball update | Reverse-slot `fireballs_hittest()` immediately before drawing | Extra/Charge callbacks occur after the fireball draw |
| Players | P1 then P2 player update | Player update commits pose/defeat/gauge facts; final view is delayed until after intervening late callbacks to preserve source draw position | Result routing follows the player and bullet draw slots |
| Bullets | Prefix update; Gauge, player, Boss, and Extra callbacks can allocate later | P2 Extra/Charge is the last bullet-producing callback before `bullets_render()` | Result control occurs only after bullets, so age-0 late spawns are visible even on a terminal display frame |

The shared audit identifying the source late mutations is
`docs/PARITY_RENDER_PHASE.md:43-74,151-168`. In particular,
`fireballs_hittest_and_render()` is not presentation-only, whereas
`shots_render()`, `enemies_render()`, and `bullets_render()` have no gameplay or
RNG mutation.

## Facts currently discarded by core

Four omissions originally blocked a completely authentic ordinary-entity
presenter. Native closes them with transient typed facts rather than moving
renderer state into `DeterministicState`.

| Blocker | Why current gameplay state is insufficient | Minimal neutral closure |
|---|---|---|
| Player horizontal pose | Source commits `patnum_movement` from one shared signed Q4.4 `player_velocity.x`. The global can be stale on banner, defeat, or move-lock paths; clamping can make a nonzero velocity produce zero displacement; CPU route probes can restore position after several velocity writes. Center deltas are therefore not equivalent. Native uses local velocities and stores neither the shared value nor the pose. | Emit one `PlayerPoseCommitted` fact per source `update_patnums`: player ID plus an optional final source-equivalent velocity-X write from that player update. The presenter owns the last shared X (Anniversary BSS initial value) and applies zero/negative/positive to neutral/left/right. No optional value means source reused the prior shared X. |
| Shared defeat record | Source samples foreground X before overwriting the shift with defeat shake, then advances one shared radius on every losing player's update. A post-update phase view contains only the new +/-6 shift and cannot reproduce simultaneous-player overwrite/increment order. Native intentionally omits the presentation globals. | Emit `PlayerDefeatStep` on every source defeat-update tick with player, pre-update timer, raw center, and pre-shake shift; presenter applies capture/increment/clear to one shared private record. |
| Shot cel reset across slot reuse | Native correctly omits source `so_anim`, but two consecutive live views do not prove the slot was not freed and reused between them. | Emit `ShotPairSpawned`; presenter resets its four-cel cursor and advances it once per live `ShotsEnemiesPhaseView`. |
| Persistent bullet16 construction | Target player, flag, age, and trajectory do not identify the source attack sheet, Kana custom construction, or a call site's temporary `is_animated=false`. Native intentionally discarded both source fields. | Emit `Bullet16Spawned` with slot, source player, named construction kind, and the source template animation bit; presenter maps that fact to a private per-slot visual key. |

The player source proof is `th03/main/player/move.cpp:20-76`,
`th03/main/player/bomb.cpp:177-237,289-300,418-484,562-597`, and the CPU retry
loop at `th03/main/player/c568.cpp:34-177`. The native contrast is
`src/core/frame/player_movement.inl:3-101`,
`src/core/frame/player_cpu.inl:105-200`, and
`src/core/game_internal.hpp:112-157`.

`PlayerPoseCommitted` deliberately collapses multiple writes within one player
update to the final write because no presentation reader exists between them.
It must still distinguish “wrote zero” from “did not write”: source
`player_move()` clears the shared velocity even for no input or invalid input,
while several early paths perform no write at all. Knockback's later vector
write wins. The field is signed `int8_t` Q4.4, matching source `SPPoint8`, not a
host `int16_t` velocity.

## Small typed API

The following is design-level spelling. It intentionally uses named callbacks
and indexed views rather than a generic ECS query, `std::variant` event bus, or
per-frame copied arrays.

```cpp
class ShotsEnemiesView {
public:
    static constexpr std::size_t shot_count = 32;
    static constexpr std::size_t enemy_count = 40;
    MainPhaseContext context() const noexcept;
    std::optional<ShotPairFact> shot(ShotPairSlot slot) const noexcept;
    std::optional<EnemyFact> enemy(EnemySlot slot) const noexcept;
};

class FireballsView {
public:
    static constexpr std::size_t fireball_count = 24;
    MainPhaseContext context() const noexcept;
    std::optional<FireballFact> fireball(FireballSlot slot) const noexcept;
};

class PlayersBulletsView {
public:
    static constexpr std::size_t player_count = 2;
    static constexpr std::size_t bullet_count = 320;
    MainPhaseContext context() const noexcept;
    std::optional<PlayerFact> player(PlayerId player) const noexcept;
    std::optional<BulletFact> bullet(BulletSlot slot) const noexcept;
    std::optional<RawPoint> trail_point(
        BulletSlot slot, TrailPoint index
    ) const noexcept;
};

struct PlayerPoseCommitted {
    PlayerId player;
    std::optional<std::int8_t> final_velocity_x_write_q4_4;
};

class GameplayObserver {
public:
    virtual ~GameplayObserver() = default;

    virtual void shot_pair_spawned(const ShotPairSpawned&) noexcept {}
    virtual void bullet16_spawned(const Bullet16Spawned&) noexcept {}
    virtual void player_pose_committed(const PlayerPoseCommitted&) noexcept {}
    virtual void player_defeat_step(const PlayerDefeatStep&) noexcept {}

    virtual void observe(const ShotsEnemiesView&) noexcept {}
    virtual void observe(const FireballsView&) noexcept {}
    virtual void observe(const PlayersBulletsView&) noexcept {}
};

// Design-level extension of the existing entry points. The pointer is borrowed
// only for this call and defaults to the current headless behavior.
StepResult Game::step(
    CoreInput, InputContext, GameplayObserver* observer = nullptr
);
std::expected<StepResult, Error> Game::finish_deferred_frame(
    DeferredFrameAction, GameplayObserver* observer = nullptr
);
```

The view objects contain only an opaque `const` implementation pointer and are
valid only for the callback. Accessors construct public POD facts by value.
Core does not allocate, sort, build an entity list, or copy 320 bullets merely
because an observer exists. The null path is one predictable branch at each
fact/phase site; the observer itself is non-owning and never enters canonical
state, reset snapshots, or state hashes.

The observer is passed per `Game::step()` or `finish_deferred_frame()` call. Its
lifetime need cover only that call: neither `Game` nor `Game::Impl` stores the
raw pointer, reference, or callback target afterward. The caller passes the
same presenter observer again when completing a deferred frame. Callbacks are
`noexcept`, cannot reenter or mutate the `Game`, and cannot control whether core
performs an operation. An asynchronous consumer must copy facts in its callback
into its own queue. Authentic presentation must attach from the scene's
source-equivalent initialization (or restore a presenter snapshot): mid-round
attachment cannot reconstruct a persistent shot cursor, bullet construction,
shared pose velocity, or defeat private state from gameplay state alone.

## RNG firewall

No new RNG observation is required for these ordinary draw paths:

- `shots_render()`, `enemy_put()`/`enemy_explosion_put()`, the fireball and
  fireball-explosion puts, player base/overlay/defeat drawing, and
  `bullets_render()` contain no shared-RNG read in the cited functions.
- Formation, enemy, bullet, fireball, player knockback, and collision code can
  consume RNG while producing the observed gameplay state. Core continues to
  consume it exactly once; the phase view exposes outcomes, never a seed or
  cursor for the presenter to rerun.
- `fireballs_hittest()` can reach fireball/Extra creation and the separately
  specified hit-circle compatibility observations. Those reads occur before
  `FireballsPhaseView`; their already-consumed raw values, if required for hit
  circles, use the existing compatibility seam and are not duplicated here.
- Lifecycle facts do not add a random read. A spawn fact is conditional only on
  the allocation that source already completed and is emitted after that
  instance's source-equivalent initialization.

An observer's presence must not change even the count or ordinal of existing
compatibility-read observations.

## Invariance and semantic test plan

No emulator run is needed to implement the neutral seam. Before any presenter
uses it, the following tests must pass.

### Core neutrality

1. Run every existing unit/oracle case and all 19 locked V13 corpus replays
   with no observer, a discard observer, and a recording observer. At every
   input boundary compare `StepStatus`, canonical state, state hash, checkpoint
   result, round summary, final route, RNG seed/ring/cursor state, and the
   existing compatibility-read sequence. Repeat on warning-clean GCC and MSVC.
2. Assert no `th03/present`, asset, raster, filesystem, locale, or audio header
   is reachable from the core observer header. Confirm `th03rv` still links only
   replay/format/core targets.
3. Instrument allocations in a focused test and prove that phase observation
   allocates no core-owned memory. A recording test may allocate its own copy.
4. Serialize recorded fact fields explicitly and compare the observation hash
   across GCC/MSVC. Never hash padding, an internal pointer, or an internal
   enum object representation.

### Schedule and control flow

1. Ordinary frame: exactly `ShotsEnemies`, `Fireballs`, `PlayersBullets`, once
   each and in that order.
2. Warning-start gameplay call: player/shot lifecycle facts may precede the
   phase transition, but no phase view occurs. Interstitial samples 1..26 emit
   no phase view; sample 27 emits all three once in order.
3. Recorded pause: the initial call may emit prefix allocation facts but no
   phase view. `finish_deferred_frame()` emits the delayed player facts and all
   three phase views once. Exiting the session does not duplicate them.
4. Route-1/2 terminal frame: all three phase views precede the terminal result;
   no later HUD/counter observation is fabricated.
5. Already-complete and invalid-context calls emit nothing. A focused native
   content-gate case records any already-completed lifecycle facts but no phase
   view.

### First-divergence entity cases

1. Spawn a shot in player update and prove it appears in the same frame's first
   view. Kill a shot in Boss late work and prove it is absent; kill one in
   fireball or Extra late work and prove the already-observed shot remains part
   of that frame's draw. Reuse its slot and prove `ShotPairSpawned` resets cel 0.
2. Cover a living enemy, a damaged enemy, frame-0 enemy explosion, keyframes
   10/31/32/35, and the free-at-36 transition for all three stock size families.
3. Cover fall/transfer fireball variants and collision outcomes that free,
   damage/reset, explode, transfer, and create an age-0 fireball. Verify reverse
   hittest order but ascending draw order.
4. Cover player edge clamping with nonzero intended velocity, invalid/no-input
   zero writes, a move-lock/defeat/banner stale write, knockback overwrite, and
   a CPU route probe that restores center after multiple attempts. Position
   delta must fail at least one fixture while `PlayerPoseCommitted` matches
   source pose.
5. Cover the fatal-hit frame's prior defeat-private values, the following
   anchor-sampling tick with pre-shake foreground shift, radius progression,
   simultaneous P1/P2 overwrite/increment order, and the 0xFF no-draw state.
6. Cover an animated default bullet16, every stock static-default producer,
   Kana custom acceleration, a trail bullet with points 5/3/1, all three cloud
   cels, both pellet tokens, and an age-0 bullet spawned by a callback after the
   prefix bullet update.
7. Feed the recorded facts into a trace-only presenter and compare exact draw
   family, cel, coordinate, clip/primitive, pass, and slot order against small
   hand-transcribed source fixtures before attempting captured pixel parity.

Later emulator visual capture should follow the manifest and raw-page procedure
already frozen in `docs/research/PRESENTATION_CONTRACT.md:964-1013`; it validates
the presenter, not gameplay determinism.

## Implementation status

The first coding parcel stopped after the first two phase boundaries:

1. Added the optional, non-owning observer plumbing; `MainPhaseContext`;
   `ShotPairFact`, `EnemyFact`, and `FireballFact`; the indexed
   `ShotsEnemiesView` and `FireballsView`; and `ShotPairSpawned` at successful
   `shots_add()` allocations.
2. Invoked `ShotsEnemiesView` immediately after `boss_render_phase_update()`
   and `FireballsView` immediately after `fireballs_hittest()`. Added
   null/discard/recording invariance, phase-order, slot-order, shot-reuse,
   enemy-keyframe, and post-hittest fireball tests.
3. The second parcel added checked player/bullet/trail accessors, pose commit,
   defeat steps, and Bullet16 construction style, followed by the single final
   `PlayersBulletsView` before `result_control_update()`. Callback-boundary
   Bullet16 detection is source-safe because the current Extra/Boss/Gauge
   producer callbacks allocate first-free slots and neither free nor retag a
   new bullet before returning.

4. The effect-lifecycle parcel added exact hit-circle and common-effect spawn
   facts plus one advance boundary for each source pool. Shared RNG stays in
   core and is consumed before observation. Kana/Kotohime Bomb callbacks use a
   closed semantic step so their shared presentation-only variables remain in
   `th03_present`; all other calls report resolved centers and consumed square
   angles. The presenter owns the 13-slot and 12-slot pools across repeat-round
   boundaries and exposes the exact post-fireball and pre-bullet draw passes.

Together these parcels yield the complete ordinary-entity fact stream and the
two adjacent effect lifecycles while keeping the risk and review surface small.
They add no generic entity abstraction, queue, renderer dependency,
localization policy, or state/hash field solely for presentation. The
no-observer path remains the verifier path.
