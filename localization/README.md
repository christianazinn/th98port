# New Classic localization packs

`schema.json` freezes stable message IDs, source text domains, placeholders,
and pixel-width budgets. Translator-facing `*.json` files are UTF-8. The
compiler emits deterministic `T3LNG001` packages; the native loader then
revalidates the schema digest, completeness/fallback, placeholders, printable
MNUFONT glyphs, and exact proportional pixel widths before accepting a locale.

The complete domains currently include the New Classic title, hierarchical
Option UI, authenticated Replay browser/save flow, Practice setup, KeyConfig,
JoyConfig, controller assignment, and Input Test. Dynamic templates declare
named placeholders together with conservative width-test values, so a
translator cannot evade a row budget by leaving the runtime value unresolved.
Host key and controller names remain platform data, but their surrounding
labels and bounded row templates are localized. The format is deliberately
limited to printable ASCII because these screens render through the
authenticated Replay Patch `MNUFONT.PF`. This is not the future Unicode-capable
product-shell domain, and Classic never reads these files.

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
menu-font pack from claiming CP932/gaiji coverage it cannot render.
