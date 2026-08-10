# TH03 presentation contract research

- Status: source-backed boundary contract; indexed, planar/GRCG, TRAM/gaiji,
  and synthetic static-MAIN startup/page-transaction foundations exist, but no
  live gameplay scene renderer or captured visual-oracle acceptance exists yet
- Date: 2026-08-09
- Primary authority: Anniversary `3e2381d4275be7287f5525de5c95389d492b0a3c`

## Purpose and conclusion

This document defines the boundary between deterministic Classic gameplay and
the future native presentation library. It inventories the visual code, state,
and assets intentionally absent from `libth03`, records the observable ordering
that an Authentic implementation must reproduce, and turns every known
gameplay/RNG-to-presentation seam into an explicit contract.

The recommended design is a minimal hybrid with a strict semantic boundary:

1. `th03_core` owns gameplay semantics only. During a step, it can optionally
   report immutable gameplay facts, semantic phase observations, and primitive
   results of deterministic compatibility RNG reads it has already consumed.
   These observations are synchronous and transient: the core owns no pending
   presentation queue and does not know how an observer will interpret them.
2. `th03_present` observes those facts and owns all scene-local visual state,
   presentation records, asset identities, and ordered Authentic draw commands.
3. A software Authentic rasterizer executes that private stream against two
   PC-98-like 640x400 indexed VRAM pages, the active
   400-line/zoomed-200-line display mode, palette state, TRAM state, and the
   scroll register.
4. The stable rendered artifact is the indexed display state, not the command
   stream: visible indexed pixels, 16-color palette, TRAM layer, and display
   metadata. A host compositor expands this to RGBA only after canonical
   rendering and hashing.

This resolves the framebuffer-versus-command-stream question without putting a
framebuffer, draw vocabulary, or renderer-shaped event queue in the simulation,
and without making low-level PC-98 operations the future Arrange API. A
framebuffer alone cannot express page-copy, masked-write, palette, TRAM, or
scroll ordering. A command stream alone would make every host renderer
reimplement obscure PC-98 composition semantics and would be a poor visual
oracle. The hybrid keeps the ordered stream private to the Authentic
implementation and exposes a canonical completed indexed frame.

This is a presentation contract, not a claim of pixel parity. All behavior
below is either directly present in the pinned source, already established by
the Phase 3 parity ledgers, or explicitly marked as a proposed native rule or
an open decision.

## Authority, scope, and terminology

All ReC98 paths and symbols in this document refer to the clean Git object for
Anniversary commit `3e2381d4275be7287f5525de5c95389d492b0a3c`, independent of any local
worktree path. The native evidence base is
`th03-port/docs/PARITY_SOURCE_COVERAGE.md`,
`th03-port/docs/PARITY_RENDER_PHASE.md`, and the two character parity ledgers.

In this document:

- **core state** means data whose value can affect later Classic mechanics,
  control flow, score, collision, replay-visible RNG, or canonical hashes.
- **presentation state** means persistent data whose only observable result is
  pixels, palette, TRAM, scroll, audio intent, or scene pacing.
- **late-phase core work** means gameplay mutations or shared-RNG work performed
  by a source function named or scheduled as rendering. It remains in
  `th03_core` because its effect is gameplay-deterministic, not because the core
  models rendering.
- **gameplay observation** means a borrowed, immutable report of a gameplay fact
  at a semantic phase boundary. It is neither a draw command nor persistent
  core state.
- **Authentic** means intended complete logical frames from the Anniversary
  authority. It does not mean hardware-dependent tearing, CPU slowdown, disk
  latency, or a particular emulator's timing.
- **Arrange** is a future rules/content/presentation identity. This document
  reserves a clean seam for it but does not speculate about its mechanics.

“Presentation-only” therefore never means “delete the entire original
function.” Several original render callbacks contain deterministic mutations;
the existing core correctly retains those mutations while omitting their draw
payload.

## Ownership and dependency contract

The required dependency direction is:

```text
replay / platform input
          |
          v
      th03_core ---- optional neutral observations ----> th03_present
          ^                  (synchronous only)              |
          |                                                  +--> th03_assets
 application phase machine                                  |
          |                                                  v
          +---- audio service                     canonical indexed frame
          +---- clock / host pacing                         |
                                                             v
                                                       host compositor
```

The boundaries are normative:

- `th03_core` owns all Classic gameplay updates, fixed-width arithmetic,
  collision, scoring, CPU decisions, result control, shared RNG state and call
  order, and gameplay-affecting writes hidden in source render callbacks.
- `th03_core` must not import `th03_present`, store a render buffer, page,
  palette, TRAM cell, sprite/cel state, asset ID, draw command, or persistent
  presentation event queue. It also cannot resolve paths, read a clock, wait
  for VSync, or make graphics/audio API calls.
- Observation is optional. A null observer and a real observer must produce
  byte-identical canonical core state, RNG state, input consumption, and
  `StepResult`. `th03rv` supplies no observer and links neither `th03_present`
  nor `th03_assets`.
- `th03_present` owns all persistent visual entities, VRAM/page state, palette
  and tone, TRAM and gaiji state, scroll, scene animation cursors, decoded
  visual assets, presentation records, and Authentic drawing rules. It turns
  neutral observations into its own visual intents and commands.
- The application owns OP/MAIN/MAINL-equivalent scene progression, input
  policy, frame pacing, file/configuration services, and the explicit resident
  handoff between scenes and core sessions.
- Audio is a sibling consumer, not part of the rasterizer. It may map neutral
  gameplay observations or application scene actions to sound cues, but sound
  IDs and playback state do not enter the core and playback must not control
  core advancement.
- `th03_assets` and `th03_present` are sibling libraries in this repository.
  The asset library decodes caller-owned byte spans into immutable models;
  presentation refers to those models through presenter-owned logical IDs.
  Original files and filenames do not enter core state, hashes, observations,
  or replay documents.
- Authentic and future Arrange presenters may consume the same neutral
  gameplay facts where meanings agree. Arrange-specific mechanics require a
  separate rules contract; they must not appear as conditionals inside Classic
  simulation.

## Proposed optional observation seam

The exact C++ spelling can change, but these concepts and ownership rules
should not.

```cpp
namespace th03 {

using CoreFrame = std::uint32_t;

struct PhaseObservation {
    CoreFrame core_frame;
    GameplayPhase phase;
    GameplayView facts; // Borrowed, immutable, valid only during observe().
};

struct CompatibilityReadObservation {
    CoreFrame core_frame;
    GameplayPhase phase;
    CompatibilityReadSite site;
    std::uint16_t ordinal;
    std::uint16_t raw_value;
};

using GameplayObservation = std::variant<
    PhaseObservation,
    GameplayFact,
    CompatibilityReadObservation
>;

class GameplayObserver {
public:
    virtual void observe(const GameplayObservation&) = 0;
};

enum class PresentationProfile : std::uint8_t {
    authentic,
    arrange, // Reserved identity, not an implemented ruleset.
};

struct IndexedDisplayState {
    IndexedFrame graphics;     // 640x400 shown graphics layer, indices 0..15.
    Palette16 palette;         // Exact 16-entry logical DAC state.
    TramSnapshot tram;         // Character/attribute cells and gaiji state.
    std::uint16_t scroll_y;    // Logical display start/scroll value.
    std::uint8_t shown_page;
    RasterMode raster_mode;    // 400-line or source-compatible 200-line window.
    std::uint64_t present_id;
};

class Presenter {
public:
    void begin_scene(const SceneInit&, const AssetProvider&);
    GameplayObserver& gameplay_observer();
    void tick(const SceneInput&); // OP/MAINL presentation-only scenes.
    IndexedDisplayState display_state() const;
};

} // namespace th03
```

`GameplayPhase` names semantic sequence points in the deterministic simulation,
not source draw calls. Examples include after player updates, after a
late-collision pass, after a gameplay-affecting callback mutation, and after
result routing. `GameplayView` exposes only the raw fixed-width gameplay facts
valid at that point. It has no sprite cel chosen solely for display, palette,
asset handle, page selector, clip, or draw opcode. An end-of-step view alone is
insufficient because source callbacks can mutate gameplay at later positions
and an early result return can omit the rest of the frame. The presenter uses
the semantic observations to advance its own source-backed private schedule.

Observation is synchronous. `Game::step()` accepts an optional observer (or an
equivalent non-owning callback), invokes it in deterministic order, and retains
nothing after each invocation. The observer must not mutate `Game`, influence
control flow, or retain borrowed spans. Snapshot/restore therefore has no
pending presentation data to copy, clear, or deduplicate. If an asynchronous
client wants a record, that client copies observations into storage it owns.

Creation, first-round initialization, and repeat-round initialization can also
consume RNG whose source use was visual (`sub_D092()` / `sub_D2E8()`) but whose
shared-stream outflow affects later gameplay. The core consumes those values
whether an observer exists or not. An attached observer sees the primitive
results immediately with only a source-site/phase tag and ordinal; with no
observer they are discarded. The core never delays them into the first gameplay
frame and never stores the corresponding particles.

## Observation vocabulary and the RNG firewall

The core must remain the only consumer of gameplay/shared RNG. Presentation
must never call `IRAND`, `randring`, `randring2`, or a native substitute using
the gameplay seed or ring cursors. If source presentation code consumes shared
RNG, core consumes it at the exact source position. If an observer is attached,
the core reports the already-consumed primitive result under a minimal
source-site/phase tag; observation never causes or suppresses a draw.

This requires a small amount of **deterministic compatibility-schedule state**
in core even when the corresponding particles are presenter-owned. This is an
irreducible property of the original shared RNG stream: omitting a value that
was drawn only for pixels changes later gameplay RNG results. The current port
already has the correct shape: `background_initial_kind` / `background_phase` and
`hud_start_flag` / `hud_start_state` / `hud_start_frame` in
`th03-port/src/core/game_internal.hpp`, with the source-timed consumption in
`src/core/frame/round.inl`. `background_phase` is canonical because the
initial-to-steady callback switch changes future shared-RNG consumption. These
fields schedule deterministic reads; they do not store particle positions,
cels, asset IDs, pixels, or any other render state. Keep only that minimal
schedule in core, and let the presenter derive its richer visual lifecycle from
observations. The same rule applies to any future minimal cadence field proven
necessary to schedule shared RNG.

The gameplay vocabulary should stay small and semantic: for example,
`CollisionResolved`, `AttackStateChanged`, and `GameplayPhaseChanged`. Their
payloads describe mechanics (participants, PID, raw gameplay coordinates,
damage, attack state, route, winner, and stage), never a visual object to create
or a command to execute.

Compatibility observations are deliberately less interpreted. Each contains
only `core_frame`, `GameplayPhase`, a stable `CompatibilityReadSite`, the
source-order ordinal, and the primitive result already returned by the shared
RNG function. `th03_present` combines that sequence with semantic gameplay
views and owns all grouping into particles, offsets, angles, cels, or effects.
The initial site/count ledger is:

