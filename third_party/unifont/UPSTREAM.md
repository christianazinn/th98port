# GNU Unifont fallback glyphs

`assets/new-classic/menu-backgrounds/T3UFN001.BIN` is deterministically
compiled from the official GNU Unifont 17.0.05 `unifont-17.0.05.hex.gz`
source. The compiler includes every valid glyph record in that source and
filters reserved Unicode noncharacters. The decompressed `.hex` SHA-256 is
`fd79af3613ec1b984a98d33428fdd43fcf06018d18059960d78edeb63d958622`;
the downloaded gzip SHA-256 is
`2ae5311c8e123e9e85f5331cd012aa99757071df23243f1487fdbf8f3acd86be`.

Regenerate with:

```text
python tools/localization/compile_unicode_font.py \
  unifont-17.0.05.hex.gz \
  assets/new-classic/menu-backgrounds/T3UFN001.BIN \
  --source-sha256 fd79af3613ec1b984a98d33428fdd43fcf06018d18059960d78edeb63d958622
```

The runtime continues to prefer authenticated `MNUFONT.PF` ASCII and composes
Latin diacritics over those exact glyphs. T3UFN001 is the deterministic last
resort for code points those source fonts cannot represent. It is distributed
under the adjacent SIL Open Font License 1.1; no host-installed font is read.
