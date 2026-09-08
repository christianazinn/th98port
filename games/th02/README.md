# TH02 native foundations

This is an asset-free, partial deterministic core, not a playable game or a
replay verifier. It has source-bounded random, player, shot, score, and spark
state surfaces, including one checked player-frame composition.

Source donor: ReC98 c6c2ee5390012b6a334163716a1703bf2edd0229:
th02/main/randfill.cpp, th02/math/randring.cpp, th02/math/randring.inc,
th02/math/randring.hpp, and th02/math/randring[bss].asm. These definitions
have no difference from frozen master f7d5a2546dff0d734ba1a04acec419cfc4d2f6c7.

The master.lib IRAND primitive is shared with TH01 and TH03. Ring fill is
game-specific: TH02 fills forward, unlike TH03's reverse fill. Fill consumes
256 RNG calls without resetting the cursor. Byte and word reads each advance
one position and do not change the seed. The word read at position 255 reads
the adjacent, already-wrapped cursor as its high byte. It does not wrap that
byte to ring[0]. Both source code-segment copies share the same state.

The native function structure follows the existing TH03 random implementation;
only fill order differs. Explicit byte assembly replaces the source's
unaligned, out-of-array reference. CMake copies the existing games' target and
warnings contract without advertising absent app/SDL/presentation targets.

Tests compare all fill cursors across six seeds and 262,144 mixed reads against
an explicit adjacent-memory model. This is source-derived numeric evidence,
not PC-98 replay parity. Startup seed/cursor initialization, masked/ranged
wrappers, input, entities, stages and the full schedule remain open.

`core/player.hpp` ports the complete `player_move` helper from
th02/main/player/player.cpp: it reads the bound back-page coordinates, adds
signed 16-bit deltas with wrapping, clamps to x=28..390/y=16..344, and updates
both the page and cached current coordinates. Its source body is unchanged
between the two donor revisions above. Native page indices are validated
before any mutation; both legal pages and all 65,536 coordinate bit patterns
are tested against eleven deltas. As an individual helper, it does not replace
the caller's page invalidation/copy, half-pixel speed adjustment, option lag,
shots, bombs, or hit/invincibility handling; the later frame composition binds
the source-owned subset of those boundaries without implying a stage schedule.

An additional numeric fixture matches the first RNG payload captured from the
historical PC-98 oracle d5ddbfdf9dc413c3a46f6964b0be1b554f086aea: seed 18,
128 IRAND calls for spark initialization, ring fill, then one item-init ring
read produce seed 0x1EF30E92, cursor 1 and canonical group hash
0xE7775247A8E61EEE. Both compiler builds reproduce the entire serialized ring
payload. This is evidence for these RNG helpers, not a native implementation
of the complete initialization sequence or a replay verification result.

Build from the repository root or directly with
`cmake -S games/th02 -B out/th02`, then build and run CTest in that directory.

`core/score.hpp` adds the complete pending-score update and immediate-bonus
operations. They preserve transfer smoothing, signed dword wrapping, one
extend per call, capped-life/bomb differences, and the bonus path's uncleared
delta. HUD/SFX requests return in source order with their values; no rendering
or playback occurs in the core. Score reset, initial extend scanning, and delta
commit are also implemented with their distinct state-write sets; the caller
still owns their schedule. Unsupported extend-table indices
are refused, not read from unrepresented DOS memory. Tests cover all previous
transfer words, threshold boundaries, negative/wrapped values, effect-setting
changes, ordered requests, and repeated updates/bonuses. These are source-based
checks, not a verified full scoring pipeline.

`movement_intent()` extracts the complete movement/pose selection block from
player_move_and_shoot at the same donor. Dedicated diagonal bits take priority;
invalid combinations fall back to aligned bits. Signed byte speeds promote
before negation, and aligned X speed 5 enables the back-page adjustment on both
axes. Its immediate caller must first apply the original hit guard; the helper
itself does not implement that guard, options, shooting, bombs or page
alternation. Global speeds/input/page become arguments and selected deltas/pose become a result;
the nested switch and page-correction behavior remain unchanged. Invalid host
pages are refused. Tests exhaust input words and each speed byte independently.

`PlayerHitState` carries the raw one-byte hit value, including the game-over
value `0xFF`, together with the two independent invincibility conditions.
`player_move_and_shoot_allowed()` ports only that function's immunity guard and
early-return decision. `prepare_player_callbacks()` ports the later update
function's same guard followed by its nonzero timer decrement. Neither helper
calls shots, miss handling, quit, rendering, bombs, or a frame driver. The
dedicated test exhausts every raw hit and timer byte across both boolean guards;
it is source-derived state coverage, not a gameplay or replay result.

