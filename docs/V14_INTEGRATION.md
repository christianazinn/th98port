# V14 native integration contract

Status: parser implementation authority and migration gate for the frozen
Replay Patch 0.4.8 V14 contract.

## Authority

V14 has two complementary authorities:

1. The canonical semantic and byte-layout schema is
   `tools/replay/FORMAT.md` in the replay-validation harness at outer commit
   `b20651e22c2a1f10adf5d8e41947bc60a4b0b1c6`. Its SHA-256 at this checkpoint
   is `DE36BF752EC204B19F8DE49B070AE8E2901952295E4499A6CF45B6531031BB61`.
2. Exact packed structures, constants, capacities, and game-side validation
   behavior are defined by the Replay Patch at ReC98 commit
   `10f40904657e22dbabe7da7164a57c10da0d0fa6`, primarily
   `th03/replay_format.hpp`. That header's SHA-256 is
   `C7FD38D59D8A0DC12670E54F7A43C828B0D7AD0EAD279D4236C60CC35E6A7D32`.

The complete validation call sites are
`th03/main/replay.cpp::replay_user_header_valid()` and its checkpoint reader,
`th03/mainl/replml.cpp::mainl_replay_user_header_valid()`, and
`th03/op_01.cpp::replay_user_header_valid()` plus the menu checkpoint reader.
The descriptive schema governs field meaning; the packed header and game-side
readers govern exact width, offset, capacity, and accepted combinations. An
unexplained conflict between them is a stop condition, not permission to guess.

## Native ownership

- `th03_formats` parses a caller-owned byte span and adapts V14 to the existing
  version-neutral `ReplayDocument` and `ReplaySession`.
- `th03_replay` executes that neutral session and owns no V14 byte layout.
- `th03rv` owns file I/O and reporting.
- `th03_core` knows neither V14 nor any replay container.

The parser must not open files, serialize native structs, depend on Turbo C++
padding, or expose PC-98 pointers and process memory. Optional broad retry
accelerators are non-authoritative caches after the logical replay end. A
reader may ignore them and execute the portable checkpoint/input path.

## Current V14 layout

- Magic `T3RPLY14`, version 13, fixed 622-byte header.
- Packed 128-byte base header followed by a 494-byte summary extension.
- The extension contains 27 14-byte round splits, nine 10-byte stage-clear
  bonus records, run-wide timed/slow frame counters, and the reached-checkpoint
  directory.
- Story reserves 15 checkpoints, ordinary VS reserves 3, and Practice reserves
  5. Every checkpoint is the existing 289-byte compact deterministic record.
- Every reached checkpoint is preserved in the neutral `ReplayDocument`,
  including its stage/round identity, sample/global/input cursors, complete
  `GameInit` recipe, and the raw V14 pre-reset seed. Historical V12 seed
  normalization belongs to the external one-way converter.
- `snapshot_offset` equals 622. `input_offset` is the header plus the complete
  scenario checkpoint reservation: 4,957 for Story, 1,489 for ordinary VS,
  and 2,067 for Practice.
- The packet-RLE input/control stream is unchanged: gameplay, interstitial,
  and explicit MAIN/MAINL controls all feed the neutral session model.
- V14 requires the complete `0x007F` summary capability set in both the base
  summary flags and extension flags.

Header summaries and telemetry are comparison/reporting data. They must never
initialize, repair, or steer simulated gameplay except for the explicit
scenario/start/checkpoint fields already defined by the format.

### Authority correction pending upstream

The prose tables in the current `FORMAT.md` snapshot list the three V14 input
offsets as 4,949, 1,481, and 2,059. Those values are each eight bytes too small.
The document's explicit 622-byte header schema and the authoritative
`replay_user_input_offset()` calculation both yield 622 + (capacity * 289),
which gives the values above. Native tests lock the explicit/source-backed
calculation. The canonical document should be corrected on its owning Replay
Patch branch; this port worktree must not silently alter that separate branch.

## Structural and full-run exit gate

Synthetic tests cover:

- exact magic/version/header/checkpoint/input offsets and scenario capacities;
- mandatory RLE, Charge, and `0x007F` capability bits;
- Story, Practice, and all ordinary VS modes;
- Practice configuration and checkpoint/header consistency;
- checkpoint count/directory bounds, anchor cursors, round state, and retry
  carry validation;
- 14-byte split and 10-byte clear-bonus parsing, packed scores, and telemetry;
- packet-RLE gameplay, interstitial, and process-control decoding;
- sealed in-progress `RECORDING` streams without a terminal process control,
  while `PARTIAL` and `FINALIZED` streams still require that control;
