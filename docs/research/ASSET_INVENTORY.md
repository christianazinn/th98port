# TH03 asset inventory

Status: inventory complete. ASSET-001 (RGB48 and PI), ASSET-002 (the shared
CDG/CD2 cel format), ASSET-003 (the shared BFT/BF2 BFNT format), and ASSET-004
(the fixed MRS planar format) are implemented with synthetic tracked fixtures
and are warning-clean on GCC/MSVC.
Read-only sweeps pass all 78 external TH03 PI files, all 33 CDG/CD2 files (66
effective cels), all 28 BFT/BF2 files (845 glyphs), and all 36 MRS files under
both stored and horizontally flipped orientations; no original bytes or
derivatives are tracked.

This remains metadata-only research for the post-core assets/presentation
milestone. It intentionally contains no original asset bytes, text, images,
audio, screenshots, or generated derivatives.

## Scope and evidence

- Read-only source inspection: user-owned `originals/th03_full` and its existing `dat_dumps/` extraction. No original bytes were copied, changed, committed, or recorded as content.
- `dat_dumps/YUME1.DAT` and `dat_dumps/YUME2.DAT` are directories of extracted entries, not DAT files. The matching original archive names are non-ASCII Shift-JIS DOS names; the directory aliases are used below to avoid name-encoding changes.
- ReC98 findings were read as static Git objects from pinned Anniversary commit `3e2381d4275be7287f5525de5c95389d492b0a3c`. No emulator or visual validation was run.
- The native port owns `th03_formats::parse_enedat()`, the implemented
  `th03_assets` decoders listed below, and a separate `th03_present`
  compositor. “Missing” below means missing from the native port, not missing
  from ReC98.

“Observed” means read-only metadata/header evidence, “source-backed” means pinned ReC98 code, and “proposed” means a native design. None means visual-parity verification.

## Archive/source identity

| Source | Form | Bytes | SHA-256 | Category |
|---|---|---:|---|---|
| YUME1.DAT | original archive; dump alias `dat_dumps/YUME1.DAT/` | 1,467,184 | `55a490346a045c231e2ce694b0d35a33c12168813b5a627d652d573980d28ccc` | DAT pack archive; graphics/music/text |
| YUME2.DAT | original archive; dump alias `dat_dumps/YUME2.DAT/` | 583,719 | `5ae8538de8e8e30a0cfd588840af680e3f167a4a5ca0951faac888b31a175a5c` | DAT pack archive; portraits/fonts/ENEDAT |
| YUME1.DAT extracted manifest | 180 rows sorted as `name<TAB>size<TAB>sha256` | 1,778,334 decoded bytes | `5c437d22c3d9286f4d4fd8313753e2013d724fe0c832a5dbb76ee93512869f41` | extracted payload identity |
| YUME2.DAT extracted manifest | 60 rows sorted as `name<TAB>size<TAB>sha256` | 1,304,464 decoded bytes | `9b58c739cc7304d2157d660837109fb7410ed12563e57bcba25e9fdc26bbda48` | extracted payload identity |

Source-backed archive semantics: `th03/formats/pfopen.asm` recognizes stored type `0xF388` and LEN/RLE type `0x9595`; `libs/master.lib/pfint21.asm` reads the 16-byte archive header, decrypts its entry table, and applies the auxiliary transform. A TH03 entry is 32 bytes: type, auxiliary byte, 13-byte DOS name, packed/original 16-bit sizes, and offset. Native import should use `thdat` (or an independently tested equivalent) outside `th03_assets`; the library consumes extracted entry spans, never archive paths.

## Observed format census

| Extension | Files | Bytes | Category |
|---|---:|---:|---|
| .PI | 78 | 807,690 | compressed indexed graphics |
| .CDG / .CD2 | 33 | 726,128 | planar cels / multi-cels |
| .MRS | 36 | 1,192,320 | portrait/bomb planar cels |
| .BFT / .BF2 | 28 | 178,080 | BFNT glyph/sprite sheets |
| .RGB | 4 | 192 | 16-color palettes |
| .M / .EFC | 22 | 68,415 | PMD songs/effect bank |
| .TXT | 43 | 151,716 | script/text/manual metadata |
| .DAT, .CFG, .NEM | 6 | 2,065,079 | archive/gameplay/persistence/unclassified |
| .EXE, .COM, .BAT, .SYS | 15 | 450,522 | game, PMD, DOS, launch support |

## Header validation and rendering semantics

| Format | Observed / source-backed facts | Faithful representation |
|---|---|---|
| PI | All 78 have `Pi` framing, mode `0x00`, four planes and inline 48-byte palette. Correct big-endian dimensions: 320x16 (18), 320x128 (12), 320x144 (6), 640x48 (1), 640x96 (5), 640x192 (5), and 640x400 (31), totaling 9,748,480 decoded indices. Every semantic stream reaches its declared output before at least three trailing zero padding bytes. `libs/master.lib/graph_pi_load_pack.asm` implements an adaptive color/history plus position/run bitstream decoder and stops at the declared output size without reading a terminator, so the native decoder intentionally tolerates that trailer. | Exact 4-bit indices, dimensions, mode, and inline RGB4 palette. Do not use PNG/RGBA as authoritative storage; retain the palette-suppression bit even though observed files include palettes. |
| CDG / CD2 | Both use the same uncompressed 16-byte little-endian TH03 CDG header followed by alpha then B/R/G/E planes per cel. CDG single-image files use `image_count == 0`; CD2 contains 2, 4, 5, 9, or 10 cels. Stock widths range from 32 to 512 pixels and heights from 24 to 230 pixels. `cdg_load_all()` is shared; neither loader nor header examines a filename extension. | Keep plane size, width/height, bottom-left VRAM offset, dword width, all four color planes, optional alpha, bottom-to-top row order, MSB-left bit order, 32-pixel source width alignment, and the renderer's 8-pixel X-position alignment. Preserve distinct source alpha-aware, no-alpha, hflip, dissolve, and upward-unput operations; do not collapse the GRCG mask into conventional RGBA alpha before compositor tests. |
| MRS | Every one of 36 is 33,120 bytes: five 6,624-byte planes, source-defined as headerless/uncompressed/upside-down 288x184, plane order alpha+B/R/G/E. There is no palette or other header metadata. | Keep the fixed 36-byte row width, 184 bottom-to-top rows, MSB-left bits, 14,640-byte bottom-left VRAM offset, nine-dword row width, alpha, and explicit horizontal orientation. `mrs_put_8()` uses alpha. Regular `mrs_put_noalpha_8()` ignores it, while the altered-color mode reads alpha for `~alpha | B`. The persistent source hflip reverses bits in every byte and then byte order in every row of all five planes. |
| BFT / BF2 | All 28 begin `BFNT\x1A` and use the 32-byte little-endian `bfnt_header`; all stock files have version `0x16` and no extended header. BF2 is not another codec: ReC98 calls `super_entry_bfnt()` on it. The 26 `col=0x83` sheets contain an inline B/R/G high-nibble palette followed by high-nibble-first packed 4-bit pixels, which `bfnt_entry_pat()` transposes to B/R/G/E planes. The two `col=0` BFT sheets contain raw 1bpp 16x16 gaiji for codes 0 through 255. | One BFNT decoder retaining the complete portable header, glyph code range/cell geometry, raw extended header, optional effective transparent-color record, RGB4 palette, and top-to-bottom/MSB-left decoded pixels. Consumer explicitly selects gaiji monochrome vs superimpose planar semantics; never branch on extension. The superimpose mask is derived later from the effective transparent color, which defaults to 0. |
| RGB | Four files are exactly 48 bytes. `palette_entry_rgb.asm` loads 16 RGB4 entries and expands each `v` to `v << 4 | v`. | Retain source order and RGB4 values until the palette/hardware boundary. |
| ENEDAT.DAT | 5,072 bytes; payload size is 5,068 and reserved word zero. | Gameplay data: retain existing `parse_enedat(std::span<const std::byte>)` in `th03_formats`, not `th03_assets`. |
| M / EFC | 21 PMD songs and one PMD effect bank; ReC98 `snd_load()` sends them to PMD work buffers. | Preserve content identity; implement PMD sequence/effect interpretation plus YM2608 synthesis, not WAV conversion. |
| @*.TXT / MUSIC.TXT | The 43 files are 28 variable-length cutscene scripts, nine fixed 11 x 3 x 60-byte win-message tables, one fixed 19 x 20 x 42-byte Music Room table, and five root documents. The stock corpus is CP932-valid; the cutscene VM itself treats non-control text as unchecked two-byte glyphs. | Preserve original byte spans and command order. Cutscene parsing/timing belongs to `th03_present`; win/Music Room fixed-record readers belong to their clients. See [`ASSET_TEXT_SCRIPTS.md`](ASSET_TEXT_SCRIPTS.md). |

