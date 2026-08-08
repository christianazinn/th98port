# Provenance map

| native area | behavior/layout authority |
|---|---|
| `core/numeric.hpp` | Anniversary `th01/math/subpixel.hpp`, PC-98 16-bit integer behavior, and the approved fixed-point constraint |
| `formats/t3case.cpp` | `state/port/T3CASE1_FORMAT.md`, `tools/port/t3case.psm1`, and oracle `th03/t3case.hpp` |
| `formats/t3split.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/export_splits.ps1`, and `tools/port/compare_th03_oracle_splits.ps1` |
| `formats/v11.cpp` | Replay `th03/replay_format.hpp`, `tools/replay/rpy_snapshot.psm1`, and `tools/port/normalize_replay.ps1` |
| `replay/session.hpp` | audited T3CASE/V11 input and process-boundary semantics; deliberately independent of either document layout |
| `core/game.cpp` | Phase 1 boundary only; no gameplay claim is made yet |
| `core/state_hash.cpp` and private deterministic state | Anniversary `th03/main/t3case.cpp:t3case_state_hash()` schema 3 plus the semantic structures named in `TH3R03_ROUND_DOSSIER.md` |
| round startup/frame sequencing | Anniversary `th03/main/{rstart.cpp,resscore.cpp,rndloop.cpp,roundcb.cpp}` at `3e2381d4` |

The native code is a clean mechanical reimplementation of documented formats
and semantics. It contains no original game assets, executable bytes, replay
corpus, framebuffer captures, or broad memory dumps.

Every Phase 2 gameplay subsystem must add its donor files/functions and oracle
coverage here before its implementation claim is considered complete. An
unexplained native divergence stops the parcel; it is not patched in a parser or
comparator to make the gate green.
