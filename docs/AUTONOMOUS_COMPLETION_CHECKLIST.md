# Autonomous completion checklist

This file preserves the requested overnight completion boundary across context
compaction. Do not report the V12 parcel complete merely because the currently
known replay cases are green.

## Completion record

The requested gameplay/replay boundary is complete as of 2026-08-09:

- all 19 locked V11/V12 files complete and all 173 embedded summaries match;
- the complete Anniversary gameplay source, stock ENEDAT program, relevant
  assembly/BSS/data, all nine character families, renderer-fed deterministic
  effects, modes, and lifecycle paths have a durable disposition in
  `PARITY_SOURCE_COVERAGE.md` and the supporting parity ledgers;
- no stock-reachable gameplay or shared-RNG discrepancy remains;
- invalid rank/demo-mode combinations are rejected and Story resident score
  initialization now follows the source's P1-only behavior;
- GCC, MSVC Debug, and MSVC Release builds and CTest pass; the host T3CASE,
  normalizer, comparator, and split-export suites pass; and the final MSVC
  Release verifier passes the complete external replay lock;
- the tested MSVC Release `th03rv.exe` is 227,328 bytes with SHA-256
  `97D3BBAE08F2184B22CA94A68FA110CC89DFE4DCD73A3F0FABB28D36CAE67E4D`;
  it uses the normal MSVC/UCRT runtime dependencies; and
- `POST_CORE_ROADMAP.md` records the V13, Replay Patch, resource-manager,
  asset, presentation, audio, application, leaderboard, and Arrange-mode
  recommendations.

The stock live-human pause/Q interaction is implemented as a staged core and
application protocol: release, choose Q or Resume, then release again without
advancing gameplay. Recorded Replay Patch pauses remain a separate replay-
coordinator contract. Continue adding replay fixtures as integration evidence;
new mismatches must still be diagnosed from source rather than repaired from
expected summaries.

## Immediate gameplay work

1. Diagnose every open V12 replay at its first source-backed divergence.
2. Keep Anniversary as the Classic gameplay authority. Replay-format metadata,
   checkpoints, or expected summaries must never repair or steer simulation.
3. Raise genuine Anniversary-to-Replay gameplay differences immediately;
   presentation-only Replay changes may remain adapter/client concerns.
4. Keep no-input emulator runs silent and asynchronous. Remove all temporary
   probes, donor edits, emulator images, logs, and native diagnostics when their
   evidence has been recorded.
5. Run both compiler test suites and the complete locked external V11/V12
   corpus before publishing the final verifier.

## Required final gameplay audit

After the known corpus is closed or classified, perform a comprehensive static
sweep of the complete original TH03 ReC98 gameplay codebase against libth03.
The audit must cover all characters, modes, callbacks, frame phases, reset and
process paths, renderer-named gameplay/RNG side effects, fixed-width arithmetic,
shared template/global state, collision/allocation order, results, and all
stock-used data-driven paths—not merely routines reached by the current corpus.
Record a durable source-to-native parity matrix, fix every source-backed
nonblocking gameplay omission, and rerun all earlier gates.

## Final handoff and recommendations

Before returning to the user:

- rebuild and smoke the Release `th03rv.exe` deliverable;
- commit and push the harness branch and the port-only standalone subtree;
- report exact corpus coverage, limitations, executable identity, and commands;
- recommend the next Replay Patch and V13 workflow, including the correct
  harness/ReC98 worktrees and branches, merge order, normalizer/resource-manager
  changes, and the stable replay-format boundary in libth03;
- remind the user to apply Anniversary's deterministic formation-previous
  landmine fix to the Replay Patch during that next change set, even if it is
  not the cause of the current replay divergences; and
- provide a concrete broader-port roadmap for asset extraction/conversion
  (including DAT contents and PI/BFT-family formats via `thdat`), authentic
  rendering and input with broad old-hardware compatibility, PMD-compatible
  music/SFX playback without WAV substitution, persistence/configuration, and
  later application clients.

Implement safe, nonblocking next steps that stay within the gameplay/replay
infrastructure scope. Leave work that requires a product choice, user input, or
a significant move into renderer/audio/application development as explicit,
ranked recommendations rather than silently expanding scope.
