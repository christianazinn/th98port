# PC-98 numeric-semantics parity ledger

Status: **static audit completed 2026-08-08; closure rechecked against the
current dirty tree; no emulator evidence in this document**

This ledger audits the native deterministic gameplay core as it existed in the
shared `libth03-phase1` worktree on 2026-08-08 (worktree base
`33ad4189695236f72ca9bbe9cc20dc15d56de84e`, with uncommitted gameplay work).
The authority is the frozen Anniversary donor at
`3e2381d4275be7287f5525de5c95389d492b0a3c`, recorded in `SOURCES.lock`.
Locations below use `file:function` rather than line numbers where concurrent
implementation work makes a line number unstable.

The audit covered every gameplay-reachable arithmetic family in:

- `include/th03/core/numeric.hpp`;
- `src/core/{random,trig,round_start,game,frame}.cpp`;
- all nine characters' normal shots, Charge Shots, Hyper effects, bombs,
  Extra Attacks, Gauge Attacks, and Boss Attacks currently dispatched by
  `frame.cpp`;
- score, combo, gauge, player, CPU, bullet, fireball, enemy-VM, collision-map,
  result, and lifecycle paths; and
- the stock `originals/th03/dat/ENEDAT.DAT` operand ranges.

This is a numeric audit, not a completeness claim for the gameplay port. A
formula marked safe says that its integer semantics match within the stated
source-used range. It does not prove that the surrounding callback, state, or
call order is complete.

## Conclusion

No host-width discrepancy found here is proven to alter a stock Anniversary
gameplay path. That conclusion relies on the source-enforced ranges documented
below, most importantly `round_speed <= 0x7F`, Boss/Gauge level `<= 16`, valid
characters, ordinary playfield coordinates, and the operands in the original
`ENEDAT.DAT`.

Two of the six originally identified latent semantic discrepancies remain:
N1 and N6. N2 through N5 are closed in the current tree with explicit
signed-word multiplication, division, wrapping threshold arithmetic, and
signed-bit-pattern operations. The remaining cases matter to the stated goal
of a mechanically faithful core, but neither is stock-reachable under the
validated input/content ranges. They must not be used to explain a stock replay
divergence without first demonstrating that their triggering range was reached.

## PC-98 arithmetic model used by this audit

Turbo C++ 4.0J has 8-bit `char`, 16-bit `int`/`unsigned int`, and 32-bit
`long`/`unsigned long`. Integral promotion therefore places signed and unsigned
bytes in signed 16-bit `int`; an expression stays 16-bit unless a 32-bit operand
or generated helper deliberately widens it. Assigning back to a byte or word
keeps the low bits. The target CPU uses arithmetic right shift for signed
values and logical right shift for unsigned values.

The main source evidence is `Research/Borland C++ decompilation.md`, plus the
actual instruction sequences in `libs/master.lib/iatan2.asm`,
`th03/math/vector.cpp`, `th03/main/collmap_slope.asm`, and the register-heavy
collision code in `th03/main/player/stuff.cpp`.

The native build's `-Wconversion -Wsign-conversion -Werror` policy is useful,
but it cannot detect a deliberately written cast that happens after a host
32-bit intermediate. Such expressions were checked manually.

## Source-used range facts

These bounds close the stock-path proofs later in the ledger.

