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
