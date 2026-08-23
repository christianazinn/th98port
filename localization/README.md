# New Classic localization packs

`schema.json` freezes stable message IDs, source text domains, placeholders,
and pixel-width budgets. Translator-facing `*.json` files are UTF-8. The
compiler emits deterministic `T3LNG001` packages; the native loader then
revalidates the schema digest, completeness/fallback, placeholders, encoding,
machine-font coverage, and exact pixel widths before accepting a locale.

The complete domains currently include the New Classic title, hierarchical
Option UI, authenticated Replay browser/save flow, Practice setup, KeyConfig,
JoyConfig, controller assignment, and Input Test. Dynamic templates declare
named placeholders together with conservative width-test values, so a
translator cannot evade a row budget by leaving the runtime value unresolved.
Host key and controller names remain platform data, but their surrounding
labels and bounded row templates are localized. English and Latin fragments
render through authenticated Replay Patch `MNUFONT.PF`; Japanese `ja-JP`
messages are compiled to CP932 and render with the exact authenticated PC-98
`FONT.ROM`. Mixed rows retain proportional Latin spacing and 16-pixel Japanese
glyphs. This remains a CP932-capable source-era domain rather than a general
Unicode product-shell renderer. Classic never reads these files.

Generate the frozen C++ tables:

```powershell
python tools/localization/compile_language_pack.py generate-cpp `
  --schema localization/schema.json `
  --source localization/en-US.json `
  --source localization/ja-JP.json `
  --source localization/qps-ploc.json `
  --ids-output include/th03/app/new_classic_message_ids.inc `
  --data-output src/app/new_classic_messages_generated.inc
```

Compile an external pack:

```powershell
python tools/localization/compile_language_pack.py compile `
  --schema localization/schema.json --source my-locale.json `
  --output my-locale.t3lang
```

An authoring file may omit messages only when it names a fallback locale. The
runtime must be supplied that exact accepted fallback catalog; it never guesses
or silently substitutes a different language.

Source-constrained in-game graphics text uses the separate `T3GTL001` format
documented in `GAME_TEXT_README.md`. Keeping the domains separate prevents a
menu-font pack from claiming CP932/gaiji coverage it cannot render. This
domain now includes every executable-resident stage/name/result label plus all
297 win-quote rows and all 380 Music Room title/comment rows. The tracked
`game-fixed-text-ja-JP.json` is regenerated from the original Japanese DAT;
the complete English and Japanese authors merge the same rows from their
authenticated executables and DAT assets.

All pre/post-round dialogue, cutscene narration, and ending text uses the
script-aware `T3CTP001` domain. The complete editable sources are
`cutscene-text-en-US.json` and `cutscene-text-ja-JP.json`; their adjacent
`*-schema.json` files contain source identity and geometry but no original
script bytes. Compile either catalog without extracting a DAT archive:

```powershell
python tools/localization/compile_cutscene_text_pack.py `
  --schema localization/cutscene-text-en-US-schema.json `
  --source localization/cutscene-text-en-US.json `
  --output localization/cutscene-text-en-US.t3ctp
```

Each `exact_glyphs` value is a source-layout constraint: one cell is one
fullwidth CP932 glyph or two halfwidth glyphs. See
`docs/NEW_CLASSIC_CUTSCENE_TEXT.md` for inventory regeneration and runtime
validation.

The definitive covered-versus-image-baked census is
`docs/NEW_CLASSIC_TEXT_LOCALIZATION_COVERAGE.md`.

## Complete release bundles

English and Japanese cover all three production text domains. Build both,
render the fixed game-text contact sheets through the production font path, and
write a deterministic source/artifact manifest with one command:

```powershell
python tools/localization/build_locale_bundle.py build `
  --font path/to/FONT.ROM `
  --preview-tool out/build/gcc/th03-game-text-preview.exe `
  --menu-preview-tool out/build/gcc/th03-menu-text-preview.exe `
  --cutscene-preview-tool out/build/gcc/th03-cutscene-text-preview.exe `
  --replay-patch-assets path/to/AZINN.DAT `
  --menu-backgrounds assets/new-classic/menu-backgrounds `
  --output out/locale-bundle
```

Repeat the command with `check` instead of `build` to rebuild independently and
compare every candidate file without modifying it. `--locale en-US` or
`--locale ja-JP` limits the command to one complete locale. Compilation and
previewing happen in a private staging directory; a failed build leaves an
existing candidate untouched.

The bundle contains 40 fixed game-text sheets, 41 menu-text sheets, and the
complete set of compact scripted-text sheets per locale. The menu sheets format
every dynamic placeholder with its schema width-test value and draw through the
exact mixed MNUFONT/FONT.ROM production path. Scripted-text sheets render every
validated T3CTP replacement run through production graphics text. See
`docs/NEW_CLASSIC_MENU_TEXT_PREVIEW.md` and
`docs/NEW_CLASSIC_CUTSCENE_TEXT_PREVIEW.md` for direct use and trust boundaries.