| Compatibility read site | Primitive sequence per source invocation | Exact source seam |
|---|---|---|
| `hitcircles_enemy_add` / `hitcircles_player_add` | two raw results when random offsets are enabled | `hitcircles_enemy_add()` / `hitcircles_player_add()` in `th03/main/hitcirc.cpp` |
| `d092_initialize` | 192 raw IRAND results in source order | `sub_D092()` in `th03/main/player/d3f9.cpp` |
| `d135_step` | 48 raw IRAND results on every early callback frame through the `round_frame > 128` switch | `sub_D135()` in `th03/main/player/d3f9.cpp`, then non-RNG `sub_D1E7()` |
| `d2e8_initialize` | 100 raw IRAND results in source order | `sub_D2E8()` in `th03/main/player/d3f9.cpp` |
| `d340_step` | 40 raw IRAND results on every early callback frame through the `round_frame > 144` switch | `sub_D340()` in `th03/main/player/d3f9.cpp`, then non-RNG `sub_D3F9()` |
| `ce5b` | one raw ring result per call | `SUB_CE5B()` in `th03/main/player/d3f9.cpp` |
| `bb12_randring` / `bb12_irand` | 256 raw ring and 256 raw IRAND results, respectively, in source order | `sub_BB12()` in `th03/main/player/bb12.cpp` |
| one stable tag per audited character callback call site | the exact raw results and order detailed in the complete seam ledger below | callbacks in `ch_shot.cpp`, `mbomb.cpp`, `chybomb.cpp`, `main_05.cpp`, and `yumemi_bomb.cpp` |

Do not add a core-side particle/offset/angle union or decode these values into a
visual payload. The site tag exists only to preserve the unavoidable source
read schedule. Palette edits, sound cue IDs, fixed effect spawns, sprite choices
derived without shared RNG, and all other presentation policy are generated
inside `th03_present` or its sibling audio adapter from semantic facts; they are
not core observations.

Presentation-owned scenes may have a separate `PresentRng`. Its seed must be
explicit in `SceneInit`; it must not alias core RNG. Two source cases need
special treatment:

- OP title/menu/selection waits increment `resident->rand` at
  `th03/op_01.cpp:336-349`, `th03/op_01.cpp:757-776`, and
  `th03/op/m_select.cpp:650-675`. `round_startup()` later copies that resident
  value into gameplay RNG. The application must count those emulated logical
  waits and hand the exact seed to `GameInit`; wall time and host refresh rate
  are forbidden inputs.
- Staff-roll flakes seed the shared source variable from `resident->rand` in
  `staffroll_and_verdict_animate()` (`th03/mainl/stf_drv.cpp`). Native code may
  use a scene-local, source-compatible RNG seeded by the explicit resident
  value because no later gameplay consumes its outflow. The seed handoff still
  needs a regression test.

## Authentic raster state and private draw stream

### Persistent state

The Authentic presenter needs the following explicit state:

- Two full 640x400 indexed graphics-page backings. An implementation may store
  planar bytes internally to make GRCG/EGC behavior direct, but their
  normalized diagnostic form is one byte per pixel with values 0 through 15.
- The GDC display mode/window. OP and MAINL use the 400-line surface; MAIN calls
  `graph_200line(0)` (`th03/main/rstart.cpp`, implementation and parameter
  meaning in `libs/master.lib/graph_200line.asm`) to select the upper 200-line
  half with 2x vertical zoom. The displayed artifact remains 640x400, while
  off-display rows in the raw page remain available as sprite/source storage.
- Separate access-page and shown-page selectors. Source code routinely draws
  to one page while showing the other, copies pages, and uses a hidden page as
  a sprite or unblit source.
- The current 16-entry palette, pending logical palette edits, `PaletteTone`,
  and a dirty/deferred-apply flag corresponding to `palette_changed` from
  `th03/hardware/palette_changed[bss].asm` and
  `th03/hardware/palette.hpp`.
- The 80x25 TRAM character and attribute cells, the gaiji/font state used by
  menu text, and text-plane visibility (`RES_X / GLYPH_W` by
  `RES_Y / GLYPH_H`, as used by `th03/main/hud/static.cpp`). Preserve the source-defined raw cell and
  attribute values; do not freeze a guessed host encoding into the ABI before
  the font/attribute audit.
- The vertical display scroll value used by `graph_scrollup()`, including the
  even-iteration `RES_Y - 4` / odd-iteration 4 shake and final zero in
  `script_op_shake()` (`th03/formats/script.hpp`).
- Current clipping bounds and enough GRCG/EGC mode state to reproduce source
  writes. Mode state can remain rasterizer-private unless a source operation
  deliberately leaves it active across calls.
- Scene clock/present ID, exact page-local animation state, and decoded asset
  slots. Presentation time advances only when the application advances a
  logical scene frame.

The host-visible RGBA texture, scale, aspect correction, filters, window size,
and monitor refresh rate are not Authentic state and are never hashed.

### Implemented planar and TRAM foundation

The portable foundation now fixes the source-backed byte semantics needed to
build the first static MAIN frame without pretending to emulate the whole EGC:

- packed graphics planes are ordered B, R, G, E, corresponding to color-index
  bits 0, 1, 2, and 3; bit 7 is the leftmost pixel in every packed byte. This
  comes directly from `planar.h`, the plane segments in `pc98.h`, the B/R/G/I
  tile-register write order in `libs/master.lib/grcg_setcolor.asm` and
  `grcg_settile_1line.asm`, and the `0x80 >> (x & 7)` point mask in
  `grcg_pset.asm`;
- `Raster` accepts caller-owned B/R/G/E spans with explicit top-to-bottom or
  bottom-to-top row order and implements direct plane replacement, per-plane
  OR, and positive-alpha masked replacement. These are the portable logical
  results of the direct planar macros in `planar.h` and SPRITE16's source-
  documented `OVERLAP_OR` / default clear behavior. The explicit reverse-row
  option is the direct join for source-format CDG/CD2 cels;
- the byte-aligned GRCG primitive implements both Tile Direct Write, where CPU
  data is ignored, and Read-Modify-Write, where set CPU bits replace all four
  destination plane bits with the corresponding tile bits. Arbitrary tile
  bytes and source `grcg_setcolor()` solid tiles use the same B/R/G/E order;
- all planar writes clip to the 640x400 backing after validating every span,
  and invalid inputs perform no partial writes. A plane-projection accessor
  reconstructs exact packed bytes from the indexed backing for byte-level
  diagnostics;
- raster mode explicitly distinguishes the normal 400-line surface from the
  upper and lower zoomed 200-line windows. Selecting either 200-line value
  also selects access page 0, exactly as `graph_200line()` does;
- `PaletteTone` remains a signed raw source word. Applying pending logical
  RGB4 entries clamps only a local tone copy to 0..200, scales toward black
  for 0..100 and toward white for 101..200 with `palette_show()`'s exact
  integer division, and leaves the raw tone unchanged;
- TRAM is stored separately as 80x25 raw 16-bit code/attribute cells. The raw
  attribute constants retain the exact `TX_*` values from
  `libs/master.lib/pc98_gfx.hpp`; `text_fillca.asm` and `text_boxfilla.asm`
  establish the row-major cell and attribute layout;
- all 256 gaiji slots retain the exact 32-byte format used by
  `gaiji_write.asm`: left byte then right byte for each of 16 rows, with bit 7
  leftmost. `gaiji_putca.asm` establishes the two raw TRAM codes for an ID:
  low byte `0x56 + (id >> 7)`, high byte `(id & 0x7F)` for the left cell and
  that high byte OR `0x80` for the right cell; and
- the TRAM diagnostic hash includes raw cells, visibility, and only gaiji
  slots referenced by complete two-cell gaiji pairs. Raw `0x0056`/`0x0057`
  remain ANK V/W unless followed by the matching right half. The hash remains
  independent of the graphics hash; and
- the host composer overlays visible TRAM after a caller-resolved graphics
  frame into a caller-owned reusable output span, using an injected ANK
  provider and source-format gaiji rows. It allocates no per-frame buffer,
  keeps fixed three-bit text colors distinct from the programmable graphics
  palette, and implements display, color, reverse, and text-over-graphics
  priority.

TRAM source calls require valid coordinates and do not define clipping, so the
portable API rejects an out-of-range cell or rectangle atomically. JIS font-ROM
rasterization, blink cadence, underline rasterization, and general EGC
register/ROP emulation remain explicit errors or deferred operations until
their complete source paths and visual oracle are in hand. Original font-ROM
bytes are injected by the application rather than embedded in the library.

The cel compositor now also ports the stock byte-aligned CDG alpha/no-alpha,
horizontal-flip, E-plane dissolve, and upward-unput paths plus MRS alpha,
no-alpha, altered-color, and persistent-flip semantics. In particular,
`mrs_put_8()` clears through individual alpha bits but tests alpha once per
32-pixel dword before ORing that complete B/R/G/E group. Synthetic coverage
distinguishes this from conventional per-pixel alpha composition.

Caller-supplied monochrome tables now have source-specific operations rather
than a generic sprite abstraction: the score digit blitter consumes stored
rows 0 through 6 bottom-up and leaves the eighth stored byte unread; pellets
select one of 16 pre-shifted variants and preserve the source's signed wrapped
VRAM-offset choice between 16- and 32-pixel writes; and staff flakes rotate a
little-endian 16-bit row by `left & 7` before ORing only the B plane. The
library defines the required layouts and validates synthetic masks, but embeds
none of the original or reconstructed bitmap content.

Decoded PI presentation covers `pi_put_8()`'s 8-pixel X/width floor,
horizontal clip, and 400-line row wrap, plus the fully defined TH03 cutscene
surface path: select one clockwise 320x200 quarter from a 640x400 PI, put it at
(160,64), or reveal it through the four cumulative `PI_MASKS` steps before the
final copy. This is grounded in `th02/formats/pi_put.cpp`,
`libs/master.lib/graph_pack_put_8.asm`, `game/cutscene.cpp`,
`th03/sprites/pi_mask.cpp`, and `GrpSurface_M4::or_masked()` in
`platform/x86real/pc98/grp_surf.cpp`. Legacy `pi_put_quarter_8()` remains
deferred because the pinned tree retains its declaration/comment but not its
implementation body; the native code does not guess it.

### Implemented static MAIN transaction

`present/main_static.hpp` now expresses the smallest source-backed part of
MAIN that needs no core observer. `MainHudState` is presenter-owned copied
input: mode and story lives, both players' halfhearts/bombs/rounds won/gauge
levels, and the shared Boss Attack level. It imports no core type and stores no
canonical gameplay state. The implementation mechanically reproduces these
Anniversary operations:

