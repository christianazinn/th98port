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

`core/score.hpp` ports the complete highest-score/reset helper from
th05/main/score_highest.cpp at the same donor. Both digit-zero continue counters
are preserved. The unsigned most-significant-first comparison commits a higher
record before clearing the run digits and three pending-score/popup fields.
No decimal sanitization is added. Global/resident fields become explicit state;
the register comparison state becomes an enum. The loop and mutation order
are retained. This is not TH04's reset-only routine or TH05's score-add pipeline.
Tests use an independent whole-value model and every byte pair at each digit;
these are source-derived checks, not user-replay or emulator evidence.

`core/extend.hpp` ports point_items_extend_update from th05/main/item/collect.cpp.
It checks the next hundred-point-item threshold and consumes at most one per
call, even if a life cannot be granted. The byte-sized extend count wraps;
the item count is an unchanged input to this helper. Unlike TH04, 99 lives
already blocks the reward (TH04 grants the hundredth). The shared award body
retains wrapped playperf arithmetic, bullet-clear minimum and ordered HUD,
popup and sound requests; per-game predicates and limits stay separate.
Item collection, other counters, presentation and the caller schedule remain
open. Exhaustive source-derived tests are not replay verification.

`core/shot.hpp` supplies TH05's distinct typed Shot and HitShot records and
the complete local hitshot_from body. It advances one animation-ring entry,
removes the incoming shot even when that entry is occupied, and retains the
source sprite choice and signed velocity division. Invalid host cursor values
refuse without mutation. Tests cover all ring slots, all animation-age bytes,
all velocity words and all sprite bytes. Shot allocation, collision callers,
and complete gameplay scheduling remain separate work.

`core/shot.hpp` now also models TH05 `sub_1240B()`'s complete local 64-shot
and 24-hitshot update over caller-provided typed state. It retains the
one-frame `F_REMOVE` retirement, unsigned type dispatch, aged-homing
normalization before the `-15984` no-target sentinel, exact Q12.4 strict
bounds, ordered alive-cache writes, byte age/pattern progression, and the
hitshot's delayed clipped retirement. TH05's velocity setter uses the shared
source-exact Q8 vector with its owning shot's angle write represented directly.
The shared master.lib `iatan2` reports its native word-DIV fault; this updater
stops exactly at that attempted homing call, retaining preceding mutations and
leaving later shots and hitshots untouched. Tests cover that fault-prefix
boundary as well as flags, missiles, homing, all hitshot ages, clipping and
stale cache tail behavior. This is a source-derived unit model, not an
emulator, oracle, playable-game or replay-equivalence claim. Allocation,
collision scheduling, rendering and stage initialization remain separate.

`core/shot_collision.hpp` supplies the source-ordered cache collision scan,
shared 64-slot spark/RNG2 consumption, Alice revenge rank/frame gating with
caller-owned persistent bullet-template origin/angle fields, and complete
word-width damage/scaling/score helpers. The concrete bullet pool remains an
explicit synchronous callback; these are source-derived native units, not a
playable-game or replay-equivalence claim. Build tests are registered as
`th05-shot-collision-tests` and `th05-shot-spawn-tests`.
