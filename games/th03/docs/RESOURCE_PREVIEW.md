# Authenticated resource preview

`th03-resource-preview` renders one decoded PI, CDG, or BFNT resource to a
deterministic 4-bpp BMP. It exists for asset audits and localization planning:
the tool uses the native product's production decoders and PI/CDG/superimpose
presentation paths instead of a second conversion library.

```text
th03-resource-preview --input SLWIN.CDG --type cdg \
  --palette TLSL.RGB --discard-alpha --output SLWIN.bmp
```

The accepted types are `pi`, `cdg`, `bfnt-superimpose`, and `bfnt-gaiji`.
Multi-cel CDGs and BFNT glyphs are packed left-to-right into a bounded atlas.
PI and superimpose BFNT inputs use their inline palette when present;
`--palette` supplies or deliberately overrides it. CDG and monochrome gaiji
previews require an external 48-byte RGB4 palette. A gaiji set is drawn as
white source bits over the selected background index because its gameplay
color belongs to the text-cell consumer rather than to the BFNT file.

`--discard-alpha` mirrors source no-alpha CDG loads such as `SLWIN.CDG`.
Otherwise the production alpha put is used. `--background 0..15` controls the
initial page color so alpha behavior can be inspected. Output is published by
no-replace rename from a partial sibling; an existing destination is never
overwritten.

Original game resources remain private inputs and must never be added to the
repository or logs. Preview outputs are diagnostic scratch artifacts, not
replacement assets.
