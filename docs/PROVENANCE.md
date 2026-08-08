# Provenance map

| native area | behavior/layout authority |
|---|---|
| `core/numeric.hpp` | Anniversary `th01/math/subpixel.hpp`, PC-98 16-bit integer behavior, and the approved fixed-point constraint |
| `formats/t3case.cpp` | `state/port/T3CASE1_FORMAT.md`, `tools/port/t3case.psm1`, and oracle `th03/t3case.hpp` |
| `formats/t3split.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/export_splits.ps1`, and `tools/port/compare_th03_oracle_splits.ps1` |
| `formats/v11.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/rpy_snapshot.psm1`, and `tools/port/normalize_replay.ps1` |
| `replay/session.hpp` | audited T3CASE/V11 input and process-boundary semantics; deliberately independent of either document layout |
| `core/game.cpp` and `core/frame.cpp` | Anniversary `th03/main/rndloop.cpp:sub_9778()` update order plus player, CPU, shot, enemy, chain/combo, charge-shot, collision-map, HUD-start, and background routines reached by the calibrated TH3R03 prefix |
| `core/state_hash.cpp` and private deterministic state | Anniversary `th03/main/t3case.cpp:t3case_state_hash()` schema 3 plus the semantic structures named in `TH3R03_ROUND_DOSSIER.md` |
| round startup/frame sequencing | Anniversary `th03/main/{rstart.cpp,resscore.cpp,rndloop.cpp,roundcb.cpp}` at `3e2381d4` |
| `core/random.cpp` | Anniversary random-ring call sites and master.lib IRAND behavior, including the adjacent-byte 16-bit cursor quirk |
| `core/trig.cpp` | public-domain master.lib `sin8[data].asm` integer table and PC-98 arithmetic-shift behavior; no host floating point |
| `formats/enedat.cpp` | Anniversary `th03/formations/*`, `th03/enemy/*`, and calibrated original `ENEDAT.DAT` layout; scripts are parsed as content, never copied executable state |

The native code is a clean mechanical reimplementation of documented formats
and semantics. It contains no original game assets, executable bytes, replay
corpus, framebuffer captures, or broad memory dumps.

Phase 2 coverage currently reaches exact hashes at TH3R03 logical samples 128
and 256 and exact scores at sample 384. Every remaining gameplay subsystem must
add its donor files/functions and oracle coverage here before its implementation
claim is considered complete. An
unexplained native divergence stops the parcel; it is not patched in a parser or
comparator to make the gate green.