- `graph_copy_page(0)` copies the complete opposite page, page 1, to page 0
  and leaves page 0 accessed. This is established by both the function's
  documented destination parameter and the alternating access writes in
  `libs/master.lib/graph_copy_page.asm`; it is not a page-0-to-page-1 copy.
- `hud_wipe()` fills all 80x25 raw TRAM cells with ANK space and
  `TX_BLACK | TX_REVERSE`, then changes only the attributes in each 36x24
  playfield rectangle (P1 X 2..37, P2 X 42..77, Y 1..24) to `TX_WHITE`.
- `hud_static_put()` retains the exact source loop directions, trailing blank
  gaiji after each bomb run, story-only life row, gauge-edge positions and
  green/red attributes. Named constants retain the source gaiji IDs for
  yin-yang, empty/full/half heart, bomb, Thalia, proportional digit 1, and
  gaiji space. Portable validation rejects only state whose intended gameplay
  ranges or resulting source TRAM coordinates are unsupported, before writing
  any cell.
- The decoded resource seam takes the two character BF2 sheets in the same
  P1-then-P2 load order as `set_callbacks_*()` / `sub_A4C3()`. Each must be the
  exact 32x32, codes-0-through-8 superimpose layout. `sub_E266()` does not draw
  the later LOSE/ROUND/ZIKICW resources: its global pattern IDs
  0,3,6,9,12,15 select cels 0/3/6 from those first two character sheets. The
  six cels are transparently put at X 544/576/608 and Y 328/360 to page 1 and
  then page 0. The mask is derived exactly as “decoded color differs from the
  BFNT transparent color”; filenames and filesystem policy remain outside the
  presenter.
- At the stable startup boundary, the applied RGB4 palette handed in from the
  preceding scene is copied back to pending state and applied. This is the
  final logical effect of `SUB_A38E()` after the intervening BF2 loads; there
  is no wait/present point at which their temporary logical palette is
  observable. Palette application uses the exact current `PaletteTone`.
- `Sprite16Staging` then mirrors `sprite16_sprites_commit()`: after
  `graph_200line(0)` it copies page 0's complete lower 640x200 source atlas
  (raw rows 200..399) to page 1, restores access page 0, and derives the packed
  positive alpha mask by ORing B/R/G/E occupancy. The negative mask is its
  exact complement. MRS resources remain separately allocated and never enter
  this atlas. The presenter owns and reuses both fixed 16,000-byte masks; no
  per-frame allocation or original asset byte is introduced.
- Prepared superimpose sheets mirror `super_entry_bfnt()` ownership rather
  than borrowing decoded asset storage. Preparation validates the source's
  byte-sized row stride and height plus the 512-pattern limit, then owns packed
  copies of B/R/G/E and the derived positive opacity plane. `superimpose_put()`
  is allocation-free, replaces every nontransparent pixel on the current
  access page, and preserves transparent destination pixels.
- The native Sprite16 operations accept semantic atlas rectangles rather than
  source byte offsets. They implement TH03's stock clear and per-plane OR
  overlap, monochrome color, the source-persistent 16-bit EGC write mask's
  effective value, unaligned EGC shift, signed 400-line-to-raw-row conversion,
  driver-level vertical clipping, and the wrapper's complete-16-pixel-column
  horizontal clipping. The source driver keeps overlap, mono color, and write
  mask as persistent state; the native API instead supplies their effective
  snapshot explicitly per operation, avoiding hidden mutable renderer state.
  The no-clip path retains 16-bit VRAM address wrapping when it lands in the
  modeled 640x400 plane; writes into the remainder of the hardware plane are
  discarded as a safety normalization for stock-invalid coordinates.
- The only ported extended put operation is the source-used downwards column:
  it horizontally clips once, repeats the same atlas rectangle by its raw
  height, and lets each copy clip against raw rows 0..199. Driver active-plane
  selection, custom ROPs, and other advertised SPRITE16 operations remain out
  of scope because TH03 has no call sites for them.
- Startup establishes source roles `page_back = 0`, `page_front = 1`, the
  upper zoomed 200-line window, access page 0, and shown page 1. The late-frame
  transaction accesses the old front,
  shows the old back, assigns that old back to `page_front`, toggles
  `page_back`, and clears only the newly accessed page: P1 X 16..303 to color
  0 and P2 X 336..623 to color 1, both for every row Y 0..183 inclusive.

Synthetic warning-clean tests cover raw HUD cells, the selected BFNT pattern
IDs and transparency, the full page-1-to-page-0 startup copy, final palette and
selectors, and two consecutive end-frame swaps/clears with exact edge pixels.
Focused Sprite16 tests additionally cover clear/OR/mono and write-mask output,
unaligned placement, whole-column and raw-row clipping, access-page isolation,
no-clip address wrapping, and the downwards-column endpoint.

`present/main_entities.hpp` is the first synchronous observer-backed MAIN
consumer. It reads only the neutral `ShotsEnemiesView`, `FireballsView`, and
`ShotPairSpawned` fact, owns all 32 private shot animation cursors, and draws
shots before enemies and post-hittest fireballs in exact ascending source slot
order. Its injected semantic atlas layout names shot cels, living-enemy family
anchors, explosion cels, and both fireball families; no asset path, source byte
offset, or original pixel enters core. Focused tests cover slot reuse, damage
strides across atlas rows, family mismatch rejection, source keyframes,
playfield clips, and both compilers.

These tests deliberately do not claim captured pixel parity. The remaining
static-frame prerequisites are explicit: the application must inject/load the
decoded GAMEFT gaiji glyphs before startup; the remaining PI-staged atlas
content and clip metadata are not modeled; and both pre-first-frame pages
still require comparison against a raw Anniversary capture. Injected ANK and gaiji host
composition is implemented; JIS, blink, and underline remain explicit deferred
paths rather than guessed behavior.

### Ordered operations

The private `AuthenticDrawOp` stream should contain only operations needed by
the source and diagnostics, for example:

```text
SetRasterMode, SetAccessPage, SetShownPage, CopyPage, ClearPage
SetClip, SetGrcgMode, SetEgcMode
PutPlanarSprite, PutSprite16, PutBFNT, PutGaiji
BlitPI, PutCDG, PutMRS, PutBF2
Line, Circle, Polygon, Trapezoid, RectFill, MaskedCopy
TramWrite, TramFill, TramClear, GaijiLoad
PaletteEntryWrite, PaletteLoad, SetTone, ApplyPalette
SetScroll, EmitAudioCue, Present
```

These names are design-level categories, not a promise to mirror master.lib's
function-per-operation API. The rasterizer must preserve overlapping copies,
plane masks, clipping, transparent colors, and page-local source reads where
they affect pixels. Commands carry stable asset handles and copied values, not
pointers into core or decoded asset storage.

`Present` marks an observable logical display state. It does not sleep. The
application decides when to show that state; a headless visual test can execute
the same command sequence without a window or clock.

### Canonical visual hash

For an initial oracle, hash fields explicitly in this order:

1. schema/version and logical scene/present ID;
2. shown-page ID, raster mode/window, and shown 640x400 graphics-layer indices;
3. all 16 logical palette entries and applied tone;
4. raw TRAM cells/attributes and referenced gaiji glyph contents;
5. scroll value and text/graphics visibility flags.

Also retain optional diagnostics for both graphics pages, access-page ID,
pending palette state, and the ordered draw-op trace. Those are valuable for
first-divergence localization but should not be confused with what the player
can see. RGBA output is deliberately not canonical because scaling, output
color space, and shader behavior are host policy.

## Exact MAIN startup and frame ordering

### Round startup

`round_startup()` in `th03/main/rstart.cpp` establishes more than gameplay
state. The Authentic application/presenter must preserve this source order:

1. Establish the round RNG seed and gameplay/round state.
2. Fill TRAM, then copy the complete graphics page 1 to destination page 0;
   `graph_copy_page(0)` leaves page 0 accessed.
3. Load formations and initialize the gameplay subsystems.
4. Character callback setup has already loaded two 9-cel character BF2 sheets
   as global superimpose patterns 0..17. Load the three additional
   LOSE/ROUND/ZIKICW BF2 resources used later by shots/results.
5. Draw the shot sprite source sheet to both VRAM pages through `sub_E266()`
   (`th03/main/player/shot.cpp:87-103`). Its IDs 0,3,6,9,12,15 select cels
   0/3/6 from the two character sheets, not the three resources in step 4.
6. Restore the gameplay palette through `SUB_A38E()`, select the upper
   zoomed-200-line mode through `graph_200line(0)`, and commit the sprite
   resources. `graph_200line()` also establishes access page 0 in master.lib.
7. Set `page_back = 0`, `page_front = 1`, access page 0, and show page 1
   (`th03/main/rstart.cpp:237-240`).
8. Reset sound state, wipe/put the HUD through `hud_wipe()` and
   `hud_static_put()`, establish clipping, then start the round song.

Page 1 is not disposable after the stage splash: source code also uses hidden
page content as sprite/unblit material. A native scene transition must hand the
prepared page state to MAIN presentation rather than reconstructing only the
visible image from assumptions.

### Gameplay frame

`sub_9778()` in `th03/main/rndloop.cpp:79-310` is the governing schedule. The
gameplay work in the update prefix remains owned by `th03_core`, but the prefix
also contains presenter ticks and mixed callbacks. Its exact order is:

1. Reset the collision map and shared bullet-template scratch state.
2. Select P1 and run P1 Extra update, Boss update, and Bomb update callback.
3. Select P2 and run P2 Extra update, Boss update, and Bomb update callback.
4. Run `hitcircles_update()`; the presenter advances its 13 visual records from
   the frame/phase observation and any preceding collision/random facts.
5. Run `shots_update()`.
6. Select P1 and run its Charge update, then select P2 and run its Charge
   update.
7. Run `sub_BB12()`: core advances that source site's minimal compatibility
   RNG cadence and reports raw already-consumed results, while the presenter
   advances its own banner/particle state.
8. Update enemy formations, bullets, enemies, then fireballs.
9. Select P1 and run its pellet Gauge callback then bullet Gauge callback;
   repeat for P2. Gameplay facts and already-consumed random values are
   observed at these positions; visual effects are created by the presenter.
10. Run common effect-pool update `SUB_CEB2()` in presenter-owned state at this
    source position.
11. Poll/map gameplay input and perform T3CASE frame I/O at the existing adapter
    boundary.
12. Update P1, then P2.

After player update, the observable late phase is exactly:

1. `snd_se_update()`.
2. Pause handling through `sub_C7A5()` if Cancel is active.
3. Global warning prelude `SUB_CA3C()`.
4. Per-player warning update `SUB_CB81(0)`, then `SUB_CB81(1)`.
5. Installed background callback `farfp_20F24()`.
6. If `byte_23AFA` requests the unused alternate cadence, even frames skip the
   draw pass and jump directly to counters.
