# TH01's renderer model

Lane `g4-th01`, parcel `PORT-W5A-TH01-RENDERER-MODEL`, 2026-08-19.

`W5A_TH01_PLAN.md` §8 names the renderer model as "the single largest unknown
in W5a for TH01" and explicitly declines to choose one. This is that choice,
and the argument for it is measurement rather than design taste.

Everything below was measured this session. Where it contradicts a document
this same lane wrote, the contradiction is stated first and the old text is
corrected in place rather than left standing.

**Pins.** Donor game code: `ReC98` `dc6cf486` (`harness/4-port-merge-main`).
`th01/` is **byte-identical** between that revision and the coverage ledger's
pin `b6ba5b0a`, verified by an empty `git diff --stat b6ba5b0a dc6cf486 --
th01/`, so no figure below depends on which of the two is used. The eight
`libs/master.lib/` files that do differ between them are all `.asm`; no header
TH01 grep matches is touched. Donor port tree: `th03-port` at this worktree's
`emdash/g4-th01` `73cecbb3`.

---

## 1. The three claims this parcel overturns

### 1.1 "`source_surface.cpp` encodes the two-page GRCG model, which does not describe TH01 at all"

`W5A_TH01_PLAN.md` §3.2 and §6, and `COPY_DIFF.md` §6.2. **Refuted at that
scope.** Classified entity by entity:

| module | public entities | page-flip specific | TH03-specific |
|---|---|---|---|
| `th03-port/include/th03/present/source_surface.hpp` | 23 | **1** (`SourceSurface::shown_page`, `:35`) | **0** |
| `th03-port/include/th03/present/raster.hpp` | 47 | **3** (`set_shown_page` `:76`, `shown_page` `:78`, `shown_graphics` `:190`) | **0** |
| `th03-port/include/th03/present/planar.hpp` | 8 | **0** | **0** |

By bytes, the whole flip footprint of the `source_surface` module — header plus
implementation, 537 lines / 15,460 B — is **3 lines / 118 B, 0.76%**. The other
two flip statements are `th03-port/src/present/source_surface.cpp:420` and
`th03-port/src/present/source_surface.cpp:443`, both of
which merely *read* `raster.shown_page()`. The module makes no page-selection
decision at all; the flip policy lives in `th03-port/src/app/classic_*.cpp`,
which holds
roughly 60 `set_shown_page`/`set_access_page`/`copy_page` call sites.

63.3% of `source_surface.cpp` is PC-98 TRAM/ANK/gaiji/JIS text composition,
which TH01 needs in full. A further 11.7% is the GDC 200-line window and the
`graph_scrollup()` wrap, which is master.lib-generic and simply inert for a
400-line game rather than wrong for one.

**Why the old ruling read as it did.** It named a *module* where the real
subject was a *policy*. `TH01_PORT_SCOPE.md` §5.4 had already got this right in
the same campaign — "What transfers from `th03_present`: `Raster` itself … What
cannot describe TH01: `MainStaticPresenter` entire" — and the two documents have
been contradicting each other since. §5.4 is the correct one.

Note that "TH01 does not use the flip" is not the same as "the flip is dead
weight in a TH01 tree": §4.3 measures a fourth TH01 binary that flips every
frame.

### 1.2 "Four link-time dependencies, two of them in the ruled-out modules"

`COPY_DIFF.md` §6.2. **One of the four is not a link-time dependency.**
`app::physical_input_source_count` is `inline constexpr std::size_t` at
`th03-port/include/th03/app/tick_input.hpp:46`;
`th03-port/src/platform/sdl_platform.cpp`
never references it at all, and its only platform-layer use is
`th03-port/include/th03/platform/sdl_input.hpp:22`. The measured set is
**three**: `app::source_surface_to_host_rgba` at
`th03-port/src/platform/sdl_platform.cpp:353`, `app::integer_letterbox_8x5` at
`th03-port/src/platform/sdl_platform.cpp:580`, and
`present::set_source_surface_oracle_hashes_enabled` at
`th03-port/src/platform/sdl_platform.cpp:247` and again at
`th03-port/src/platform/sdl_platform.cpp:518`. Widening
to the whole copied platform library adds one the old table missed —
`app::TickInputScheduler::push_live_event`, at
`th03-port/src/platform/sdl_input.cpp:404`.