- truncated, overlapping, overflowed, unsupported, and inconsistent layouts;
- ignoring an optional tail only after the declared logical input range.

These tests establish that the adapter implements the written format. Data
Manager 0.4.2 converted the complete locked six-file V11 and thirteen-file V12
corpus. The converted sizes and SHA-256 identities are frozen in
`REPLAY_V14_CORPUS_LOCK.tsv`; the semantic gate reproduces all 173 embedded
summaries, all 152 stored checkpoints, and terminal results. The externally
preserved 0.4.2 Windows converter used for regeneration is 13,214,558 bytes,
SHA-256 `2924fbfbbf792ac18ff548bf2f287ad10377d87e3273ab3132076e2727043ae8`;
neither that executable nor the converted replay bytes are stored here.

`th03_core` now exposes a format-neutral `RoundStartObservation` immediately
after initial construction and every repeat reset. It contains all resident
start fields, the effective pre-reset seed, the complete reconstructed random,
formation, CPU-charge, cursor, and player-start runtime, and the complete
round-state/carry record. It contains no compressed input cursor, decoder
latch, presentation state, or format identifier. The replay runner records
these observations after initial `Game::create()`, `start_next_round()`, and
the next Story-stage `Game::create()`. `th03rv` independently constructs the
expected observation from every neutral checkpoint recipe and compares it to
the sequential observation keyed by logical sample. It reports the first
checkpoint and exact semantic field mismatch and never uses `CanonicalState`'s
sparse hash.

This stronger gate exposed conversion defects that the lossy round summaries
could not, plus one deliberate legacy-compatibility boundary:

- Data Manager 0.4.0 synthesizes V11 Story boss rank with a maximum of 15,
  while TH03's `GBA_BOSS_LEVEL_MAX` and Anniversary behavior use 16. The first
  mismatch appears at stage-9 Lunatic checkpoint 9 in converted V11 TH3R00,
  TH3R01, TH3R04, TH3R05, and the V11-format file named `v12s/TH3R06.RPY`.
- V11 only stored one selectable checkpoint per reached Story stage, so a
  lossless structural conversion cannot invent retry-round checkpoint state.
  The verifier therefore validates every preserved checkpoint exactly while
  accepting additional sequential retry starts that have no directory entry.
  This is a narrow legacy-conversion policy: native V14 writers still record
  every reached round, and no stored checkpoint may be missing from the run or
  differ semantically.
- Early V12 recorders could overwrite a round-0 checkpoint's reset-seed field.
  The established compatibility rule is `round_id == 0 ? resident_rand :
  stored round_reset_seed`. Data Manager 0.4.0 copied these fields unchanged,
  making later checkpoints in converted V12 TH3R07, TH3R11, and TH3R12
  unreconstructable.
- The native V14 adapter initially attached the validated Practice retry policy
  only to checkpoint 0. It now attaches that same neutral policy to every
  Practice checkpoint; focused tests and TH3R10/TH3R13 pass this part of the
  semantic gate.

These were conversion/adapter defects, not gameplay mismatches. Data Manager
0.4.2 fixes the Story boss-rank clamp and early-V12 reset-seed conversion. V11
retry synthesis is explicitly out of scope: the source format did not contain
that state, its UI never offered those checkpoints, and no future V11
recordings are expected.

## Historical-format retirement gate

The V11/V12 parsers were temporary validation inputs. Their retirement gate
was:

1. Convert every file in the locked 19-file V11/V12 corpus without modifying
   the originals.
2. Freeze the converted V14 size and SHA-256 identities externally.
3. Run every converted file through the V14-only `th03rv` path on GCC and MSVC.
4. Reproduce all 173 embedded round summaries and every existing terminal
   result.
5. Add malformed/corruption coverage at the V14 boundary.

The summary/terminal portion of that gate is green. Native V11/V12 headers,
parsers, dispatch, and direct
format tests are removed; old files must pass through the external Data Manager
converter. T3CASE and T3SPLIT remain independent PC-98 oracle formats and are
not retired by this migration.

## V14 gate result

The observation and stored-checkpoint comparison implementation are complete.
The Data Manager 0.4.2 corpus passes 19/19 finalized runs, 173/173 summaries,
and all 152 preserved checkpoints on GCC and MSVC. A replay browser may offer
every preserved checkpoint; converted V11 files naturally expose stage starts
only. Optional broad accelerators remain caches and cannot replace this
portable proof.
