# TH04 native foundations

This is an asset-free, partial core, not a playable game or replay verifier.
Movement input, word motion updates, and the player boundary clamp are implemented, sharing direction
application with TH05. The source owner is th04/main/player/move.cpp at ReC98
c6c2ee5390012b6a334163716a1703bf2edd0229; move.hpp supplies TH04's fixed
64/48 aligned/diagonal speeds in Q12.4 subpixels. The helper body is unchanged
from frozen master f7d5a2546dff0d734ba1a04acec419cfc4d2f6c7.

Movement accepts the exact original direction combinations, including separate
keypad diagonal bits, and preserves components it does not assign. No-input and
invalid input leave both components unchanged. The caller's velocity reset and
input masking are not hidden in this helper. Animation, focus/input sensing,
entities, and the gameplay schedule remain open.
TH05 has an assembly-specific high-input-bit mask which TH04 does not apply.

The public th04_core target supplies the TH04 speed policy to the shared
helper. Shared tests cover dispatch and signed arithmetic; the game-specific
test checks fixed speeds and refusal behavior. These are source-derived unit
tests, not native/PC-98 replay equivalence.

Motion integration follows th04/math/motion.inc, and the player clamp follows
th04/main/player/pos.cpp (actually linked by both games). The saved previous
position remains the pre-update coordinate, even if it was outside the field;
16-bit wrapping occurs before the x=8..376/y=8..352-pixel clamps. Clamping does
not clear velocity. The game test composes movement and clamping; shared tests
cover all 65536 coordinate patterns against thirteen deltas on both axes.

Build via the native repository root or directly with
`cmake -S games/th04 -B out/th04`, then build and run CTest there.

`core/random.hpp` exposes the source-shared TH04/05 random ring: reverse fill
and cursor reset, then overlapping word draws with the original pre-increment
cursor spill at slot 255. Both code-segment instances use one state. The full
word cursor is retained; unsupported high-byte addresses and divide-by-zero
produce no result rather than an invented random value. The zero-divisor path
still consumes its draw. Startup seeding, call schedules, and range wrappers
are not implemented. Shared tests cover all word values and cursor states;
they are not an emulator parity claim.
