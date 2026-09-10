# TH01 headless gameplay core

This is a headless gameplay library, not a replay verifier or playable game.
The foundations below are now composed into the source gameplay schedule.
No replay equivalence is claimed. The historical source coverage ledger also
includes presentation and process owners that this gameplay MVP does not port.

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
comparisons. `player.cpp` now composes it with dashes, shots, swings, the three
combo modes, bombs, invincibility, and player/Orb collision. Visual particle
respawns retain their shared RNG draws even without drawing pixels.

IRAND is shared with TH03 because both use the exact same master.lib routine.
The seed remains 32 bits, independent of TH01's `frame_rand`; its return masks
only the high word's sign bit. TH03's random ring stays game-specific.

Validation uses GCC and MSVC with warnings as errors. `th01-core-tests` covers
raw-key handover, consumed held inputs, pause/OK ordering, the 19/20-frame bomb
boundary, latched flags, counter wrap, reset scope, and coordinate wrap before
clamp. IRAND is compared over six 65,536-sample sequences against an independent
three-MUL/ADC limb model and existing TH03 known answers. These are source-based
unit checks; the PC-98 oracle remains a separate future integration gate.

The gameplay schedule now composes Orb motion and shot interactions.
Orb return velocity differs from its clamped stored velocity; shot collisions
perform an additional Orb update. Neither can be replaced by a conventional
generic physics loop.

The integer-only Orb helpers are now available in `core/orb.hpp`: horizontal
motion, horizontal reflection, and the rotation-frame block. These retain
overshoot (no clamp), separate movement arguments from stored velocity, and
reflect before choosing animation direction. Tests cover all 65,536 coordinate
bit patterns against all 25 argument/stored-velocity combinations, plus the
asymmetric ten-frame rotation cycle. They are integrated with vertical forces,
collisions, portals, and the update schedule. Full x87 trajectory equivalence
is not established by scalar tests.

`core/shots.hpp` additionally implements the complete source spawn and
pellet/boss hit helpers from `th01/main/player/shot.cpp`, with dimensions and
overlap macros from `shot.hpp`, `th01/sprites/pellet.h`, and
`th01/math/overlap.hpp`. Returned slot events allow later unblit/SFX handling;
the separate shot-Orb update loop implements its extra Orb integration.
`th01-shot-tests` checks slot allocation, inclusive edges, differing decay
eligibility, 16-bit arithmetic, and no mutation on refusal/miss.

The vertical scalar helpers now live in `core/orb.hpp` and `src/core/orb.cpp`.
They preserve unclamped returned delta separately from clamped stored velocity,
signed frame division, all force modes and the two stationary-surface RNG
draws. A 38-row original MODEL_LARGE guest experiment supplies raw binary64
expected outputs. This is not complete Orb trajectory parity. The host requires
nearest binary64 rounding, disables fast-math/contraction, and refuses
nonfinite/out-of-range conversions. Calls do not advance the frame counter
or perform position/collision updates; `gameplay.cpp` owns that ordering.

## Connected gameplay boundary

`gameplay_begin_stage` initializes an already loaded scene. It does not model
DOS process startup or increment the stage. `gameplay_tick` performs one source
iteration and stops at an explicit boundary. Pause resumes through
`gameplay_resume`, which finishes the deferred score suffix exactly once;
SinGyoku's route modal resumes before the Orb portion of that same iteration.
`gameplay_restart_life` preserves the source miss/reset arithmetic and RNG.
`gameplay_stage_bonus` accepts the completed stage's zero-based `stage_id` and
performs bonus arithmetic only; stage advance and the bonus input modal remain
client responsibilities. Stage entry clears completed-stage/hit flags before
the next iteration, including same-process card-stage transitions.

The core owns card/obstacle/item pools and archive stage decoding. Original
assets are read-only external inputs, never embedded in the native repository.
The headless command accepts six raw keyboard groups per sample, not a TH01
replay file. It is a useful bring-up driver, not an end-to-end replay verifier.

GCC/MSVC tests check source-derived boundaries and patterns. The original-data
stage sweep checks reachability and compiler agreement under synthetic input.
Neither replaces future validation against recorded PC-98 sessions. Rendering,
audio, menu polling, resident/process import, and full-session replay scheduling
remain outside this MVP.