| Quantity | Source-used range / evidence | Consequence |
|---|---|---|
| Q12.4 coordinate | signed 16-bit `subpixel_t`; player centers clamp to X `128..4480`, Y `384..5632` | Player movement additions cannot overflow after one legal velocity, but entity additions still require word wrapping. |
| Q4.4 speed | signed `Subpixel8` or unsigned `SubpixelLength8`, one byte | Byte arithmetic must narrow before later tests if the source stores it first. |
| `round_speed` | capped at `0x7F` in Anniversary `th03/main/rndloop.cpp`; initialized and recapped in native `round_start.cpp` | `pellet_speed * round_speed <= 255 * 127 = 32385`, inside positive signed 16-bit. |
| `enemy_speed` | `round_speed / 32`, hence `0..3` | Enemy duration and speed-tuning products are small for stock scripts. |
| Boss/Gauge level | clamped to `0..16` | Every generated cadence/count denominator in the nine character callbacks is nonzero, and every `level * 2`/`* 4` fits a byte. |
| Bullet group count | source type is byte; all stock callers use small counts (fixed rings at most 32, dynamic character counts far below 128) | Stock `i * 0x100` ring numerators do not cross the signed-16 overflow boundary. Arbitrary count values can. |
| Original ENEDAT | static parse of the frozen 5072-byte file: 184 spawns; 346 speed operands `32..128`; 618 durations `0..250`; 142 angle speeds `-8..8`; 44 circular/sine additive components `-64..56`; spawn divisors X `-4..40`, Y `-8..32`; no loop opcode and no `EO_CLIP_BOTTOM` | All stock enemy speed-tuning, angle, duration, spawn-shift, and size products fit their intended intermediates. |
| Entity sizes/hits | byte fields; stock enemy/fireball sizes and hit counts are far below byte maxima | `size * 3`, `size * 6`, `hits * 2`, and `hits * 16` fit signed 16-bit even under the full byte domain where noted below. |
| Result counters | five byte counters plus byte lives | Worst representable result sum is `44,880,000`, below signed-long maximum; the source's signed-long `/ 192` and native unsigned division agree. |

The ENEDAT bounds above were obtained by a read-only semantic walk of the
original data using the layouts in Anniversary `th03/formats/enedat.hpp`; no
generated file was retained.

## Actionable discrepancies

### N1 — `iatan2()` does not reproduce the signed `CMP` at magnitude `0x8000`

Severity: **medium latent exactness debt; stock coordinates proven safe**.

- Source: `libs/master.lib/iatan2.asm`. It forms each magnitude in a 16-bit
  register, then executes `CMP DX,BX`, `JE`, `JL`. `JL` is a **signed** branch.
  Thus a magnitude of `0x8000` sorts as negative.
- Port: `src/core/trig.cpp:iatan2()`. `magnitude()` returns `uint16_t`, and the
  branch uses ordinary unsigned `abs_x > abs_y`.
- Difference: if exactly one wrapped delta is `INT16_MIN`, the selected ratio
  and quadrant path can differ before the angle is narrowed to a byte.
- Stock proof: every current gameplay call passes a playfield/entity delta
  whose live endpoints are separated by much less than 32768. Transfer paths
  first convert bounded screen/playfield coordinates with explicit word wrap.
- Required fix: model the assembly's 16-bit bit patterns but perform the
  magnitude ordering with `int16_t` interpretation, including the equality
  case, then retain the existing unsigned 32-bit numerator construction for
  `DIV`.
- Required test: all quadrant combinations involving `0`, `1`, `0x7FFF`, and
  `0x8000`, checked against a literal model of the assembly.

### N2 — closed: enemy tuning and angle products wrap before division

Severity: **medium for replacement ENEDAT; safe for the stock file**.

- Source: `th03/main/enemy/enemy.cpp:enemy_speedtune_1()`,
  `enemy_speedtune_2()`, and `enemy_angle_update()`.
  `enemy_speed * v`, `2 * v`, and `angle_speed * (0x100 * 2)` are signed
  16-bit `int` expressions. The source comment at `enemy_angle_update()`
  explicitly documents the overflow beginning at positive angle speed `0x40`.
- Current port: `src/core/frame.cpp:enemy_speedtune()` and
  `enemy_angle_update()` use `exact::wrap_multiply()` at each source product,
  perform the signed division afterward, and use `exact::wrap_add()` at the
  following word assignment/addition.
- Stock proof: original angle speeds are `-8..8`; stock tuned velocity is at
  most the vector component (`<=128`) plus a raw additive component
  (`-64..56`), and `enemy_speed <=3`. Every relevant product is therefore far
  inside signed 16-bit.
- Closure: the current spelling preserves each source sequence point and keeps
  the two deliberately inaccurate divisions in `enemy_speedtune()` separate.

### N3 — closed: generic bullet-ring division uses signed-word arithmetic

Severity: **low-to-medium for custom attack parameters; stock-safe**.

