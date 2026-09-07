# TH03 text, cutscene scripts, and persistent data

Status: source-backed format audit, 2026-08-09. This closes the formerly
generic `.TXT` portion of `ASSET_INVENTORY.md` and corrects the prior
unclassified status of `YUME.NEM`. It is metadata and behavior research only:
no original text, bytes, decoded text, or derived artifacts are included.

## Authority and method

Classic source authority is the clean Anniversary object
`3e2381d4275be7287f5525de5c95389d492b0a3c`, as fixed by
[`../SOURCES.lock`](../SOURCES.lock). Code citations below have the form
`path:function` and refer to that Git object, read with `git show`; the
extracted-file observations are read-only metadata scans of the user-owned
`originals/th03_full` installation and its `dat_dumps/YUME1.DAT/` directory.
The directory is an extraction alias, not an archive copied into this repo.

The two archive identities, per-file metadata, and the graphics/audio formats
remain in [`ASSET_INVENTORY.md`](ASSET_INVENTORY.md). This note neither
duplicates hashes nor changes that inventory.

## Reconciled text census

All 43 stock `.TXT` files (151,716 bytes) decode under Windows-932/Shift-JIS
with strict fallback. That confirms the stock corpus' encoding, but does not
mean that the original interpreter validates Shift-JIS.

| Family | Files | Bytes | Exact observed structure | Runtime owner |
|---|---:|---:|---|---|
| `@00DM0`–`@08DM0`, `@00DM1`–`@08DM1` | 18 | 47,370 | Variable-length CRLF plaintext scripts | MAINL cutscene presenter |
| `@00ED`–`@08ED`, `@99ED` | 10 | 23,751 | Variable-length CRLF plaintext scripts | MAINL cutscene presenter |
| `@00TX`–`@08TX` | 9 | 17,820 | Each is 1,980 bytes = 11 message blocks × 3 rows × 60 bytes; every physical row is 58 data bytes plus CRLF | MAINL win screen |
| `MUSIC.TXT` | 1 | 15,960 | 380 fixed physical rows of 42 bytes (40 data bytes plus CRLF), equivalently 19 track comments × 20 rows | OP Music Room presenter |
| Root documentation | 5 | 46,815 | Variable-length CRLF Shift-JIS text; the existing inventory's deterministic `SHIFTJIS-NONASCII-*` aliases avoid filename-normalization claims | No pinned TH03 loader found |
| **Total** | **43** | **151,716** | — | — |

The first two rows are one language and total 28 scripts / 71,121 bytes. The
direct fixed-record `@nnTX` reader is a different format despite the common
extension. `MUSIC.TXT` is also a direct fixed-record table, loaded by the
TH03 instantiation of the inherited Music Room; its selection labels and PMD
file names are compiled in `th03/shiftjis/music.hpp`.

## Cutscene script language

### Load and byte model

`th03/cutscene/cutscene.cpp:cutscene_script_load` opens a named virtual file,
gets its size, allocates exactly that many bytes, and reads the file verbatim.
It appends neither a NUL terminator nor a length field. The caller then runs
`cutscene_animate()` until the explicit `\$` opcode stops it. The DOS path
therefore trusts a complete, well-formed archive entry; an incomplete script
can read past its allocation. A native span API must retain the original
semantics for valid content while turning truncation or a missing terminator
into an error rather than reproducing that overread.

`th03/formats/script.hpp` defines the shared primitives:

- A separator is any ASCII control byte (`< 0x20`, including CR, LF, and NUL)
  or ASCII space. Separators are skipped by the outer interpreter loop.
- A backslash introduces one case-insensitive ASCII opcode. A parameter number
  is zero to three ASCII decimal digits; no sign or arbitrary-width number is
  accepted. Defaults are command-specific and are stored in a shared mutable
  default variable, so an omitted number can intentionally use the most
  recently installed default in a few paths.
- A filename parameter is copied until a separator or space, with the
  original `PF_FN_LEN - 1` maximum. The comma that introduces a filename is
  consumed by the calling opcode, not by this helper.
- Any non-separator, non-backslash byte begins a two-byte `ShiftJISKanji`
  glyph. The stock data is CP932-valid, but the interpreter simply fetches the
  next byte: it performs no lead/trail-byte validation, Unicode conversion, or
  replacement behavior.

This is a bytecode-like plaintext language, not a NUL-delimited text file and
not a generic Unicode markup format. Preserve the original bytes through
parsing; decoding a glyph to Unicode is a presentation decision that must not
lose its source code-unit boundaries.

### TH03 opcode semantics

The following is the TH03 branch of
`th03/cutscene/cutscene.cpp:script_op`, not a cross-game abstraction. `ddd`
means the source's optional one-to-three-digit parameter.

