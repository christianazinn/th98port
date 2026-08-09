# Provenance map

| native area | behavior/layout authority |
|---|---|
| `core/numeric.hpp` | Anniversary `th01/math/subpixel.hpp`, PC-98 16-bit integer behavior, and the approved fixed-point constraint |
| `formats/t3case.cpp` | `state/port/T3CASE1_FORMAT.md`, `tools/port/t3case.psm1`, and oracle `th03/t3case.hpp` |
| `formats/t3split.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/export_splits.ps1`, and `tools/port/compare_th03_oracle_splits.ps1` |
| `formats/v11.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/rpy_snapshot.psm1`, and `tools/port/normalize_replay.ps1` |
| `replay/session.hpp` | audited T3CASE/V11 input and process-boundary semantics; deliberately independent of either document layout |
| `core/game.cpp` and `core/frame.cpp` | Anniversary `th03/main/rndloop.cpp:sub_9778()` update order and the complete stock-used gameplay callback inventory documented by the `PARITY_*` ledgers |
| `core/state_hash.cpp` and private deterministic state | Anniversary `th03/main/t3case.cpp:t3case_state_hash()` schema 3 plus the semantic structures named in `TH3R03_ROUND_DOSSIER.md` |
| round startup/frame sequencing | Anniversary `th03/main/{rstart.cpp,resscore.cpp,rndloop.cpp,roundcb.cpp}` at `3e2381d4` |
| `core/random.cpp` | Anniversary random-ring call sites and master.lib IRAND behavior, including the adjacent-byte 16-bit cursor quirk |
| `core/trig.cpp` | public-domain master.lib `sin8[data].asm` integer table and PC-98 arithmetic-shift behavior; no host floating point |
| `formats/enedat.cpp` | Anniversary `th03/formations/*`, `th03/enemy/*`, and calibrated original `ENEDAT.DAT` layout; scripts are parsed as content, never copied executable state |

The native code is a clean mechanical reimplementation of documented formats
and semantics. It contains no original game assets, replay corpus, or
framebuffer captures. One explicit 268-byte compatibility profile reproduces
initialized Borland runtime DGROUP bytes at `0x1076..0x1181`. Anniversary
`player_hittest()` can read and bit-mask that region when a broad CPU collision
probe wraps before `_collmap` at `0x4BA0`; replacing the profile with zero or a
boolean occupancy map changes gameplay. The profile is isolated, named, and
mapped to the `cputype`, `fbrk`, `signal`, `_pathops`, `srchenv`, `srchstr`,
`setvbuf`, `sysnerr`, `setenvp`, `strings`, and `loadprog` runtime contributions
shown by the Anniversary-compatible MAIN linker map. It is compiler-runtime
compatibility data, not a game asset or a general resident-memory image.

Phase 2 matches all 46 canonical rows through TH3R03's first natural round
completion. The core consumes 6,773 logical samples, including 47 exact
27-sample warning waits that do not advance either round counter, and returns
route 2 at round frame 5,503/result frame 336. Its final scores and schema-3
hash match Anniversary.

Phase 3 has complete frozen-oracle matches for TH3R00 (715/715 rows, 100,642
samples), TH3R01 (964/964, 135,215), TH3R02 (626/626, 91,939), TH3R03
(186/186, 25,211), TH3R04 (627/627, 86,437), and TH3R05 (783/783, 111,747).
TH3R00 was closed by preserving source fallthrough that releases a pending P2
Ellen Charge Shot in the same update after CPU movement activates Hyper.
TH3R01 was closed by restoring the complete Rikako Boss switch grouping:
source state 6 enters the four-spread callback and consumes its angle/type
draws before emitting eight bullets.

Static parity ledgers cover all nine characters, the complete stock-used frame
pipeline, RNG consumers, numeric semantics, repeat-round reset, and terminal
MAIN/MAINL session mutations. Schema 3 does not identify every hidden
entity/projectile subsystem, so future first divergences must still be
localized with semantic diagnostics rather than inferred from the aggregate
hash. See `PHASE3_CORPUS_STATUS.md` for the live evidence ledger.