## Decoder mapping and gaps

| Input | ReC98/THTK authority | Native disposition |
|---|---|---|
| top-level DAT | `th03/formats/pfopen.asm`, `libs/master.lib/pfint21.asm`; `thdat` | Importer-only extraction/provenance boundary; no assets-library archive decoder. |
| PI | `graph_pi_load_pack.asm`; `th02/formats/pi_load.cpp`, `pi_put.cpp`; TH03 shim | Implemented as strict `decode_pi()` over a caller-owned span. |
| CDG/CD2 | `th03/formats/cdg.h`, `th03/formats/cdg.inc`, `th03/formats/cdg_load.cpp`, `th03/formats/cdg_put.asm`, `th03/cdg_p_na.asm` | Implemented as one strict extension-agnostic `decode_cdg()`/`decode_cdg_cel()` path. The model retains raw alpha+B/R/G/E planes and source geometry; alpha discard is an explicit policy matching the no-alpha loaders. Compositor effects remain presentation work. |
| MRS | `th03/formats/mrs.hpp`, `mrs.cpp`, `main/hitc_mrs.cpp`, plus every `mrs_put_*()` caller | Implemented as strict `decode_mrs()` over exactly 33,120 caller-owned bytes. It returns a fixed `MrsImage`/`PlanarCel4` with alpha+B/R/G/E planes. `horizontal_flip()` applies the source's persistent, involutive transformation and records the resulting orientation. No filename or extension reaches the decoder. |
| BFT/BF2 | `libs/master.lib/super.inc`, `bfnt_header_read.asm`, `bfnt_header_analysis.asm`, `bfnt_extend_header_skip.asm`, `bfnt_palette_set.asm`, `bfnt_entry_pat.asm`, `gaiji_entry_bfnt.asm`, `super_entry_bfnt.asm` | Implemented as one strict extension-agnostic `decode_bfnt()` path with an explicit gaiji/superimpose policy. Gaiji retains raw skipped extension bytes and monochrome glyphs; superimpose follows bounded linked records for tag `0x10`, converts B/R/G palette order, and exactly transposes packed indices to B/R/G/E planes. |
| RGB | `libs/master.lib/palette_entry_rgb.asm`, `platform/x86real/pc98/palette.cpp` | Implemented as exact-length `decode_rgb48()` retaining RGB4 source components. |
| ENEDAT | ReC98 `formats/enedat.hpp`/enemy loader; native `src/formats/enedat.cpp` | Already `th03_formats`, not assets. |
| M/EFC/PMD | `th02/snd/snd.h` and TH03 sound shims; external PMD driver | Missing PMD/YM2608 stack; separate audio parcel. |
| TXT scripts | `th03/formats/script.hpp`, `th03/cutscene/cutscene.cpp`, MAINL callers | Format and ownership are fully inventoried in [`ASSET_TEXT_SCRIPTS.md`](ASSET_TEXT_SCRIPTS.md). A bounded cutscene VM is deferred until a MAINL client consumes it; the two fixed-record tables are client-local data. |
| YUME.CFG / YUME.NEM | `th03/formats/cfg.hpp`, `th03/formats/scoredat.hpp`, score load/save code | Mutable configuration and high-score/unlock persistence, outside core/assets. Native migration policy and parsers are deferred to the application layer. |
| TOUHOU.DAT | shared TH01-TH03 `th01/zunsoft.cpp`; TH03 `GAME.BAT` invokes `ZUN.COM -3`, whose TH03 build embeds the shared logo program | Authenticated 8,272-byte, palette-bearing 64x16 superimpose BFNT (`ZUNOP`, codes 0..15) consumed only by the pre-OP ZUN Soft opening. |
| PMD/DOS executables | PMD binaries are external support programs | External support programs; do not import or redistribute without separate authority. |

## Proposed `th03_assets` boundary

Pure portable library: caller-owned `std::span<const std::byte>` in; typed model or offset-rich `AssetError` out. It must not open a file, invoke `thdat`, discover an installation, write cache data, create GPU resources, emulate PMD, or know a window.

```text
filesystem/Data Manager importer
  ownership check + archive extraction + Shift-JIS/case resolution
  source manifest + cache policy
                     |
                     v
            th03_assets (span-only decoders)
  PI / CDG / MRS / BFNT / RGB -> neutral indexed/planar models
                     |
                     v
th03_present canonical compositor -> platform texture upload

th03_formats::parse_enedat -> GameContent -> th03_core
PMD sequence/effect adapter -> semantic audio events -> audio backend
```

Neutral models: `Palette16Rgb4` (source-order RGB4); `PlanarCel4` (B/R/G/E, optional alpha, source row/bit orientation, 8-pixel alignment and geometry); `PiImage` (4-bit indices, palette/mode); `CdgFile` (header+cels, retaining zero single-image count); `BfntSheet` (header/palette/glyph range/cells); and `MrsImage` (fixed planar cel). Decoders reject truncation, invalid dimensions/counts, overflow, payload mismatch and unsupported combinations before allocation. Models own decoded memory; no stored view outlives the caller span.

MRS has no format-specific master.lib codec. The original loader composes the
generic `file_ropen()` / `file_read()` / `file_close()` calls with
`hmem_allocbyte()` and reads one `mrs_t` verbatim. Its renderers then use
`grcg_setcolor()` and direct B/R/G/E VRAM writes. `mrs_put_noalpha_8()`'s
optional altered-color expression (`~alpha | B`, with R/G/E unchanged) is a
presentation operation rather than decoded asset state.

### MRS source audit

- `mrs_load()` allocates and reads exactly one fixed `mrs_t`; it parses no
  header and never examines the filename. `mrs_free()` only releases that huge
  allocation. The DOS code assumes trusted complete files, while the native
  span boundary rejects both short and trailing data.
- `mrs_put_8()` traverses rows from the stored bottom row upward. It consumes
  alpha first through the GRCG mask path, then conditionally ORs the matching
  B/R/G/E dword. `mrs_put_noalpha_8()` directly writes B/R/G/E; only its
  altered-color branch also reads alpha for the B expression above.
- `mrs_hflip()` transforms all 33,120 bytes, then swaps bytes independently in
  each of the 920 stored rows (five planes times 184 rows). Therefore the
  portable operation flips alpha and all four colors and is exactly
  involutive.
- `main/hitc_mrs.cpp` loads each player's EX portrait into slots 0/1 and BM
  image into slots 2/3, then flips the right-playfield slots 1 and 3.
  `main/player/warning.cpp` is the alpha-aware portrait consumer. `main_05.cpp`,
  `main/player/ch_shot.cpp`, `chybomb.cpp`, `yumemi_bomb.cpp`, and
  `main/bullet/bullet.cpp` use the no-alpha renderer. No other MRS loaders,
  transforms, or renderers occur in the pinned TH03 source.

Versioned cache is above the library: key by source SHA-256, decoder schema/version, importer version and content/profile identity; store source logical name/validation status in user-local `cache/vN/<sha256>/`. Schema/decoder changes invalidate entries. Never commit originals, decoded cache blobs or flattened PNG/WAV. The importer owns legal-install discovery, pinned `thdat` provenance, logical lookup, purge/rebuild and hard mismatch errors.

## Synthetic fixtures

All fixtures are generated in test code or tiny non-game literals—never original bytes, names, screenshots, audio or extracted files.