- Source: `th02/main/bullet/impl.hpp:bullet_group_ring_impl`, instantiated by
  `th03/main/bullet/bullet.cpp:group_velocity_set()`. The loop index is signed
  16-bit `int`; `(i * 0x100) / count` is a signed 16-bit expression.
- Current port: `src/core/frame.cpp:bullets_add_group()` converts the loop index
  and `0x100` to signed words, applies `exact::wrap_multiply()`, and then
  performs signed division by the byte count promoted to a signed word.
- Stock proof: all source-used fixed and dynamic ring counts remain below 128;
  the largest fixed ring is 32. For any byte `count`, the unsigned product is
  at most 65024, so this is specifically a signedness/division discrepancy,
  not a 16-bit unsigned overflow.
- Closure: counts allowing `i >= 128` now reproduce the wrapped negative
  numerator before signed truncating division. The separate zero-count policy
  remains N6.

### N4 — closed: CPU target thresholds use source word wrapping

Severity: **low latent exactness debt; stock target bounds proven safe**.

- Source: `th03/main/player/c433.cpp` and `c568.cpp` construct flags from
  `word_2142E + TO_SP(16)`, `word_2142E + TO_SP(-16)`, and the analogous Y
  thresholds. `word_2142E`/`word_21430` are signed 16-bit words.
- Current port: `src/core/frame.cpp:cpu_target_movement()` applies
  `exact::wrap_add()` / `exact::wrap_subtract()` to all three thresholds before
  the signed comparison. `cpu_initial_avoidance_input()` already narrowed its
  coordinate difference and remains equivalent.
- Stock proof: `cpu_target_point()` validates its enemy-derived point and
  otherwise uses `(144,300)`; player centers are bounded. No stock threshold
  approaches a word endpoint.
- Closure: arbitrary signed-word endpoints now follow the source threshold
  wrap rather than a host-width addition.

### N5 — closed: signed-word negation and Yumemi division use bit-pattern views

Severity: **low latent exactness debt; neither edge is stock-reachable**.

1. `src/core/frame.cpp:extra_attacks_update_reimu()` now uses
   `exact::wrap_negate()` for both bounce velocities, explicitly preserving
   `0x8000 -> 0x8000`.
2. `src/core/frame.cpp:extra_attacks_update_yumemi()` now converts the stored
   extent with `exact::from_bits<int16_t>()`, performs signed `/ 4`, and applies
   `exact::wrap_add()` before passing the height onward.

Closure: both operations now preserve the Anniversary signed-word bit pattern
and operation order for arbitrary stored values, not only the stock range.

### N6 — native validation changes two out-of-domain source arithmetic cases

Severity: **low, deliberate-safety-policy decision required**.

- `src/core/round_start.cpp:story_safety_frames()` clamps the calculated
  table index to `0..5`. Anniversary `th03/main/resscore.cpp:sub_9EBF()` only
  clamps values above 5; a negative `(round_id - rem_credits) + 3` indexes
  before the table. Stock credits (`<=3`) and initial/repeat round IDs keep the
  index nonnegative.
- `src/core/frame.cpp:bullets_add_group()` explicitly returns for a zero
  generic-ring count. The source macro divides by zero. No stock caller passes
  zero for a generic ring.

These are not accidental host-promotion errors, but they are observable
numeric differences. Decide and document whether public native APIs reject
invalid state/content before simulation (preferred) or whether the core must
preserve these landmines. Silent in-core normalization is neither behavior.

## State-width and signedness ledger

