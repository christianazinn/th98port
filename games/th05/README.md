# TH05 native foundations

This is an asset-free, partial core, not a playable game or replay verifier.
Only the complete movement-input helper is implemented. Its source owner is
th05/player.asm at ReC98 c6c2ee5390012b6a334163716a1703bf2edd0229;
th04/main/player/move.hpp declares its variable signed aligned/diagonal Q12.4
speeds. Direction application is shared with TH04, but TH05 ignores input
bits 12..15 before dispatch while TH04's C++ switch rejects them. The TH05
wrapper retains that difference instead of assuming identical definitions.

The caller supplies MovementSpeeds from its state. There is deliberately no
default character speed or focus decision here. Zero and negative speeds are
retained, including original 16-bit overflow. The th05_core target supplies
this policy to the common helper without clearing untouched velocity axes.
The normal caller's INPUT_MOVEMENT mask is still separate. Position integration, clamping, input sensing,
character speed initialization, focus updates, and the full schedule remain open.

Shared tests exhaust input dispatch and signed speed arithmetic; the TH05 test
checks that changing the supplied speeds takes effect and does not fall back
to TH04 constants. These are source-derived unit tests, not replay equivalence.

Build via the native repository root or directly with
`cmake -S games/th05 -B out/th05`, then build and run CTest there.
