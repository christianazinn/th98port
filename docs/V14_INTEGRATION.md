# V14 native integration contract

Status: parser implementation authority and migration gate for the V14
contract retained by Replay Patch 0.4.18.

## Authority

V14 has two complementary authorities:

1. The canonical semantic and byte-layout schema is
   `tools/replay/FORMAT.md` in the replay-validation harness at outer commit
   `551bbd97204e1aeeec8264cc83617d5c5e669ab2`. Its SHA-256 at this checkpoint
   is `571970292F441041172A05390FDA92AA996EF5B5CC163D04048BB744DAD9D42E`.
2. Exact packed structures, constants, capacities, and game-side validation
   behavior are defined by the Replay Patch at ReC98 commit
   `5cc773e85c7db97c1c09888fa5531faa392de136`, primarily
   `th03/replay_format.hpp`. That header's SHA-256 is
   `0EE16E826E9C7303202CA52CF1CCD68B2C2E7EEE858661E7D09739F8B5BE664A`.

Replay Patch 0.4.18 at ReC98 commit
`44f1184c3c7dc7b8080316640c8cbc1cd5336201` is the current product donor. It
retains this layout while explicitly accepting arbitrary bytes in the final
106-byte extension.

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

- Magic `T3RPLY14`, version 14, fixed 896-byte header.
- Packed 128-byte base header followed by a 494-byte summary extension and a
  274-byte identity extension. The latter stores a compact ruleset/recording
  descriptor, local role/source, two 16-byte profile UUIDs, a 16-byte match
  ID, two length-prefixed 57-byte fallback nametag slots, and a 106-byte
  opaque extension. UUID profile names remain authoritative; the serialized
  names are match-time/offline fallbacks. Public readers assign no meaning to
  the opaque extension and parse/write transformations preserve it exactly.
  Publicly generated documents initialize it to zero unless an application
  release supplies its own product-owned writer.
- The extension contains 27 14-byte round splits, nine 10-byte stage-clear
  bonus records, run-wide timed/slow frame counters, and the reached-checkpoint
  directory.
- Story reserves 15 checkpoints, ordinary VS reserves 3, and Practice reserves
  5. Every checkpoint is the existing 289-byte compact deterministic record.
- Every reached checkpoint is preserved in the neutral `ReplayDocument`,
  including its stage/round identity, sample/global/input cursors, complete
  `GameInit` recipe, and the raw V14 pre-reset seed. Historical V12 seed
  normalization belongs to the external one-way converter.
- `snapshot_offset` equals 896. `input_offset` is the header plus the complete
  scenario checkpoint reservation: 5,231 for Story, 1,763 for ordinary VS,
  and 2,341 for Practice.
- The packet-RLE input/control stream is unchanged: gameplay, interstitial,
  and explicit MAIN/MAINL controls all feed the neutral session model.
- V14 requires the complete `0x007F` summary capability set in both the base
  summary flags and extension flags.

Header summaries and telemetry are comparison/reporting data. They must never
initialize, repair, or steer simulated gameplay except for the explicit
scenario/start/checkpoint fields already defined by the format.

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
Manager 0.5.6 converts V11, V12, and V13 to V14. The locked integration corpus
contains the 19 historical V11/V12 conversions plus the genuine V13 WR
TH3R06. The historical subset reproduces all 173 embedded summaries and all
152 stored checkpoints; the WR reproduces its 13 summaries. Converted sizes
and SHA-256 identities are frozen in `REPLAY_V14_CORPUS_LOCK.tsv`; neither the
converter executable nor converted replay bytes are stored here.

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

The V11/V12/V13 parsers were temporary validation inputs. Their retirement gate
was:

1. Convert every file in the locked 19-file V11/V12 corpus and the genuine
   V13 WR without modifying the originals.
2. Freeze the converted V14 size and SHA-256 identities externally.
3. Run every converted file through the V14-only `th03rv` path on GCC and MSVC.
4. Reproduce all 186 embedded round summaries and every existing terminal
   result.
5. Add malformed/corruption coverage at the V14 boundary.

The summary/terminal portion of that gate is green. Native V11/V12/V13 headers,
parsers, dispatch, and direct
format tests are removed; old files must pass through the external Data Manager
converter. T3CASE and T3SPLIT remain independent PC-98 oracle formats and are
not retired by this migration.

## V14 gate result

The observation and stored-checkpoint comparison implementation are complete.
The Data Manager 0.5.6 corpus passes 20/20 finalized runs on GCC and MSVC. The
historical subset passes 173/173 summaries and all 152 preserved checkpoints;
the genuine WR passes 13/13 summaries. A replay browser may offer every
preserved checkpoint; converted V11 files naturally expose stage starts only.
Optional broad accelerators remain caches and cannot replace this portable
proof.
