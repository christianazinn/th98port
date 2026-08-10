# V13 native integration contract

Status: parser implementation authority and migration gate for the frozen
Replay Patch 0.4.8 V13 contract.

## Authority

V13 has two complementary authorities:

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

- `th03_formats` parses a caller-owned byte span and adapts V13 to the existing
  version-neutral `ReplayDocument` and `ReplaySession`.
- `th03_replay` executes that neutral session and owns no V13 byte layout.
- `th03rv` owns file I/O and reporting.
- `th03_core` knows neither V13 nor any replay container.

The parser must not open files, serialize native structs, depend on Turbo C++
padding, or expose PC-98 pointers and process memory. Optional broad retry
accelerators are non-authoritative caches after the logical replay end. A
reader may ignore them and execute the portable checkpoint/input path.

## Current V13 layout

- Magic `T3RPLY13`, version 13, fixed 622-byte header.
- Packed 128-byte base header followed by a 494-byte summary extension.
- The extension contains 27 14-byte round splits, nine 10-byte stage-clear
  bonus records, run-wide timed/slow frame counters, and the reached-checkpoint
  directory.
- Story reserves 15 checkpoints, ordinary VS reserves 3, and Practice reserves
  5. Every checkpoint is the existing 289-byte compact deterministic record.
- Every reached checkpoint is preserved in the neutral `ReplayDocument`,
  including its stage/round identity, sample/global/input cursors, complete
  `GameInit` recipe, and the raw V13 pre-reset seed. Historical V12 seed
  normalization belongs to the external one-way converter.
- `snapshot_offset` equals 622. `input_offset` is the header plus the complete
  scenario checkpoint reservation: 4,957 for Story, 1,489 for ordinary VS,
  and 2,067 for Practice.
- The packet-RLE input/control stream is unchanged: gameplay, interstitial,
  and explicit MAIN/MAINL controls all feed the neutral session model.
- V13 requires the complete `0x007F` summary capability set in both the base
  summary flags and extension flags.

Header summaries and telemetry are comparison/reporting data. They must never
initialize, repair, or steer simulated gameplay except for the explicit
scenario/start/checkpoint fields already defined by the format.

### Authority correction pending upstream

The prose tables in the current `FORMAT.md` snapshot list the three V13 input
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

These tests establish that the adapter implements the written format. The
Data Manager 0.4.0 converter at harness commit
`b20651e22c2a1f10adf5d8e41947bc60a4b0b1c6` then converted the complete locked
six-file V11 and thirteen-file V12 corpus. The converted sizes and SHA-256
identities are frozen in `REPLAY_V13_CORPUS_LOCK.tsv`; both GCC and MSVC
`th03rv` reproduced all 173 embedded summaries and terminal results.

This is full sequential playback from each replay's process anchor. All later
round checkpoints are parsed and preserved, but the run does not yet compare
their complete reconstructed state at the corresponding sequential preroll
boundaries. That semantic comparison is required before the selectable-start
UI is accepted.

## Historical-format retirement gate

The V11/V12 parsers were temporary validation inputs. Their retirement gate
was:

1. Convert every file in the locked 19-file V11/V12 corpus without modifying
   the originals.
2. Freeze the converted V13 size and SHA-256 identities externally.
3. Run every converted file through the V13-only `th03rv` path on GCC and MSVC.
4. Reproduce all 173 embedded round summaries and every existing terminal
   result.
5. Add malformed/corruption coverage at the V13 boundary.

That gate is green. Native V11/V12 headers, parsers, dispatch, and direct
format tests are removed; old files must pass through the external Data Manager
converter. T3CASE and T3SPLIT remain independent PC-98 oracle formats and are
not retired by this migration.

## Remaining V13 gate

The format, converter, converted-corpus identities, full checkpoint collection,
and full-run verifier are now frozen. Before a playable replay browser claims
arbitrary checkpoint selection, expose a format-neutral complete round-start
observation and compare every preserved record with state reached by sequential
anchor preroll. Optional broad accelerators remain caches and cannot replace
that portable proof.
