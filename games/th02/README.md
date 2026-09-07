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
