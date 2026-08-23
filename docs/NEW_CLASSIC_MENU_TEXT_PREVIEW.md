# New Classic menu-text translator preview

`th03-menu-text-preview` renders all 403 `T3LNG001` messages through the same
mixed `NewClassicMenuFont` path used by New Classic. Printable Latin uses the
authenticated Replay Patch MNUFONT; Japanese and half-width kana use the exact
caller-supplied PC-98 `FONT.ROM`.

Each message is formatted with the schema's conservative placeholder values.
The lavender line immediately after the rendered text row marks its maximum
accepted pixel width. `preview.json` (`T3MTP001`) records the stable message ID,
page, position, rendered width, and maximum width for automated review.

Normal translator/release use authenticates and loads the complete Replay
Patch asset archive:

```powershell
out/build/gcc/th03-menu-text-preview.exe `
  --replay-patch-assets path/to/AZINN.DAT `
  --menu-backgrounds assets/new-classic/menu-backgrounds `
  --font path/to/FONT.ROM `
  --language-pack localization/ja-JP.t3lang `
  --output out/menu-text-preview-ja-JP
```

Hermetic tests and tools may pass `--menu-font path/to/MNUFONT.PF` instead of
the archive and background arguments. The loose file still passes the exact
T3PFNT1 shape/checksum decoder; this option does not introduce a second glyph
format or renderer.

The preview tool writes 41 deterministic 640x400 PPM sheets and one JSON bounds
report. Malformed fonts and language packs fail before an output directory is
created. Complete release builds normally invoke it through
`build_locale_bundle.py`, which stages all outputs privately and identifies the
font, renderer, source package, and every generated sheet in `T3LCB001`.
