# New Classic source-presentation and generated-locale policy

New Classic does not treat every visible word as permission to replace its
pixels. Japanese uses the authenticated original Japanese presentation and
English uses the authenticated English Patch presentation wherever either
already supplies a complete surface. This is especially important for
purpose-drawn gaiji: the Japanese character selector and scripted dialogue are
more legible than generic thin machine-font output because their glyphs were
drawn for those exact cells.

Text that was already English in the original Japanese game remains English in
every locale until an explicit product decision changes it. This includes the
source `STAGE`, `ROUND`, and `START` banners (and therefore `STAGE X START`),
the result banners, Continue/Game Over labels, and registration labels. The
machine-readable rules live in
`localization/source-presentation-policy.json`; the
`validate_source_presentation.py` test resolves catalog fallbacks and rejects a
locale that changes any source-English invariant.

This divides presentation into four policies:

1. **Authenticated source profile.** Japanese and English select the original
   or English Patch BFNT/CDG/script asset and do not redraw it merely because a
   catalog exists. Character Select, Japanese staff art, and the default
   Japanese/English scripted dialogue belong here.
2. **Source-English invariant.** The original pixels and English wording are
   retained across locales. `STAGE X START` is the canonical example.
3. **Semantic runtime text.** New product UI and explicitly migrated game text
   use stable IDs and translator-authored UTF-8 catalogs.
4. **Generated source-compatible asset.** A locale that must replace text
   embedded in gaiji, BFNT, CDG, or picture art supplies a deterministic
   replacement asset rather than falling back to proportional runtime text.

## Automated asset-generation direction

The generated-asset path should be data-driven and reproducible, not manual
pixel editing:

1. Inventory each authenticated source sheet or picture: surface ID, source
   profile, cell rectangles, safe drawing mask, palette roles, baseline,
   alignment, and maximum occupied bounds.
2. Inventory every independent type role within the surface, then compile
   translator UTF-8 through that role's own pinned face, advance, baseline, and
   optional stroke/outline kernel. Sharing a cell size does not imply sharing a
   font. The adapter centers by reviewed cell metrics and maps only named
   palette roles.
3. Emit the consumer's native structure (BFNT/CDG) or a source-digest-bound
   sparse `T3PTP001` overlay. Original pixels remain external; only replacement
   pixels and geometry are distributable.
4. Render contact sheets and production-scene snapshots. Compilation fails on
   overflow, clipping, mask escape, protected-pixel changes, missing glyphs,
   invalid palette roles, or a source-profile digest mismatch.
5. Compare Japanese and English defaults to their authenticated inputs rather
   than to a generated approximation. Generated assets are acceptance-tested
   only for locales that actually request different wording.

For dialogue, the same model keeps the authenticated script command stream and
timing while replacing only glyph runs. A future locale can pair `T3CTP001`
with a generated, per-surface gaiji bank and Unicode-to-gaiji map; this gives
Japanese-like wide strokes without changing waits, page operations, fades, or
audio. For baked picture text, aligned Japanese/English assets can help locate
candidate text regions, but the committed result remains an explicit mask and
style contract rather than an unchecked image-difference heuristic.

Human review is still required for typography and translation quality. The
automation removes repetitive extraction, cell drawing, packing, overflow
checking, and preview capture; it does not pretend that layout taste can be
inferred safely from source pixels.

## Implemented Character Select contract

`T3CSP001` implements the first generated source-compatible asset path for the
non-shipping `es-TEST` locale. Its source-free payload is a native BFNT name
sheet plus a native alpha-bearing CDG stat-label overlay. The package binds the
three authenticated English inputs by SHA-256; New Classic rejects it before
scene construction if the active `CHNAME.BFT`, `SLWIN.CDG`, or `TLSL.RGB`
differs. Japanese and English never select the generated package.

The name compiler preserves the source's two 64x16 cels per character and uses
a pinned 16-pixel Courier Prime bitmap role with a wide 10-pixel monospaced
advance and serif letterforms. The separately authored stat labels retain the
compact, unexpanded one-pixel GNU Unifont role. They occupy exactly three 56x16
safe cells at source offset (8, 7); eight additional transparent columns satisfy
CDG's 32-pixel width requirement without touching the source rule, border, or
stars. Deterministic compiler tests cover the role distinction, accented UTF-8,
missing glyphs, overflow, schema drift, contact-sheet geometry, native-format
structure, and protected columns. Runtime tests independently parse and
authenticate the package, bind the private English source profile, and compose
it through the production superimpose/CDG paths.
