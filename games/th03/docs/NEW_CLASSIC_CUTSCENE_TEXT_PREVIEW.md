# New Classic scripted-text preview

`th03-cutscene-text-preview` renders every validated replacement run in a
`T3CTP001` cutscene-text pack through the same PC-98 graphics-text preparation
and drawing functions used by production. This makes dialogue, endings, and
interstitial text reviewable without launching or navigating the game.

```powershell
out/build/gcc/th03-cutscene-text-preview.exe `
  --font path/to/FONT.ROM `
  --cutscene-text-pack localization/cutscene-text-ja-JP.t3ctp `
  --output out/cutscene-text-preview-ja-JP
```

The output contains compact 640x400, 4-bit BMP contact sheets and a
deterministic `T3STP001` `preview.json`. The report identifies every run by
script and ordinal and records its page, position, glyph count, and measured
pixel width. A page contains at most 24 runs so no row can overlap another.

The preview deliberately obtains run data from the production `T3CTP001`
loader. Its read-only tooling accessor exposes only already-validated
replacement runs; gameplay remains source-bound through `replacement_glyphs`
and cannot use the accessor to bypass script identity or geometry checks.

`build_locale_bundle.py` runs this renderer for every complete locale and
requires the report and actual BMP page set to cover every compiled run. The
tool requires an exact 288,768-byte `FONT.ROM`; it does not ship, synthesize,
or silently replace original glyph data.