| State family | Anniversary representation | Native representation | Verdict |
|---|---|---|---|
| player/entity/bullet/fireball/Boss coordinates and velocities | signed word `subpixel_t` | `int16_t` or `Fixed<int16_t,4>` | Width correct. Mutation paths use explicit wrapping helpers at the audited arithmetic boundaries, including Reimu's bounce negation. |
| signed/unsigned Q4.4 values | `Subpixel8` / `SubpixelLength8` bytes | `Fixed<int8_t,4>` / `Fixed<uint8_t,4>` or explicit byte | Correct. Ordinary speed adjustments explicitly narrow to byte before clamping. |
| angles, animation frames, small counters, pattern parameters | byte | `uint8_t` (signed deltas use `int8_t`) | Correct modulo-256 behavior. Raw `++`, `--`, and explicit casts are defined for unsigned bytes. |
| word timers, gauges, HP, pattern frame, CPU frame/safety | unsigned word unless source explicitly casts signed | `uint16_t` | Correct modulo-65536 storage. Signed Boss-HP defeat test explicitly casts the bit pattern to `int16_t`. |
| `round_frame`, global frame, RNG seed, result total | dword | `uint32_t` | Correct. Increment/multiply/add is defined modulo 2^32. |
| resident random seed | signed dword storage | `int32_t`; converted to `uint32_t` for RNG | Bit-preserving modulo conversion is correct. |
| player half-hearts | source struct uses `shalfhearts_t` (`int8_t`) | `uint8_t` | **Type mismatch, stock range safe.** Native manually casts for the CPU low-heart test and reproduces clamp-to-zero behavior for `0..10`; exact arbitrary-byte semantics would be clearer with signed storage or explicit signed views at every comparison. |
| Kotohime Extra vertical velocity word | source accesses `slot+0x14` as signed `subpixel_t` | `uint16_t` | Bit storage and current positive addition agree; signed type is obscured at the architecture boundary. Prefer `int16_t` before reuse expands. |
| Yumemi Extra extent/duration words | source uses mixed raw `uint16_t`, `int`, and explicit signed `IDIV` register views | `uint16_t` plus named signed bit-pattern view at `/4` | Storage and operation-specific signed division are correct after N5 closure. |
| array iteration/indexing | source signed 16-bit `int` in most loops | host `size_t` or fixed-width indices | Safe where used only to visit fixed arrays. Generic bullet-ring arithmetic explicitly converts to and wraps as a signed word after N3 closure. |

## Exhaustive subsystem ledger

“Safe” below always means safe for the source-used ranges listed earlier.

### Numeric primitives, RNG, and trigonometry

| Port location | Anniversary authority | Sensitive operations | Verdict |
|---|---|---|---|
| `numeric.hpp:bits/from_bits`, `wrap_add`, `wrap_subtract`, `wrap_negate` | 8086/386 low-byte/low-word arithmetic throughout | narrowing and modulo wrap | **Exact.** Work occurs in the unsigned representation and converts by bit cast, avoiding host signed-overflow UB. |
| `numeric.hpp:wrap_shift_left` | word/byte `SHL` | left shift and discarded high bits | **Exact** for all counts below width; helper deliberately defines larger counts as zero. No gameplay caller shifts by an out-of-width count. |
| `numeric.hpp:arithmetic_shift_right` | `SAR` and source signed shifts | negative right shift | **Exact**, including explicit sign fill. |
| `Fixed::from_integer_wrapping`, `to_integer_floor`, `to_integer_trunc`; `DecimalSubpixel::to_integer_trunc` | Q12.4/Q4.4 and decimal fixed-point source types | narrowing before shift; floor shift vs truncating division | **Exact.** These deliberately distinguish `SAR` from signed `/`. |
| `random.cpp:irand()` | master.lib IRAND sequence used by Anniversary | dword multiply/add, high-word extraction | **Exact modulo 2^32.** The native unsigned arithmetic is the correct defined spelling. |
| `random.cpp:randring_fill/next8/next16` | `th03/main/randfill.cpp`, `main_04_randring.asm` | reverse loop, byte cursor wrap, word assembly | **Exact.** Cursor byte overflow and index-255 high-byte wrap are explicit. |
| `trig.cpp:cosine8/sine8/vector2()` | public-domain tables; `th03/math/vector.cpp:vector2_raw` | byte angle wrap; signed 32-bit multiply; arithmetic `>>8`; word narrowing | **Exact.** Native `int32_t` products match `MOVSX` plus 32-bit multiply in the source macro. |
| `trig.cpp:iatan2/vector2_between_plus()` | `libs/master.lib/iatan2.asm`, `th03/math/vector.cpp` | wrapped coordinate subtraction, magnitude, `<<8`, unsigned division, quadrant byte wrap | Exact except N1. Coordinate differences in `vector2_between_plus()` correctly use `wrap_subtract`. |

### Round setup, lifecycle, scoring, gauge, and result