`player_update_state_step()` composes that existing update-prefix helper with
the typed player-shot lifetime boundary and a synchronous miss callback. Its
return path preserves the source's subtlety that a hit observed after shot
update returns even if the callback clears it, while a callback that leaves
`0xFF` sets quit and then clears the raw byte. Refused shot lifetime work
returns after the hit/timer prefix and before miss or rendering. The VRAM-only
render tail, page driver, frame schedule, replay, and playable-session
behavior remain unimplemented.

PlayerMissUpdateCallbacks binds the complete state body of the source
player_miss_update_and_render() to that existing callback seam. It preserves
the source's 0/24/43/68-frame transitions: initial sound, score commit and
sparks; survivor power/shot-level reset or game-over item mode; lives/bombs,
page-position reset, and respawn movement. Item and spark owners remain
borrowed, and presentation is a synchronous source-pixel/pattern callback.
Invalid host page or spark state, and an unrepresented source table index,
return their typed failure after the already-written source prefix; no rollback
or stage/page scheduler is invented. Focused tests cover normal and terminal
miss paths, raw byte/table boundaries, and update-caller quit conversion. This
is source-derived state coverage, not gameplay, replay, or session parity.

`player_reset()` now supplies the donor's per-attempt partial write set over
the existing hit/miss/pool/lifetime/stream/volley/movement owners. It clears
only shot flags, not payload; resets the selection-independent values; and
then applies the 0/1/2 shottype switch's deliberately uneven dispatcher,
speed, powered-pattern, option-visual, and C-decay writes. In particular, A
retains a previously set powered-option pattern and A/B retain a prior option
decay interval, while any other source shottype executes the common prefix
without choosing a default. Page position, score/lives/bombs, stream cadence,
C's cycle, stage/continue scheduling, rendering, replay, and playable-session
behavior remain outside this function. Focused warm-state controls assert the
full partial-write boundary for A/B/C and invalid selector values.

`player_move_and_shoot_shot_stream_step()` now uses one checked synchronous
callback path: `PlayerShotVolleyDispatcher` selects the existing A/B/C volley
body over the shared pool, lifetime latch, RNG, and stream patnums, while an
effect seam receives only the following sound/bomb calls. Invalid native
selection/page and B's modeled source `iatan2()` fault return at the volley
call position, before sound/cooldown/phase/bomb work. The small context retains
only source globals with no previous owner (selection, positions/page, B target
and unused word, C cycle). A separate player-frame composition now supplies its
positions/page at the selected-volley call boundary; neither surface is reset
setup, a page driver, or a frame schedule. Tests cover normal A/B/C dispatch
and each stop boundary, but this remains source-grounded state coverage rather
than gameplay, replay, or session parity.

`player_frame_invalidate()` and `player_frame_move_and_shoot()` compose the
state-only frame boundary before that dispatcher. Invalidation first forwards
the existing shot invalidation, then emits player and two option rectangles
from the old explicit back page, then copies only the player front coordinate
to back. Movement applies the original hit guard without decrementing
invincibility, derives movement/pose, performs the checked back-page move, and
writes the left option from the moved-and-clamped player coordinate minus the
requested delta. It updates the dispatcher's existing writer-position context
at the selected-volley call and preserves its success/refusal prefixes. No
implicit page flip, reset, miss/respawn, renderer, or stage schedule is added.
Focused controls cover event order, same/invalid page behavior, the hit guard,
all aligned and diagonal input encodings, field-edge option placement, and
actual A-volley Q12.4 spawn positions.

`core/shot.hpp` now supplies TH02's fixed 38-slot, 16-byte player-shot record
and the three pool writer helpers. `shot_add()` and `shot_option_add()` update
only the caller-selected current back-page slot in source field order; regular
shots derive their Q12.4 velocity through the shared exact master.lib vector
at length 192, while option shots retain their explicit velocity and initial
decay value of one. `shots_free_all()` clears flags only, retaining all stale
payload. Native-invalid page/slot input is refused before mutation. Free-slot
scans, selected shottype callbacks, RNG, update/render, hit tests, and frame
scheduling are still separate; these tests are source-derived writer coverage,
not gameplay, replay, or session parity.

`shot_a_volley()` is the source-complete shottype-A regular-volley callback on
top of those writers. It starts at pool slot zero, scans only free entries,
preserves every level-0-to-9 pattern and its ring draws, exits at the source
pattern boundary or scan limit, and clears the option-shot latch even with a
full pool. At level 9 it carries its per-shot patnum changes back to the
caller-tail seam before the later sound callback. This A callback itself does
not fire options or update/render/collide/schedule shots; its B/C siblings and
the selected-dispatch binding are separate completed surfaces. The tests are
source-derived callback coverage, not a full shot or gameplay implementation.