That leaves **one** genuine link-time dependency on a "ruled-out" module, and
§1.1 has just un-ruled it: `set_source_surface_oracle_hashes_enabled` is the
oracle switch, class (d) instrumentation, not renderer semantics.

### 1.3 "60 call sites across 24 files"

`W5A_TH01_PLAN.md` §5, of `egc_copy_rect_1_to_0_16`. Two things are wrong with
it and they point in opposite directions.

It is an **occurrence** count of the string, not a call-site count: those 60
occurrences are 47 of `egc_copy_rect_1_to_0_16(` plus 13 of the differently
named macro `egc_copy_rect_1_to_0_16_word_w`. Reducing the 47 by one
declaration (`th01/hardware/egc.h:17`), one definition
(`th01/hardware/egcrect.cpp:3`), four wrapper-macro **bodies** (`egc.h:23`,
`th01/formats/ptn.hpp:82`, `th01/formats/ptn.hpp:131`, `th01/formats/grc.hpp:45`)
and two prose mentions (`th01/formats/ptn.hpp:45`, `th01/formats/ptn.hpp:47`)
leaves **39 direct call sites in 17 files**.

But the four wrapper macros are themselves call sites of the primitive, and
they are where most of the demand is:

| wrapper | defined | call sites |
|---|---|---|
| `egc_copy_rect_1_to_0_16_word_w` | `th01/hardware/egc.h:22` | 12 |
| `ptn_sloppy_unput_16` | `th01/formats/ptn.hpp:81` | 16 |
| `ptn_sloppy_unput_quarter_16` | `th01/formats/ptn.hpp:130` | 6 |
| `grc_sloppy_unput` | `th01/formats/grc.hpp:44` | 12 |

**39 + 46 = 85 source call sites.** So the plan's figure over-counts what it
claims to count and under-counts the thing that matters by 25. This number was
reduced twice, independently and by different methods, and both reductions land
on 85.

The count is still a *source* count. Several of the 85 sit inside further macro
bodies that expand more than once, and nobody has measured the post-preprocessor
figure; doing so needs a build of the donor, which this parcel did not run.

---

## 2. The decision

> **TH01's renderer is `th03-port`'s presentation model, with the page-flip
> policy unused by three of the four binaries, and exactly one primitive added:
> an unclipped, 16-dot-granular page-1 → page-0 restore whose overflow wraps
> into the adjacent VRAM line. One hardware mode is owed in exchange for one
> that is not needed: TCR in, TDW out.**

Nothing about the two-page 640×400 model, the plane order, the palette, TRAM,
the GDC scroll, or the readout needs to change for TH01. The `th01-port` tree
gets its own copy under its own prefix, as the campaign's copy-first rule
requires, and copies each piece when a ported draw path needs it — not now, and
not speculatively.

The two sized gaps, stated once so they are not mistaken for zero: the
unclipped restore (§5, 85 call sites, built here) and TCR (§4.2, 8 call sites,
not built here).

---

## 3. What the SDL layer requires of a renderer

This is the half of the question that turned out to have the smallest answer.

The copied platform layer's texture path is exactly
`th03-port/src/platform/sdl_platform.cpp:353` → `app::source_surface_to_host_rgba`
(`th03-port/src/app/host_surface.cpp:119-181`). That function's entire input is
two members of `present::SourceSurface`:

| member | declared | what the converter does with it |
|---|---|---|
| `pixels` | `th03-port/include/th03/present/source_surface.hpp:33` | one `graphics_page_pixels`-long span of already-resolved values |
| `graphics_palette` | `th03-port/include/th03/present/source_surface.hpp:34` | 16 RGB4 entries, applied per pixel |

It reads **none** of `shown_page`, `raster_mode`, `effective_scroll_y`,
`palette_tone`, `oracle_hashes`, `host_overlay` or `reduced_flash_*`. Grepping
both `host_surface` files for `planar|Grcg|BitSource|Page` returns **zero
hits**. There is no page selector in the signature.

So the seam between a TH01 renderer and the host is **one 640×400 span of
indices plus sixteen palette entries**, and everything page-shaped,
plane-shaped and EGC-shaped lives on the renderer's side of it, invisible to
SDL. `Vram::to_indexed()` in this tree is that seam's graphics half; TRAM
composition, which `source_surface.cpp` already does game-neutrally, is the
other half and is not written here.