| Port location | Sensitive operations | Range proof / verdict |
|---|---|---|
| `round_start.cpp:initial_round_speed()` | byte `round_id <<4/5`, add, narrowing | **Exact.** Native casts to byte at the source assignment point. Stock result is later capped at `0x7F`. |
| `initial_boss_level()` and repeat level increments | byte add, `/2`, `*2`, clamp | **Exact.** `wrap_add()` preserves every source byte assignment; level cap 16 proves all later character products safe. |
| `story_safety_frames()` | signed index subtraction; long `/3`, `*2`, `*3+500`; word clamp | 32-bit arithmetic matches source `long`; lower-index policy differs only as N6. The `0xFFFA * 3 + 500` Lunatic case is correctly computed wide then capped. |
| runtime ring setup | IRAND `% formation_count`, `&3 <<6`, `&1 <<7`, byte indices | **Exact for validated content** (`1..24` formations). Host `size_t` modulo has the same quotient/remainder in this range. |
| `frame.cpp:score_add()` | decimal `%10` and `/10`; raw MSD byte add | **Exact.** Carry is a word; per-digit values stay tiny. Most-significant byte uses `wrap_add()` to preserve the assembly's omitted AAA. |
| `gauge_avail_add()`; charge/drain in `player_update_finish()` | word additions/subtractions and min/max comparisons | **Safe/exact.** Gauge is bounded by `0xFF0`; adding a byte cannot approach word overflow. Hyper drain stays above 64 before subtracting 6. |
| `combo_add()` and `fire_point_based_boss_attack_or_panic()` | dword saturated bonus sum; word threshold `+5120`; byte counters | **Exact.** Combo explicitly widens like source `uint32_t`; threshold explicitly narrows to word after addition. |
| `result_bonus_update()` | five dword products, dword sum, `/192`, word cap and subtraction | **Exact over the full representable counter domain.** Maximum 44,880,000 also proves source signed-long division and native unsigned division agree. |
| `game.cpp:next_story_stage_init()` and repeat init | dword-to-signed-dword bit conversion; byte stage/round increment; character `/2` | **Exact for valid characters/stages.** Negative/invalid character input is rejected by bounds or becomes no dispatch; it is outside stock state. |

### Bullet, fireball, explosion, and chain arithmetic

| Port location | Sensitive operations | Range proof / verdict |
|---|---|---|
| `bullet_speed_adjusted()` | byte addition before `1.0..8.0` clamp | **Exact.** The cast to `uint8_t` occurs before comparisons, matching `bullet_template.speed.v += bullet_base_speed.v`. |
| `bullets_add_group()` spread/fixed/generic-ring cases | small `variant * 3/6/8`, angle byte additions, signed offsets, index shifts, wrapped signed-word ring division | **Exact.** Variants `0..2` and fixed indices `0..31` fit signed word; generic rings explicitly wrap the signed-word numerator before division. Zero generic-ring count remains the N6 policy difference. |
| random bullet groups | `%`, masks, random speed addition, count comparison | **Exact.** RNG results and speed/angle narrowing occur in source order. `count` semantics intentionally produce `count+1` bullets for the random groups. |
| transfer bullet/fireball/Extra helpers | `% (288<<4)`, screen/playfield `*320`, Q12.4 `<<4`, speed `/3` or `/4` | **Safe/exact.** Coordinates remain within about 10240 before explicit word shift/wrap; transfer velocity bases plus difficulty terms remain below 142. |
| `bullets_update()` / trail update | word position adds, byte age/speed, acceleration thresholds, Q coordinate `>>5` | **Exact.** Entity motion uses wrap helpers; bullet collmap indexing is preceded by nonnegative/in-bounds tests. C++20 signed right shift is arithmetic, and exact helper use covers semantic sites. |
| explosion/pellet collision | `size * 6`, `hits *2`, bonus `+round_speed`, `>>1` | **Safe over full byte size/hit inputs** for products (`<=1530` and `<=510`); byte/word assignments explicitly narrow. |
| fireball/enemy chain scoring | `against *3`, `hits *16`, charge-slot products/subtractions, ring cursor `%16` | **Safe/exact.** `255*16=4080`; slot is derived from capped round speed (`0..3`); byte charge accumulation intentionally wraps. |
| chain pellet speed | `(pellet_speed * round_speed) >>7` then add and clamp | **Safe/exact for all byte pellet speeds under source `round_speed<=127`**: product maximum 32385. Right shift is nonnegative. |