`shot_b_volley()` adds shottype B's complete selected spawn body on the same
pool: one source-ordered free scan writes the regular volley and then its
option volley. It preserves all level shapes, the level-8 option fallthrough
that overwrites each first pattern with level 9's final pattern, full-power
patnum changes, target marker/latch gates, and target/fallback homing angles.
Homing consumes the shared literal master.lib `iatan2_source()` model; a
modeled source DIV fault reports the prefix already written rather than using
TH03's deliberately normalized edge policy. This B callback remains a spawn
body only: option-shot movement, hit testing, invalidation/update/render,
sound, frame scheduling, replay, and playable-session claims are absent.

`shot_c_volley()` adds shottype C's selected spawn callback. It increments the
byte cycle before every source branch, then shares one free-slot scan between
regular and option writes. Tests exhaust all 256 initial cycle bytes at each
valid level to retain the source's distinct modulo/mask cadence and the
level-2-only latch gate. Fully powered C uses the executable `volley_i >= 3`
condition, which lifts the final two writes of its five-shot regular volley;
this deliberately follows the condition instead of the adjacent contradictory
"last three" source comment. Option movement, update/render, collision,
sound, frame scheduling, replay, and playable-session behavior remain outside.

`shots_hittest()` now supplies the source's simulation-only player-shot
collision scan. It preserves the 35-of-38 slot bound, strict and intentionally
asymmetric regular/option hit windows, signed Q12.4 conversion, regular decay
state/velocity changes, option repeated damage, the low-byte bomb gate, and
the unusual 16-bit-only `score_delta` addition. A typed synchronous spark seam
records the source effect call at its exact point; it owns no spark pool and
must not mutate the borrowed shot/score state. Tile invalidation, position
copying, shot update/rendering, enemy HP, stage scheduling, replay, and a
playable session remain separate. Focused tests cover every damage-table level,
strict and negative-coordinate boundaries, slots 34 versus 35–37, low-word
carry loss, and callback-visible mutation order; they are source-derived unit
coverage, not gameplay or replay verification.

`shots_update_lifetime()` and `shots_invalidate_lifetime()` provide the
player-shot state half of the corresponding source bodies. Update moves all
38 source-alive selected-page slots with Q12.4 word arithmetic, source bounds,
regular and option decay, and persistent regular slot-animation bytes.
Invalidation emits a typed tile request before each raw-nonfree slot is freed
or copied from the explicit front page to the explicit back page, and clears
the option latch only when no surviving option shot remains. Rendering, scroll
conversion, page flips, and stage scheduling remain unimplemented; the later
player-frame composition is a caller, not a render queue or page driver.

`PlayerShotSparkAllocatorCallbacks` is the explicit synchronous bridge from
the existing player-shot collision effect seam into that allocator. For valid
source state, regular, large, and option hits allocate their TH02 sparks at
the original callback point and selected back page. A host-invalid allocator
refusal is a sticky caller-visible failure after the collision prefix; it is
not rollback or a successful complete simulation. Stage seeding and all
remaining spark lifecycle work remain separate.

`sparks_update()` and `sparks_invalidate()` now provide the two complete
source state-write boundaries without a render or page driver. The former
updates only raw alive entries on an explicit back page, including its word
math, age/bounds removal, and no implicit position flip. The latter makes its
source-sized tile invalidation call for every nonfree raw flag before removing
or copying the source front page to its opposite page. Rendering, scrolling,
the tile system, page scheduling, and stage seeding remain separate.

`sparks_stage_seed()` is deliberately only the spark subblock from the larger
`bullets_and_sparks_init()`: it returns source effect defaults and seeds each
spark's free flag, angle, Q4.4 base speed, and bitwise interval mode through
two IRAND calls. It preserves all other spark state and buffered random state;
bullet reset/special/rank state, the later random-ring fill, and the stage
schedule remain unimplemented.

`sparks_add()` now supplies TH02's independent 64-slot spark allocator. Its
typed 20-byte record preserves the source's two unknown bytes and both page
positions; the allocator writes only the selected back page and source-owned
live/age/render/velocity fields. It retains the one unsigned top-coordinate
subtract, normal seeded-default branch, sprite ring's signed 16-bit angle
numerator, overwrite order, and low-byte-only ring update—including the
observable nonpositive-count update. Stage seeding of angle/speed/mode, spark
update, invalidation, rendering, frame scheduling, replay, and a collision
binding remain separate. These focused tests are source-derived allocator
coverage, not gameplay or replay verification.

`items_miss_add()` now supplies the source's persistent 20-slot miss-drop
allocator. It scans raw free flags in slot order, initializes at most five
entries on both pages with signed Q12.4 tops, and keeps the global game-over
mode and shared TH02 random ring explicit. Normal full-quota drops preserve
the source's one-big-power selection and short-circuit ring consumption;
game-over drops are all big power with no ring reads. The trajectory is chosen
from the separately supplied global player X rather than the item spawn left,
including the inclusive 128/256 field boundaries and signed table widening.
Item movement, collection, invalidation/rendering, miss/respawn composition,
and stage scheduling remain unimplemented.