1. RGB: 48 bytes, source order, deferred nibble expansion, length/truncation.
2. PI: generated valid header/bitstream with known indices; color/history and position/run paths; magic/plane/aspect errors, lower mode-bit preservation, malformed runs and output overflow; compare indices, not RGBA.
3. CDG/CD2: 16-byte header + five synthetic planes; zero single-count, multi-cel CD2, alpha/no-alpha, geometry, malformed payload and selection bounds; test both compositor policies separately.
4. BFNT: 32-byte header + generated monochrome or packed-index glyph data; range/geometry/palette, linked and transparent-color extension records, gaiji/superimpose policy, exact B/R/G/E transposition, bad magic/color and truncated/overlapping/trailing data.
5. MRS: generated 33,120-byte five-plane image; fixed geometry, upside-down mapping, complete alpha+B/R/G/E order, byte+bit hflip and double-flip restoration. Alpha/no-alpha compositing remains a presentation test.
6. Integration: fake importer and temporary cache; hash/schema invalidation, case-insensitive lookup, extraction provenance, and a link test proving no filesystem dependency in `th03_assets`.
7. Keep ENEDAT fixtures in `th03_formats`; add only importer-to-GameContent handoff coverage.

Reference frames start synthetic. User-local original frame captures remain ignored, with only hashes/provenance tracked.

## Decoder implementation parcels

1. **ASSET-001 — indexed foundation, RGB, PI (complete).** Span-only `th03_assets`, palette/index models, `decode_rgb48()`, mechanically translated PI decompression and RGB/PI fixtures pass on both compilers; all 78 external PI files pass the read-only sweep.
2. **ASSET-002 — CDG/CD2 cels (complete).** One shared header/multi-image parser handles stored count zero as one cel, retains alpha+B/R/G/E and exact source/VRAM geometry, supports bounded single-cel selection, and makes alpha discard explicit. Synthetic tests cover single/multi-cel, alpha policy, plane order, geometry and malformed inputs. The DOS loaders assume trusted installation data and perform unchecked reads/seeks; the native span boundary rejects inconsistent geometry, reserved segment values, truncation and trailing payload rather than reproducing unsafe reads. A read-only sweep decoded all 33 external files and 66 effective cels with no extension fork or failures. `th03_present` now owns the source-backed alpha-aware, no-alpha, horizontal-flip, E-plane dissolve, and upward-unput operations.
3. **ASSET-003 — BFNT sheets (complete).** One span decoder handles BFT and BF2 without seeing an extension. The explicit gaiji policy enforces the exact source domain (monochrome 16x16 codes 0–255) and returns raw 1bpp glyphs; the superimpose policy enforces four-bit indexed color, retains palette/transparent-color semantics, and returns B/R/G/E planes. Synthetic strict tests pass on both compilers, and a read-only sweep decoded all 28 external files and 845 glyphs (2 gaiji sheets and 26 superimpose sheets).
4. **ASSET-004 — MRS images (complete).** One fixed-size span decoder owns the decoded alpha+B/R/G/E planes, rejects both truncation and trailing data, and preserves the source's bottom-first/MSB-left geometry. A separate in-memory operation performs the exact persistent hflip without consulting a filename; applying it twice restores every plane. Synthetic full-plane comparisons pass on GCC/MSVC; read-only sweeps decoded and flipped all 36 external files. Alpha-aware, regular no-alpha, and altered-color blits remain presentation operations.

The text/script research parcel is complete in
[`ASSET_TEXT_SCRIPTS.md`](ASSET_TEXT_SCRIPTS.md). PMD/YM2608 remains separate
after licensing/rhythm-data decisions. ENEDAT stays in `th03_formats`.

## Unknown, proprietary, and legally sensitive resources

- Original entries and decoded cache are copyrighted user-owned data. Hashes are identity metadata, not redistribution authorization; import requires a user installation and remains untracked.
- PMD music/effects, PMD binaries, and YM2608 rhythm ROM/sample material require focused licensing/provenance review. An engine license does not authorize distributing game music, effects, drivers, ROMs or captured audio.
- `YUME.NEM` is four encrypted/checksummed 206-byte high-score sections and is
  application persistence, not an imported asset. `TOUHOU.DAT` is a static,
  source-owned logo BFNT; its original bytes remain user-supplied runtime
  content and are never committed or redistributed by the native codebase.
- DAT extraction is an external-tool boundary: pin/test importer-tool versions before claiming reproducible import.

## Per-file metadata manifest

Metadata only. `root/SHIFTJIS-NONASCII-*` aliases non-ASCII DOS names deterministically, avoiding encoding normalization. `YUME1.DAT/` and `YUME2.DAT/` are extraction-directory aliases.

