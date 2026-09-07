# TH05 native foundations

This is an asset-free, partial core, not a playable game or replay verifier.
Movement input, word motion updates, and the player boundary clamp are implemented.
The movement-input source owner is
th05/player.asm at ReC98 c6c2ee5390012b6a334163716a1703bf2edd0229;
th04/main/player/move.hpp declares its variable signed aligned/diagonal Q12.4
speeds. Direction application is shared with TH04, but TH05 ignores input
bits 12..15 before dispatch while TH04's C++ switch rejects them. The TH05
wrapper retains that difference instead of assuming identical definitions.

The caller supplies MovementSpeeds from its state. There is deliberately no
default character speed or focus decision here. Zero and negative speeds are
retained, including original 16-bit overflow. The th05_core target supplies
this policy to the common helper without clearing untouched velocity axes.
The normal caller's INPUT_MOVEMENT mask is still separate. Input sensing,
character speed initialization, focus updates, and the full schedule remain open.

Shared tests exhaust input dispatch and signed speed arithmetic; the TH05 test
checks that changing the supplied speeds takes effect and does not fall back
to TH04 constants. These are source-derived unit tests, not replay equivalence.

The position update/clamp is genuinely source-shared with TH04: both link
th04/player_p.cpp and the same th04/math/motion.inc macro. It saves the old
position, wraps word additions, then clamps x=8..376/y=8..352 pixels while
leaving velocity intact. The game test composes the supplied-speed movement
and clamp helpers; the shared motion test exhausts coordinate bit patterns.

Build via the native repository root or directly with
`cmake -S games/th05 -B out/th05`, then build and run CTest there.

`core/random.hpp` exposes the source-shared TH04/05 random ring: reverse fill
and cursor reset, then overlapping word draws with the original pre-increment
cursor spill at slot 255. Both code-segment instances use one state. The full
word cursor is retained; unsupported high-byte addresses and divide-by-zero
produce no result rather than an invented random value. The zero-divisor path
still consumes its draw. Startup seeding, call schedules, range wrappers, and
TH05's inline bullet draws remain separate. Shared tests cover all word values
and cursor states; they are not an emulator parity claim.