The remaining `SourceSurface` members are read by `th03-port/src/platform/sdl_platform.cpp` only when
emitting the `T3RGB1`/`T3PXH2` oracle rows, at
`th03-port/src/platform/sdl_platform.cpp:186-204`,
`th03-port/src/platform/sdl_platform.cpp:224-239` and
`th03-port/src/platform/sdl_platform.cpp:272-283`. None of it reaches the
texture.

---

## 4. What TH01 asks of a renderer

Measured at `dc6cf486` over `th01/`. Counts are **occurrences** unless the row
says otherwise, because that is what `git grep` returns and conflating the two
is what produced §1.3.

| primitive | call sites | already in `th03_present`? |
|---|---|---|
| `graph_accesspage_func` | 217 | yes — `Raster::set_access_page` |
| `egc_copy_rect_1_to_0_16` | **85** (39 direct + 46 via 4 wrappers) | **no — see §5** |
| `z_vsync_wait_and_scrollup` | 36 | yes — `Raster::set_scroll_y` + the GDC wrap in `source_surface.cpp` |
| `grcg_setcolor_rmw` | 37 | yes — `GrcgWriteMode::read_modify_write` |
| `grcg_off_func` | 34 | yes |
| `graph_copy_accessed_page_to_other` | 20 | yes — `Raster::copy_page` |
| `graph_r_line_unput` | 15 | a second unclipped p1→p0 path, 32-dot granular |
| `end_pic_show` | 15 | the same EGC copy, open-coded in `th01/end/pic.cpp:15` |
| `grcg_setcolor_tcr` | 8 | **no — TCR is absent from the whole donor tree** |
| `egc_copy_rows_1_to_0` | 2 | the full-width case of the §5 primitive |
| `egc_wave_unput` | 2 | the per-row-displaced case of the §5 primitive |
| `graph_move_byterect_interpage` | 1 | yes — `Raster::copy_page` |
| `graph_showpage_func` | **1**, `th01/hardware/graph.cpp:94`, always with 0 | yes, and see §4.3 |
| `z_graph_readdot`, `z_grcg_pset`, `graph_r_box`, `z_graph_clear_col`, `graph_copy_byterect_from_accessed_page_to_other` | **0** | dead call-graph nodes, still linked into the originals |

Plane-direct access, the figure that decides storage: **101 occurrences in 24
files** — 28 `VRAM_CHUNK`, 9 `VRAM_SNAP`, 42 `VRAM_PLANE_[BRGE]`, 22 `Planar<>`.
GRCG references, for contrast, are 163 across 23 files
(`W5A_TH01_PLAN.md` §2).

### 4.1 The EGC demand is one configuration, not an engine

This is the finding that most reduces the work, and it cuts directly against
the shape of the risk everyone has been carrying.

"General EGC register/ROP emulation" is deferred twice — at
`th03-port/docs/research/PRESENTATION_CONTRACT.md:390` and at
`th03-port/docs/research/PRESENTATION_CONTRACT.md:1229` — and
`th03-port/docs/research/PRESENTATION_CONTRACT.md:1236` instructs:
"Add EGC operations only when a ported draw path actually needs their distinct
logical result." That deferral has been read as a TH01 blocker, because TH01 is
the EGC game.

Measured: **TH01 puts the EGC into exactly one configuration and never varies
it.** `egc_setup_copy()` (`th01/hardware/egc_impl.hpp:4`) enables all four
planes, selects a memory read latch, sets a 16-bit transfer length and an
all-ones mask; every TH01 EGC copy runs under it. `egc_setup_copy_masked()`
sits immediately beside it in the same donor header and has **zero call sites
anywhere under `th01/`** — it belongs to TH02 and later.

So TH01 needs no EGC register file, no ROP, no mask register and no bit-length
register. Under that one configuration a 16-bit access at a plane-B offset
(`egc_chunk()`, `planar.h:248`) moves 16 dots across all four planes, which on
a host is two bytes per plane. The contract's deferral is not a TH01 blocker;
it never was.

### 4.2 TCR is the one genuinely missing hardware mode

