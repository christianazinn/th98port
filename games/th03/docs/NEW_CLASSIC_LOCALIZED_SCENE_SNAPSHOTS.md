# New Classic localized scene snapshots

`th03-localized-scene-snapshot` renders one deterministic, production-composed
640x400 snapshot for each of TH03's 28 authenticated dialogue, ending, and
extra-ending scripts. Unlike the `T3STP001` translator contact sheets, these
images retain the real decoded PI background, palette, script coordinates,
page operations, and PC-98 font path used by the game.

For each script, `T3LSS001` selects the longest localized text run, with the
run nearest the script midpoint winning ties. This avoids over-selecting shared
opening text while making the 28 images useful layout-stress cases without
publishing thousands of nearly identical glyph-by-glyph frames. The tool still
runs every script to its authenticated terminator and verifies that the
complete localized glyph stream was consumed.

Japanese uses the extracted source content directly:

```powershell
out/build/gcc/th03-localized-scene-snapshot.exe `
  --content X:/path/to/content `
  --cutscene-text-pack localization/cutscene-text-ja-JP.t3ctp `
  --output out/scene-snapshots-ja-JP
```

English scripts come from the authenticated language overlay:

```powershell
out/build/gcc/th03-localized-scene-snapshot.exe `
  --content X:/path/to/content `
  --language-overlay X:/path/to/content/TH3EN.DAT `
  --cutscene-text-pack localization/cutscene-text-en-US.t3ctp `
  --output out/scene-snapshots-en-US
```

The output directory must not exist. The tool first composes all scenes in
memory, writes BMPs and `snapshots.json` into a sibling partial directory, and
renames that directory only after every file succeeds. Failure therefore never
publishes a partial snapshot set. Original game bytes remain external and no
snapshot output belongs in the repository.
