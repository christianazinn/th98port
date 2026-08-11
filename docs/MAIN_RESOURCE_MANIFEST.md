# MAIN external resource manifest

This is the app-owned external-data contract for the first native MAIN
resource scope. It contains names, sizes, roles, and decoder ownership only.
It contains no original bytes, decoded images, audio, or archive data.

## Extracted-root contract

`th03-app --data <root>` receives an already extracted root, not an original
DAT archive and not an installation directory. The one accepted layout is:

```text
<root>/
  YUME1.DAT/
    TLSL.RGB
    00SL.CD2 02SL.CD2 ... 16SL.CD2
    ST.CD2 STNX0.PI ... STNX5.PI
    0016.PI ... 1716.PI
    00EX.PI ... 17EX.PI
    EN2.PI
    ENEMY00.PI ... ENEMY04.PI
  YUME2.DAT/
    ENEDAT.DAT
    GAMEFT.BFT
    MIKOFT.BFT
    LOSE.BF2 ROUND.BF2 ZIKICW.BF2
    00CH.BF2 ... 17CH.BF2
    00EX.MRS ... 17EX.MRS
    00BM.MRS ... 17BM.MRS
```

For the supplied read-only extraction, `<root>` is
`th03_full/dat_dumps`. `YUME1.DAT` and `YUME2.DAT` in that directory are
directory aliases for extracted entries; they are not the non-ASCII original
archive filenames. Import/extraction remains a Data Manager or `thdat`
operation. This loader intentionally has no DAT parser.

The loader accepts only manifest keys, joins only checked simple components,
canonicalizes the archive and selected file, and rejects a resolved path
outside the canonical root (including a link/junction escape where the host
exposes the resolved target). Manifest filenames and archive directories use
the supplied extraction's ASCII-uppercase spelling (the original DOS call
sites may spell their extensions differently) and must match the extracted
directory entry with that exact case even on a case-insensitive host; a
case-only mismatch is a named load error. It reads exact expected byte counts and a checked-in
FNV-1a-64 corruption fingerprint into immutable
`shared_ptr<const vector<byte>>` values. The fingerprint is an integrity
check, not a security signature; provenance/authenticity remains the SHA-256
extraction identity recorded in `research/ASSET_INVENTORY.md`. One
`MainResourceLoader` is one scene scope:
repeated requests return its cached bytes, so no per-frame file I/O occurs.
`th03_assets` receives a caller-borrowed byte span only.

## MAIN direct-VS batch

`direct_vs_resource_keys()` deterministically loads these five static MAIN
entries, then the complete MAINL page handoff required by a direct launch:
`EN2.PI`, both selected character quintuples, and the P2-selected opponent
group. A duplicate selected asset is loaded once.

| Logical role | Extracted path | Bytes | Decoder | Donor authority |
| --- | --- | ---: | --- | --- |
| `main.enemy_scripts` | `YUME2.DAT/ENEDAT.DAT` | 5,072 | `th03_formats::parse_enedat` | `th03/main/enemy/enemy.cpp:287-300` |
| `main.game_font` | `YUME2.DAT/GAMEFT.BFT` | 8,224 | BFNT gaiji | `th03/main/entry.cpp:42-45`; `th03/shiftjis/fnshared.hpp:3` |
| `main.hud.lose_sheet` | `YUME2.DAT/LOSE.BF2` | 4,176 | BFNT superimpose | `th03/main/rstart.cpp:230`; `story_startup[data].asm:135-136` |
| `main.hud.round_sheet` | `YUME2.DAT/ROUND.BF2` | 5,712 | BFNT superimpose | `th03/main/rstart.cpp:231`; `story_startup[data].asm:137-138` |
| `main.hud.result_sheet` | `YUME2.DAT/ZIKICW.BF2` | 4,176 | BFNT superimpose | `th03/main/rstart.cpp:232`; `story_startup[data].asm:139-140` |

For each of the nine characters and both source palette variants, the source
uses the paletted index `NN` from 00 through 17. `hitc_mrs.cpp:29-43` begins
from `00CH.BF2`, patches its two digits from the selected paletted character,
then changes the suffix in place for the two MRS loads.

| Logical role pattern | Extracted path pattern | Bytes each | Decoder |
| --- | --- | ---: | --- |
| `main.character.<character>.paletted_<0|1>.sheet` | `YUME2.DAT/NNCH.BF2` | 4,688 | BFNT superimpose |
| `main.character.<character>.paletted_<0|1>.extra_portrait` | `YUME2.DAT/NNEX.MRS` | 33,120 | MRS |
| `main.character.<character>.paletted_<0|1>.bomb_portrait` | `YUME2.DAT/NNBM.MRS` | 33,120 | MRS |

The direct batch returns each immutable blob together with its manifest spec,
so decoder selection never depends on an undocumented vector position. The
app decodes these supplied byte blobs through `th03_assets`; it does not
put filenames, paths, or a cache into that library. Presentation receives the
decoded models, never this loader.

## MAINL handoff resources included in the direct-VS batch

