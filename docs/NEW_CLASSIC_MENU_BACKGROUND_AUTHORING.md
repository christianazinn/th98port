# New Classic custom-menu background authoring

New Classic's retained menus render a 640x400, 16-color indexed page. Custom
repo-owned backgrounds and runtime menu text share that palette, so ordinary
image conversion is not safe: background pixels must never use the six indices
whose colors the UI replaces for shadows, headings, advisory text, selection,
and white body text.

`compile_menu_background.py` turns a PNG, JPEG, BMP, or other Pillow-supported
source into the exact pair consumed by the runtime. It fits to 640x400,
quantizes the art into only indices `0-7`, `10`, and `14`, installs the fixed UI
colors at `8`, `9`, `11`, `12`, `13`, and `15`, and writes two uncompressed
640x200 4-bpp BMP halves. No DOSBox, `mg098`, PI intermediate, or original game
asset is involved.

Install the author-only image dependency, then compile into a new directory:

```text
python -m pip install Pillow
python tools/localization/compile_menu_background.py \
  --input artwork.png \
  --output-root out/menu-background-M008 \
  --name M008 \
  --fit cover \
  --dither floyd-steinberg
```

`cover` preserves aspect ratio and center-crops. `contain` letterboxes with the
color selected by `--matte #RRGGBB`; `stretch` performs an exact nonuniform
resize. Alpha is composited over the same matte before fitting. The compiler
refuses an existing output directory or stale sibling `.tmp` directory.

The result contains:

- `M008A.BMP` and `M008B.BMP`, the runtime inputs;
- `M008-preview.png`, rendered with the exact RGB4-rounded runtime palette; and
- `M008.json`, a `T3MBG001` review record containing the source identity,
  Pillow/compiler version, transformation, palette, index counts, and output
  SHA-256 identities.

Review the preview rather than the source image: it shows the actual indexed
result. A zero count at every reserved index is a mechanical acceptance gate,
not an aesthetic judgment. After review, copy only the two BMP halves into
`assets/new-classic/menu-backgrounds` under a runtime-owned pair name and update
the mapping and README in the same change. The compiler deliberately does not
modify reviewed assets in place.
