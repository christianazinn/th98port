# New Classic in-game text packs

`game-text-schema.json` freezes stable source-constrained message identifiers,
CP932 encoding, and exact byte geometry. Translator-facing `game-text-*.json`
files are UTF-8. The compiler emits deterministic `T3GTL001` packages and the
runtime independently revalidates the schema digest, body digest, locale and
fallback contract, CP932 structure, and every exact row length.

This is deliberately separate from `T3LNG001`: that format is printable-ASCII
text rendered with authenticated MNUFONT, while this domain feeds the PC-98
graphics-text and result-label paths. Classic never loads either format.

Generate the frozen C++ IDs and built-in English/Japanese tables:

```powershell
python tools/localization/compile_game_text_pack.py generate-cpp `
  --schema localization/game-text-schema.json `
  --source localization/game-text-en-US.json `
  --source localization/game-text-ja-JP.json `
  --ids-output include/th03/app/new_classic/content/new_classic_game_text_ids.inc `
  --data-output src/app/new_classic/content/new_classic_game_text_generated.inc
```

Compile an external catalog:

```powershell
python tools/localization/compile_game_text_pack.py compile `
  --schema localization/game-text-schema.json `
  --source localization/game-text-my-locale.json `
  --output game-text-my-locale.t3gtl
```

Render all 779 compiled messages through the production PC-98 font and
graphics-text path without launching the game:

```powershell
out/build/gcc/th03-game-text-preview.exe `
  --font path/to/FONT.ROM `
  --output out/game-text-preview `
  --game-text-pack game-text-my-locale.t3gtl
```

The 45 640x400 PPM contact sheets and deterministic `preview.json` make glyph
coverage, exact row widths, and placement reviewable in one pass. They cover
the 61 executable-resident labels, 41 localized image-text labels, all 297
win-quote rows, and all 380 Music Room title/comment rows. See
`docs/NEW_CLASSIC_GAME_TEXT_PREVIEW.md` for the output contract.

The fixed DAT-resident rows can be regenerated directly from a legal TH03
archive or extracted member directory:

```powershell
python tools/localization/make_fixed_game_text_author.py `
  --archive path/to/YUME1.DAT `
  --locale ja-JP `
  --output game-fixed-text-ja-JP.json
```

Pass `--base-author` to merge those rows into a full `T3GTL-AUTHOR-1`
catalog. The extractor preserves exact 58-byte win rows and 40-byte Music Room
rows, validates CP932 and CRLF geometry, and accepts the English overlay's one
source-authentic exception: its final Ellen row omits only the trailing CRLF.

Build the complete Japanese author from those fixed rows plus the exact
executable-resident text authenticated by the port:

```powershell
python tools/localization/make_japanese_game_text_author.py `
  --schema localization/game-text-schema.json `
  --fixed localization/game-fixed-text-ja-JP.json `
  --op path/to/unpacked/OP.EXE --mainl path/to/unpacked/MAINL.EXE `
  --main path/to/unpacked/MAIN.EXE `
  --output localization/game-text-ja-JP.json
```

The tool accepts only the frozen original Japanese executable hashes. It
extracts all 19 Music Room labels, nine stage titles/names, ten registration
names, and five result labels, then verifies that the merged catalog contains
all 779 schema entries in exact CP932 geometry.

An authoring file may omit entries only when it names an already validated
fallback locale. The current runtime renderer is CP932-constrained; future
Unicode font work can add another explicit domain without reinterpreting these
bytes.

Scripted dialogue uses a separate run inventory because its authenticated
scripts also contain control, timing, picture, page, and audio commands. The
repository now includes complete UTF-8 English and Japanese author catalogs
plus source-text-free schemas, so translators do not need to extract DAT
members just to start or compile a locale. Maintainers can still regenerate
those schemas through the production script VM. See
`docs/NEW_CLASSIC_CUTSCENE_TEXT.md` for the complete workflow and current
runtime-substitution boundary.
