# Autonomous completion checklist

This file preserves the requested overnight completion boundary across context
compaction. Do not report the V12 parcel complete merely because the currently
known replay cases are green.

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