| Spelling | Source-backed action |
|---|---|
| `\n`, `\sddd`, `\s-` | Move to the next line, or finish/reset the four-line text box. `\s` can wait for input; `-` suppresses that wait. A full `\n` falls through to the same box change. |
| `\cddd`, `\bddd` | Set text color (default white) or one of the source weight/effect modes (default bold). |
| `\w...` | White fade (`wi`/`wo`), ordinary frame wait, cancel-aware wait (`wk`), or measure wait (`wm`, optionally cancel-aware), with source defaults and optional second comma parameter. |
| `\vddd`, `\vpddd`, `\tddd` | Set glyph interval, select displayed page, or set palette tone. Tone changes retain the source's pre-change one-frame delay when not fast-forwarding. |
| `\f...` | Black fade (`fi`/`fo`) or music fade (`fmddd`). The source-marked `fm` integer-width landmine is part of the legacy behavior; a presenter should explicitly choose a safe, bounded native contract instead of accidentally narrowing differently. |
| `\gddd`, `\gaddd` | Screen shake, or render a gaiji at the current cursor then advance it. In TH03, `ga` has no glyph-interval delay. |
| `\kddd` | Wait for input. TH03 reads the parameter but deliberately passes an indefinite wait; the source calls this a quirk because most stock uses supply a nonzero parameter. |
| `\@` | Clear both graphics pages. It does not refresh the saved text-box background, so later box restores can use a stale snapshot; the source marks this a landmine. |
| `\p,...`, `\p=`, `\p@`, `\pp`, `\p-` | Load a PI background with its palette, show/snapshot it (with palette application for `=`), palette-only apply, or free it. |
| `\=ddd`, `\==ddd,ddd` | Blit/clear one of the background's cutscene-image quarters, or reveal a quarter through the ordered PI mask sequence with an inter-mask delay. |
| `\m$`, `\m*`, `\m,filename` | Stop BGM, start BGM, or stop-load-start a song. |
| `\eddd` | Force a sound effect. |
| `\$` | Stop the interpreter. Unknown opcodes otherwise do nothing. |

`cutscene_animate()` initializes the cursor, white/bold text state, interval,
and a boxed background snapshot; writes TH03 glyphs to graphics page 0; and
adds the per-glyph interval delay. Holding Cancel sets `fast_forward`, which
skips the guarded waits and text delay paths. Consequently, a faithful native
implementation is an ordered presentation VM with input, palette, PI/CDG
composition, gaiji, BGM/SFX, and frame-boundary services—not a pure decoder
that produces a static string.

### Script selection and lifetime

`th03/mainl/mlentry.cpp:main_cutscene` mutates the `@00DM0.TXT` template to
select a player-indexed `@nnDM0.TXT` or `@nnDM1.TXT` script before loading and
running it. `th03/mainl/ending.cpp:ending_staff_and_regist` similarly mutates
the `@00ED.TXT` template for the nine regular ending scripts and separately
loads `@99ED.TXT` under its extra-ending condition. The template declarations
are in `th03/mainl/screens[data].asm`, `rsedat[data].asm`, and
`vstf[data].asm`.

The script allocation is freed immediately after each blocking animation
(`cutscene_script_free`). Its source bytes belong to a legal-install importer
and the presenter, never to `th03_core`; the decoded PI/CDG/BFNT assets it
requests stay in `th03_assets` and are composed by `th03_present`.

## Fixed win-message tables

`th03/mainl/screens.cpp:win_load` treats `@nnTX.TXT` as a direct binary
record table, not as a cutscene script. `WIN_LINES == 3` and
`WIN_LINE_SIZE == 60` are frozen in `th03/formats/win.hpp`. It selects one of
11 records, seeks to `message_id * (3 * 60)`, reads exactly three 60-byte
records, appends an in-memory NUL to each, and renders the three lines.

The 11 records account exactly for the 1,980-byte file size. The selected
message ID is based on the winner/loser and Story-stage route in `win_load`;
the nine files are selected from the winner character table in
`screens[data].asm`. A native reader, if needed by a concrete win-screen
client, should accept a span of exactly 1,980 bytes and return the 11 × 3
fixed byte records without stripping CRLF or normalizing Shift-JIS.

## Music Room comment table

`th03/op_music.cpp` includes `th02/op/m_music.cpp`, whose TH03 branch opens
`MUSIC.TXT`. `th02/formats/musiccmt.hpp` fixes the TH03 layout:

- 19 tracks, matching the 19 entries in `th03/shiftjis/music.hpp:MUSIC_FILES`;
- 20 successive records per track: one title and 19 comment lines;
- each record is 42 bytes: a 40-byte Shift-JIS/ANK text field followed by two
  unused padding bytes, stored as CRLF in the stock file; and