### Shots, Charge Shots, Hyper, bombs, player, and CPU

| Port location / character | Sensitive operations | Verdict |
|---|---|---|
| `shots_add/update`, shared hit tests | Q12.4 adds/subtracts, `paletted_character` signed `/2`, byte frame | **Safe/exact.** Shot coordinates and hitbox arithmetic use wrap helpers. Valid palettes are 1..18. |
| `movement_velocity/apply_velocity/move_human` | byte speed boost/penalty; multiply by `-1/0/1`; word add then clamp; input masks/shifts | **Exact.** Byte adjustment narrows at assignment, signed products are at most 112, and center addition wraps before the source-equivalent clamp. |
| Reimu Charge/Hyper | byte angle `+0x20`; speed decrement; point shifts; wrapped coordinate collision | **Safe/exact.** All indices and shifts are bounded by 7/12-node arrays. |
| Mima Charge | frame `%4` and `/4`; Q coordinate wrap; byte speed decrement | **Safe/exact.** Frame selects at most 15 nodes; no negative division. |
| Marisa Charge/Hyper | 12-node shifts, byte frame, Q Y subtraction | **Safe/exact.** No multiply wider than array-derived `node*2/4`; all values fit. |
| Ellen Charge/Hyper | byte state `node*2/4`, vector table `node%4`, Q word movement | **Safe/exact.** Node maxima make products <=126; state storage narrows to byte like source. |
| Kotohime/Yumemi Charge | signed Q velocity subtraction; byte frame `%2`; radius comparisons | **Safe/exact** in live ranges. |
| Kana/Rikako/Chiyuri Charge | angle byte add; unsigned radius word increments; reverse indexing; fixed Q collision radii | **Safe/exact.** Radius and coordinate mutations explicitly wrap; indices never enter arithmetic outside small bounded constants. |
| `release_gauge()` / bombs / Hyper drain | gauge word thresholding and subtraction; bomb frame `%4/8/16`; byte animation arithmetic | **Safe/exact.** All divisions/moduli have positive constants. Presentation RNG consumption is numeric-state relevant but its count/order is outside this arithmetic-only verdict. |
| player damage/defeat/knockback | signed byte damage view, byte subtraction/clamp, vector math, byte angle wrap | **Equivalent for stock half-hearts `0..10` and damage `<=6`**. Native half-heart signedness is a state-type debt noted above. |
| `cpu_initial_avoidance_input()` | wrapped collision deltas; route index `strategy*3`; signed coordinate thresholds | **Exact.** The subtraction explicitly narrows to `int16_t`; strategy is source-used `0..1`, and native clamps defensive outliers. |
| `cpu_target_movement()` | threshold additions and bit shifts | **Exact after N4 closure.** Thresholds wrap as signed words before comparison; Boolean-to-byte shifts `<<1/2/3` are exact. |
| `cpu_safe_move/player_update_cpu()` | hitbox byte increments, `%3`, cadence `%`, gauge `>>4`, ring cursor `%64`, word frame increment | **Safe/exact.** All divisors are fixed nonzero; unsigned word/byte counters wrap as in source. |

### Enemy VM, formation, and collision map

