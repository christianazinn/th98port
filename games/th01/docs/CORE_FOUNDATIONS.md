# TH01 deterministic foundations

This is a partial, asset-free core library, not a replay verifier or playable
game. Its first slice implements production REIIDEN input sensing, player
horizontal clamping, and the shared master.lib IRAND primitive. No replay
equivalence is claimed. The source coverage ledger remains incomplete.

Donor: ReC98 `c6c2ee5390012b6a334163716a1703bf2edd0229`.

| Native surface | Complete donor definitions |
|---|---|
| `core/input.hpp`, `core/input.cpp` | `th01/main_01.cpp`: `bomb_doubletap_update`, `input_sense`; `th01/hardware/input.hpp`: input macros; `th01/hardware/input_mf.cpp`: initial flags; `th01/hardware/input_rs.cpp`: reset; `th01/main/player/bomb.hpp`: window; `platform/x86real/pc98/keyboard.hpp`: raw key masks |
| `core/motion.hpp` | `th01/main/player/move.cpp`: `player_move_and_clamp`; `th01/main/player/player.hpp` and `th01/main/playfld.hpp`: bounds; `pc98.h`: signed 16-bit coordinates |
| `shared/include/pc98/random.hpp` | `libs/master.lib/random.asm`: `IRAND`; `libs/master.lib/master.hpp`: `irand_init`; existing TH03 `src/core/random.cpp` implementation |

`RawInput` is one latched replay/keyboard sample, retaining the separate arrow
and keypad masks. The DOS routine reads groups twice and ORs them; its replay
adapter returns the same latched groups throughout one sensing pass. Platform
sampling must establish this snapshot before invoking `sense_input`.

Input history and logical outputs are distinct state. Gameplay can consume a
shot by clearing `shot`; polling a still-held Z must leave it cleared until the
next physical edge. The bomb flag is likewise a consumer-cleared latch. Reset
of repeat history clears slots 0 through 13 only, while full input reset also
clears action/menu flags. Neither reset clears the hit flag or doubletap clock.

Input sensing does not advance time. The donor increments `frame_rand` before
sensing gameplay input and increments `bomb_doubletap_frame` after the player
and item updates. Modal/interstitial polls do neither. Integration must retain
that order. The sixteen history bytes retain slots 14 and 15 for checkpoint
fidelity, but test-mode recursive memory screens are outside this library.
FUUIN and OP have different input state machines and are still unimplemented.

Player clamping preserves the 16-bit coordinate assignment before signed
comparisons. It is only the complete movement helper; dashes, shots, swings,
combos, bombs, and the whole player update state machine remain unimplemented.

IRAND is shared with TH03 because both use the exact same master.lib routine.
The seed remains 32 bits, independent of TH01's `frame_rand`; its return masks
only the high word's sign bit. TH03's random ring stays game-specific.

Validation uses GCC and MSVC with warnings as errors. `th01-core-tests` covers
raw-key handover, consumed held inputs, pause/OK ordering, the 19/20-frame bomb
boundary, latched flags, counter wrap, reset scope, and coordinate wrap before
clamp. IRAND is compared over six 65,536-sample sequences against an independent
three-MUL/ADC limb model and existing TH03 known answers. These are source-based
unit checks; the PC-98 oracle remains a separate future integration gate.

Next: preserve the complete gameplay schedule, implement Orb motion and shot
interactions, and establish the donor's floating-point conversion behavior.
Orb return velocity differs from its clamped stored velocity; shot collisions
perform an additional Orb update. Neither can be replaced by a conventional
generic physics loop.

The integer-only Orb helpers are now available in `core/orb.hpp`: horizontal
motion, horizontal reflection, and the rotation-frame block. These retain
overshoot (no clamp), separate movement arguments from stored velocity, and
reflect before choosing animation direction. Tests cover all 65,536 coordinate
bit patterns against all 25 argument/stored-velocity combinations, plus the
asymmetric ten-frame rotation cycle. These helpers still need integration with
vertical forces, collisions, portals, and the actual update schedule. No x87
or host floating-point equivalence is assumed by this addition.
