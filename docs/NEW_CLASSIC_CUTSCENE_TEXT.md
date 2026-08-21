# New Classic scripted-text inventory

The `th03-cutscene-text` developer tool inventories all 28 authenticated TH03
cutscene and ending scripts through the same `CutsceneScriptVm` that renders
them in the game. It does not parse a second copy of the script language and it
does not rewrite commands, waits, pictures, page changes, or audio operations.

The tool emits deterministic `T3CTR001` JSON to standard output. Each maximal
glyph run receives a stable identifier plus its source offset, original screen
coordinates, exact glyph count, CP932 bytes, and source digest. A run ends at
every non-glyph source operation and at a VM-generated full text-box restore.
The original script and extracted Japanese text are external, locally derived
inputs and must not be committed.

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
  --output cutscene-text-ja-JP.json
```

The scaffold is `T3CTR-AUTHOR-1`. Every message retains an `exact_glyphs`
constraint. Missing scripts, missing or duplicated run IDs, malformed CP932,
changed source identities, and invalid geometry fail before output is written.
This is an authoring/inventory boundary, not a runtime language pack. The next
parcel will compile replacements and bind them to the production VM while
preserving the authenticated control and timing stream.

The authenticated original corpus currently yields 1,533 runs and 25,849
glyphs; the longest run is 39 glyphs. Repeated extraction is byte-identical.
These aggregate facts are safe to record, but the generated inventory and
author scaffold are not redistribution artifacts.