7. Enable EGC.
8. P1 Bomb callback, then P2 Bomb callback.
9. P1 Boss renderer, then P2 Boss renderer.
10. `shots_render()`.
11. `enemies_render()`.
12. `fireballs_hittest_and_render()`; its hit testing remains core work at this
    exact schedule position.
13. `hitcircles_render()`.
14. P1 Extra renderer then Charge renderer; then P2 Extra renderer and Charge
    renderer.
15. P1 player renderer, then P2 player renderer.
16. Disable EGC.
17. P1 overlay renderer, then P2 overlay renderer.
18. Effect-pool renderer `SUB_CEE0()`.
19. `bullets_render()`.
20. Defeat/result banner and route control: `sub_C2F9()` and, where selected,
    `sub_E3F2()`. Any route/score/return mutation stays in core at this point.
21. Gauge overlays `sub_C830()` and `sub_C8C4()`.
22. Score/extend rendering `sub_D52E()`.
23. Warning portraits `SUB_C9FE(0)`, then `SUB_C9FE(1)`.
24. HUD-start animation `sub_BE5D()`.
25. `combos_update_and_render()`.
26. Result trapezoid callback `fp_1FBC0()`.
27. Demo/round-mode callback `fp_1E6EA()`.
28. Perform source pacing. `byte_23AFA == 0` waits for one unit; the alternate
    path waits only on even frames and requests two units.
29. If `palette_changed`, call `palette_show()` and clear the flag.
30. Access `page_front`; show `page_back`; assign the return value to
    `page_front`; toggle `page_back`.
31. Starting at Y=183 in the newly accessed page, use
    `playfield_rows_fill_288()` to fill the 288-pixel-wide P1 playfield on every
    row back through Y=0 with GRCG color 0; repeat for P2 with color 1; turn
    GRCG off. The `288` names the horizontal width, not a row count
    (`th03/main/hitc_sfx.asm`).
32. Increment round/result counters and perform the 64/1024-frame difficulty,
    ring-refill, damage, and CPU-frame cadence in core.

This ordering has two consequences for the API. First, one `Game::step()` must
offer synchronous semantic phase observations rather than only a single
end-of-step snapshot. The presenter advances its private schedule around
late-phase core work and stops when an early return omits later observations;
the core still owns neither a draw slot nor an output batch. Second, the renderer must draw
the completed frame into the current back page, apply a deferred palette
change, swap show/access roles, and only then clear the next drawing page's
playfield bottoms. Clearing a generic framebuffer before drawing is not
equivalent.

### Warning timing and palette/TRAM interaction

`SUB_CA3C()` in `th03/main/player/warning.cpp` performs the global warning
prelude: it starts sound effect 19, redraws the gauge levels, writes the warning
text to TRAM in white, and performs exactly 27 logical waits while alternating
palette tone 100 and 150. Those waits consume interstitial input in the replay
oracle but do not advance gameplay.

`SUB_CB81(pid)` in `th03/main/player/d3f9.cpp` owns each player's 30-frame warning palette/text transition and
the steady gauge-palette oscillators `byte_20E98[pid]` and
`byte_20E9A[pid]` from
`th03/main/hitbox_defeat_rikako_bss.asm`. `SUB_C9FE(pid)` in
`th03/main/player/warning.cpp` draws the MRS warning portrait and returns that
side to the white warning state. Palette edits set `palette_changed`; they are
applied in the single late-frame slot above, not eagerly by the host.

TRAM is a concurrent visual layer, not text rasterized permanently into the
graphics page. Warning text and OP menu gaiji must remain independently
addressable until the source clears or overwrites them.

## Exact OP, MAINL, cutscene, high-score, and staff ordering

These scenes live outside deterministic gameplay, but they are inside the
Authentic product. They belong to the application plus `th03_present`, not to
`Game`.

### OP title and menus

`op_animate()` in `th03/op/m_main.cpp`:

- loads `TL01.PI` to both pages, pre-shifts page 0 by 2 pixels, sets the PI
  palette, and begins with page 1 shown;
- on each scroll frame waits once, updates the palette/tone, shows the prepared
  `page`, toggles `page`, switches access to that other page, then shifts that
  other page by 4 pixels to prepare it for its next display;
- runs the explicit 100/200 tone flashes; and
- switches to the `TL02.PI` menu background on page 0 in the source order.

The menu implementation in `th03/op_01.cpp` combines TRAM gaiji labels with a
graphics-page window animation. It clears TRAM before the transitions that
require it. Each emulated wait/menu loop frame increments `resident->rand` at
the source sites cited in the RNG section; the native loop must increment by
logical scene frames, never by elapsed host time.

### Character selection

`select_init_and_load()` and the selection loop in
`th03/op/m_select.cpp:155-675` load the four CDG groups and `TLSL.RGB`, then use
double buffering. Each active loop iteration:

1. clears the currently accessed page;
2. updates/renders the animated curves;
3. draws portraits, stats, names, extra labels, and cursors;
4. processes selection input/confirmation;
5. waits for the logical frame;
6. selects the previously shown page for access, toggles and shows the completed
   page; and
7. advances the fade/tone and `resident->rand` state.

This Anniversary order is authoritative. Do not restore an older page-flip
order that exposes a partially drawn selection frame.

### Music room

`th03/op_music.cpp` includes the shared implementation in
`th02/op/m_music.cpp`. The scene keeps a hidden-page/background snapshot,
draws the polygon field on the B plane, performs `frame_delay(1)`, then shows
the accessed page and toggles access. Text/comment changes update both pages
and refresh the saved B-plane background. Polygon RNG is presentation-local;
its exact scene seed and call order must be captured before implementation.

### Win screen and stage splash

`win_animate_and_wait()` in `th03/mainl/screens.cpp:120-169`:

1. clears page 1, clears page 0, and shows page 0;
2. applies tone 0 and the initial palette;
3. draws the win graphics/logo, starts the song, and black-fades in;
4. advances the timed logo/portrait cels;
5. applies tone 200, draws the portrait/text/logo state, switches to
   `logo1.rgb`, and white-fades in;
6. waits for input, then black-fades out.

`stage_splash_load()` and `stage_splash_show_and_wait()` in
`th03/mainl/screens.cpp:226-353`:

1. show page 0 and access page 1 while loading the portrait and PI resources;
2. set tone 0, show the palette, apply the PI palette, and copy the prepared
   page to page 0;
3. draw the two portraits, stage number, and text, then black-fade in;
4. clear/access page 1 and build the side-shot / center-opponent source image;
5. load BGM/effect resources and perform the scripted waits;
6. white-fade out, access and clear page 0, white-fade in, clear TRAM, reapply
   the PI palette, and restore the resident palette state used by MAIN.

The page-1 source image built here is part of the handoff to the following MAIN
presentation; this is another reason the contract retains both pages.

### Cutscenes and continue

The TH03 cutscene engine is `th03/cutscene/cutscene.cpp`; story/ending entry is
in `th03/mainl/ending.cpp`, and the continue scene is
`th03/cutscene/continue.cpp`.

- The engine directly draws each script glyph to visible page 0 and waits the
  configured per-glyph interval unless fast-forwarding.
- Text-box backgrounds are snapped/restored using page 1 and masked EGC copies
  (`box_bg_snap()`, `box_bg_put()`, `box_1_to_0_masked()`, and
  `box_1_to_0_animate()`).
- Script operations explicitly choose access/show pages, load/apply PI
  palettes, copy pages, set tone, wait, and fade. Preserve their command order;
  do not flatten a script line into a precomposed still.
- `script_op_shake()` starts with `RES_Y - 4` at iteration 0, uses 4 on odd
  iterations, continues for an inclusive `0..count` loop, waits one frame per
  position unless fast-forward, then resets scroll to 0.
- Continue uses its own PI/CD2 assets, palette/tone progression, input loop,
  and route result. Its route belongs to the application phase machine; its
  animation state belongs to presentation.

### High-score registration

`regist_menu()` and helpers in `th03/hiscore/regist.cpp` are a mixed
application/presentation screen:

- `regist_load_and_put_initial_both()` sets tone 0, accesses/shows page 0,
  loads `regib.pi`, draws the rank CDG, loads `regi2.bft` and `regi1.bft`, then
  copies page 0 to page 1 and returns to access page 0.
- `regist_rows_unput_and_put()` copies page 0 to page 1 before redrawing every
  row. `regi_unput()` reads planar rows from page 1 and restores them to page 0;
  the hidden page is an exact unblit source.
- The alphabet/name cursor rerenders only after input changes and then waits
  one logical frame. Name/score persistence is an application service; glyph
  layout and page restoration are presentation.
- The source's conditional fade and its preloading of Continue/Game Over assets
  are observable transition behavior, not core behavior.

### Staff roll

`staffroll_and_verdict_animate()` in `th03/mainl/stf_drv.cpp`,
`staffroll_blue_plane_clear()` / `staffroll_flakes_tick()` in
`th03/mainl/stf_bclr.cpp`, and the CDG operations in
`th03/mainl/stf_cdg.cpp` form a double-buffered scene:

- initialize palette and both pages, seed/reset/spawn flakes, and start with
  explicit page/tone state;
- for each logical frame, clear the appropriate B plane, execute any current
  CDG dissolve/slide/overlay operation, then tick the flakes through spawn,
  update, render, wait, show `page_back`, toggle it, and access the other page;
  phases without a CDG operation go directly from the clear to the flake tick;
- retain the per-page `stf_center_y_on_page[]` coordinates because the source
  deliberately offsets the two page states by a frame; and
- perform palette changes at their exact scripted slots.

The staff-roll flake state and RNG are presentation-owned, but the initial seed
is an explicit resident handoff as described above.

The implemented `present/staff_flakes.hpp` foundation keeps that ownership
boundary literal. It stores all 80 records, seeds a private 32-bit
`0x015A4E35` LCG from the caller-supplied resident value, and consumes exactly
four `IRAND` results for each eligible spawn. Motion uses caller-injected
signed Q8 sine/cosine tables, source-width Q12.4 wrapping, and the original
one-shot edge corrections. Each tick performs spawn, update, B-plane render,
the post-render 80-to-50 lag reset, show-old-page, toggle, and access-new-page
in that order. The complete B-plane clear is a separate raster primitive so
the caller can retain the source CDG interleave. Host waiting, the VSync
counter reset, CDG scene scheduling, palette changes, and audio remain in the
future MAINL scene driver; the tick receives only the already-sampled
`vsync_Count1 > 1` result.

## Presentation-only state inventory

This table lists persistent state intentionally absent from canonical
`libth03` gameplay state. If a field in one of these source families is later
found to feed gameplay, the gameplay-affecting state and mutation must move to
core; an optional observer may report the resulting gameplay fact afterward.
This table is not permission to guess.

