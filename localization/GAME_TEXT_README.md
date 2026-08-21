# New Classic in-game text packs

`game-text-schema.json` freezes stable source-constrained message identifiers,
CP932 encoding, and exact byte geometry. Translator-facing `game-text-*.json`
files are UTF-8. The compiler emits deterministic `T3GTL001` packages and the
runtime independently revalidates the schema digest, body digest, locale and
fallback contract, CP932 structure, and every exact row length.

This is deliberately separate from `T3LNG001`: that format is printable-ASCII
text rendered with authenticated MNUFONT, while this domain feeds the PC-98
graphics-text and result-label paths. Classic never loads either format.

Generate the frozen C++ IDs and built-in English table:

```powershell
python tools/localization/compile_game_text_pack.py generate-cpp `
  --schema localization/game-text-schema.json `
  --source localization/game-text-en-US.json `
  --ids-output include/th03/app/new_classic_game_text_ids.inc `
  --data-output src/app/new_classic_game_text_generated.inc
```

Compile an external catalog:

```powershell
python tools/localization/compile_game_text_pack.py compile `
  --schema localization/game-text-schema.json `
  --source localization/game-text-my-locale.json `
  --output game-text-my-locale.t3gtl
```

An authoring file may omit entries only when it names an already validated
fallback locale. The current runtime renderer is CP932-constrained; future
Unicode font work can add another explicit domain without reinterpreting these
bytes.

Scripted dialogue uses a separate run inventory because its authenticated
scripts also contain control, timing, picture, page, and audio commands. Build
`th03-cutscene-text`, run it against a legal content installation, and pass its
`T3CTR001` output to `make_cutscene_text_author.py` to create a UTF-8
`T3CTR-AUTHOR-1` scaffold. Both tools use the production script VM rather than
a shadow parser. See `docs/NEW_CLASSIC_CUTSCENE_TEXT.md` for the complete
workflow and current runtime-substitution boundary.
