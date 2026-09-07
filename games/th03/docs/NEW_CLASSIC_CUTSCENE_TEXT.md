# New Classic scripted-text inventory

The `th03-cutscene-text` developer tool inventories all 28 authenticated TH03
cutscene and ending scripts through the same `CutsceneScriptVm` that renders
them in the game. It does not parse a second copy of the script language and it
does not rewrite commands, waits, pictures, page changes, or audio operations.

The tool emits deterministic `T3CTR001` JSON to standard output. Each maximal
glyph run receives a stable identifier plus its source offset, original screen
coordinates, exact glyph count, CP932 bytes, and source digest. A run ends at
every non-glyph source operation and at a VM-generated full text-box restore.
The original binary scripts remain external inputs. The repository tracks
separate source-profile schemas plus UTF-8 Japanese and English author
catalogs; it does not track `T3CTR001` inventories or original script bytes.

Build and extract an inventory from a legal content installation:

```powershell
cmake --build out/build/gcc --target th03-cutscene-text
out/build/gcc/th03-cutscene-text.exe --data X:\path\to\content `
  > inventory.json
```

Generate a UTF-8 translator scaffold:

```powershell
python tools/localization/make_cutscene_text_author.py `
  --inventory inventory.json `
  --locale ja-JP `
  --output cutscene-text-ja-JP.json `
  --schema-output cutscene-text-ja-JP-schema.json
```

For a locale based on the complete English asset profile, inventory that
profile rather than the Japanese source:

```powershell
out/build/gcc/th03-cutscene-text.exe --data X:\path\to\content `
  --language-overlay X:\path\to\TH3EN.DAT > inventory-en.json
python tools/localization/make_cutscene_text_author.py `
  --inventory inventory-en.json --locale en-US `
  --output cutscene-text-en-US.json `
  --schema-output cutscene-text-en-US-schema.json
```

The scaffold is `T3CTR-AUTHOR-1`. Every message retains an `exact_glyphs`
constraint. One source VM iteration is exactly 16 pixels and two bytes: it can
contain one fullwidth CP932 glyph or two halfwidth glyphs. The English overlay's
source-private `0xA0` blank is presented to translators as an ordinary space;
both have the same width and blank pixels.

Compile the complete catalog and validate it against all 28 selected scripts:

```powershell
python tools/localization/compile_cutscene_text_pack.py `
  --schema localization/cutscene-text-en-US-schema.json `
  --source localization/cutscene-text-en-US.json `
  --output cutscene-text-en-US.t3ctp
out/build/gcc/th03-cutscene-text.exe --data X:\path\to\content `
  --language-overlay X:\path\to\TH3EN.DAT `
  --cutscene-text-pack cutscene-text-en-US.t3ctp > inventory-validated.json
```

`T3CTR-SCHEMA-1` contains the stable script/run identity, geometry, and source
hashes required by the compiler, but no source text. Translators can therefore
copy either tracked author catalog and compile it entirely from the repository;
maintainers can regenerate the schema from a legal installation to prove that
the source contract has not drifted.

`T3CTP001` has a deterministic header/body digest, locale and source-profile
identity, complete script/run directories, and per-run source identities.
Missing scripts, missing or duplicated run IDs, malformed CP932 cells, changed
source identities, invalid geometry, and partial catalogs fail before output is
written or the locale is selected.

Use the pack in New Classic or the replay renderer with
`--cutscene-text-pack <locale.t3ctp>`. A Japanese pack authenticates against the
original script profile. Any other locale authenticates against the complete
English overlay and must use the same locale as the selected menu/game-text
catalogs. Live New Classic language switching applies a catalog only to its
matching source profile. The maintained standalone's Japanese and English
choices retain the original or English Patch glyph presentation; their tracked
catalogs are reproducible inventory/authoring baselines, not a reason to redraw
identical source text. An explicitly supplied custom pack still replaces glyph
runs. The runtime never edits or reparses translated script bytes: the
authenticated source VM emits its normal operations, and only decoded
`CutsceneDrawGlyph` payloads are substituted. Commands, waits, cursor movement,
page changes, pictures, fades, audio, and input boundaries therefore remain
source-owned. Classic and stock Japanese never open a pack.

The authenticated Japanese profile currently yields 1,533 runs and 25,849
glyph cells, with a 39-cell maximum run. The English profile yields 1,732 runs
and 32,847 cells, with a 30-cell maximum run. Repeated extraction, schema
generation, author generation, and package compilation are byte-identical. The
complete tracked catalogs live beside `game-text-*.json` under `localization/`.
