# Historical replay verification

Snapshot: **2026-08-08**.

`th03rv` is the unbounded-speed replay-verification client for `libth03`. It
accepts a historical V11 or definitive V12 `.RPY`, reconstructs a neutral
`ReplaySession`, executes the same Classic gameplay core used by the headless
oracle, prints the round states computed by the port, and compares them with
the replay's embedded compact round summaries.

```powershell
th03rv.exe path\to\replay.RPY
th03rv.exe path\to\replay.RPY --enedat path\to\ENEDAT.DAT
```

The one-argument form searches beside the replay and in development-harness
ancestors. A standalone installation can set `TH03_ENEDAT`. The executable
does not contain or redistribute original game data.

## Dependency boundary

```text
V11 reader ----\
                -> ReplayDocument -> ReplaySession -> replay runner -> libth03
V12 reader ----/          |
                          +-> expected compact round splits

future V13 reader --------^  (same document/session boundary)
```

- `th03_formats` parses bytes from a caller-owned span. It performs no file I/O.
- `th03_replay` coordinates recorded pause input and MAIN/MAINL boundaries. It
  knows no V11/V12 layout and no expected scores.
- `th03_core` receives only explicit fixed-width startup/runtime/round state and
  logical input. It contains no replay parser, filesystem, or platform code.
- `th03rv` owns paths, `ENEDAT.DAT` loading, reporting, and exit status.

V12 compact checkpoints are recipes, not native struct images. Random,
formation, and CPU-charge tables are reconstructed after `GameContent` supplies
the actual formation count. The portable round-state/carry fields are restored
field by field. Practice is represented as the source `GM_VS_1P_CPU` scenario
plus a small retry/result policy; its byte layout remains in the V12 adapter.

Later V12 round checkpoints are not independent process images. The Replay
Patch resolves a selection to the same process anchor and sequentially
prerolls input unless an optional validated broad-state cache is available.
`th03rv` therefore starts at checkpoint 0 and simulates the complete stream.
Optional accelerators are performance caches and are deliberately unnecessary
for correctness.

## What a match proves

Each replay summary contains stage/round, route, winner, and two lossy packed
eight-digit scores captured after `replay_route()`. `th03rv` derives the same
values from simulated state; it never uses them to initialize, repair, or steer
gameplay. Source score packing applies `% 10` to each raw digit, which the
comparison preserves.

A complete match proves that the recorded logical input ran through every
recorded process boundary and produced every recorded round result. These
summaries are much sparser than schema-3 T3SPLIT checkpoints, so they extend
coverage but do not replace the frozen Anniversary oracle.

## Current external corpus

Original replay files remain ignored outside this repository. Their byte sizes
and SHA-256 identities are frozen in `REPLAY_CORPUS_LOCK.tsv`; notably, V12
TH3R07 and TH3R12 are byte-identical. The first direct run over `corpus/v11s`
and `corpus/v12s` produced:

| Corpus file | Format/mode | Samples | Current direct result |
|---|---|---:|---|
| V11 TH3R00 | V11 Story | 100,642 | Match, 12/12 summaries |
| V11 TH3R01 | V11 Story | 135,215 | Match, 13/13 summaries |
| V11 TH3R02 | V11 Story | 91,939 | Match, 12/12 summaries |
| V11 TH3R03 | V11 Story | 25,211 | Match, 4/4 summaries |
| V11 TH3R04 | V11 Story | 86,437 | Match, 13/13 summaries |
| V11 TH3R05 | V11 Story | 111,747 | Match, 13/13 summaries |
| V12 TH3R00 | V12 Story | 90,061 | Match, 11/11 summaries |
| V12 TH3R01 | V12 Story | 102,201 | Match, 11/11 summaries |
| V12 TH3R02 | V12 Story | 30,326 | Match, 5/5 summaries |
| V12 TH3R03 | V12 Story | 119,156 | Open after 7 exact summaries; first context divergence at sample 59,608 |
| V12 TH3R04 | V12 Story | 118,762 | Open after 11 exact summaries; first context divergence at sample 96,307 |
| V12 TH3R05 | V12 ordinary VS | 26,213 | Match, 3/3 summaries |
| V12-dir TH3R06 | V11 Story | 99,290 | Match, 11/11 summaries |
| V12 TH3R07 | V12 Story | 101,557 | Match, 9/9 summaries |
| V12 TH3R08 | V12 Story | 103,852 | Open after 5 exact summaries; first context divergence at sample 34,638 |
| V12 TH3R10 | V12 Practice | 11,256 | Open after 2 exact summaries; first context divergence at sample 8,611 |
| V12 TH3R11 | V12 Story | 6,254 | Match, 3/3 summaries |
| V12 TH3R12 | V12 Story | 101,557 | Match, 9/9 summaries |
| V12 TH3R13 | V12 Practice | 10,146 | Open after 2 exact summaries; first context divergence at sample 6,820 |

The open rows are gameplay/input-context evidence, not parser failures. They
must be diagnosed at their first source-backed divergence; neither the format
adapter nor the expected summaries may hide them.

The Replay Patch's three-digit combo display is not one of those gameplay
divergences. Anniversary stores at most 99 in the active display field while
retaining the raw 8-bit chain and defeat-screen maximum; Replay stores the raw
value in the display field as well. Both execute the same unconditional
post-99 timer refresh before that assignment. The Classic core therefore keeps
Anniversary's display state and canonical normalization without changing score,
combo duration, or V12 summary verification.

V12 TH3R05's source-backed correction was shared bullet-template inheritance.
At sample 3,626, a Kana Boss ring must inherit angle `0x40` from Kana Extra
earlier in the same gameplay prefix; native had hardcoded 0. The rotated ring
altered the collision map and CPU avoidance before the later sample-3,941
context stop. The inherited-angle and post-final-mutation audit is recorded in
`PARITY_GLOBAL.md`; the corrected core consumes all 26,213 samples and matches
all three frozen summaries.