| State family | Exact source | Native owner and disposition |
|---|---|---|
| Graphics pages, display mode, and selectors | `page_front` / `page_back` in `th03/main/rstart.cpp` and `rndloop.cpp`; `graph_200line(0)` and `libs/master.lib/graph_200line.asm`; MAINL `page_back` in `th03/mainl/mlsv[bss].asm` | Authentic rasterizer; keep two complete pages, 400/200-line window/zoom state, and access/show selectors |
| Palette/tone/deferred apply | `Palettes`, `PaletteTone`, `palette_changed`; `th03/hardware/palette.hpp`, `palette_changed[bss].asm` | Authentic palette state; edits ordered, application deferred at source slot |
| TRAM and gaiji | `hud_static_put()`, warning text, OP menu gaiji, cutscene/high-score text; source font tables | Authentic TRAM/gaiji state; compositor retains it as a separate layer |
| Vertical display scroll | `graph_scrollup()` uses in `th03/formats/script.hpp` and cutscene code | Authentic display state; exact value, not camera motion in core |
| Main playfield foreground offsets as visual transform | `playfield_fg_x[]` / helpers in `th03/main/playfld.cpp` and `playfield_fg_x[bss].asm` | Core retains values that feed gameplay coordinates; a semantic phase view exposes those gameplay values; presenter applies visual transform and shake |
| Shot animation | `shotpair->so_anim` advanced by `shots_render()` in `th03/main/player/shot.cpp` | Presenter-owned per-shot-pair cel phase; gameplay spawn/reset identity is observed without adding cel state to core |
| Hit circles | 13 `hitcircle_t` records and rotating enemy cursor in `th03/main/hitcircle_bss.asm`; add/update/render in `hitcirc.cpp`; MRS/palette in `hitc_mrs.cpp` and `hitc_pal.cpp` | Presenter owns the 12 rotating enemy slots, fixed player slot, age/cel/color, and process lifetime; core reports exact resolved Q12.4 centers after consuming any randomized pair |
| Background particles | 80-record backing and type-A/type-B callbacks `sub_D092/sub_D135/sub_D1E7` and `sub_D2E8/sub_D340/sub_D3F9` in `th03/main/player/d3f9.cpp` | Presenter owns particles, visual angles, and motion; core retains only the deterministic compatibility schedule and reports each raw source result in order |
| Common visual-effect pool | 12 `cee0_rec_t` records plus cursor; `SUB_CDBD`, `SUB_CE0C`, `SUB_CE5B`, `SUB_CEB2`, `SUB_CEE0` in `d3f9.cpp` and associated BSS | Presenter owns allocation/overwrite/render/angle progression and Kana/Kotohime shared Bomb values; core reports resolved centers, advance timing, and each already-consumed `SUB_CE5B` low byte |
| HUD-start animation | 16 banner records followed by 128 particle records in `th03/main/player/bb12.cpp` and `hudstart.cpp` | Presenter owns records/cels and interprets raw site-tagged results; core retains only the minimal cadence needed to schedule shared RNG |
| Warning presentation | `SUB_CA3C/SUB_C9FE` in `th03/main/player/warning.cpp`; `SUB_CB81` in `d3f9.cpp`; `hitc_mrs.cpp`, `hitc_pal.cpp`, `byte_20E98/byte_20E9A` | Presenter owns tones, portrait/cel/text state, and oscillators; app owns 27 interstitial waits; core timing remains unchanged |
| Static HUD/playfields | `th03/main/hud/static.cpp`, `th03/main/playfld.cpp`, `th03/main/sprite16.cpp`, `th03/main/player/scoreblt.cpp` | Authentic assets/layout/raster code; score/gauge values come from the applicable semantic phase view |
| Player sprite/glow/overlay | `player_render()` and `player_overlay_render()` in `th03/main/player/bomb.cpp`, plus player render BSS/data | Presenter owns visual cel/page alternation and glow state unless a field is proven to feed core; semantic phase views supply raw gameplay state |
| Gauge/result/combo visuals | `gaugeovl.cpp`, `score_rn.asm`, `combo.cpp`, `shot.cpp::sub_E3F2()`, `hitcirc.cpp::sub_B4A8/sub_B60A` | Split: core owns score/result/combo mutation and callback selection; presenter owns raster/palette/animation payload at the same schedule position |
| Character Charge/Extra draw state | render callbacks enumerated in the source disposition matrix below | Presenter owns scratch cursors, clips, sprite cels and stale render-only bytes; core retains Reimu's render-owned Extra age increment |
| Character Bomb visual state | Reimu/Mima/Marisa records, Ellen vectors/release cursor, later-character palette/effect geometry | Split: persistent visual records in presenter; all shared RNG and gameplay-affecting late mutations remain core, with gameplay facts and raw site-tagged results optionally observed |
| Character Boss visual state | `gba_boss_render_*()` and per-character render bytes | Presenter owns sprite/geometry/clip state; core retains Mima/Kotohime and any other proven later-gameplay writes |
| OP scene state | `th03/op/m_main.cpp`, `th03/op_01.cpp`, `th03/op/m_select.cpp`, shared music room | Application/presenter; explicit resident seed handoff to core |
| MAINL scenes | `screens.cpp`, cutscene/continue, high-score registration, staff roll | Application/presenter; persistence and route choices outside presenter |

## Source-to-native disposition matrix

The matrix is intentionally organized by source responsibility rather than by
native file name because no presentation tree exists yet.

| Anniversary source symbols/files | Source responsibility | Native disposition |
|---|---|---|
| `th03/main/rndloop.cpp::sub_9778()` | Governing update/render/present schedule | Core owns deterministic calls/control and reports semantic boundaries synchronously; presenter owns and executes the private draw schedule |
| `th03/main/rstart.cpp::round_startup()`; `th03/main/player/shot.cpp::sub_E266()` | Page/resource/HUD setup plus gameplay initialization | Split startup transaction; core creates round state, presenter prepares pages/resources before first frame |
| `th03/main/hud/static.cpp::hud_wipe/hud_static_put`; `playfld.cpp`; `sprite16.cpp`; `scoreblt.cpp` | Static frame, playfield transforms, sprite and numeric glyph drawing | Port mechanically into Authentic translator/rasterizer; expose only semantic layout to Arrange |
| `th03/main/player/shot.cpp::shots_render()` | Draw shots and advance `so_anim` | Draw/cel state in presenter; semantic phase views identify live gameplay shot records |
| `th03/main/enemy/enemy.cpp::enemies_render()` | Enemy draw payload | Presenter reads immutable enemy gameplay facts at the applicable semantic phase |
| `th03/main/enemy/fireball.cpp::fireballs_hittest_and_render()` | Gameplay hittest interleaved with drawing | Core performs hittest at the exact late phase; presenter continues after the post-hittest observation |
| `th03/main/hitcirc.cpp::hitcircles_*`; `hitc_mrs.cpp`; `hitc_pal.cpp` | Randomized hit effect, 13-slot lifecycle, MRS and palette work | Core reports the collision fact and raw site-tagged results; interpretation/lifecycle/raster/palette live in presenter |
| `th03/main/player/d3f9.cpp::sub_D092..sub_D3F9` | Backgrounds and common 12-record effect pool | Presenter owns visual records; core retains minimal compatibility cadence, reports already-consumed shared-RNG results, and preserves call position |
| `th03/main/player/bb12.cpp::sub_BB12`; `hudstart.cpp::sub_BE5D` | HUD-start records and rendering | Presenter records use observed consumed random values; core retains only the minimal compatibility cadence; draw at the corresponding private presenter position |
| `th03/main/player/warning.cpp::SUB_CA3C/SUB_C9FE`; `th03/main/player/d3f9.cpp::SUB_CB81` | Warning waits, TRAM, palette, MRS portrait | App owns interstitial waits/input; presenter owns visual state; audio cue is separate; do not advance core during prelude |
| `th03/main/player/gaugeovl.cpp::sub_C830/sub_C8C4`; `score_rn.asm::sub_D52E`; `combo.cpp::combos_update_and_render()` | Gauge/score/combo state plus drawing | Split functions: core mutations already remain in suffix; presenter observes the resulting gameplay facts and schedules the source-position draw privately |
| `th03/main/player/shot.cpp::sub_C2F9/sub_E3F2`; `hitcirc.cpp::sub_B4A8/sub_B60A` | Defeat/result text, timing, and trapezoid transitions | Core owns result gates/routes/scores/callback state; presenter owns banner/trapezoid drawing |
| `chargeshot_render_reimu/mima/marisa/ellen` in `ch_shot.cpp` and `main_08.cpp`; character 4-8 renderers in `main_10.cpp`, `main_09.cpp`, `main_11.cpp`, `main_07.cpp`, `main/bullet/bullet.cpp` | Charge draw callbacks and scratch cursors | Presenter-only draw state; no presenter RNG. Core remains source of charge records |
| `exatt_render_*` in `main_06.cpp` and `main/player/exatt.cpp` | Extra draw callbacks | Presenter-only except Reimu's source render-owned age increment, which remains core at the exact late gameplay phase |
| `bomb_func[]` targets in `ch_shot.cpp`, `main/bullet/mbomb.cpp`, `main/player/chybomb.cpp`, `main_05.cpp`, `main/player/yumemi_bomb.cpp` | Bomb palette/effect geometry plus visual RNG and foreground feedback | Core retains foreground writes, shared RNG, and Ellen release coupling; it reports gameplay facts and already-consumed values only; presenter owns all draw records/palette effects |
| `gba_boss_render_*` in `main_03p.cpp` through `main_03x.cpp` | Per-character Boss sprites/effects and some hidden mutation | Core retains proven Mima/Kotohime renderer-fed gameplay state; presenter owns the remaining sprite/geometry/clip/animation state |
| `th03/op/m_main.cpp::op_animate/op_fadein_animate/box_animate`; `th03/op_01.cpp` | Title/menu graphics, TRAM, fades, resident RNG cadence | Native application scene + Authentic presenter; explicit logical wait count feeds resident seed |
| `th03/op/m_select.cpp::select_*` | Character selection assets, curves, pages, fades | Native application scene + presenter; preserve Anniversary flip order |
| `th03/op_music.cpp` / `th02/op/m_music.cpp` | Music-room UI, B-plane polygons, double buffering | Native application/presenter; presentation-local RNG and separate audio service |
| `th03/mainl/screens.cpp` | Win and stage-splash loading/animation/page handoff | Application/presenter; retain both pages across MAINL-to-MAIN transition |
| `th03/cutscene/cutscene.cpp`; `formats/script.hpp`; `mainl/ending.cpp`; `cutscene/continue.cpp` | Script interpreter, glyph pacing, boxes, fades, shake, continue | Application owns script/route/input; presenter owns visual command execution; audio service consumes script audio ops |
| `th03/hiscore/regist.cpp` | High-score table, name entry, hidden-page unblit | Application owns score document and input; presenter owns pages/fonts/glyph animation |
| `th03/mainl/stf_drv.cpp`; `stf_bclr.cpp`; `stf_cdg.cpp`; `th03/end/staff.cpp` | Staff-roll flakes, CDG effects, palette, page cycling | Presenter plus scene-local source-compatible RNG; no core dependency |
| `th03/hardware/*`, master.lib graphics calls, undecompiled graphics ASM listed by `Tupfile.lua` | PC-98 ports, page/palette/TRAM/GRCG/EGC primitives | Replace with tested software semantics; do not expose hardware ports or far pointers in public API |