| Source | Path | Ext | Bytes | SHA-256 | Category |
|---|---|---|---:|---|---|
| installation root | root/AUTOEXEC.BAT | .BAT | 6 | `3cc29a7afa1d4edc37a1d828216eee1a6ba9d9d556f2a7838e79b83152deaabe` | launch/system script |
| installation root | root/YUME.CFG | .CFG | 8 | `7c9fa136d4413fa6173637e883b6998d32e1d675f88cddff9dcbcf331820f4b8` | configuration/persistence |
| installation root | root/GAME.BAT | .BAT | 696 | `10a0cabdd6f99ce8b2f49e78cb942dca4a2e120f691d5aa489e84e43e100daf1` | launch/system script |
| installation root | root/YUME.NEM | .NEM | 824 | `ccdd4593fe650c7bceb7dc725bc823cb3b9853af33b8e521e89c34ca30691506` | high-score/unlock persistence |
| installation root | root/SHIFTJIS-NONASCII-1.TXT | .TXT | 4346 | `642efccc80a46f05aeb07dc1076298f2dea3b07dc4ed3c3d039c7b91429bcd2c` | documentation: Shift-JIS text |
| installation root | root/SHIFTJIS-NONASCII-2.TXT | .TXT | 4918 | `7fe20c6341d0181204cf07f4fcd73ca94070b8b590ccb69b79ab46ded1665809` | documentation: Shift-JIS text |
| installation root | root/SHIFTJIS-NONASCII-3.TXT | .TXT | 7075 | `8a1f04c0eda4d3720352d8a09cd08fc3708a9ba6b2bdcdfcdd125f3092b86244` | documentation: Shift-JIS text |
| installation root | root/README.TXT | .TXT | 7410 | `6ac45f51e33442ccba2cf21746e8243d603d02c9d5a65062a77fdaa48ab5e4cd` | documentation: Shift-JIS text |
| installation root | root/TOUHOU.DAT | .DAT | 8272 | `a9c31cf058b528479fe9c5b8fb7a4ac538000c8c2f3b9fb276cbc3dd7aa20532` | ZUN Soft opening BFNT |
| installation root | root/ZUN.COM | .COM | 16242 | `5896bbd673aeb10f0df9c1e34716c64fda83655a788ae1ac0fcf58e3616add27` | PMD/DOS support executable |
| installation root | root/CANBE.EXE | .EXE | 17972 | `c1449649dc90376e6bac997e4e967a978b00a9668ba8ed85a1efea510d82f888` | support executable |
| installation root | root/PMD.COM | .COM | 20379 | `cbbe9bd610aedda586d8bd7f6dd13d21f037a089458a21ddda0b11a53b4b29e4` | PMD/DOS support executable |
| installation root | root/SHIFTJIS-NONASCII-4.TXT | .TXT | 23066 | `5613cbabca5b1e2533cb55f9c114c7ea400b418150aa85c1c66a4a2159dcabeb` | documentation: Shift-JIS text |
| installation root | root/COMMAND.COM | .COM | 24931 | `1d9b48105875d689ecd02be01d8135202c6131f709221b023536b01a00e029e9` | PMD/DOS support executable |
| installation root | root/PMDB2.COM | .COM | 25642 | `39e2855df81b73eec90dde753fa838bafaf8139f828078433b00e143cabc938c` | PMD/DOS support executable |
| installation root | root/PMD86.COM | .COM | 28783 | `a6c6af4d94981c0029afc730189860bc76d9d6e788657bcd4c82793b577e930c` | PMD/DOS support executable |
| installation root | root/OP.EXE | .EXE | 36041 | `d7bfa7f1f8afe0943763ce76437d12ed412b8452716dd64f937c94796eba57ab` | game executable |
| installation root | root/MAINL.EXE | .EXE | 37975 | `5b613023b4ae021794ff778871a732e7af0d0457ccdb3613e3458f2a609e466c` | game executable |
| installation root | root/MAIN.EXE | .EXE | 130882 | `f41fde47ea36bf4d985ff9127b67fe93d5ecb58cc36e7cffab86959db7f2ce6b` | game executable |
| installation root | root/YUME2-original-archive.DAT | .DAT | 583719 | `5ae8538de8e8e30a0cfd588840af680e3f167a4a5ca0951faac888b31a175a5c` | archive: DAT pack |
| installation root | root/YUME1-original-archive.DAT | .DAT | 1467184 | `55a490346a045c231e2ce694b0d35a33c12168813b5a627d652d573980d28ccc` | archive: DAT pack |
| YUME1.DAT extracted | YUME1.DAT/@00DM0.TXT | .TXT | 4770 | `0250023d3d882c9fa3cf51ad1ff2dfc848212d96d4f664c6ad7452d27c823429` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@00DM1.TXT | .TXT | 1885 | `2684e6565c0292a2d60c58c216c15159496247a37f99476e35922c2d9712589c` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@00ED.TXT | .TXT | 4425 | `69a01b0ce7fd33c2fcec3e0d3a9ad0ff177376bc095e68092bf8e5c6f0097267` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@00TX.TXT | .TXT | 1980 | `ec4beb915d0680fa2a5538b32c918a5ed604074c19d7bc86130758f813eabf61` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@01DM0.TXT | .TXT | 4493 | `62abdfb7abcc051d3c6a1bed945c5f9161e0b9a4109e85d0965fe4bb79e4be0e` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@01DM1.TXT | .TXT | 1776 | `8a082d5963e4483905d9051da87cd437ea0f29d00c736b92f63ea9e17306fc3a` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@01ED.TXT | .TXT | 2670 | `c05c6fa4e1fff9ade719f2b6652db0520b79c4dfc2b720324c034191fa042539` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@01TX.TXT | .TXT | 1980 | `18e10e06ada47ae8f2a5d6875f917df6266e05f1d390c97fb17ec252afed4b24` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@02DM0.TXT | .TXT | 4491 | `1f5db81b7d7e3d6b355e3cfdbeca2cc08409baf2c16a1f4a2d7dfa7d362415d0` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@02DM1.TXT | .TXT | 1671 | `72a479a741a6ded1eb6bdd57627b91cdb0f11f86afb048f7ba2db50c8ce78c49` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@02ED.TXT | .TXT | 2597 | `cf8e433f5011d93300080ed6f38d682060fcccfe00ad82c8e11eadd1cdf2e2fc` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@02TX.TXT | .TXT | 1980 | `c534464b0bea8e38773747707e33ce9fa7d85368ee4db8c308faf8d138d9e167` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@03DM0.TXT | .TXT | 3968 | `f9d98ea07a651d0e9e73145d880cbf9ddcbe8673b09cb4df8abfe094aa23f019` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@03DM1.TXT | .TXT | 1620 | `47b8f129b3e65272d35ef756e92ec887bfd9ef405d7efd2a9a752d25c338e9a2` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@03ED.TXT | .TXT | 2532 | `36f507440c0f7ee4a1a4bab577167651e9d3c62b177043d38b0fe43a808e2ee5` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@03TX.TXT | .TXT | 1980 | `44e9026a978acc7f2fbd6e9c912bbdbc79d98c5bb18e9a96342b524cd9138d3d` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@04DM0.TXT | .TXT | 3950 | `4164b4a66222ef8c5479ee24263f84d8c25500667b851513e6bb56c26d8e64b1` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@04DM1.TXT | .TXT | 1532 | `f57b45a44ce694e0e69111314f95f6e9e9dbfba0dd605234ec7cde71396d2f64` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@04ED.TXT | .TXT | 2812 | `0d7ab16f7093339ebe06dfa4f1f3ae9dd79040626cb80ec58ac39acdfcff719e` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@04TX.TXT | .TXT | 1980 | `e9b6d5fb12f384d08456293d32d366621ad04cc7792ceb4b1736fca53d2fb3a3` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@05DM0.TXT | .TXT | 4571 | `fd5d0eaa06b84e8676975211059a1b202a729ccb2b625101099d354cb54db2e1` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@05DM1.TXT | .TXT | 1560 | `25574c31557c546f691beaa0a229609a9d30e95fa89f796ceb299d1ac8520eec` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@05ED.TXT | .TXT | 2426 | `a6417e22dde0220bc55978d75cc67d0dd81aab0cd03ded04921e0c5f07d48977` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@05TX.TXT | .TXT | 1980 | `133cb63b489010bc6b85949762a26f0d298c8e79bfd083bbb055fcd4eeba6679` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@06DM0.TXT | .TXT | 4139 | `1fa900ba0b3d4733eec5a98112959c7c1ac27d4bb4410978429af549e06e8102` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@06DM1.TXT | .TXT | 1816 | `1269442a92bd1eca75749d9e9675a7ff8f03533ecfa9cf283333f05fbfb38fbf` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@06ED.TXT | .TXT | 2376 | `2a72243142517306f15a0e5d3d8e5deaac3f8ea2e59b1ae31103fa46c73fdf1e` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@06TX.TXT | .TXT | 1980 | `34b1067bebff4364b3396d8ceb3271247f71e0a9b62f26c088c40741940422db` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@07DM0.TXT | .TXT | 1741 | `015b48f810edbbc854dfa9a32b031bea09e7dfbd01dcca18725a15334959305f` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@07DM1.TXT | .TXT | 866 | `efa240043c7ae9867fa831c313fd8099cbe3bead1f6790cb57967d2d73f00454` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@07ED.TXT | .TXT | 1611 | `58c831d97bb314782e0588a5a8ffb57fe714e209eaa1a6be3d5f3ea48bc9d128` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@07TX.TXT | .TXT | 1980 | `69bc5a722fbda101cdeacf0ded3db9ecca66e33e7a31b511f6da7a3ed3ffae71` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@08DM0.TXT | .TXT | 1943 | `d5f052c8c14c56d89473e85d37a27cb654ffa84a0b54387da481961d0dc8d2c7` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@08DM1.TXT | .TXT | 578 | `d6c9e41c405cf0d9fc642ec3f4f077b6ac863d544c09f58aba515d31407170cc` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@08ED.TXT | .TXT | 1687 | `0e3257f6fbd35868085b89ce8bfc93c49008b7d9580a41f274fc8079c724922a` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@08TX.TXT | .TXT | 1980 | `f63c39f3f66d468030cd01acda1f81123bf0858d838cdc7535b425e93f905a1e` | text/script |
| YUME1.DAT extracted | YUME1.DAT/@99ED.TXT | .TXT | 615 | `922e55525bf11e77218da8621f0fd9dfbe0eb9e1f5e2bbf846b5daab92ff5f3d` | text/script |
| YUME1.DAT extracted | YUME1.DAT/0016.PI | .PI | 449 | `5ff2993fb519b2edeaeedb97578405e9c5a5f8afb7e9ca2dc92f2cad6d8ef796` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/00EX.PI | .PI | 2530 | `6d4e94a010d1243a42d1620215dab49f7bbb2b5dcb70c59456f73f4249ab020e` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/00MM.M | .M | 3989 | `69543a1e9d17d2c3e7f6fc6901284dad46c220a0d9c13c090505b973812c4038` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/00SL.CD2 | .CD2 | 46096 | `1e69794a8c38a2500c2291e5e66b20c0d71f3b03124d7571055b164fd330cb2f` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/0116.PI | .PI | 447 | `acb34388c22a7a443abb5debde26eedea5498404754be95f88092498e17bc9d2` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/01EX.PI | .PI | 2532 | `97e2092a5d4a466c059270ed613803b8a0e3725af29e6a2ca76e418712a1a6f4` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/01MM.M | .M | 4865 | `78228cbfcff0ab66a220b116230ad764b9886a50f2d72d5dd6802f0a5e247150` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/0216.PI | .PI | 330 | `306be4bae430bd1615de24ab336e525f4466371956e05c6ca6a3030a5872398b` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/02EX.PI | .PI | 2929 | `633050ba87d4f9e2c59489580d1619ef196a884bda2812d1c9be19afc9e56ee9` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/02MM.M | .M | 4911 | `2e148e9f276d21a4e3667243df69b76a1bb8d07ddf5fd37d1c6cb2065e48296b` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/02SL.CD2 | .CD2 | 46096 | `d676c82c8b9e6bd90438c71830aea6f0761228eae62a10f9f5c07533781c0bc8` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/0316.PI | .PI | 326 | `abe7edf8eea2c7d932a54fd36069e80431ffe1d02d917bc9e3e9a6bb6d80de92` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/03EX.PI | .PI | 2931 | `a60d2770abd8298f116fd5efa645ed0895d32d3df3f1b14eb1dea8c9e82094dc` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/03MM.M | .M | 4913 | `05d5540ea63e4dbee720318d2f7efc644baa344ea174dba772f85fccbaf52080` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/0416.PI | .PI | 432 | `df464c56b01ef2bf91528e1c4d8c44c64ad1f284afdbcdd7935b30afa182350c` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/04EX.PI | .PI | 1987 | `3a37284d7644cfc95568d1b9d29811394b84f001f1163accece12f41e88dce98` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/04MM.M | .M | 4880 | `729971e9289212f5da36f0e1c0719a17c2c3d0dff2b403c9609d5c4c687e74c8` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/04SL.CD2 | .CD2 | 46096 | `4b21b1aaad33b91616191978bb7b3bb55a910979502b0796412410d9b0fabca5` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/0516.PI | .PI | 432 | `ba6b7707d20ccb26d2b6bc88c5500272bd67184081aafc97ef69e2c8ef5387cc` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/05EX.PI | .PI | 1953 | `27b9edddf4ab516fecde8bd64eea3a0ea366a1cd95f53b494d99532931a2ae2f` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/05MM.M | .M | 4739 | `5ac0dddd416d9a3e1adf00272d1dc7c8aeb580c3ed4a8173608c7cf89aa3c6d5` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/0616.PI | .PI | 588 | `afcac71952dac4360f1778e6b6923f30f6f8c9d118540d1e0096f4567051a71d` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/06EX.PI | .PI | 1953 | `2704ed37efbdb75aa5d14032ba7f369ed3550031311205b33fd26239c5331a79` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/06MM.M | .M | 7436 | `baada063eacb36a2bb7a2a84a27819a5d78b93fef5bd2b9f4fd8c39e1e443bbe` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/06SL.CD2 | .CD2 | 46096 | `6cfecb95be770a362a86559613328135bf9b9876b6c28bcc0bbceb6f9ea7e431` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/0716.PI | .PI | 585 | `bb36c819f2f4ced365146fb78a3d1d8ae1b238f40e4f6edc821732201716fbb1` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/07EX.PI | .PI | 1855 | `bd37f0072ebe424392cf18b457d26ed697e78be5c2d51fd2a2654b10c21d20bf` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/07MM.M | .M | 4055 | `cfc2d90b9e00d504574c32f6cb5dd7a3234e325c528ad165b7547e17c36988c8` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/0816.PI | .PI | 266 | `6fe8e8a5844377b2019bcb4d39b307d0afced2af10cca046ca438bf0fc248a48` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/08EX.PI | .PI | 2276 | `5a46abeab28b4f688d0422341d98d2c5e2083fdb6efa997463556515e774c836` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/08MM.M | .M | 4569 | `f52a027e759a2ac6a076af2242136587ff6d49ad231d45997dc7dd731fb57d54` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/08SL.CD2 | .CD2 | 46096 | `8999f6ab6d447082b15ac2856693bc63a690bcea0b5980ed91206525951076ae` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/0916.PI | .PI | 266 | `9865d50fbd12632c89e7113d4b9fffc68bbacbce66db1b649af24dfad56a315b` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/09EX.PI | .PI | 2277 | `61a0ae68ac160075b26f9ecfa656029bf9e09f79274e92b47b842c0732cb4b0e` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/1016.PI | .PI | 311 | `1dd56ce116dfccb25c29b8000fa0e1f9e8574549e81d91612b13624401c4dda0` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/10EX.PI | .PI | 4104 | `0b9c57d4e9f85c46744bd769064d79f6ab8f4ef84bd9bf514e9c915b47fa6c9c` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/10SL.CD2 | .CD2 | 46096 | `49111c5ca17da45cc144a3fbcb522b97aaa7f49ceea8a0ae66e64d28e8c876b9` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/1116.PI | .PI | 311 | `c0525c8bdb380e7011d07a47403440bea7d5058b50e426e07bf9ec64f2390233` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/11EX.PI | .PI | 4049 | `93b403cd5a77377e0c88354ccc58976bc59b7efe8bbc50d4e4302f8e6fb9b0be` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/1216.PI | .PI | 265 | `59af1277397e1074b2d38f35394417771d426d4ffc2d7be421c9abd1995abc3c` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/12EX.PI | .PI | 1743 | `037e29cb09e05c22839682402b9ea85b952322fada570bba1622f898b1d2b3ea` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/12SL.CD2 | .CD2 | 46096 | `eacd339ad01a143572ec9053621fffc759b1e1e7d5b7ae5cdeddf2bb328cac5e` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/1316.PI | .PI | 265 | `d5ede6af2819417881bdfcc436ea9acda6787bd8a0e19744dbb191febec37f4a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/13EX.PI | .PI | 1710 | `a421087309f34364885d960fd9f609b41c4c9756443aa044fc8a69366b7ddeb0` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/1416.PI | .PI | 235 | `bfe4127ed424ddca01d8c608991abd649f1e64edfd85719021a8571e0d212d4b` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/14EX.PI | .PI | 2742 | `1bf6fd3a7848dea21a47cc12c5ec77eb93dd663f3d744bba66c8ee9465bcef06` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/14SL.CD2 | .CD2 | 46096 | `8afc234abd50b764163a6b0166e666ee2437ead16f9c407f1e207d27789ad6e7` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/1516.PI | .PI | 236 | `9ed847cddb0aaf5f1e8086e529c7dc830d26bb9acdfb0488d3bf9b5e1fbecc9a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/15EX.PI | .PI | 2717 | `9886430a556facf959c4e5aef2b4dc4fb09ca9daf29c76459952a2a8408ea311` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/1616.PI | .PI | 416 | `d4fb91fe3b903f0413d443a77015daef49874103450a195ea16ea2cacf5ce668` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/16EX.PI | .PI | 3168 | `6338bb41cd9287aab24484c76cab8c729a72de32ff8b70777abf1c45eee68204` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/16SL.CD2 | .CD2 | 46096 | `5de6a11f9a90d125986b34de1757bbb8093ee11f49880b37f7f24481c3e20e68` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/1716.PI | .PI | 416 | `6e7328144f5fb37c08f3f926013d0d38e9f503e0b1a6b24a70c148e2109e527b` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/17EX.PI | .PI | 2929 | `3accdba4b1ceda08b07a0aaa2717b65c304d0f8edbe7a645487b32efb61847f0` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/99SL.CDG | .CDG | 23056 | `fdd5b922ba2fe38a4968b8b44bbcf9c6cc290e3c1d844af80794d32ff16d3bfe` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/CHNAME.BFT | .BFT | 9296 | `e2285dadb3a1d0c818325ad1d686afea674d101b5cd7512555e84b25c0f577f1` | font/sprite: BFNT sheet |
| YUME1.DAT extracted | YUME1.DAT/CONTI.CD2 | .CD2 | 9856 | `9b5bd6141ce9aebc889d53105cf4daa6753f4fefa633ce3ae98911ffc2205b09` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/CONTI.PI | .PI | 5646 | `0dc1b552b9591d7e5ccd2a42b728e68e02e05bee1b880ce077827827d20b628f` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DEC.M | .M | 3367 | `e4d91c75b8c906b7e1579a6e10fd7df447437474fec2272269f90ef8d72d5ad5` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DEMO1.M | .M | 2319 | `c8c8332c435a9ce9bd3e08a3805020354f4fd040e0ee6cdbdbea7d0b59e3ecc0` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DEMO2.M | .M | 1145 | `dced2a96837d0e2bb571ba15a537e5001f283422552f9ccdb337fd76f0052d8f` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DEMO3.M | .M | 1284 | `d353d9d3aeade39f8e9f20711c0be515b9e94029f3102679802130bd50bf5aa9` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DEMO4.M | .M | 821 | `aeaa2523c29d87593098ee0a2c0e8dd4e5714f15b1880a9c9cd830d191aed798` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DEMO5.M | .M | 1193 | `e45a4514f716ab65a098d4c7d61c79b14b6e1bbd29d189c8b0133b905756f1ad` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/DM1A.PI | .PI | 4652 | `13c5e640ab5be050eed4adb6328679850b5b97c2552a52169132a670a628829d` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM1B.PI | .PI | 6922 | `c5df8dac309c47cd055cdd635eaf037b45aa1a140b222ba843ee6c6bf6f798e9` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM1C.PI | .PI | 11335 | `bd493b9e180b0c4b9e4a77353b2004364bf048beba6a66c5c13de2bd37764af7` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM1D.PI | .PI | 8032 | `db79c0526d22d32f46cb747883ba7bfeaa9400c85c48f25233faba9e34a83194` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM1E.PI | .PI | 19686 | `f243e9b6e2dd5fb6c0147b25350666e16b0a3f65aae1a493e43b6209c8385b8e` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM1F.PI | .PI | 20612 | `63731c562c0f79bc32b56aaa0a45ed33e5afbc33ee2884f3ef68c85eb80535fd` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM2A.PI | .PI | 25510 | `fa3b11c84ecf303fd81e92959e4e9cb4645ad55ffc333a7ba6b7104e3886e478` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3A.PI | .PI | 31219 | `598f9c541ea9d14fbca5566ac2173b53bca24fc1f2c45cdcaac667760264ad93` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3B.PI | .PI | 25365 | `8d6898792310db35758a0e6ef2d9079ff12372f2b237f904f8b504064d9396dc` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3C.PI | .PI | 46445 | `4e286c40a49fb55f068817f00832736fad630e50854731dfa0109db8255867ed` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3D.PI | .PI | 16766 | `c1a0162540fe65bd99befc4ce215ca125aefcad24d080ebda7f65fd5369a6846` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3E.PI | .PI | 35710 | `88f8044b8c5e3ced14114a6caef60661b944c8dc29b27b8dfdeedf5dcfcc98eb` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3F.PI | .PI | 30599 | `5ba4c9f312c4892563f2d1e1fad0567b6c8bc0a754b06e7356cdd1223795337d` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3G.PI | .PI | 32247 | `d451f423151b210b4d6a7b974949018be807c2bd44f0fe0ae5bb52447ad2a3ef` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3H.PI | .PI | 23546 | `15cf87af1287d675282d3199ec160425f5c08bdbe79c44cb09355a55f4aa49ad` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3I.PI | .PI | 44898 | `0e4e1c460adb151d6158128e70f999fa0eede82de9e6a34c31983ef4cfd187fd` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3J.PI | .PI | 33620 | `c58e3b2ebcc2e8460cf751dc7a7861329a3ea46d8794f8bafaac56a33fb2590a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3K.PI | .PI | 28267 | `f001184baa333c8ee8499d2a3cfb55f359ba23f3b4ad6412958169c3b8be5a1f` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3L.PI | .PI | 18283 | `edbcc0c41c42eadfa34a6ba978f57ec39e994b99bd52775706cf3ca85a9dc192` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3M.PI | .PI | 27358 | `3e945354edd1921e934b809cca739d4a3da51f283eca2940368e10b44a0969d3` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3N.PI | .PI | 34717 | `521ce60b98835f7d304b6b2d26e7a1e85d4a644faeaefb9164d3cdc21827e71e` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3O.PI | .PI | 38614 | `b3f17c8fc28285ccff511a8935713692c773de7aa4100e01b205c0751971de14` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM3P.PI | .PI | 20050 | `dbc777888a4d26b88289efb0b00b072821173f752afe473323d47cbaca16f54a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/DM4A.PI | .PI | 22284 | `389c0c9adf944c2b07c70e8bca990e80bb7f664e8d9d1e992151eedeab7a0a01` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ED.M | .M | 3378 | `278f22b1277c651154d35dd3f26cd326b5b7dcd7fd5ff2d34e5b44b837a8efda` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/EDBK1.RGB | .RGB | 48 | `276dfaadf672c67feec6cc63ffd320cf12544752c31d0d30c6f6dbcfe0447148` | palette: 16-color RGB4 |
| YUME1.DAT extracted | YUME1.DAT/EN2.PI | .PI | 1841 | `12930ddd64487be4e01ffdd2719dd0caade1071a2b18d74ed5a52d61afc4c520` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ENEMY00.PI | .PI | 7378 | `46ded362fc856ab7a8d08b47275386dec3e7a4c706a6a2caa7bf9c0b2193713a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ENEMY01.PI | .PI | 7295 | `72bdd306395afa9ab4b37d0ebd70f04f541fe99909060a323fe0a4f82a362bd5` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ENEMY02.PI | .PI | 7064 | `f6d3d9c3f2ddcf35aebf3251055b261f54fc5e2305774cbfeb95d156936d3d6b` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ENEMY03.PI | .PI | 7046 | `d839d31dfe0ab0994a26c4e30dc93597ee49f1dcd5b8fb72408b147dde1bb103` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/ENEMY04.PI | .PI | 7035 | `275935f53fa8d66e4c53a421903736b2b1ca70784897b29f80d1491560eb9d69` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/LOGO.CD2 | .CD2 | 57616 | `cd8a083214707082b5395b2a86d7c80a9da1204e28e5a8c961bc5cc4c79ff1c6` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/LOGO0.RGB | .RGB | 48 | `ffdae2641fba3d0dc736d3c16f684ec82364e22b4e2631bd0573ce4b3512e578` | palette: 16-color RGB4 |
| YUME1.DAT extracted | YUME1.DAT/LOGO1.RGB | .RGB | 48 | `a88e709ad8eef65cf95e92cc610097ec83e69c0a93d8938869cef9fdbd6aa9ba` | palette: 16-color RGB4 |
| YUME1.DAT extracted | YUME1.DAT/LOGO5.CDG | .CDG | 11536 | `fe61e31daa3fa32210f1093877b4f519bc01ade702dd94e966b6f459b9d6d898` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/MIKOFT.BFT | .BFT | 8224 | `4109eb3d82421a2529622120974f32c5133dcddd29f798fc4e070928423ea6c3` | font/sprite: BFNT sheet |
| YUME1.DAT extracted | YUME1.DAT/MUSIC.TXT | .TXT | 15960 | `16e8df8ddc9102d80024911e23e86002d08ef5148ac120ca081831cae38fb231` | music metadata/text |
| YUME1.DAT extracted | YUME1.DAT/OP.M | .M | 4431 | `47dac7ea9ff036d18975d8e57e9c065ee8a49ae1ba7a6994466ca45e5072d268` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/OP3.PI | .PI | 6732 | `7151fbde986981d4bbb4e158ed246f989aebbcf2bcb9de855c1e7cadf944871c` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/OPWIN.BFT | .BFT | 3152 | `0ccb604770ea646a6cf7f1602160a9b94ab28cfbc4701f9b98455a39c5242869` | font/sprite: BFNT sheet |
| YUME1.DAT extracted | YUME1.DAT/OVER.M | .M | 508 | `79f0a8bef5ebfd898ed22105211b59b4c6711cbbeabb494156d127c40d1e0e34` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/OVER.PI | .PI | 3571 | `5059eb83541c11ece4f4078ebe3d9b1fd005413af9adfaf293f297be0145df19` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/REGI1.BFT | .BFT | 25168 | `9e85765e3b7cfb0b87024a56390d22fef822365f50feac986e569383d661d877` | font/sprite: BFNT sheet |
| YUME1.DAT extracted | YUME1.DAT/REGI2.BFT | .BFT | 25168 | `1e869a420079b368c135dc9bfd7f3fc33f9d8d7e71ffcb4240608f5fccdd616b` | font/sprite: BFNT sheet |
| YUME1.DAT extracted | YUME1.DAT/REGIB.PI | .PI | 44603 | `ad720882b38716327c9b287bc3ba88a0fd0e16c16bd0e1ba4e428c14c2fcdae9` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/RFT0.CDG | .CDG | 17616 | `367e2bbeaae948764bab66da329270992954b7817a220ca9153806d4937011df` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/RFT1.CDG | .CDG | 17616 | `a6c74f237b286402d55e1aa3669457a354029b60bf2e7f418120644128578658` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/RFT2.CDG | .CDG | 17616 | `5151fe0b5efc4a54c4d1a42935b1c9262dff9efb0f0faa510d2f20755fd3f5d4` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/RFT3.CDG | .CDG | 17616 | `c871fd7ec98a26ae1d199da68636e782f403c52a3cbf6f08614d40855eddc0f0` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/SCORE.M | .M | 983 | `e2d4269885e7fefb3ca839d3069826cf410a316dec4baea528570ea3cac44557` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/SELECT.M | .M | 2437 | `6ff6cfff4d056aa363fb2657a1761aa23982169777515da1fc3103fce3820874` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/SLEX.CD2 | .CD2 | 7936 | `f4f1925738bdf647b9990e9ce6f309115932081d0e5f83d1e68776d74db88216` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/SLEX.CDG | .CDG | 2576 | `35c2b141de6535952c42601da0b2673ff18b3ed32c8d1cb459e86fd517c491ee` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/SLWIN.CDG | .CDG | 5136 | `12d61b8bae4b4cec57bdb7a417fb0b3fce2a854d421ab390b51299ec7df94dae` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/ST.CD2 | .CD2 | 6816 | `9cbbf6a9026367aaee7f047166230efcb6f5ddc5f09f7df50f9f11c7e1b5612c` | graphics: CDG-family multi-cel |
| YUME1.DAT extracted | YUME1.DAT/STF1.CDG | .CDG | 7696 | `a97c76049cd281db1802bb4750bd0d57976b8baa8b65725ad55c89f0e768159c` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF10.CDG | .CDG | 11056 | `593cda5b20b65be1a5a71f8266b0fe2ee0474629fa81f921525853b0200a42f9` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF11.CDG | .CDG | 6256 | `775f1b47e368329023287bfff1b8b5eb28417bc6535fc53187dd71e969a0d515` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF12.CDG | .CDG | 5056 | `7bfdedc998a16f30fd772e299e18d340ba66c5f733a560cfc88464c20626894f` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF2.CDG | .CDG | 4336 | `cc1ffa862fd3525b53519588b4b9c505a61c353db0f33e130bcd73a643ac0e38` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF3.CDG | .CDG | 8336 | `6b493b4d4bfb0d0537bf7efe7ed2fd1da6ed5b96dc48297b425158fa69b84616` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF4.CDG | .CDG | 8016 | `6f8c4679581bc93575160245d3a2a82ab6dbfd24c7ccf180d6da3753067b8f06` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF5.CDG | .CDG | 5136 | `2cb887cbcf3585987cc844a1c715f3cf864a625e7e6243e34b038c7732dd95f3` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF6.CDG | .CDG | 10256 | `796a72627fc5260535774e51bc9d7f3fe393fa7023cb2fd2114658d678f84042` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF7.CDG | .CDG | 8976 | `05e69481708699fa1247d2eefba37a03cd65a35ee295bdbc4adba206a5b60c13` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF8.CDG | .CDG | 36816 | `25c969eeddfa5c0e26de54673a46fefca14b51e2923142872156bfd61feb6da2` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STF9.CDG | .CDG | 4336 | `9e362ba1c949d8fbae795336a05a91445e6f12826263f74f18ac0270b78fd1c0` | graphics: CDG planar cel |
| YUME1.DAT extracted | YUME1.DAT/STNX0.PI | .PI | 2732 | `1d2a3d12bceb788a1577ad6659f2f8bb0a889f92ca45f296f193b13e5550d214` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/STNX1.PI | .PI | 1231 | `c97b16938c9e519c361d4f4a6393271d0146fe6c99fd9f184d90f911ca295353` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/STNX2.PI | .PI | 2057 | `63754d25e07795abaec3d6d19de1e27a419316cc9dca721cb09e5ad78c14af98` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/STNX3.PI | .PI | 2831 | `f1cd76654ea6993876da184904950e9f6c98f934d22fdcbedb4bb8ab5a52aad7` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/STNX4.PI | .PI | 2901 | `11074332541b8d432d8ae55ce1027cbe56cddbcf7b2eaa79e259c3bbafe7ae0a` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/STNX5.PI | .PI | 2058 | `d6b90ce19369465253b9054eb46692f5b923d78e2cf61be433d66b3e4a94c739` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/TL01.PI | .PI | 4192 | `92d9e25638f9bd6f485e4a18de56547e17328b4e2fbb2b107fd39ca28db1a140` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/TL02.PI | .PI | 31779 | `9e257157b6de2dd2a6abeaeab2796ab47794c964b599cb133ac96de2ffc38986` | graphics: PI compressed indexed image |
| YUME1.DAT extracted | YUME1.DAT/TLSL.RGB | .RGB | 48 | `67e1d8d78691282dca2c95abb3d942c25f5f256f77211a1ec1d9693c5c327d45` | palette: 16-color RGB4 |
| YUME1.DAT extracted | YUME1.DAT/WIN.M | .M | 888 | `88be9cbf95eb87358151385918aa9468c4e6892b6eac9a473713fd99f4ded8be` | music: PMD sequence |
| YUME1.DAT extracted | YUME1.DAT/YUME.EFC | .EFC | 1304 | `d535609f167eca0f6f26e8eb0fc7f27b16ae1607de931f0dc74de73ad9511262` | sound: PMD effect bank |
| YUME2.DAT extracted | YUME2.DAT/00BM.MRS | .MRS | 33120 | `492ecc3a66807e9b9b2a3f45d063bc758ed7545bf1bc4dbe70579370a0a35d3e` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/00CH.BF2 | .BF2 | 4688 | `43c6d07e2566a8105cea947fc64c47310068e6e7eaa50129542673bf94bbccc2` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/00EX.MRS | .MRS | 33120 | `e76cf82d7283f16cbaced56aa8919bf34e1f80876664ceca0da233fdc398a85f` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/01BM.MRS | .MRS | 33120 | `320decba4dced7175fbb8b1af959e63118c442149d835ba41e86dc8a21a47966` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/01CH.BF2 | .BF2 | 4688 | `fe702ce26fc9a3f5627bfd0b3a15666d9e3ea7b7825e8c81dcb25cdf94aa975c` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/01EX.MRS | .MRS | 33120 | `2e4373c0d1e1ec999b274f0734da69739b62caec1ff3b9200e7d75de45cfa355` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/02BM.MRS | .MRS | 33120 | `de4edf7fb15ea355bc743bc12890ca78ac0c58449daa1c4a8914c69202f02a7b` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/02CH.BF2 | .BF2 | 4688 | `598d57223ea18ffaf400ca6e25c13ffeafd8abd8fbbfef485750a00837d21b48` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/02EX.MRS | .MRS | 33120 | `b75be93b9c50b5bbc69bfd45d3bf40d1c6885fb3e51239084ead9ac0915b774a` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/03BM.MRS | .MRS | 33120 | `aa56bf71c020e5a1ce4c9f9115850e98dd2de80285d3aa08bfd8be0a3ba7bc08` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/03CH.BF2 | .BF2 | 4688 | `b51214d4c9db057c0245baa17b8e13bd745a734f6a211548143cc87a53b69234` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/03EX.MRS | .MRS | 33120 | `ac9557689826c1ccaa03a4aaaba749212879f490c4a1074efaab7c0f1dbbc65e` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/04BM.MRS | .MRS | 33120 | `d4241254a1bdc43ca3ec51300be4e9770832a294f95d10018bc85c858ca7f6ab` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/04CH.BF2 | .BF2 | 4688 | `a4596cce78ba0916039e1d28c8467809bcbf2f786846db398af19297f79b44bb` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/04EX.MRS | .MRS | 33120 | `f1ac2835fc6b8d9cec7d12c4ad200f39be04e595f6ba325dcc7cb4d28401f33e` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/05BM.MRS | .MRS | 33120 | `4e4b68d2031a9c8a1f749369675958fbbf9fd2ac15a30b2b649ada79e5f85eea` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/05CH.BF2 | .BF2 | 4688 | `28b381da6b4e7d4f366eb07a8d64b154b2470cff79503fb66ba923c2a1925b81` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/05EX.MRS | .MRS | 33120 | `1aeb9f82c1a9699f1c3bf12481ede01e676c5c78625a67a1a3972d65db120596` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/06BM.MRS | .MRS | 33120 | `8ddd6de6dda02dffbbe622c72ad2e40588fea466ca63b984eb213cc681d4ebe5` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/06CH.BF2 | .BF2 | 4688 | `ee5eff3721881a38cc4ba1474d3234fdb20ddd12e33e37718aea610403cd0673` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/06EX.MRS | .MRS | 33120 | `a18b31ee122297f3957caad3f8fecc31cba77924e14bac222ed27a458962a9b0` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/07BM.MRS | .MRS | 33120 | `8ddd6de6dda02dffbbe622c72ad2e40588fea466ca63b984eb213cc681d4ebe5` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/07CH.BF2 | .BF2 | 4688 | `ea3c826f7828b3208b8593b432060d7105ced0bbd443e2e5c0b1bfba1c21e8ff` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/07EX.MRS | .MRS | 33120 | `2173936c8967b3d71f057421e8d1179e3f7645f282bf9eb6ef0e13cd5a9e0be8` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/08BM.MRS | .MRS | 33120 | `29d27987ec87150dae3067f91956f2f8af02796a9a70b9bc4967e862f3139cbb` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/08CH.BF2 | .BF2 | 4688 | `f49ec4d92fac476bc33ad244d41bd581908b18cc9490f91660b26d6dff075bb5` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/08EX.MRS | .MRS | 33120 | `34ed7e92878c7addafd6dba3cf1aecadb84670c53c52fe36ea1f1eb2eb515028` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/09BM.MRS | .MRS | 33120 | `57d3e771d2d41ba906f927a4b8f1a7c9cac12d1e9176363a41c6c6f17918eaff` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/09CH.BF2 | .BF2 | 4688 | `3abae0c185654bb1a395220f66f2ab04f70b426282477a72f0bfe28409d0a4ed` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/09EX.MRS | .MRS | 33120 | `f1dae41ff77d55532de80336b2fd7c5f1dad8faf77ee60cbf53a494f69cccab4` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/10BM.MRS | .MRS | 33120 | `2b3f14fcee59cc64d08897956de76e52cba8b69f15c86a203e8d7ee9072224a8` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/10CH.BF2 | .BF2 | 4688 | `9cf502c59264dd7471a694695fc91b5125c85991a2ea2d9c261758459055d940` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/10EX.MRS | .MRS | 33120 | `5530b39064687cba22980a4b8f0c8db925a763b18051da734d09bb7362799556` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/11BM.MRS | .MRS | 33120 | `a4081d67237f3dbcda84ad061e5b53759a0c2216c8256492cc62fa8e90f9d80b` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/11CH.BF2 | .BF2 | 4688 | `45d390238a13b11f6a95ccde897c152922917c8951f79397c0d126dbc01eb23c` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/11EX.MRS | .MRS | 33120 | `cb61fa7d1f350c88dddcd11e6b6c7836abdb43e04047d9008bc2e2571a08e420` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/12BM.MRS | .MRS | 33120 | `72662b5175c3a46b9a6be1f10770c790cad35f301b4dc612574ac36a16216a5e` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/12CH.BF2 | .BF2 | 4688 | `c60d8c86b2c042f037ed132ee7329de8c8c17e81452b3ea373f312ef485e065d` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/12EX.MRS | .MRS | 33120 | `ca4379b16f4274850b2398dc037a98a16d958325cf38c666e5df8c6a97cdd264` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/13BM.MRS | .MRS | 33120 | `89bbede8034fe822c6b2e71d5202a2269293a7aaa9f2d75b5717d0b89b1b0500` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/13CH.BF2 | .BF2 | 4688 | `d7d9dfec5560f5de5369d2c3e807253f0f6b72390dd28138807c7f863737e5e0` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/13EX.MRS | .MRS | 33120 | `8885f567b5763f39113d20d2067ad99baf684211927b9e25212b49c0fd6c3516` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/14BM.MRS | .MRS | 33120 | `4bfea7823ad659df086b24221b084cd37e2433af95d7abc640b4a51fbd10a142` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/14CH.BF2 | .BF2 | 4688 | `90c6180ed60fc8dcb2440e6ae7f651c0d311bb928db26da2bf3fc6de0220719b` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/14EX.MRS | .MRS | 33120 | `ea534bb5e1c0d31c63c400fab44a75c6285f0c6d88bf8a7593344c67d12838ca` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/15BM.MRS | .MRS | 33120 | `2bad2b52dd681f0290496e2433e85f324ccdf697b1ed82e889715a77f098852c` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/15CH.BF2 | .BF2 | 4688 | `59d8a5efe2116514ab1f86d6f9be619c4e859415bd56570df7093465c9c8e39c` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/15EX.MRS | .MRS | 33120 | `fa34862eb4e301535d8258d44d84a7af07b5103b033463d579de52e72c2fee31` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/16BM.MRS | .MRS | 33120 | `69e336a5cf9da8d056b37ec779b56c502d9420fb0e4a4c07632b44e4621b7d48` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/16CH.BF2 | .BF2 | 4688 | `6247852393bcfeacbf4978d3611fe606ebe6d181131ad81092b6ea9ae3f10883` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/16EX.MRS | .MRS | 33120 | `b405ad8757f514720ad908bdb2fbf5f2adc6c3a0916d24f7030d1f9b8c01ea44` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/17BM.MRS | .MRS | 33120 | `ba2c36681291db0f8c4aa845463833442969430b13d952c611ccc1bc3bc89cd3` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/17CH.BF2 | .BF2 | 4688 | `bb17327134b2eb36f8455a6ad6b58f62141b68b15a4d28293fb4c512d1e59547` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/17EX.MRS | .MRS | 33120 | `df5cc63f6e01e6803935e9db2889dae99c2ebb7b64ce45ec5d376999c03e6d8e` | graphics: MRS planar portrait/bomb cel |
| YUME2.DAT extracted | YUME2.DAT/ENEDAT.DAT | .DAT | 5072 | `183856f7553f588ed9c5b1ed9b65146471570cde55245cc86b370103e3ced943` | stage/gameplay: enemy formation scripts |
| YUME2.DAT extracted | YUME2.DAT/GAMEFT.BFT | .BFT | 8224 | `ef2348d5834c7e92caefc0ef89e835e33a8cdbd2392cea30a3e1e9bf6c97a30f` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/LOSE.BF2 | .BF2 | 4176 | `653697fd41da353d6a7d4ac3c514a5a70a6f49cbf660ed995940d38be7dfd323` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/NUM8.BF2 | .BF2 | 400 | `2f9726d46f0c78d686af49e2202207a52a23517776f63f4cc8b7464266df92e7` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/ROUND.BF2 | .BF2 | 5712 | `ab80692232054b506b58bed47d646e0c1420fd7e361259380b6feeddb5120885` | font/sprite: BFNT sheet |
| YUME2.DAT extracted | YUME2.DAT/ZIKICW.BF2 | .BF2 | 4176 | `a46211dbfa5497bd7c240f9910ed57b40a84d91d38cab8ee72d9ad928c3252f3` | font/sprite: BFNT sheet |
