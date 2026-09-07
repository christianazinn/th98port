# TH04 native foundations

This is an asset-free, partial core, not a playable game or replay verifier.
Only the complete movement-input helper is implemented, sharing direction
application with TH05. The source owner is th04/main/player/move.cpp at ReC98
c6c2ee5390012b6a334163716a1703bf2edd0229; move.hpp supplies TH04's fixed
64/48 aligned/diagonal speeds in Q12.4 subpixels. The helper body is unchanged
from frozen master f7d5a2546dff0d734ba1a04acec419cfc4d2f6c7.

Movement accepts the exact original direction combinations, including separate
keypad diagonal bits, and preserves components it does not assign. No-input and
invalid input leave both components unchanged. The caller's velocity reset and
input masking are not hidden in this helper. Position integration, clamping,
animation, focus/input sensing, entities, and the gameplay schedule remain open.
TH05 has an assembly-specific high-input-bit mask which TH04 does not apply.

The public th04_core target supplies the TH04 speed policy to the shared
helper. Shared tests cover dispatch and signed arithmetic; the game-specific
test checks fixed speeds and refusal behavior. These are source-derived unit
tests, not native/PC-98 replay equivalence.

Build via the native repository root or directly with
`cmake -S games/th04 -B out/th04`, then build and run CTest there.