- `cmt_load(track)` seeks `track * 20 * 42`, reads 840 bytes, and writes a
  terminating NUL over each record's first padding byte before rendering the
  title and comments.

This accounts exactly for 19 × 20 × 42 = 15,960 bytes. It is presenter-owned
Music Room data, not PMD sequence data and not a generic text document. A
future Music Room client should retain the fixed 40-byte text fields and
perform CP932 decoding only where it renders them.

## Root persistence and unclassified data

These files are mutable application data, not imported game assets. They do
not belong in `th03_assets`, the deterministic core, or a replay document.

| File | Exact source-backed format and runtime semantics | Native ownership |
|---|---|---|
| `YUME.CFG` | Eight-byte serialized `cfg_t`: three option bytes (BGM mode, key mode, rank), a 16-bit unused field, a 16-bit resident segment pointer, and one debug byte (`th03/formats/cfg.hpp`, `th02/formats/cfg.hpp`). OP reads it verbatim through `cfg_load_and_set_resident`; ordinary saves overwrite only the first four bytes, while exit saves write all eight and reset the resident pointer (`th03/op_01.cpp:cfg_load`, `cfg_save`, `cfg_save_exit`). The pinned source does not validate hostile values. | Future client settings/persistence adapter with explicit migration and safe validation; never a content-import requirement. |
| `YUME.NEM` | Four 206-byte rank sections, matching the observed 824-byte stock file. Each section is a little-endian 16-bit checksum plus 204 bytes: 80 backwards-stored name-glyph IDs, cleared flag, unknown byte, 100 score-glyph IDs, 10 optional character IDs, 10 stage-glyph IDs, and two keys. `scoredat_encode_and_save` sets keys, computes the pre-encryption byte sum, encrypts backward using the stored keys/feedback; `scoredat_load_and_decode` decrypts forward and recreates all ranks on a missing file or checksum failure (`th03/formats/scoredat.hpp`, `scoredat.inc`, `scoredat.cpp`, `score_es.cpp`, `score_ld.cpp`, `scorecry.hpp`). It stores high scores and the clear/unlock flag. | Future high-score/unlock persistence adapter. A safe parser/writer may use synthetic fixtures, but it must be versioned separately from static-content import. |
| `TOUHOU.DAT` | The read-only installation file is 8,272 bytes. TH03 `GAME.BAT` invokes `ZUN.COM -3` before `OP.EXE`; the TH03 ZUN build embeds the shared TH01-TH03 `th01/zunsoft.cpp`, which opens `touhou.dat` through `super_entry_bfnt()`. It is a palette-bearing 64x16 BFNT (`ZUNOP`, codes 0..15), not persistence. | Authenticated static input to the native one-time ZUN Soft startup owner. Original bytes remain external and untracked. |

## Native boundary and follow-up

No code change is proposed by this research parcel. A small span decoder is
not yet independently useful: the 28 scripts require the ordered presenter
VM described above; the win and Music Room tables are trivial client-local
fixed-record reads; and `YUME.CFG`/`YUME.NEM` are mutable persistence rather
than assets. Implementing any of them now would add an unconsumed API and blur
the asset/presentation/persistence boundaries.

When a MAINL client is in scope, split the work deliberately:

1. Add a bounded, synthetic-fixture-only cutscene parser/VM contract in
   `th03_present`, preserving byte spans, command order, defaults, explicit
   termination, and timing/input effects. Its source of truth is
   `script.hpp` plus TH03 `script_op`, not a generic text parser.
2. Add client-local win and Music Room table readers with exact 1,980-byte /
   15,960-byte and index validation. Decode CP932 only at the text-rendering
   boundary.
3. Add versioned application persistence for configuration and scores only
   after the product chooses migration/compatibility policy. Test the NEM
   cipher/checksum against synthetic sections; do not use original files as
   tracked fixtures.
4. Keep root manuals outside the playable path. `TOUHOU.DAT` is the one
   source-established exception: the startup owner consumes it before the
   product selector, matching the original executable chain.

Validation performed for this note: all 43 `.TXT` paths and 151,716 bytes
reconcile to `ASSET_INVENTORY.md`; the source-visible split is 28 cutscene
scripts + 9 win tables + 1 Music Room table + 5 root documents; all 43 decode
under strict CP932; `@nnTX` sizes reconcile to 11 × 3 × 60;
`MUSIC.TXT` reconciles to 19 × 20 × 42; and `YUME.NEM` reconciles to 4 × 206
bytes. No emulator ran and no originals were copied, modified, logged as
content, or committed.
