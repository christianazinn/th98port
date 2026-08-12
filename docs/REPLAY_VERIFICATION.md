# Historical replay verification

Snapshot: **2026-08-09**.

`th03rv` is the unbounded-speed replay-verification client for `libth03`. It
accepts a V13 `.RPY`, reconstructs a neutral `ReplaySession`, executes the same Classic
gameplay core used by the headless oracle, prints the round states computed by
the port, and compares them with the replay's embedded compact round summaries.
Only finalized files can produce a complete verification result. Non-finalized
`RECORDING`/`PARTIAL` artifacts remain structurally readable for diagnostics,
but never claim a complete match; an active `RECORDING` stream may naturally
end as input-exhausted without a process control.

```powershell
th03rv.exe path\to\replay.RPY
th03rv.exe path\to\replay.RPY --enedat path\to\ENEDAT.DAT
```

The one-argument form searches beside the replay and in development-harness
ancestors. A standalone installation can set `TH03_ENEDAT`. The executable
does not contain or redistribute original game data.

## Dependency boundary

```text
V13 reader -> ReplayDocument -> ReplaySession -> runner -> libth03
                         |
                         +-> expected round splits + report metadata
```

- `th03_formats` parses bytes from a caller-owned span. It performs no file I/O.
- `th03_replay` coordinates recorded pause input and MAIN/MAINL boundaries. It
  knows no V11/V12/V13 layout and no expected scores.
- `th03_core` receives only explicit fixed-width startup/runtime/round state and
  logical input. It contains no replay parser, filesystem, or platform code.
- `th03rv` owns paths, `ENEDAT.DAT` loading, reporting, and exit status.

V13 compact checkpoints, inherited from V12, are recipes rather than native
struct images. Random,
formation, and CPU-charge tables are reconstructed after `GameContent` supplies
the actual formation count. The portable round-state/carry fields are restored
field by field. Practice is represented as the source `GM_VS_1P_CPU` scenario
  plus a small retry/result policy; its byte layout remains in the format
  adapter.

Later V13 round checkpoints are not independent process images. The Replay
Patch resolves a selection to the same process anchor and sequentially
prerolls input unless an optional validated broad-state cache is available.
`th03rv` therefore starts at checkpoint 0 and simulates the complete stream.
At every sequential round boundary it captures a complete format-neutral
round-start observation and compares each preserved checkpoint by logical
sample. Additional retry starts are accepted when a legacy V11 conversion has
no corresponding stored checkpoint; every checkpoint that does exist must be
present in the run and match exactly. Optional accelerators are performance
caches and are deliberately unnecessary for correctness.

V13 also preserves 14-byte round rows with real VSync counts, nine clear-bonus
records, and run-wide Slowdown counters as report-only metadata. None of these
fields initializes, repairs, or steers gameplay. Exact authority, layout, and
retirement gates are recorded in `V13_INTEGRATION.md`.

## V13 structural status

The strict span parser is covered synthetically for all source-backed input
offsets (Story 4,957, ordinary VS 1,489, Practice 2,067), compact checkpoints,
RLE/process controls, Practice consistency, summaries, telemetry, optional
tails, pending sealed streams, and malformed boundaries on GCC and MSVC.
TH03 Data Manager 0.4.2 converted all 19 frozen historical inputs; their V13
sizes and SHA-256 identities are locked in `REPLAY_V13_CORPUS_LOCK.tsv`, and
both compilers reproduce all 173 summaries, all 152 stored checkpoints, and
terminal results. The native V11/V12 parsers are therefore retired. T3CASE and
T3SPLIT remain separate oracle formats.

All reached V13 checkpoints are structurally parsed and preserved, including
their raw pre-reset seed. `th03rv` now compares their complete reconstructed
runtime and round carry against the state reached by process-anchor preroll,
reporting the first checkpoint and field mismatch. A finalized replay can only
report `verification=match` if process execution, round summaries, and every
preserved checkpoint all agree.

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

Original replay files remain ignored outside this repository. Their historical
byte sizes and SHA-256 identities are frozen in `REPLAY_CORPUS_LOCK.tsv`; the
converted V13 identities are frozen in `REPLAY_V13_CORPUS_LOCK.tsv`. Notably,
source V12 TH3R07 and TH3R12—and their V13 conversions—are byte-identical. The
conversion and direct V13 run produced:

| Corpus origin | V13 mode | Samples | Current direct result |
|---|---|---:|---|
| V11 TH3R00 | Story | 100,642 | Match, 12/12 summaries |
| V11 TH3R01 | Story | 135,215 | Match, 13/13 summaries |
| V11 TH3R02 | Story | 91,939 | Match, 12/12 summaries |
| V11 TH3R03 | Story | 25,211 | Match, 4/4 summaries |
| V11 TH3R04 | Story | 86,437 | Match, 13/13 summaries |
| V11 TH3R05 | Story | 111,747 | Match, 13/13 summaries |
| V12 TH3R00 | Story | 90,061 | Match, 11/11 summaries |
| V12 TH3R01 | Story | 102,201 | Match, 11/11 summaries |
| V12 TH3R02 | Story | 30,326 | Match, 5/5 summaries |
| V12 TH3R03 | Story | 119,156 | Match, 12/12 summaries |
| V12 TH3R04 | Story | 118,762 | Match, 13/13 summaries |
| V12 TH3R05 | ordinary VS | 26,213 | Match, 3/3 summaries |
| V12-dir TH3R06 | Story | 99,290 | Match, 11/11 summaries |
| V12 TH3R07 | Story | 101,557 | Match, 9/9 summaries |
| V12 TH3R08 | Story | 103,852 | Match, 13/13 summaries |
| V12 TH3R10 | Practice | 11,256 | Match, 3/3 summaries |
| V12 TH3R11 | Story | 6,254 | Match, 3/3 summaries |
| V12 TH3R12 | Story | 101,557 | Match, 9/9 summaries |
| V12 TH3R13 | Practice | 10,146 | Match, 3/3 summaries |

All 19 Data Manager 0.4.2 V13 files complete on GCC and MSVC. All 173 embedded
summaries and all 152 preserved checkpoints match. Expected summaries and
checkpoints remain comparison-only; neither the format adapter nor replay
runner uses them to repair or steer gameplay.

The semantic gate caught these defects before the 0.4.2 regeneration:

- a V11 conversion clamp of boss rank to 15 instead of TH03's actual maximum
  16 in TH3R00, TH3R01, TH3R04, TH3R05, and the V11-format `v12s/TH3R06`;
- sparse V11 checkpoint directories, which faithfully preserve the stage
  starts available in V11 but cannot contain retry state that V11 never
  recorded;
- missing early-V12 round-0 reset-seed normalization in TH3R07, TH3R11, and
  TH3R12; and
- a native PracticeRules propagation omission, now fixed and covered for every
  checkpoint (TH3R10 and TH3R13 pass).

The required V12 conversion rule is `round_id == 0 ? resident_rand : stored
round_reset_seed`; Data Manager 0.4.2 implements it along with the corrected
Story boss-rank clamp. V11 retry checkpoint synthesis is deliberately omitted:
the verifier instead validates every stored checkpoint and tolerates extra
sequential retry starts. The regenerated identities are frozen in
`REPLAY_V13_CORPUS_LOCK.tsv`.

### Repeat-boundary localization

The five formerly failing files crossed a route-1 repeat before their first
visible context failure. Their recorded/native random-ring cursors at the
relevant round starts were R03 `139/143`, R04 `111/125`, R08 `71/114`, R10 `163/167`, and R13
`132/138`. Earlier repeat starts in the same files agree where present (R04
`149/149` and `27/27`, R08 `191/191`, R10 `109/109`, and R13 `136/136`). The
delta therefore accrues during the preceding active round rather than inside
the repeat reset itself.

The common source-backed cause was the result-banner early return in
`th03/main/hitbox.cpp:hitbox_hittest()`. Native Boss, Enemy, and Fireball paths
already modeled that shared guard, but `extra_attack_bounds_hittest()` did not.
Charge collisions against moving Extra Attacks therefore consumed two visual
hit-circle ring values after the result banner was active. In TH3R03 the first
extra reads occurred at global frames 58,952 and 58,954, producing the exact
four-value retry cursor delta. Applying the guard at the shared Extra Attack
hitbox boundary removes the equivalent drift in all five files without
changing object updates, collision maps, expected data, or replay parsing.

The original uninitialized formation-previous landmine is not the cause in
these recordings. Replaying the exact startup IRAND sequence from every V12
`round_reset_seed` shows that each recorded `formation_first` equals the entry
produced by Anniversary's deterministic repeat predecessor of 1. In the only
relevant case whose first raw candidate is 1, both recording and Anniversary
reject it and accept the same second candidate. Once the first accepted entry
matches, the remaining formation ring is generated identically. Replay Patch
should still receive Anniversary's landmine fix in its next maintenance set,
but changing libth03's deterministic predecessor cannot fix this corpus.

The Replay Patch's three-digit combo display is not one of those gameplay
divergences. Anniversary stores at most 99 in the active display field while
retaining the raw 8-bit chain and defeat-screen maximum; Replay stores the raw
value in the display field as well. Both execute the same unconditional
post-99 timer refresh before that assignment. The Classic core therefore keeps
Anniversary's display state, while New Classic explicitly selects the Replay
Patch field and renderer policy at MAIN creation. Both use the same canonical
normalization without changing score, combo duration, V12 summary verification,
or checkpoint bytes.

V12 TH3R05's source-backed correction was shared bullet-template inheritance.
At sample 3,626, a Kana Boss ring must inherit angle `0x40` from Kana Extra
earlier in the same gameplay prefix; native had hardcoded 0. The rotated ring
altered the collision map and CPU avoidance before the later sample-3,941
context stop. The inherited-angle and post-final-mutation audit is recorded in
`PARITY_GLOBAL.md`; the corrected core consumes all 26,213 samples and matches
all three frozen summaries.