| Port location | Sensitive operations | Range proof / verdict |
|---|---|---|
| `enemy_run()` spawn | signed script byte `*128`, mirrored `4608-x`, size-word byte `*16`, byte/word IP addition | **Safe/exact for stock scripts.** Spawn products range `-1024..5120`; size words are tiny. Word IP narrowing matches source storage. |
| linear/circular/sine motion | vector 32-bit multiply, signed byte additive vector, word position wrap | **Exact after N2 closure.** Tuning products wrap before division; additive `Subpixel8` is raw Q4.4 and correctly sign-extends without an extra scale. |
| `wait_and_advance()` | `(duration/2)*3 - (enemy_speed*duration)/6`; word frame/IP | **Safe/exact.** With duration <=250 and speed <=3, all signed-16 intermediates fit. |
| `add_formation()` / formation cursors | byte cursor increment, host-size bounds comparisons | **Exact for parser-validated stock content.** Cursor wraps at 256 exactly like the source ring. |
| `collmap_set_enemy/fireball/rect_striped()` | signed center `>>5`, `size*3/16`, half sizes, clipping and host indices | **Safe/exact.** C++20 right shift of negative signed values is arithmetic. Negative indices are clipped before conversion to `size_t`. |
| `collmap_set_slope_striped()` | word slope subtraction; signed 32-bit `slope_width*y/92`; word add; logical coordinate shift | **Exact.** Anniversary `collmap_slope.asm` explicitly uses 16x16 `IMUL` to DX:AX and signed `IDIV`; native `int32_t` matches it. |
| legacy `player_hittest()` | signed `SAR`, signed-byte X clipping, unsigned `MUL`, register-alias byte wrap, variable shifts, 16-bit DGROUP offset wrap | **Mechanically represented.** Native narrows offsets and byte/word temporaries at the same stages and preserves the negative-X stale-`CL` behavior. Hitbox sizes used by gameplay are positive `4..80`, so all shift counts are valid. |

### Extra Attacks, all characters

| Character / port location | Sensitive operations | Verdict |
|---|---|---|
| shared transfer and collision helpers | signed word movement/endpoint comparisons; radius adds/subtracts; target-player byte xor | **Exact in source-used coordinate range.** Movement and collision boundaries use wrap helpers. |
| Reimu | gravity word `+1`, bounce negation, random angle `bit*0x60`, speed `/8` | **Exact after N5 closure.** Bounce negation uses explicit signed-word wrap, including `INT16_MIN`. |
| Mima | word timer increment; byte speed increments; bounce negation; vector recomputation | **Exact.** Mima uses `wrap_negate()` and `wrap_add`; timer comparisons are unsigned and small. |
| Marisa | Q Y `-256`; `collmap_h/2` and `/32`; byte frame | **Safe/exact.** Height is nonnegative `0..5888`; signed truncating division equals source. |
| Ellen | seven-node reverse copy; signed radius `+0x18`; signed-byte angle delta; vector multiply | **Safe/exact.** Radius stops at `0x0D00`; all products use the exact vector helper. |
| Kotohime | target/velocity masks, signed word Y addition, milestone byte, ring speed/angle increments | **Safe/exact in live positive-velocity range.** State signedness debt is noted above. |
| Kana | byte speed/frame increments; `%16`; vector | **Safe/exact.** Object clips long before byte speed overflow in source-used paths. |
| Rikako | byte angular difference reinterpreted as `int8_t`; signed `/8`; speed increment | **Exact.** Native `bit_cast<int8_t>` preserves the source byte and signed division truncates toward zero. |
| Chiyuri | pair index `+=2`; byte frames; signed 32-bit slope interpolation | **Exact.** Pair indices remain `0..15`; slope helper matches assembly. |
| Yumemi | duration `+8` word wrap, extent `<<2/3` low-word use, extent `/4`, word coordinate adds | **Exact after N5 closure.** Extent `/4` uses a signed bit-pattern view and shifts preserve low bits. |

### Gauge and Boss Attacks, all characters

The per-character level formulas use only `level /2,/3,/4,/5,/8,/10` and
`level *2/*4` with level `0..16`; all are exact. Pattern frames are unsigned
bytes or words, `%` denominators are initialized positive, and angle/radius
state is explicitly narrowed after mutation.