### Process/build closure outside gameplay core

The Anniversary `Tupfile.lua:624-693` MAINL/OP link list closes the
non-gameplay executable surface. Its remaining modules have these native
owners; they are not unclassified gaps:

| Source group | Exact local modules | Native disposition |
|---|---|---|
| OP entry/routing | `th03/op_main.cpp`, `th03/op_sel.cpp`, `th03/op_01.cpp`, `th03/op/m_main.cpp`, `th03/op/m_select.cpp`, `th03/op_music.cpp` | Application phase machine plus presenter; persistent options/profile state stays in application |
| MAINL entry/routing | `th03/mainl_sc.cpp`, `th03/mainl/mlentry.cpp`, `th03/mainl/screens.cpp`, `th03/mainl/ending.cpp`, `th03/cutscene.cpp`, `th03/cutscene/continue.cpp`, `th03/regist.cpp`, `th03/staff.cpp` | Application scenes plus presenter; route/credits/score persistence is application state |
| MAINL graphics helpers | `th03/mainl/cdgunput.cpp`, `cdgdislv.cpp`, `stf_bclr.cpp`, `stf_cdg.cpp`, `stf_vrd.cpp`, `stf_drv.cpp` | Authentic scene algorithms over the common software rasterizer |
| Shared visual format/primitives | `th03/cdg_load.cpp`, `cdg_p_na.asm`, `cdg_put.asm`, `hfliplut.cpp`, `sprites/pi_mask.cpp`, the `th03/pc98/*.cpp` set, and `platform_objs.cutscene/font/forkbanner/pi_surf/surf/vblank` | Asset decoder and Authentic rasterizer/platform adapters; no core dependency |
| Process/platform services | `th03/cfg_lres.cpp`, `initmain.cpp`, `initop.cpp`, `exit.cpp`, `exitmain.cpp`, input/wait modules, `frmdelay.cpp`, and `snd_*` modules | Native application/config/input/clock/audio services; semantic state crosses typed boundaries only |
| Resident/bootstrap support | `th03/res_yume.cpp`, `th03.asm`, MAINL BSS/data ASM, `mainl/t3mlpad.cpp` | Replace DOS resident/process layout with explicit application/session state; translate only source-observable presentation data |
| Oracle-only adapters | `th03/op/t3op.cpp`, `th03/mainl/t3caseml.cpp`, `th03/main/t3case.cpp` | Validation harness only; never a renderer or product scene dependency |

The MAIN link list at `Tupfile.lua:696-834` contains many files with both
simulation and drawing. Those are covered by the frame schedule and subsystem
matrix above rather than assigned wholesale. All presentation data/BSS entries
from that list are covered by the presentation-state and asset tables below.

### Character render-side exceptions that core must continue to own

The detailed evidence is in `PARITY_CHARACTERS_0_3.md` and
`PARITY_CHARACTERS_4_8.md`. The presentation implementation must honor these
already-resolved seams rather than moving callbacks wholesale:

- Reimu Extra Attack increments age in its source render phase. Core retains
  the increment at `extra_attacks_render_phase_update()`; presenter draws from
  the observed gameplay state after that mutation.
- Mima Boss rendering mutates state later used by gameplay. Core retains that
  source-position mutation.
- Ellen Bomb rendering owns the shared release cursor/reset and consumes the
  angle used by subsequent bomb update behavior. Core retains the coupling and
  can report the resulting gameplay fact and raw site-tagged RNG result.
- Kotohime state-0 Boss rendering (`gba_boss_render_kotohime()`,
  `kotohime_120A0()`, `kotohime_12103()` in `th03/main_03u.cpp`) seeds later
  gameplay orbit angle/radius. Core's `boss_render_phase_update()` retains it;
  only the transient draw mode is presenter-owned.
- Chiyuri state-0 Boss renderer writes materialization-only angle/radius values,
  but source evidence shows they are consumed only inside that renderer and
  overwritten before gameplay use. They remain presenter-owned.
- Ellen's Boss `byte_1F354` animation counter, shot `so_anim`, render scratch
  pointers, and character 4-8 stale fields audited as having no gameplay/RNG
  reader remain presentation state. If pixel work exposes dependence on their
  initial contents, reproduce the observed Anniversary initialization rather
  than introducing host-uninitialized storage.

## Complete gameplay/RNG-to-presentation seam ledger

The following is the minimum exhaustive seam set established by the current
coverage ledgers and pinned source. Each row needs either a neutral observation
or an explicitly presenter-owned derivation before Authentic can claim
coverage. A row never authorizes renderer state in core.

| Seam | Core obligation | Presentation obligation |
|---|---|---|
| Entity positions/state (players, shots, bullets, enemies, fireballs, bosses, Charge, Extra) | Make exact raw fixed-width gameplay facts observable at the semantic phases that bracket late gameplay mutations | Copy required values synchronously; apply source coordinate conversion, PID/playfield transform, clipping, cel, and draw order |
| Foreground shake/offset | Retain values because source feeds them back into gameplay coordinate paths; expose them as gameplay facts | Use the observed value for every affected visual coordinate; do not independently animate it |
| Hit-circle creation | Consume the two source random values exactly when enabled; report the collision fact plus the raw results/site/ordinals | Interpret the results, allocate the 12-enemy rotating slot or shared player slot, then own age/cel/color/page behavior |
| `sub_D092/sub_D2E8` compatibility schedule | Retain only the schedule discriminator because it controls later shared-RNG consumption; for one path report 192 then 48 raw results per early frame through the `round_frame > 128` switch, and for the other report 100 then 40 raw results per early frame through the `round_frame > 144` switch | Interpret the read-site sequence as the Authentic background algorithm; own all particle records and later non-RNG vector motion; never consume core RNG |
| Common `CEE0` effects | Retain any source call that consumes RNG (`SUB_CE5B`) and report its raw result/site/ordinal | Derive non-RNG spawn intents from semantic gameplay facts; interpret the RNG result and reproduce 12-slot overwrite/allocation, type behavior, draw order, and type-2 per-slot angle progression |
| HUD-start burst | Consume all 256 ring and 256 IRAND results in source order and report only raw results/site/ordinals | Interpret the sequence, build the 16-banner/128-particle presentation state, and draw at the private `sub_BE5D()` schedule position |
| Warning | Expose warning gates/result facts; do not advance gameplay during 27 waits | Own TRAM/palette/portrait state and exact 27/30-frame visual timing |
| Reimu Bomb | Retain frame 0..31 velocity-then-X RNG order, foreground shift on frames 64..111, and reset from 112; report raw results/site/ordinals | Interpret results and own falling-sprite records and palette/effect drawing |
| Mima Bomb | Retain one shared-RNG draw on frames 8,16,...,64, foreground shift on frames 81..143, and reset from 144; report its raw result/site/ordinal | Interpret the result and own omitted column records and drawing |
| Marisa Bomb | Retain X-then-Y draw order every eighth frame from 64 through 127, alternating foreground shift in that interval, and reset from 128; report raw results/site/ordinals | Interpret coordinates and own flash records and drawing |
| Ellen Bomb | Retain shared release cursor, reset/release order, and one RNG draw; report the gameplay coupling plus raw result/site/ordinal | Interpret the angle and own eight per-player vector arrays' visual representation and draw payload |
| Kotohime/Kana Bomb | Retain foreground shift on frames 64..127 and reset from 128; no shared RNG is consumed | Own palette/tone and `SUB_CDBD` geometry state |
| Rikako Bomb | Retain foreground shift on frames 64..127, consume one ring value on frames 64,72,...,120, and reset from 128; report each raw result/site/ordinal | Interpret results and own palette plus `word_220EC` line-radius visualization |
| Chiyuri Bomb | Retain foreground shift on frames 64..143, consume four `SUB_CE5B()` values at each of frames 64,80,96,112,128 (20 total), and reset from 144; report raw results/site/ordinals | Interpret results and own the 20 effect spawns plus palette/tone geometry |
| Yumemi Bomb | Retain foreground shift on frames 65..143, consume one `SUB_CE5B()` value at frames 72,80,...,136 (nine total), report raw results/site/ordinals, reset from 144, and preserve order before later Boss/Extra callbacks | Interpret results and own the nine spawned effects plus palette/tone geometry |
| Gauge callbacks | Retain all bullet/pellet mechanics and any `SUB_CE5B` draw; report semantic attack milestones and raw results/site/ordinals only | Derive `SUB_CDBD/SUB_CE0C` geometry, interpret RNG results, and own palette actions at the original private schedule position |
| Boss render callbacks | Perform the proven Mima/Kotohime gameplay mutations at the exact late phase; expose the post-mutation gameplay facts | Own sprite, orbit, materialization, defeat-palette, and clipping payload |
| Extra render callbacks | Perform Reimu age increment at the exact late phase | Observe the updated gameplay age; own all character draw payload and render scratch state |
| Charge render callbacks | No extra core output beyond ordinary gameplay phase facts; callbacks consume no shared RNG in the audited paths | Own character draw payload, cursor, and clip state |
| Score/gauge/combo/result | Perform mutations and control decisions before reporting the corresponding gameplay facts | Render exact displayed values and transitions without feeding them back |
| OP `resident->rand` cadence | Application computes exact logical-wait-derived resident value and supplies it through `GameInit` | Scene rendering must not add or omit logical waits based on host speed |

For any new seam discovered during pixel work, the resolution rule is simple:
if it can affect later gameplay or shared RNG, it belongs in core at the exact
source schedule position. If it cannot, it belongs in presenter state. Never
duplicate a source RNG call in both layers.

## Asset inventory and loading boundary

`th03_assets` and `th03_present` are separate sibling CMake libraries inside
this port repository; no new repository is required. They can be implemented
in parallel after freezing a small immutable decoded-model vocabulary:

```text
external bytes --> th03_assets --> decoded models --+
                                                  +--> th03_present --> indexed frame
th03_core ------ optional neutral observations ----+
```

The first join point is a real static MAIN frame. Before then, `th03_assets`
can prove RGB/palette and PI decoding with synthetic byte fixtures while
`th03_present` proves two-page indexed state, palette, TRAM, clipping, and
primitive semantics with hand-authored decoded models. Neither library waits
for the other's full format or scene coverage.