`grcg_setcolor_tcr()` (`th01/hardware/graph.h:43`) puts the GRCG into Tile
Compare Read, where a VRAM read returns 1 for dots matching a colour and 0
otherwise. `th03-port` implements TDW and RMW (`planar.hpp:65-70`) and grepping
the whole tree for `tcr|tile_compare|compare_read` returns **zero hits**.

The demand is small and localized: **8 call sites in 2 files** —
`th01/main/boss/b20m.cpp` ×7 (all `grcg_setcolor_tcr(COL_AIR)`, snapping air
particles off page 1 by colour and re-putting them on page 0 in RMW) and
`th01/op_01.cpp:47` ×1 (extracting colour 4 from page 1 into per-column
bitmaps). It is recorded here as a known, sized gap and is not implemented in
this parcel, because no ported draw path needs it yet and the contract's rule
above applies to us as much as to the donor.

The exchange is favourable in aggregate: TH01 uses **RMW and TCR and never
TDW**. The donor implements TDW and RMW. So the port gains one mode it does not
need and owes one mode it does — a one-for-one trade, not a new subsystem.

### 4.3 "TH01 never page-flips" is true of three binaries out of four

`W5A_TH01_PLAN.md` §2 establishes that `graph_showpage_func` has exactly one
call site and that `th01/zunsoft.cpp:42` calls master.lib's same-stem
`graph_showpage` **macro**, a different symbol. That is correct, and the
conclusion drawn from it — "TH01 never page-flips" — is correct for OP.EXE,
REIIDEN.EXE and FUUIN.EXE. It is **false for the fourth link target**.

`th01/zunsoft.cpp:246-250` is a textbook flip:

```
page_show(page_back); page_back ^= 1; page_access(page_back);
```

`zunsoft.com` is `MODEL_TINY`, one translation unit, and drives master.lib
directly (`graph_start`, `graph_clear`, `grcg_setcolor`, `grcg_pset`,
`page_show`, `page_access`) rather than `th01/hardware/`. It is a second,
independent renderer inside the same game.

This matters for §1.1's arithmetic. The three flip identifiers in `Raster` are
droppable for the *game*, and are **not** droppable for the *tree*: whatever
eventually ports `zunsoft.com` needs exactly them. So the honest form of the
finding is not "TH01 does not need the flip" but "TH01's flip demand is one
binary and 178 residual lines, and the donor already covers it".

---

## 5. The delta, and why it is exactly one primitive

`egc_copy_rect_1_to_0_16()` (`th01/hardware/egcrect.cpp:3`) blits page 1 to
page 0 at the same position, over an X range snapped **outward** to 16-dot
registers: from `floor(left/16)*16` to `ceil((left + w)/16)*16`. It clips
nothing. Its own header says so — "Does not clamp the X coordinate … the blit
operation will wrap around into the next or previous line" — and
`egc_wave_unput()` (`th01/hardware/egcwave.cpp`) exists to feed it a per-row
sine-displaced X specifically so that it does.

`Raster::copy_page()` (`th03-port/src/present/raster.cpp:444`) cannot express
that, for one reason and one only: it clips, through `clipped_mapping()` at
`th03-port/src/present/raster.cpp:122-175`, and its `page_offset()` at
`th03-port/src/present/raster.cpp:177` is `y * 640 + x` with no defined
behaviour outside the page.

**The donor tree already contains this function with the clipping in it.**
`NewClassicMenuFont::restore_rect()`
(`th03-port/src/app/new_classic_assets.cpp:554-592`), whose declaration
(`th03-port/include/th03/app/new_classic_assets.hpp:103`) calls itself the "exact
page-1-to-page-0 EGC restore granularity used by mnufont.cpp", does:

| `restore_rect()` | TH01's `egc_copy_rect_1_to_0_16()` |
|---|---|
| `aligned_left = left & ~0x0F` | `x_floor = x & 0xFFF0` — same floor |
| `aligned_right = (right + 15) & ~0x0F` | loop `column < x_end` stepping 16 from `x_floor` — arithmetically identical |
| `copy_page(Page::one → Page::zero)` | same pages, same position |
| `set_access_page(Page::zero)` at the end | `graph_accesspage_func(0)` last inside the inner loop |
| `left = max(left, 0)`, `right = min(right, 640)`, `bottom = min(...)` | **nothing** |

So the whole renderer gap between the two games is that last row. This tree
implements the primitive as `Vram::egc_copy_rect_1_to_0_16()`, structured as
`restore_rect()` with the three clamps removed; every other difference is
listed and justified in the header's own comments.

