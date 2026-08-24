# Courier Prime Character Select display face

`CourierPrime-Regular.ttf` is the regular Courier Prime face from the official
Google Fonts repository. It supplies only the wide monospaced serif role used
when a generated Character Select name sheet cannot retain an authenticated
Japanese or English source sheet.

- upstream: `https://github.com/google/fonts/tree/main/ofl/courierprime`
- source SHA-256: `72f793376f8e2841656bf21d77a5de010f2929bd6956a22ee848ad0c7eb978af`
- license: SIL Open Font License 1.1, in `OFL-1.1.txt`
- rasterizer: Pillow 10.2.0 with its FreeType 2.13.2 build

Regenerate the tracked deterministic bitmap role with:

```text
python tools/localization/compile_character_select_role_font.py \
  third_party/courier-prime/CourierPrime-Regular.ttf \
  localization/character-select-name-glyphs.txt \
  tools/localization/T3CSF001-NAMES.BIN \
  --pixel-size 16 \
  --source-sha256 72f793376f8e2841656bf21d77a5de010f2929bd6956a22ee848ad0c7eb978af
```

The role package freezes monochrome glyph pixels and a 10-pixel advance. The
compiler rejects other rasterizer versions rather than publishing an
environment-dependent bitmap. The runtime does not load the TTF or any
host-installed font.