Original assets stay external and untracked. `th03_present` requests decoded
models by a presenter-owned stable logical ID; an injected `AssetProvider`
resolves the original pack/file for Authentic or a replacement bundle for a
future profile. Filename mutation in the source is a loader implementation
detail, not a public API. `th03_core` links neither library and never sees an
asset ID or model.

The Anniversary build inventory is in `Tupfile.lua`; filename constants are in
`th03/shiftjis/fns.hpp`, `music.hpp`, `bgm.hpp`, and the listed data ASM files.

| Asset family | Source names / defining files | Use and disposition |
|---|---|---|
| Packfiles | OP/ending pack `OP_AND_END_PF_FN` in `th03/shiftjis/fns.hpp`; gameplay pack filename `aCOul` in `th03/main/story_startup[data].asm` | External source assets; map to `AssetPackId`, never expose Shift-JIS path to core |
| Main fonts/BF2/MRS | `MIKOFT.bft`, `GAMEFT.bft`, `opwin.bft`, `chname.bft`, `lose.bf2`, `round.bf2`, `zikicw.bf2`, character `00ch.bf2` family and `??ex.mrs` / `??bm.mrs` mutations | Decode in asset/presenter layer; exact sprite masks/palettes are Authentic content |
| High-score fonts/art | `regi1.bft`, `regi2.bft`, `regib.pi`, `rft0.cdg`; source in `th03/hiscore/regist.cpp` and `shiftjis/regist.hpp` | Presenter; score document remains application persistence |
| OP title/menu | `TL01.PI`, `TL02.PI`, `TLSL.RGB`, `op3.pi`, `MUSIC.TXT`; character `00SL.CD2` through the source-selected character names; `slex.cd2`, `99sl.cdg`, `slwin.cdg`, `slex.cdg` | OP scene/presenter |
| MAINL win/stage | `logo0.rgb`, `logo1.rgb`, `logo.cd2`, `logo5.cdg`, `st.cd2`, `stnx0.pi`, `stnx1.pi`, source-mutated `0016.pi` character files, `EN2.pi`, `ENEMY00.pi`..`ENEMY04.pi` | MAINL scene/presenter; retain palette data and page handoff |
| Win/cutscene scripts | `@00TX.TXT`..`@08TX.TXT`, `@00DM0.TXT`, `@00ED.TXT`, `@99ED.TXT`, and script-selected PI names; tables in `mainl/screens[data].asm` and `continue_cutscene[data].asm` | Application script service plus presenter; preserve byte/script semantics for Authentic |
| Continue/Game Over | `conti.pi`, `conti.cd2`, `over.pi` | MAINL application/presenter |
| Staff roll | `edbk1.rgb`, `stf1.cdg`, `stf11.cdg`, `stf2.cdg`..`stf12.cdg`; tables in `mainl/vstf[data].asm` and `rsedat[data].asm` | Staff scene/presenter |
| Built-in bitmap tables | score digits, pellets, flakes and other compiled bitmaps listed in `Tupfile.lua`; data ASM such as `peldat.asm`, `pbpat_data.asm`, `sfntdat.asm`, `rtextdat.asm`, `5_powers_of_10*.asm` | Re-express as verified native asset tables or generated build inputs; do not invent cels |
| Presentation BSS/data | `soch_data.asm`, `gba_hitc_enemy_data.asm`, `playfield_fg_x[bss].asm`, `warn*.asm`, `hitcircle_bss.asm`, `ccorddat.asm`, `cmb_data.asm` and character render BSS/data | Translate field-by-field into typed presenter state; never copy raw DGROUP layouts |
| Music/effects | `op.m`, `select.m`, character/stage `.M` files, `dec.m`, `demo1.m`..`demo5.m`, `ed.m`, `score.m`, `win.m`, `over.m`, and `YUME.EFC` | Separate audio asset/service; cue IDs are derived outside core from application actions or neutral gameplay observations |

PI, CDG/CD2, MRS, BFNT/BFT, BF2, RGB, and compiled bitmap decoding need
format-level tests with tiny copyright-safe fixtures. Visual tests may load the
user's external original data, but original data and decoded frame dumps remain
ignored artifacts.

## Visual-oracle capture plan

No emulator run is part of this research parcel. The following is the objective
plan for later empirical validation using the pinned Anniversary authority.

### Capture mode

Create an oracle-only instrumentation patch on a disposable worktree derived
from the pinned Anniversary commit. It must not modify the shipping branch or
the native implementation authority. Run PC-98 silently: disable or stub audio
output without changing the order of source `snd_*` calls, and use no physical
or live human input. Gameplay cases are driven by the existing deterministic
T3CASE/replay harness; presentation-only scenes use a fixed scripted input
schedule injected by the harness. “No input” here means no uncontrolled host
input—the manifest contains every injected logical sample.

Use the same original game-data image for all captures and record its hashes.
Use complete process runs so OP/MAINL resident and page handoffs are preserved;
do not launch a helper function in an invented initial state.

### What to capture

At every observable logical-frame boundary, capture:

- all four planes of both full 640x400 graphics-page backings;
- access-page and shown-page selectors;
- GDC raster mode, selected 200-line half where applicable, and zoom/display
  parameters needed to derive the physical 640x400 output;
- all 16 hardware palette entries, `PaletteTone`, and pending logical palette
  state;
- TRAM character/attribute memory and the gaiji glyphs referenced by the frame;
- vertical scroll/display-start state;
- process/scene ID, logical frame/present ID, gameplay and interstitial sample
  cursors, round/result counters, route/stage/PID facts, and live RNG identities;
- a compact ordered trace of page, palette, TRAM, scroll, GRCG/EGC, and high-
  level sprite/blit calls between this boundary and the previous one.

The default boundary is the stable state that will be displayed during the
next logical wait. For MAIN, add an explicit marker after the late palette
apply/page show and the next-page playfield clears in `sub_9778()`; the visible
page is unchanged by those clears. Also log the individual palette/show/clear
calls so an ordering error is diagnosable. For OP/MAINL helpers, mark the state
at each `frame_delay()`/VSync wait boundary and at every explicit show-page
transition that can expose a new image.

Convert planar pages to indexed pixels offline with one tested converter. Keep
the raw dump as authority and generate PNGs only as review artifacts. A capture
manifest must include source commit, executable SHA-256, original-data hashes,
instrumentation patch hash, run/case identity, capture hook schema, and exact
logical input schedule.

### Minimum capture corpus

The first corpus should cover behavior classes rather than convenient stills:

1. OP title scroll, tone flashes, main-menu TRAM/window animation, character
   selection curves/confirmation/fade, and music-room polygon/page cycling.
2. Stage splash through MAIN startup, including both prepared pages and static
   HUD.
3. A normal gameplay interval with moving players, ordinary shots, bullets,
   enemies, fireballs, background type A and type B, and page alternation.
4. The entire 27-frame warning prelude and both 30-frame per-player warning
   transitions, with palette/TRAM/portrait captures.
5. Spawn/active/terminal frames for hit circles, the common 12-record effect
   pool, HUD-start banners/particles, gauges, combos, score/extend, defeat
   banner, result trapezoid, and round result.
6. For every character 0..8: at least one active frame of ordinary/hyper shot,
   Charge, Extra, Gauge, Boss state 0 materialization, active Boss pattern, Boss
   defeat, and Bomb. Add dense captures around every visual-RNG seam.
7. Win screen, story cutscene glyph/box/fade/shake, Continue, Game Over,
   high-score view and name entry, and the full staff-roll page/palette cycle.

The 19-file locked V11/V12 corpus is an excellent set of deterministic gameplay
drivers but is not automatically complete visual coverage. Associate every
captured behavior with a source symbol and maintain a coverage table; add a
targeted scripted case when no existing replay reaches a required visual state.

### Comparison and triage

Comparison proceeds in layers:

1. Verify core schema-3 state and RNG first. A gameplay mismatch is not a
   renderer bug.
2. Compare shown page, palette, TRAM, and scroll hashes separately.
3. On a page mismatch, compare both page hashes and find the first differing
   plane byte/pixel.
4. Align the ordered operation trace and locate the first missing, extra, or
   differently parameterized operation.
5. Produce a pixel diff and palette-index histogram only after the first
   operation divergence is known.

Acceptance is byte-exact for raw indexed pages/palette/TRAM/scroll at selected
logical frames. RGBA screenshot similarity is useful for review but is not the
gate.

## First implementation slices and objective tests

Each slice is deliberately small enough to prove one boundary before importing
more source behavior.

### Slice 0: freeze the neutral seam and no-observer invariance

Implement the minimal borrowed `GameplayObservation` vocabulary and an optional
non-owning `GameplayObserver` argument. Add no queue, collector, visual type, or
presentation state to `th03_core`; a test-owned observer may copy observations
for diagnostics.

Exit tests:

- every existing Phase 3 oracle case remains identical with the observer
  absent, a discard observer attached, and a test-owned recording observer
  attached;
- core snapshots contain no observations or pending delivery state;
- observation types contain no asset, sprite/cel, palette, page, TRAM, draw,
  path, clock, platform, or graphics type;
- `th03rv` links only replay/format/core targets and uses the no-observer path;
- a dependency test rejects any `th03_present`/platform include from core.

### Slice 1A: asset decoding runway (parallel)

Implementation status: ASSET-001 is complete. The stdlib-only span decoders
cover RGB48 and PI with synthetic fixtures; all 78 external TH03 PI files also
pass a read-only compatibility sweep without entering the repository.

Create `th03_assets` as a separate library target in this repository. Begin
with caller-span RGB/palette and PI decoders and small immutable decoded models.
Use only synthetic copyright-safe byte fixtures in tracked tests.

Exit tests:

- malformed/truncated spans fail explicitly and never trigger filesystem I/O;
- known synthetic RGB/PI inputs produce exact indexed pixels and raw palette
  values on GCC and MSVC;
- decoded models contain no path, file handle, mutable global cache, or renderer
  dependency; and
- `th03_core` and `th03rv` do not link `th03_assets`.

### Slice 1B: raster state and primitive semantics (parallel)

Implementation status: the platform-neutral foundation is complete for two raw
640x400 indexed pages, access/show selection, clipped indexed and masked
operations, exact B/R/G/E packed-plane import/projection, direct/masked/OR
planar writes, GRCG TDW/RMW tile writes, exact signed `PaletteTone` application,
400-line/upper/lower zoomed-200-line state, raw 80x25 TRAM cells, all 256
source-format gaiji slots, scroll state, separate provisional graphics/TRAM
hashes, injected ANK/gaiji final text composition, and TH03's used Sprite16
clear/OR/mono/mask, clipping, no-clip, and downwards-column operations. General
EGC register/ROP behavior, JIS, blink, and underline remain open; this slice is
not a complete PC-98 graphics-hardware emulator.