One difference worth stating separately, because it is a behaviour and not a
clamp: `restore_rect()` sets the access page unconditionally on success, while
the donor sets it as the last act of the **inner** loop, so an empty rectangle
leaves the selector alone. That difference is unobservable while the clipping
is present and observable once it is gone, so the donor's spelling is the one
implemented, and it is pinned by a test.

---

## 6. Storage: a decision that is smaller than it looks

`Vram` stores four 32,000-byte planes per page. `th03-port`'s `Raster` stores
one chunky `Index4` per pixel (`raster.hpp:199`) and synthesises plane bytes on
demand (`planar.cpp:409`).

**Both reproduce TH01's wrap correctly**, and it is worth saying so plainly
because the opposite is easy to assume. A row-major chunky page and a linear
plane-byte offset are the same linear order scaled by eight, so an overflowing
X runs into the following row either way. Nothing in §5 forces planar storage.

The choice is transcription cost, and the counted criterion is §4's 101
plane-direct accesses in 24 files. TH01's drawing code reads and writes named
planes through raw pointers (`graph_invert_32x32_8()` takes a
`dots8_t far *plane`) and addresses VRAM by a linear plane-byte offset
(`vram_offset_t`, `planar.h:87`) rather than by (x, y). Chunky storage makes
each of those a pack/unpack. The donor's drawing goes through GRCG writes and
sprite blits, where chunky is the cheaper side. The costs invert with the game,
so the storage does — but this is a cost argument, not a correctness one, and
it should be revisited if a later parcel measures the pack/unpack to be free.

### 6.1 Two donor spellings that are not one function

`vram_offset_shift()` (`planar.h:105`) shifts; `vram_offset_muldiv()`
(`planar.h:128`) divides. For negative X an arithmetic right shift floors while
integer division truncates toward zero, so they disagree by one plane byte —
eight pixels — wherever `x < 0` and `x % 8 != 0`. The donor chose between them
per call site for code-generation reasons and TH01 uses both. A port that
unifies them moves pixels. Both are provided and the disagreement is pinned by
a test.

### 6.2 `& 0xFFF0` does not survive a 32-bit `int`

`egcrect.cpp:13` is `x_floor &= 0xFFF0`. Under Turbo C++'s 16-bit `int` that
literal *is* −16 and the statement is an arithmetic floor that works for
negative X. On a 32-bit host the same literal is 65,520 and the statement is
something else entirely: `-8 & 0xFFF0` is `65520`, not `-16`. Negative X is not
hypothetical — §5's wave unblitter produces it on purpose. The header spells
the floor `x & ~(egc_register_dots - 1)`, and the test asserts the naive
transcription disagrees, on this host, rather than asserting it about a
compiler nobody here runs.

---

## 7. What was built, and what was deliberately not

Built — one library, one translation unit, one test executable:

- `th01-port/include/th01/present/vram.hpp`, `th01-port/src/present/vram.cpp` — the geometry
  constants, both offset spellings, the 16-dot floor, a two-page four-plane
  surface, the access-page selector, the single EGC chunk copy, the unclipped
  16-dot restore, and `to_indexed()` (§3's seam).
- `th01-port/tests/test_present_vram.cpp` — 11 cases, 86 assertions, pinning §5's outward
  snap, the wrap in both directions, the four-plane chunk, the access-page
  spelling, §6.1, §6.2, the readout's colour order, and the out-of-plane
  refusal. `checks=86 failures=0` on both compilers.

Not built, on purpose, and each for a stated reason:

| not built | why |
|---|---|
| palette, TRAM, the GRCG RMW path, scroll, the 200-line window | `th03_present` already models all of them game-neutrally (§1.1). They are copy-when-needed items, and copying them now would be extraction ahead of a diff. |
| TCR | §4.2: a real gap, 8 call sites, no ported draw path needs it yet. |
| the page-flip identifiers | §4.3: needed by `zunsoft.com` and by nothing else in TH01. Copy them with whatever ports that binary. |
| any EGC register/ROP model | §4.1: TH01 never leaves one configuration, so there is nothing to model. |
| a `SourceSurface` producer | §3: it needs the TRAM half, which is `source_surface.cpp`'s job and a copy parcel of its own. |
| a presenter, a frame loop, a flip | TH01 has no flip. `W5A_TH01_PLAN.md` §3.2 owns the frame-boundary question and this parcel does not touch it. |

### 7.1 The guard that is a guard, not a measurement

`Vram` refuses any access that would leave a plane's 32,000 bytes, and the
refusal is atomic. That is **not** a claim about the hardware. Wrapping from one
row into the next is ordinary linear addressing and is reproduced exactly; what
a PC-98 does past the end of a plane is something this campaign has not
measured, and whether TH01's 85 call sites can reach it is a static question
nobody has answered. The refusal exists so that the unmeasured case is loud
instead of silently wrong, and the test pins the refusal rather than a
behaviour.

---

## 7.2 Controls

Five mutations, run after the parcel's commit existed so that `git checkout --`
could be the restore. Each one asserts that it applied and that it was not a
no-op; each is required to fail the suite **and** to make the failure message
name the guard under test, because a mutation that produces a generic red proves
only that something broke.

| mutation | failing assertions | first message |
|---|---|---|
| re-add the three clamps `restore_rect()` has | 4 | "the overhang landed on row 1 byte 0 — it wrapped, it was not clipped" |
| spell the 16-dot floor `x & 0xFFF0`, as the donor does | 7 | "floor(-1) (expected -16, got 65520)" |
| move only plane B in the EGC chunk copy | 6 | "plane low byte (expected 165, got 0)" |
| unify the two offset spellings (§6.1) | 2 | "muldiv(-1,0) truncates (expected 0, got -1)" |
| set the access page unconditionally, `restore_rect()`-style | 2 | "but it copies nothing and therefore does not touch the selector" |

All five RED; unmutated `checks=86 failures=0` restored afterwards, and
`git status --porcelain` clean.

**The first run of this harness was a false result and is recorded because the
mechanism generalises.** Its builds were incremental. MinGW Make compares
mtimes at one-second granularity and a mutate → build → restore → mutate cycle
finishes well inside that window, so a restored header did not trigger a
recompile: mutations 3 and 4 were reported "caught" while the failures printed
were mutation 2's. The fix is two rules, both now in the harness — **every build
is `--clean-first`**, and **the unmutated baseline is re-run inside each
mutant's own environment before its mutation is applied**, so a contaminated
tree is a loud refusal rather than a green.

---

## 8. Prior art this parcel should have found sooner

`th03-port/docs/research/PRESENTATION_CONTRACT.md` (1,497 lines) is the
repository's renderer design document and it already specifies everything this
parcel had to re-derive. Its persistent state model is at
`th03-port/docs/research/PRESENTATION_CONTRACT.md:296`; the B/R/G/E plane order,
the MSB-left bit order, TDW versus RMW, the `palette_show()` tone transform and
the gaiji/TRAM layout are at
`th03-port/docs/research/PRESENTATION_CONTRACT.md:334`; the disposition rule for
`th01/hardware/*`-shaped code is at
`th03-port/docs/research/PRESENTATION_CONTRACT.md:904`.

`kb/` does not point at it. There is no `kb/` entry for the EGC, the GRCG, or
the PC-98 VRAM layout, and `kb/INDEX.md`'s six directories contain no hardware
bucket. The only `kb/` file that touches this subject as a decision is
`kb/decisions/0006-native-repo-and-publishing-model.md`, whose "TH01 is the
awkward case" section is where the §1.1 ruling originates — it argues that
`pc98_raster`'s "two-page 640x400 GRCG model may simply not fit TH01" because
TH01 draws through the EGC and never page-flips.

Its first half is true with one exception (§4.3), its second half is true, and
the conclusion does not follow, for the reason §1.1 gives: the flip is a policy
three identifiers wide, and the EGC demand is one configuration (§4.1) rather
than an engine.
**ADR 0006 is not amended by this document** — amending an accepted ADR needs
its own ADR, and `pc98_raster` does not exist yet in any case, so nothing is
mis-built today. This is the evidence a future ADR would rest on.

The `kb/` gap is filed rather than closed here: a `kb/` leaf pointing at
`PRESENTATION_CONTRACT.md` and at `ReC98/platform/x86real/pc98/{egc,grcg}.hpp`
is worth one small parcel, and it is not this one's write set.
