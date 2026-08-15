# New Classic localization packs

`schema.json` freezes stable message IDs, source text domains, placeholders,
and pixel-width budgets. Translator-facing `*.json` files are UTF-8. The
compiler emits deterministic `T3LNG001` packages; the native loader then
revalidates the schema digest, completeness/fallback, placeholders, printable
MNUFONT glyphs, and exact proportional pixel widths before accepting a locale.

The initial complete domain is the New Classic title and hierarchical Option
UI. It is deliberately limited to printable ASCII because it renders through
the authenticated Replay Patch `MNUFONT.PF`. This is not the future
Unicode-capable product-shell domain, and Classic never reads these files.

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