MAIN receives its hidden Sprite16 source sheet from MAINL's stage transition.
A direct VS launch therefore loads these resources explicitly and reproduces
that page handoff before MAIN startup; it does not invent boss pixels or read
paths inside the presentation library.

| Logical role pattern | Scene | Extracted path | Decoder | Source authority |
| --- | --- | --- | --- | --- |
| `main.character.<character>.paletted_<0|1>.stage_shot` | MAINL stage transition | `YUME1.DAT/NN16.PI`, exact size locked per NN | PI | `th03/mainl/screens[data].asm:54`; `th03/mainl/screens.cpp:355-389` |
| `main.character.<character>.paletted_<0|1>.stage_extra_shot` | MAINL stage transition | `YUME1.DAT/NNEX.PI`, exact size locked per NN | PI | `th03/mainl/screens.cpp:355-389` |
| `main.stage.enemy_center` | MAINL stage transition | `YUME1.DAT/EN2.PI`, 1,841 bytes | PI | `th03/mainl/screens.cpp:293-298` |
| `main.character.<character>.stage_enemy_group` | MAINL stage transition | `YUME1.DAT/ENEMY00.PI` through `ENEMY04.PI`, exact size locked per group | PI | `th03/mainl/screens.cpp:300-324` |

`select.palette_handoff` (`YUME1.DAT/TLSL.RGB`, 48 bytes) remains a checked
manifest entry for the Select-to-MAINL scene boundary, but it is intentionally
excluded from `direct_vs_resource_keys()`. The final opponent PI load supplies
MAINL's palette handoff for direct VS, so loading TLSL there would be redundant.
Specifically, MAINL leaves the selected `ENEMY0n.PI` in PI slot 0, applies that
image's palette, and stores it in the resident handoff. A direct launcher must
therefore apply the P2 opponent-group PI palette and make it the process
baseline before `MainStaticPresenter::round_startup()`; the six-PI staging
helper reproduces pixels only and deliberately does not guess palette policy.

There is no PI or RGB load in the pinned MAIN startup/round-loop path itself:
MAIN receives this resident palette/page handoff, then `rstart.cpp` loads its
three BF2 sheets and `hitc_mrs.cpp` loads selected character data.
`NUM8.BF2` exists in the extraction inventory but has no pinned MAIN
loader/site, so it is deliberately not a direct-VS dependency. The app must
construct the direct-launch raster/palette seed explicitly; the checked PI
inputs reproduce only MAINL's source-owned hidden atlas handoff.

## MAINL Story stage-splash batch

`mainl_stage_resource_keys()` closes the visible Story splash and the hidden
MAIN handoff as one source-ordered resource transaction. It selects one
base-character portrait for each player, the optional numbered-stage sheet,
the full-page base, exactly one top background, and the existing six hidden
atlas inputs. Duplicate player selections are loaded once.

| Logical role pattern | Extracted path | Decoder | Selection rule |
| --- | --- | --- | --- |
| `mainl.gaiji_font` | `YUME2.DAT/MIKOFT.BFT` | BFNT gaiji | Always |
| `mainl.character.<character>.stage_portrait` | `YUME1.DAT/00SL.CD2` through `16SL.CD2` | CDG/CD2 | Base character; palette selects one of two cels |
| `mainl.stage.number_sheet` | `YUME1.DAT/ST.CD2` | CDG/CD2 | Numbered Story backgrounds only; cel is `story_stage + 1` |
| `mainl.stage.base` | `YUME1.DAT/STNX0.PI` | PI | Always |
| `mainl.stage.background.<1..5>` | `YUME1.DAT/STNX1.PI` through `STNX5.PI` | PI | Story numbered, Yumemi, Chiyuri, decisive stage 6, or non-Story |

The background precedence is literal donor policy: non-Story uses `STNX5`,
then Story P2 Chiyuri uses `STNX3`, P2 Yumemi uses `STNX2`, stage 6 uses
`STNX4`, and every other Story stage uses `STNX1` plus `ST.CD2`. The final P2
enemy-group palette and both complete graphics pages become the private Raster
capability consumed by a fresh Story MAIN process. MAIN creates a new TRAM and
installs `GAMEFT.BFT`; MAINL's `MIKOFT.BFT` cells do not cross that boundary.

`main_static_resource_keys()`, `main_private_resource_keys()`, and
`mainl_stage_resource_keys()` expose the campaign-static, per-MAIN, and
per-stage lifetimes separately. Batch loading builds both the result and a
candidate cache before committing either, so a missing or invalid late member
leaves the loader cache unchanged.

`MainResourceLoader` is built as the application-owned `th03_app_resources`
library. It remains separate from filesystem-free decoders and presentation:
the normal constructor enforces both size and fingerprint, while its explicit
size-only policy exists solely for hermetic synthetic tests and inspection
tools, never native app startup. Its tests exercise named missing, wrong-size,
wrong-fingerprint, duplicate-logical-role, case, malformed-key, transactional
late-batch, and resolved-link-escape outcomes using temporary synthetic roots
only.