Implement two indexed pages, access/show/copy/clear, palette/tone/deferred
apply, TRAM storage, scroll, clipping, and the minimum source-backed planar/
GRCG raster operations in `th03_present`, using hand-authored decoded models
rather than waiting for real asset decoders. Add EGC operations only when a
ported draw path actually needs their distinct logical result.

Exit tests:

- page writes affect only the access page; show changes only display selection;
- overlapping page copies and masked planar writes match hand-authored byte
  fixtures;
- deferred palette edits are invisible until `ApplyPalette`;
- TRAM and graphics remain separate through hashing/composition;
- scroll changes presentation output without modifying either page;
- identical command streams produce identical hashes on GCC and MSVC.

Slices 1A and 1B start after their small decoded-model interface is agreed and
run independently. Their first required join is Slice 2.

### Slice 2: one static MAIN frame and page swap

Implementation status: the core-independent synthetic parcel is complete, and
the first Slice 3 observer consumer now draws shots, enemies, and fireballs.
It ports raw `hud_wipe()` / `hud_static_put()`, the full page-1-to-page-0
handoff copy, injected decoded character-BF2 preparation for `sub_E266()`,
final palette restoration at the stable startup boundary, the lower-half
Sprite16 atlas copy and positive/negative mask commit, source page-role
selection, and the exact two-color 288x184 next-page clear after every swap.
Captured Anniversary page comparison and GAMEFT gaiji loading by the
application remain deferred and are therefore still required before Slice 2 is
accepted.

Port `hud_wipe()`, `hud_static_put()`, playfield layout, `sub_E266()` resource
sheet preparation, and the exact end-of-frame swap/clear transaction. Use
external original assets only in an ignored integration test.

Exit tests:

- both pre-first-frame pages match captured oracle pages;
- two consecutive page swaps prove access/show identity and the two differently
  colored, 288-pixel-wide playfield clears through Y=183;
- gameplay traces with no observer and with the presenter observer are
  identical.

### Slice 3: ordinary live gameplay

Implementation status: the complete three-phase core observation seam is
implemented. Presenter sub-slices cover shot pairs, enemies and their
explosions, post-hittest fireballs and their explosions, player bases and
BFNT overlays, shared defeat-ring geometry, Bullet16s and trails, pellet
clouds, and odd-flag pellets. The player and bullet passes remain separate at
the exact `SUB_CEE0()` insertion slot. Hit circles and the common-effect pool
now occupy their exact post-fireball and pre-bullet slots using presenter-owned
fixed pools and source-exact GRCG vectors. Ordered PaletteTone events remain a
later parcel; defeat palette application is not approximated at the final view
because later source callbacks can overwrite it.

Observe raw gameplay facts at the documented semantic phases and add Authentic
drawing for players, overlays, ordinary shots, enemies, bullets, and fireballs
in exact order. Keep all hit testing in core.

Exit tests:

- selected TH3R03 frames match raw page/palette/TRAM oracle captures;
- a deliberate one-slot ordering perturbation fails at the expected first
  overlapping pixel;
- shot animation advances once per private presenter schedule position and does
  not enter the core hash.

### Slice 4: RNG-fed effects

Implementation status: hit circles and the common-effect pool are implemented,
including process-scoped overwrite cursors, exceptional cross-player Bomb
state, and exact vector primitives. Both backgrounds and HUD-start animation
remain.

Exit tests:

- a test-owned recording observer records the exact compatibility-site counts,
  ordinals, and raw results while core RNG hashes remain unchanged;
- presenter reruns from a copied, client-owned observation trace without access
  to core RNG;
- arena/slot overwrite and terminal-age frames match the raw visual oracle;
- type-A/type-B initialization and HUD-start burst match dense captures.

### Slice 5: mixed late-frame UI and warnings

Add gauges, score/extend, combos, warning TRAM/palette/MRS, result banners, and
the result trapezoid.

Exit tests:

- all 27 warning waits consume interstitial samples and zero gameplay frames;
- palette changes appear only at the late apply slot;
- P1/P2 warning and gauge order is distinguishable in the operation trace;
- a full round result matches oracle captures without changing the Phase 3
  route/score/state hashes.

### Slice 6: all character visual callbacks

Port Charge, Extra, Gauge, Bomb, and Boss draw payloads character by character,
starting with the character pair already exercised by the smallest useful
capture.

Exit tests for each character:

- core RNG/compatibility-observation count/value sequence at every listed seam;
- source render-side core mutation still occurs with the presenter disabled;
- materialization, active, defeat, and Bomb frames match indexed oracle state;
- presenter-owned stale bytes have an explicit source-backed initialization
  test.

### Slice 7: OP and MAINL application scenes

Implement title/menu, selection, music room, win/stage splash, cutscene,
Continue/Game Over, high-score registration, and staff roll. Reuse the same
rasterizer and output schema.

Exit tests:

- scene-local page/palette/TRAM/scroll sequences match oracle captures;
- OP logical waits produce the exact `resident->rand` handed to `GameInit`;
- stage splash transfers both pages into MAIN startup correctly;
- cutscene shake executes the inclusive alternating sequence and resets to 0;
- high-score unblit proves page 1 is used as source;
- staff-roll output is deterministic from explicit resident seed and does not
  alter a subsequent core seed unless the source application handoff says so.

## Risks and required safeguards

| Risk | Consequence | Safeguard |
|---|---|---|
| Treating a render callback as pure | Lost gameplay mutation or RNG divergence | Keep `PARITY_RENDER_PHASE.md` and character ledgers as gates; test core with presenter absent |
| Letting presenter consume shared RNG | Core behavior depends on rendering/preroll/profile | Report raw already-consumed results with minimal site/phase tags; no RNG handle in presenter API |
| Reconstructing visual RNG from seed later | Wrong order when effects overlap or rendering is skipped | Observe raw consumed results at the original core call site and interpret only in presenter |
| Flattening to one framebuffer | Breaks hidden-page sources, page alternation, unblit, and partial clears | Two-page raster state and explicit access/show/copy operations |
| Exposing low-level commands as the Arrange contract | Couples future art/rules to PC-98 implementation detail | Keep `AuthenticDrawOp` private; share only neutral gameplay observations |
| Hashing RGBA/window output | Host GPU/color/filter differences masquerade as fidelity failures | Hash indexed pages/palette/TRAM/scroll before host composition |
| Applying palette edits immediately | Warning/Bomb/Gauge timing differs | Model pending edits and source `palette_show()` slots |
| Rasterizing TRAM into VRAM too early | Menu/warning text clears and gaiji behavior differ | Preserve TRAM as a separate canonical layer |
| Driving OP seed by wall time | Same selection input can start different gameplay RNG | Count explicit logical scene waits and pass resident seed |
| Assuming presentation BSS starts clean | Pixel differences in stale source fields | Source audit plus clean capture; model observed stale state intentionally, never host garbage |
| Depending on original asset paths | Core/replay portability and legal distribution problems | Presenter-owned typed asset IDs and injected external asset provider |
| Visual instrumentation changing timing/state | False oracle | Hash patched executable and patch, keep audio calls/order, cross-check core/RNG against frozen oracle |
| Overfitting the current gameplay corpus | Green stills but missing effects/scenes | Behavior-class coverage matrix and targeted deterministic scene scripts |

## Open product and architecture decisions

These require an explicit decision before their affected implementation slice;
none should block the neutral observation seam.

1. **Remaining TRAM attributes.** Raw cells, paired gaiji, injected ANK,
   reverse, visibility, fixed text colors, and graphics priority are now
   implemented. JIS font-ROM glyphs, blink cadence, and underline still need
   a complete source/hardware audit before the faithful host compositor accepts
   them. Keep raw capture bytes as the oracle regardless.
2. **Palette representation.** Decide whether the public `Palette16` stores raw
   PC-98 4-bit channels or widened bytes. Authentic comparison must retain raw
   values and exact tone arithmetic either way.
3. **Planar internal storage.** The foundation keeps indexed byte-per-pixel
   pages and losslessly imports/projects B/R/G/E packed bytes. Revisit a native
   packed backing only if a later EGC/unaligned operation has a source-visible
   intermediate result that cannot be represented by the tested logical write;
   do not change storage merely for assumed performance.
4. **Presentation rewind.** Decide whether product replay scrubbing requires
   presenter snapshots. If yes, version presenter state separately from core
   state; never add visual entities to canonical gameplay snapshots.
5. **Fast-forward policy.** Decide which completed logical frames may be
   dropped by the host. Core and presenter state must still advance through
   every frame whose callbacks mutate gameplay or consume shared RNG. When a
   presenter observer is attached it must receive each required observation;
   the primary `th03rv` path remains observer-free.
6. **Authentic aspect/output policy.** Integer scale, 4:3 correction, filtering,
   CRT treatment, and fullscreen are host options applied after canonical
   indexed output; choose defaults during the Windows application milestone.
7. **Text localization.** Replays profile localization may replace semantic
   strings, while Authentic Anniversary should preserve source glyph/layout.
   Define which screens are profile content before implementing translation.
8. **Audio identity and timing.** Choose the PMD/EFC playback implementation
   and whether Authentic audio is sample- or command-validated. Keep frame-
   stamped cues independent from the visual rasterizer.
9. **Asset distribution.** Decide whether the product requires an original
   installation, supports an importer/cache, or ships separately licensed
   replacements. This does not change logical asset IDs.
10. **Visual oracle publication.** Raw frame dumps may be derived from original
    assets. Decide retention/distribution policy; hashes and source/capture
    manifests can be tracked even when dumps remain ignored.
11. **Arrange consumption level.** The recommendation is neutral gameplay
    observations, not Authentic low-level commands or Classic visual state.
    Confirm once Arrange's actual mechanics/content specification exists.
12. **Hardware quirks outside complete frames.** Authentic currently targets
    complete logical frames and intentionally excludes tearing and CPU timing.
    Any later “Original hardware” profile would be a distinct product decision,
    not an extension of this renderer.

## Definition of done for the presentation boundary

The contract is successfully implemented when all of the following are true:

- the full existing headless corpus is identical with presentation disabled,
  observed, rendered, fast-forwarded, and preroll-discarded;
- every seam in the ledger has a neutral observation or documented
  presenter-owned derivation, and no presenter code can reach gameplay RNG;
- the Authentic software rasterizer reproduces selected raw Anniversary page,
  palette, TRAM, and scroll captures at byte granularity;
- OP/MAINL scene pacing and resident handoffs are deterministic with silent,
  scripted, no-live-input runs;
- assets and persistence are injected services outside core, and `th03rv`
  links neither `th03_assets` nor `th03_present`;
- command ordering is diagnosable but the private command vocabulary is not a
  public Arrange dependency; and
- the Windows host only scales/composites completed indexed display states and
  never changes core or presenter logical cadence.

Until those gates are met, the honest status is “source-complete gameplay with
presentation under construction,” not “Authentic-complete.”
