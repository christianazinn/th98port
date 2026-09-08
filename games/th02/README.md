# TH02 native foundations

This is an asset-free, partial deterministic core, not a playable game or a
replay verifier. Random-ring fill, byte/word reads and the page-bound player
movement helper are implemented.

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
are tested against eleven deltas. The helper does not replace the caller's
page invalidation/copy, half-pixel speed adjustment, option lag, shots, bombs,
or hit/invincibility handling. Those remain unimplemented schedule dependencies.

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
axes. The caller must first apply the original hit guard; this helper does not
implement that guard, options, shooting, bombs or page alternation. Global
speeds/input/page become arguments and selected deltas/pose become a result;
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

`player_move_and_shoot_shot_stream_step()` carries only the tail's two stream
phases/cooldowns, handoff patnums, and powered spread adjustment. Its borrowed
typed callbacks execute synchronously in the source volley/sound/bomb order;
the volley callback may update the handoff patnums before later callbacks see
them. Shot pools, shottype callbacks, RNG, bomb effects, HUD, option placement,
and the frame schedule are deliberately not implemented. Tests cover the A/B
cooldown/SFX asymmetry, A's one-volley jump, release phases, spread bounds, and
the callback-visible mutation order. This is a caller-tail seam, not a native
shot or gameplay implementation.

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
caller-tail seam before the later sound callback. It deliberately excludes the
other shottype callbacks, option firing, update/render, collisions, and frame
scheduling; the tests are source-derived callback coverage, not a full shot or
gameplay implementation.

`shot_b_volley()` adds shottype B's complete selected spawn body on the same
pool: one source-ordered free scan writes the regular volley and then its
option volley. It preserves all level shapes, the level-8 option fallthrough
that overwrites each first pattern with level 9's final pattern, full-power
patnum changes, target marker/latch gates, and target/fallback homing angles.
Homing consumes the shared literal master.lib `iatan2_source()` model; a
modeled source DIV fault reports the prefix already written rather than using
TH03's deliberately normalized edge policy. It remains a spawn callback only:
shottype C, option movement, hit testing, invalidation/update/render, sound,
frame scheduling, replay, and playable-session claims are absent.
