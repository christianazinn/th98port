# New Classic cutscene-picture localization

Status: audited architecture, 2026-08-23.

TH03's 28 cutscene programs reference 24 authenticated 640x400 PI pictures.
The production script VM now inventories every full-background and quarter
presentation and proves that all 24 pictures are reachable. This is the source
of truth; filename scraping is not, because scripts use both full-image and
quarter presentation commands and are inconsistent about filename case.

## Evidence

Every picture was decoded through `th03-resource-preview` and inspected as four
320x200 quarters. The Japanese and current English archives differ in only
four pictures:

| Picture | Changed RGB pixels | Difference bounds |
|---|---:|---|
| `DM3B.PI` | 3,674 | `(90, 228)` through `(272, 399)` |
| `DM3D.PI` | 852 | `(213, 133)` through `(630, 168)` |
| `DM3F.PI` | 546 | `(495, 6)` through `(618, 38)` |
| `DM3N.PI` | 1,438 | `(113, 152)` through `(248, 200)` |

Those are bounded hand-drawn raster replacements, not strings rendered with
FONT.ROM. The remaining source pictures include additional player-visible
lettering that the existing English archive deliberately leaves unchanged:

- `DM1B.PI`: the switch's `ON` and `OFF` labels;
- `DM1D.PI` and `DM3C.PI`: decorative game-title/logo lettering;
- `DM1E.PI` and `DM3O.PI`: handwritten notes;
- `DM1F.PI` and `DM3A.PI`: drawn impact sound effects;
- `DM3B.PI`: notebook cover, headings, and marginal writing;
- `DM3D.PI` and `DM3F.PI`: short handwritten remarks;
- `DM3H.PI`: drawn ellipsis used as part of the scene;
- `DM3K.PI`: shop signs;
- `DM3N.PI`: laboratory signs, notes, and an explosion sound effect; and
- `DM3P.PI`: school/society signage and a small handwritten effect.

These regions mix lettering with illustration, antialiasing, and the source
16-color palette. Replacing them with a generic graphics font would lose the
art direction and can damage neighboring pixels. A full translated PI archive
also duplicates copyrighted source pixels and makes unchanged-art identity
impossible to review.

## Package boundary

The planned distributable format is `T3PTP001`, a sparse decoded-image patch:

1. A localizer edits a transparent 640x400 indexed PNG for each changed
   picture. Transparent pixels mean "retain the authenticated source pixel".
   Opaque colors must map exactly to one of that picture's 16 palette entries.
2. A JSON author manifest binds the locale, uppercase picture member name,
   decoded source palette/pixel digest, and intended changed bounds.
3. The compiler validates dimensions, palette membership, alpha, declared
   bounds, nonempty changes, unique members, and source identity. It emits
   deterministic row-major runs of replacement palette indices, never the
   original picture.
4. New Classic authenticates and decodes the ordinary PI first, verifies the
   decoded-source digest, then applies the selected locale's runs before the
   existing page/palette/presentation schedule sees the image.
5. Classic never loads or applies `T3PTP001`.

The runtime format owns no coordinates, timing, quarter selection, palette
transitions, or drawing commands beyond sparse pixel replacement. Those remain
with the authenticated source picture and script VM. A Japanese package may be
an explicitly complete no-op profile; the source image is never copied into
the repository.

## Gates

- canonical VM extraction reaches the same 24-member inventory for both
  authenticated Japanese and English script sets;
- decoded source digest mismatch fails before any replacement is published;
- compiler output is deterministic and contains no unchanged pixel runs;
- malformed alpha, dimensions, palette colors, overlaps, or out-of-bounds runs
  fail closed;
- applying a synthetic package changes exactly the declared pixels and leaves
  every other decoded pixel and palette entry identical;
- full-image and quarter presentation snapshots retain the source lifecycle,
  page ownership, palette, and timing; and
- a Classic snapshot remains byte-identical with a package present.

The format and tooling can be implemented without committing original assets.
Shipping complete English/Japanese picture localization still requires
reviewed replacement artwork for every region classified above.