| Family | Sensitive operation groups | Verdict |
|---|---|---|
| nine Gauge Attacks (`gauge_pattern_*`) | frame `%` cadence; `/2,/4`; angle `<<2`; center mirror subtraction; random `%` bounded Q widths; byte speed/angle wrap | **Safe/exact.** Generated parameters stay nonzero and below a byte. Center and movement mutations use signed words where required. |
| shared Boss launch/move/chooser/hittest/defeat | word coordinate wrap; byte random pattern/count; signed previous-pattern comparisons; byte damage accumulation; word HP subtraction plus signed-HP test | **Exact.** HP is subtracted modulo 65536 and then viewed as signed, matching the source's `static_cast<int16_t>(word_1F34A)`. |
| Reimu Boss | word frame `<<5`, `(i<<8)/count`, angle/radius byte/word wrap, cadence `%` | **Safe/exact.** Counts `4..6`; numerator/radius shifts are narrowed to word at source storage points. |
| Mima Boss | cadence `%`, half phase `/2`, level `/4`, `*2/*4`, orbit angle signed byte | **Safe/exact.** All parameters are bounded and nonzero. |
| Marisa Boss | `256/count`, level `*2`, direct-pair coordinate constants, frame `%` | **Safe/exact.** Counts are small positive bytes; division is positive. |
| Ellen Boss | `(i<<8)/count`, speed `/2`, level `/3,/10,*2`, nested ring counts | **Safe/exact.** Maximum loop indices and products fit signed 16-bit. |
| Kotohime Boss | `(i<<8)/count`, nested `(inner<<7)/count`, `inner*0x30/count`, level halves/thirds | **Safe/exact.** Source-used counts keep every signed-16 numerator below overflow. |
| Kana Boss | byte speed `+3/+8`, angle `+6`, frame `%`, level halves | **Safe/exact.** Mutations narrow to byte at source assignment points. |
| Rikako Boss | `(i<<8)/count`, byte speed `+8`, frame `%`, level `/4` | **Safe/exact.** Counts/indices are small. |
| Chiyuri Boss | lattice `(i*0x70/count)<<4`, `(i<<6)/count`, signed angle offset, speed `+/-2` | **Safe/exact.** Native narrows Q coordinates and angles after the same formula; source-used count is at least 12. |
| Yumemi Boss | random `%5`/group choice, random Q X `%4608`, byte angle/radius delta, level `*4`, pattern frame `*8` then signed-word test | **Exact.** The render-phase comparison explicitly casts the low word of `pattern_frame*8` to signed, matching `length *= 8; static_cast<int16_t>(length)`. |

## Proven-safe comparison and promotion idioms

The following recurring native idioms do not require per-site fixes:

- A byte compared with a nonnegative literal or another byte is equivalent in
  all source-used ranges, even though host promotion is 32-bit rather than
  TCC's 16-bit.
- A `uint16_t` compared with a positive constant is equivalent unless the
  source explicitly takes a signed view. The Boss HP and Yumemi division sites
  were checked separately.
- `%` and `/` by positive constants are equivalent when the dividend is
  nonnegative and the preceding product is range-proven. All frame cadence
  operations fall in this class.
- Casting an addition/product to `uint8_t` or `uint16_t` at the same source
  assignment point correctly preserves low bits. A cast only after a later
  division/comparison does not; the closed N2/N3 sites now wrap before those
  operations.
- Unsigned byte/word `++`, `--`, `+=`, and `-=` in native C++ have defined
  modulo behavior after assignment and match source storage. Signed coordinate
  mutation should continue to use `exact::*` helpers.
- Constant Q shifts such as `288 << 4` are compile-time values within signed
  16-bit. Variable Q shifts were individually checked above.
- `size_t` loop indices are harmless when used only for array traversal and
  compared with small fixed sizes. They must not replace source signed-16
  arithmetic without a range proof.

## Recommended closure work

1. Make `iatan2()` instruction-exact at `0x8000` and add a 16-bit exhaustive
   differential test for the magnitude-order/quadrant scaffold (the table
   lookup need only be sampled once the ordering model is proven).
2. Decide the invalid-input contract for N6. Prefer rejecting malformed
   `GameInit`/content at construction, then keep the deterministic core free of
   silent behavior-changing clamps.
3. Normalize architecture field signedness (`halfhearts`, Kotohime velocity)
   or add named signed/bit views so future arithmetic cannot accidentally use
   the host interpretation.
4. Keep the stock range assertions close to the parser/initializers: ENEDAT
   formation/count limits, round-speed cap, Boss/Gauge level cap, and nonzero
   dynamic pattern denominators.

After those changes, rerun the static ledger and add focused native unit tests.
Replay-oracle validation remains necessary, but it should then be confirming a
specified integer model rather than discovering promotion errors by accident.
